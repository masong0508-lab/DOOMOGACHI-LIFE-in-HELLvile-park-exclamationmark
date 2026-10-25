"""show check-game [--full] - the game-unchanged gate (spec 4.4; AT1.1).

Run it before every push.

  1. Paths (milliseconds). Every file changed between the merge-base with
     the shared branch and HEAD, and every uncommitted change in the work
     tree, must be under show/. The game dirs (core gba ps1 tools docs/show
     trailer) and the repo root are never touched by a show commit.
  2. --full (minutes). Build the game's own `ps1` preset twice into a
     scratch dir - from the merge-base and from HEAD (both exported with
     `git archive`, so only committed files count) - and compare the SHA-256
     of doomogachi.exe, OVL.BIN and TEXT.BIN. Never the .bin disc image:
     mkpsxiso stamps it with the build time (spec 4.4, C24). A build's hashes
     are cached per game-tree id in show/build/check-game/, so the base is
     built once per game change.

    show check-game                       paths only
    show check-game --full [--jobs 2]     paths + the hash build
    show check-game --base REF            compare with REF instead of origin/<shared branch>
Exit 0 = pass. Python API: path_check(base) -> [Diag], full_check(base) -> [Diag].
"""
import hashlib
import io
import json
import os
import shutil
import subprocess
import tarfile
import time

from .. import errors, paths
from ..errors import Diag

GAME_OUTPUTS = ("doomogachi.exe", "OVL.BIN", "TEXT.BIN")
GAME_TREE_DIRS = ("core", "ps1", "tools")      # what the ps1 preset reads
MIN_FREE_GB = 3.0


def git(*args, check=True):
    p = subprocess.run(["git"] + list(args), cwd=str(paths.REPO), stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
    if check and p.returncode:
        errors.fail("GATE-GIT", "git %s failed: %s" % (" ".join(args), p.stderr.decode(errors="replace").strip()))
    return p.stdout


def merge_base(base_ref):
    if not git("rev-parse", "--verify", "--quiet", base_ref + "^{commit}", check=False).strip():
        errors.fail("GATE-GIT", "no such ref %s (git fetch origin %s first)" % (base_ref, paths.SHARED_BRANCH))
    return git("merge-base", "HEAD", base_ref).decode().strip()


def _outside_show(p):
    return not p.startswith(paths.SHOW_PREFIX)


def path_check(base_ref):
    """spec 4.4 step 1: [Diag] for every changed path outside show/"""
    mb = merge_base(base_ref)
    diags = []
    changed = [p for p in git("diff", "--name-only", "-z", mb, "HEAD").decode().split("\0") if p]
    for p in changed:
        if _outside_show(p):
            diags.append(Diag("GATE-PATH", "committed since the merge-base %s: a show commit may only "
                              "change files under show/" % mb[:10], file=paths.REPO / p))
    st = git("status", "--porcelain=v1", "-z", "--untracked-files=all").decode().split("\0")
    i = 0
    while i < len(st):
        e = st[i]
        i += 1
        if len(e) < 4:
            continue
        code, p = e[:2], e[3:]
        if code[0] in "RC":            # renames carry the old path next
            i += 1
        if _outside_show(p):
            diags.append(Diag("GATE-PATH", "uncommitted change (%s) outside show/: it must not reach a "
                              "show commit" % code.strip(), file=paths.REPO / p))
    return diags, mb, len(changed)


def tree_key(rev):
    ids = []
    for d in GAME_TREE_DIRS:
        ids.append(git("rev-parse", "%s:%s" % (rev, d)).decode().strip())
    return hashlib.sha1(" ".join(ids).encode()).hexdigest()[:16]


def _export(rev, dest):
    data = git("archive", "--format=tar", rev, *GAME_TREE_DIRS)
    with tarfile.open(fileobj=io.BytesIO(data)) as t:
        t.extractall(dest, filter="data")


def _sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def build_game(rev, label, jobs, work, log):
    """build the game's ps1 preset of `rev` into work/; {output: sha256}"""
    src, bdir = work / ("src-" + label), work / ("build-" + label)
    for d in (src, bdir):
        if d.exists():
            shutil.rmtree(d)
    src.mkdir(parents=True)
    _export(rev, src)
    env = paths.env_for_build()
    t0 = time.time()
    for cmd in (["cmake", "--preset", "ps1", "-B", str(bdir)],
                ["cmake", "--build", str(bdir), "-j", str(jobs), "--target", "doomogachi", "textbin"]):
        p = subprocess.run(cmd, cwd=str(src / "ps1"), env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
        log.write("$ %s\n%s\n" % (" ".join(cmd), p.stdout))
        if p.returncode:
            errors.fail("GATE-BUILD", "the game's ps1 build of %s failed (log: %s):\n%s"
                        % (rev[:10], paths.rel(work / "build.log"), p.stdout[-2000:]))
    out = {}
    for name in GAME_OUTPUTS:
        f = bdir / name
        if not f.exists():
            errors.fail("GATE-BUILD", "the game's ps1 build of %s made no %s" % (rev[:10], name))
        out[name] = _sha(f)
    out["_seconds"] = round(time.time() - t0, 1)
    return out


def _disk_free_gb(p):
    u = shutil.disk_usage(str(p))
    return u.free / 1e9


def full_check(base_ref, jobs=2, keep=False, rebuild_base=False, out=print):
    """spec 4.4 step 2: [Diag] if any game output hash differs"""
    mb = merge_base(base_ref)
    head = git("rev-parse", "HEAD").decode().strip()
    cache_dir = paths.BUILD / "check-game"
    cache_dir.mkdir(parents=True, exist_ok=True)
    # one work dir per run: several agents run the gate at once, and a shared
    # dir let one run delete another's half-built tree ("made no doomogachi.exe")
    work = paths.SHOW_WORK / "check-game" / ("run-%d" % os.getpid())
    work.mkdir(parents=True, exist_ok=True)
    free = _disk_free_gb(work)
    if free < MIN_FREE_GB:
        errors.fail("GATE-DISK", "%.1f GB free in %s; the game build needs a few hundred MB and the "
                    "brief stops work under %.0f GB" % (free, work, MIN_FREE_GB))
    kb, kh = tree_key(mb), tree_key(head)
    results = {}
    with open(work / "build.log", "w") as log:
        cached = cache_dir / ("%s.json" % kb)
        if cached.exists() and not rebuild_base:
            results["base"] = json.loads(cached.read_text())
            out("check-game: base %s (game tree %s): cached hashes from %s"
                % (mb[:10], kb, results["base"].get("_built", "?")))
        else:
            out("check-game: building the game's ps1 preset at the merge-base %s (game tree %s) ..." % (mb[:10], kb))
            results["base"] = build_game(mb, "base", jobs, work, log)
            results["base"]["_built"] = "%s at %s" % (mb[:10], time.strftime("%Y-%m-%d %H:%M"))
            cached.write_text(json.dumps(results["base"], indent=1))
        out("check-game: building the game's ps1 preset at HEAD %s (game tree %s) ..." % (head[:10], kh))
        results["head"] = build_game(head, "head", jobs, work, log)
        results["head"]["_built"] = "%s at %s" % (head[:10], time.strftime("%Y-%m-%d %H:%M"))
        (cache_dir / ("%s.json" % kh)).write_text(json.dumps(results["head"], indent=1))
    if not keep:
        shutil.rmtree(work, ignore_errors=True)
    diags = []
    out("  %-15s %-16s %-16s" % ("output", "merge-base", "HEAD"))
    for name in GAME_OUTPUTS:
        a, b = results["base"][name], results["head"][name]
        out("  %-15s %-16s %-16s %s" % (name, a[:16], b[:16], "same" if a == b else "DIFFERENT"))
        if a != b:
            diags.append(Diag("GATE-HASH", "%s differs between the merge-base %s and HEAD %s: the show "
                              "branch changed the game build" % (name, mb[:10], head[:10])))
    out("  (HEAD build %.0f s with -j%d)" % (results["head"]["_seconds"], jobs))
    (cache_dir / "last.json").write_text(json.dumps({"base": mb, "head": head, "results": results,
                                                     "pass": not diags}, indent=1))
    return diags


def run(args):
    base = args.base or paths.SHARED_REMOTE_REF
    diags, mb, n = path_check(base)
    if diags:
        errors.report(diags)
        print("check-game: FAIL (paths)")
        return 1
    print("check-game: paths PASS (%d file(s) changed since the merge-base %s, all under show/; "
          "the work tree is clean outside show/)" % (n, mb[:10]))
    if args.full:
        diags = full_check(base, args.jobs, args.keep, args.rebuild_base)
        if diags:
            errors.report(diags)
            print("check-game: FAIL (hashes)")
            return 1
        print("check-game: full PASS (doomogachi.exe, OVL.BIN, TEXT.BIN identical)")
    return 0


def register(sub):
    p = sub.add_parser("check-game", help="the game-unchanged gate (spec 4.4): paths; --full adds the hash build")
    p.add_argument("--full", action="store_true", help="also build the game twice and compare EXE/OVL.BIN/TEXT.BIN")
    p.add_argument("--base", help="the ref to compare with (default origin/%s)" % paths.SHARED_BRANCH)
    p.add_argument("--jobs", "-j", type=int, default=2)
    p.add_argument("--keep", action="store_true", help="keep the scratch sources and builds")
    p.add_argument("--rebuild-base", action="store_true", help="ignore the cached merge-base hashes")
    p.set_defaults(func=run)
