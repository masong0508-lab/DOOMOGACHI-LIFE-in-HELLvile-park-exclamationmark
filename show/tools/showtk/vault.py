"""vault.py - preserve and restore what the show needs that is not in git (spec 10.7, WP0).

The emulator, its SDL3 library, the BIOS and the PS1 toolchain live only on
this container's disk (/opt/ps1emu/pcsx-redux is a symlink into the session
scratchpad). This module packs them into deterministic .tar.xz archives,
split at 95 MiB so each piece fits a GitHub file, pins every hash in
show/vault.lock, and restores them on a fresh container.

    show vault pack  [--out DIR] [--only emu,sdk]   archives -> DIR (default <scratch>/vault),
                                                    pins -> show/vault.lock
    show vault verify [--dir DIR]                   AT0.3: installed tools, models and archives
                                                    against show/vault.lock
    show vault fetch DIR [--root /] [--force]       restore from a directory of parts

Archives (this week; spec 10.7 order: emulator, BIOS and SDK first):
    vault-emu   the stripped pcsx-redux, libSDL3, openbios.bin, the GPL and zlib licences
    vault-sdk   /opt/psn00bsdk and the mipsel cross toolchain (the dpkg file lists of its
                packages and the shared libraries it loads)
The TTS/ASR models are pinned (hashes) but not packed this week (DEFERRED.md).
Where the archives go next (a private repo, an orphan branch or release
assets) is the creator's decision; nothing here pushes anywhere.

Archives are reproducible: members sorted, mtime 0, uid/gid 0, single-thread
xz preset 6. Members are stored relative to `/` and restored under --root.
fetch never overwrites a file whose content differs unless --force (and never
writes through a symlink into the scratchpad).
"""
import hashlib
import io
import lzma
import os
import shutil
import subprocess
import tarfile
import time
import tomllib
from pathlib import Path

from . import errors, paths
from .errors import Diag

PART_BYTES = 95 * 1024 * 1024
XZ_PRESET = 6

TOOLCHAIN_PACKAGES = (
    "binutils-mipsel-linux-gnu", "cpp-12-mipsel-linux-gnu", "cpp-mipsel-linux-gnu",
    "gcc-12-mipsel-linux-gnu", "gcc-12-mipsel-linux-gnu-base:amd64", "gcc-mipsel-linux-gnu",
    "libgcc-12-dev-mipsel-cross",
)
TOOLCHAIN_PROBES = ("/usr/bin/mipsel-linux-gnu-ld", "/usr/bin/mipsel-linux-gnu-as",
                    "/usr/lib/gcc-cross/mipsel-linux-gnu/12/cc1")

# model files pinned in vault.lock (relative to <scratch>/theme; not packed this week)
MODEL_GLOBS = (
    "final_dl/npm_kokoro_fp32/kokoro-fp32.onnx",
    "final_dl/npm_kokorojs/x/package/voices/*.bin",
    "final_dl/npm_whisper_small/m/*",
    "final/work/models/*.onnx",
)


# --- hashing -------------------------------------------------------------------
def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def tree_manifest(root):
    """sorted 'sha256  relpath' lines of every regular file (symlinks as '-> target')"""
    root = Path(root)
    lines = []
    for dp, dns, fns in os.walk(root):
        dns.sort()
        for fn in sorted(fns):
            p = Path(dp) / fn
            r = p.relative_to(root).as_posix()
            if p.is_symlink():
                lines.append("link  %s -> %s" % (r, os.readlink(p)))
            elif p.is_file():
                lines.append("%s  %s" % (sha256_file(p), r))
    return lines


def manifest_hash(lines):
    return hashlib.sha256(("\n".join(lines) + "\n").encode()).hexdigest()


# --- what goes in --------------------------------------------------------------
def _redux_src():
    """the emulator's source tree (licence files), next to the real binary"""
    return Path(os.path.realpath(paths.EMU)).parent


def emu_members(stage):
    """[(member path, source path or ('link', target))] for vault-emu; strips into stage"""
    stage.mkdir(parents=True, exist_ok=True)
    stripped = stage / "pcsx-redux"
    real = Path(os.path.realpath(paths.EMU))
    if not stripped.exists() or stripped.stat().st_mtime < real.stat().st_mtime:
        subprocess.run(["strip", "--strip-all", "-o", str(stripped), str(real)], check=True)
    src = _redux_src()
    m = [("opt/ps1emu/pcsx-redux", stripped),
         ("opt/ps1emu/openbios.bin", paths.BIOS),
         ("opt/ps1emu/LICENSE.pcsx-redux", src / "LICENSE"),
         ("opt/ps1emu/LICENSES.pcsx-redux.md", src / "LICENSES.md"),
         ("usr/local/lib/" + paths.SDL3.name, paths.SDL3),
         ("usr/local/lib/libSDL3.so.0", ("link", paths.SDL3.name)),
         ("usr/local/lib/libSDL3.so", ("link", "libSDL3.so.0"))]
    sdl_lic = paths.SCRATCH / "sdl3" / "LICENSE.txt"
    if sdl_lic.exists():
        m.append(("opt/ps1emu/LICENSE.SDL3.txt", sdl_lic))
    return m


def _dpkg_files(pkgs):
    out = subprocess.run(["dpkg", "-L"] + list(pkgs), stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True).stdout
    files = set()
    for line in out.splitlines():
        p = Path(line.strip())
        if line.startswith("/") and (p.is_file() or p.is_symlink()) and not p.is_dir():
            files.add(p)
    return files


def _shared_libs(binaries):
    libs = set()
    for b in binaries:
        out = subprocess.run(["ldd", b], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             text=True).stdout
        for line in out.splitlines():
            if "=>" in line:
                p = line.split("=>")[1].split("(")[0].strip()
                name = Path(p).name
                if p and not name.startswith(("libc.so", "libm.so", "ld-linux")):
                    libs.add(Path(p))
    return libs


def _member_of(p):
    """absolute path -> archive member; /lib is /usr/lib on this image"""
    s = str(p)
    if s.startswith("/lib/"):
        s = "/usr" + s
    return s.lstrip("/")


def sdk_members():
    m = []
    for f in sorted(paths.PSN00BSDK.rglob("*")):
        if f.is_symlink():
            m.append((_member_of(f), ("link", os.readlink(f))))
        elif f.is_file():
            m.append((_member_of(f), f))
    tc = _dpkg_files(TOOLCHAIN_PACKAGES)
    libs = set()
    for lib in _shared_libs(TOOLCHAIN_PROBES):
        libs.add(lib)
    for f in sorted(tc | libs, key=str):
        if f.is_symlink() and not f.resolve().exists():
            continue
        if f.is_symlink():
            real = f.resolve()
            link_m, real_m = _member_of(f), _member_of(real)
            m.append((link_m, ("link", os.path.relpath("/" + real_m, "/" + str(Path(link_m).parent)))))
            m.append((real_m, real))
        else:
            m.append((_member_of(f), f))
    seen, uniq = set(), []
    for name, src in m:
        if name not in seen:
            seen.add(name)
            uniq.append((name, src))
    return uniq


# --- archives ------------------------------------------------------------------
def write_archive(members, out_path):
    """deterministic tar.xz; returns (sha256, bytes)"""
    members = sorted(members, key=lambda x: x[0])
    tmp = Path(str(out_path) + ".tmp")
    with lzma.open(tmp, "wb", preset=XZ_PRESET, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC64) as xz:
        with tarfile.open(fileobj=xz, mode="w", format=tarfile.PAX_FORMAT) as tar:
            for name, src in members:
                ti = tarfile.TarInfo(name)
                ti.mtime = 0
                ti.uid = ti.gid = 0
                ti.uname = ti.gname = ""
                if isinstance(src, tuple):
                    ti.type = tarfile.SYMTYPE
                    ti.linkname = src[1]
                    ti.mode = 0o777
                    tar.addfile(ti)
                    continue
                st = os.stat(src)
                ti.size = st.st_size
                ti.mode = 0o755 if st.st_mode & 0o111 else 0o644
                with open(src, "rb") as f:
                    tar.addfile(ti, f)
    os.replace(tmp, out_path)
    return sha256_file(out_path), out_path.stat().st_size


def split_parts(path, part_bytes=PART_BYTES):
    """split a file into <name>.partNN if it is larger than part_bytes; [(name, sha, bytes)]"""
    size = path.stat().st_size
    if size <= part_bytes:
        return [(path.name, sha256_file(path), size)]
    parts = []
    with open(path, "rb") as f:
        k = 0
        while True:
            b = f.read(part_bytes)
            if not b:
                break
            pp = path.with_name("%s.part%02d" % (path.name, k))
            pp.write_bytes(b)
            parts.append((pp.name, hashlib.sha256(b).hexdigest(), len(b)))
            k += 1
    path.unlink()
    return parts


# --- vault.lock ------------------------------------------------------------------
def _q(s):
    return '"%s"' % str(s).replace("\\", "\\\\").replace('"', '\\"')


def write_lock(lock, path=None):
    path = path or paths.VAULT_LOCK
    L = ["# show/vault.lock - pins of everything the show needs that is not in git (spec 10.7).",
         "# Written by `show vault pack`; checked by `show vault verify` (AT0.3) and `show vault fetch`.",
         "# Hashes are SHA-256. Archive parts are what a vault location (repo, branch, release) holds.",
         "version = 1",
         "packed = %s" % _q(lock["packed"]),
         ""]
    for key, t in lock["tools"].items():
        L.append("[tools.%s]" % key)
        for k, v in t.items():
            if isinstance(v, list):
                L.append("%s = [%s]" % (k, ", ".join(_q(x) for x in v)))
            elif isinstance(v, int):
                L.append("%s = %d" % (k, v))
            else:
                L.append("%s = %s" % (k, _q(v)))
        L.append("")
    L.append("[models]")
    L.append("# <scratch> is SHOW_SCRATCH (showtk/paths.py); pinned, not packed this week (show/DEFERRED.md)")
    L.append("root = %s" % _q(lock["models"]["root"]))
    for r, h in sorted(lock["models"]["files"].items()):
        L.append("%s = %s" % (_q("file:" + r), _q(h)))
    L.append("")
    for name, a in sorted(lock["archives"].items()):
        L.append("[archives.%s]" % name)
        L.append("file = %s" % _q(a["file"]))
        L.append("sha256 = %s" % _q(a["sha256"]))
        L.append("bytes = %d" % a["bytes"])
        L.append("members = %d" % a["members"])
        L.append("parts = [")
        for pn, ph, pb in a["parts"]:
            L.append("  { name = %s, sha256 = %s, bytes = %d }," % (_q(pn), _q(ph), pb))
        L.append("]")
        L.append("")
    Path(path).write_text("\n".join(L))


def read_lock(path=None):
    path = Path(path or paths.VAULT_LOCK)
    if not path.exists():
        errors.fail("VAULT-LOCK", "no %s yet: run `show vault pack`" % paths.rel(path))
    with open(path, "rb") as f:
        lock = tomllib.load(f)
    files = {k[5:]: v for k, v in lock.get("models", {}).items() if k.startswith("file:")}
    lock.setdefault("models", {})["files"] = files
    return lock


def pin_tools():
    real = Path(os.path.realpath(paths.EMU))
    tc_files = sorted(_dpkg_files(TOOLCHAIN_PACKAGES), key=str)
    tc_lines = ["%s  %s" % (sha256_file(f) if f.is_file() and not f.is_symlink() else "link", f)
                for f in tc_files]
    sdk_lines = tree_manifest(paths.PSN00BSDK)
    rev = ""
    try:
        rev = subprocess.run(["git", "-C", str(_redux_src()), "rev-parse", "HEAD"], stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True).stdout.strip()
    except OSError:
        pass
    return {
        "emulator": {"path": str(paths.EMU), "sha256": sha256_file(real), "source": "pcsx-redux " + rev},
        "sdl3": {"path": str(paths.SDL3), "sha256": sha256_file(paths.SDL3)},
        "bios": {"path": str(paths.BIOS), "sha256": sha256_file(paths.BIOS)},
        "psn00bsdk": {"path": str(paths.PSN00BSDK), "manifest_sha256": manifest_hash(sdk_lines),
                      "files": len(sdk_lines)},
        "toolchain": {"packages": list(TOOLCHAIN_PACKAGES), "manifest_sha256": manifest_hash(tc_lines),
                      "files": len(tc_lines)},
    }


def pin_models():
    root = paths.THEME
    files = {}
    for g in MODEL_GLOBS:
        for f in sorted(root.glob(g)):
            if f.is_file():
                files[f.relative_to(root).as_posix()] = sha256_file(f)
    return {"root": "<scratch>/theme", "files": files}


# --- commands --------------------------------------------------------------------
def pack(out_dir=None, only=("emu", "sdk"), log=print):
    out_dir = Path(out_dir or paths.VAULT_DIR)
    out_dir.mkdir(parents=True, exist_ok=True)
    stage = paths.SHOW_WORK / "vault-stage"
    t0 = time.time()
    lock = {"packed": time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime()), "archives": {}}
    log("vault: pinning the tools ...")
    lock["tools"] = pin_tools()
    log("vault: pinning the models (hash only) ...")
    lock["models"] = pin_models()
    old = {}
    if paths.VAULT_LOCK.exists():
        try:
            old = read_lock().get("archives", {})
        except errors.ShowError:
            old = {}
    for name in ("emu", "sdk"):
        key = "vault-" + name
        if name not in only:
            if key in old:
                lock["archives"][key] = dict(old[key], parts=[(p["name"], p["sha256"], p["bytes"])
                                                             for p in old[key]["parts"]])
            continue
        members = emu_members(stage) if name == "emu" else sdk_members()
        for f in out_dir.glob(key + ".tar.xz*"):
            f.unlink()
        arc = out_dir / (key + ".tar.xz")
        log("vault: writing %s (%d members) ..." % (arc.name, len(members)))
        sha, size = write_archive(members, arc)
        parts = split_parts(arc)
        lock["archives"][key] = {"file": arc.name, "sha256": sha, "bytes": size,
                                 "members": len(members), "parts": parts}
        log("vault: %s %.1f MB, %d part(s), sha256 %s" % (arc.name, size / 1e6, len(parts), sha[:16]))
    real = Path(os.path.realpath(paths.EMU))
    if "emu" in only:
        lock["tools"]["emulator"]["stripped_sha256"] = sha256_file(stage / "pcsx-redux")
    elif paths.VAULT_LOCK.exists():
        prev = read_lock()["tools"].get("emulator", {}).get("stripped_sha256")
        if prev:
            lock["tools"]["emulator"]["stripped_sha256"] = prev
    write_lock(lock)
    (out_dir / "SHA256SUMS").write_text("".join(
        "%s  %s\n" % (ph, pn) for a in lock["archives"].values() for pn, ph, pb in a["parts"]))
    shutil.copy(paths.VAULT_LOCK, out_dir / "vault.lock")
    if stage.exists():
        shutil.rmtree(stage)
    log("vault: done in %.0f s -> %s (pins in %s; emulator %s)"
        % (time.time() - t0, out_dir, paths.rel(paths.VAULT_LOCK), real.name))
    return lock


def _parts_stream(lock_arc, src_dir):
    """verify each part and return the whole archive's bytes"""
    buf = io.BytesIO()
    diags = []
    for p in lock_arc["parts"]:
        f = Path(src_dir) / p["name"]
        if not f.exists():
            diags.append(Diag("VAULT-MISSING", "%s is not in %s" % (p["name"], src_dir)))
            continue
        b = f.read_bytes()
        if hashlib.sha256(b).hexdigest() != p["sha256"] or len(b) != p["bytes"]:
            diags.append(Diag("VAULT-HASH", "%s does not match vault.lock" % p["name"], file=f))
        buf.write(b)
    if diags:
        raise errors.ShowError(diags)
    data = buf.getvalue()
    if hashlib.sha256(data).hexdigest() != lock_arc["sha256"]:
        errors.fail("VAULT-HASH", "%s: the joined parts do not match vault.lock" % lock_arc["file"])
    return data


def fetch(src_dir, root="/", force=False, only=None, log=print):
    lock = read_lock()
    root = Path(root)
    wrote = skipped = kept = 0
    for name, a in sorted(lock["archives"].items()):
        if only and name.replace("vault-", "") not in only:
            continue
        data = _parts_stream(a, src_dir)
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:xz") as tar:
            for ti in tar.getmembers():
                if ti.name.startswith("/") or ".." in Path(ti.name).parts:
                    errors.fail("VAULT-UNSAFE", "%s: unsafe member %s" % (a["file"], ti.name))
                dest = root / ti.name
                if dest.is_symlink() and "scratchpad" in os.path.realpath(dest) and not ti.issym():
                    log("vault: keep %s (a link into the scratchpad; never written through)" % dest)
                    kept += 1
                    continue
                if ti.issym():
                    if dest.is_symlink() or dest.exists():
                        skipped += 1
                        continue
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    os.symlink(ti.linkname, dest)
                    wrote += 1
                    continue
                if not ti.isfile():
                    continue
                payload = tar.extractfile(ti).read()
                if dest.exists():
                    if hashlib.sha256(payload).hexdigest() == sha256_file(dest):
                        skipped += 1
                        continue
                    if not force:
                        log("vault: keep %s (differs from the vault; --force replaces it)" % dest)
                        kept += 1
                        continue
                dest.parent.mkdir(parents=True, exist_ok=True)
                tmp = dest.with_name(dest.name + ".vault-tmp")
                tmp.write_bytes(payload)
                os.chmod(tmp, ti.mode)
                os.replace(tmp, dest)
                wrote += 1
        log("vault: %s restored under %s" % (name, root))
    log("vault: %d file(s) written, %d already present, %d kept as they were" % (wrote, skipped, kept))
    if str(root) != "/":
        log("vault: run the restored emulator with SHOW_EMU=%s and LD_LIBRARY_PATH=%s"
            % (root / "opt/ps1emu/pcsx-redux", root / "usr/local/lib"))
    return wrote, skipped, kept


def verify(vault_dir=None, check_tools=True, check_models=True, check_archives=True):
    """AT0.3: [Diag] for every hash that does not match vault.lock, and a list of OK lines"""
    lock = read_lock()
    diags, ok = [], []
    t = lock.get("tools", {})
    if check_tools:
        emu = t.get("emulator", {})
        if paths.EMU.exists():
            h = sha256_file(os.path.realpath(paths.EMU))
            if h in (emu.get("sha256"), emu.get("stripped_sha256")):
                ok.append("emulator %s %s" % (paths.EMU, "original" if h == emu.get("sha256") else "stripped"))
            else:
                diags.append(Diag("VAULT-HASH", "the emulator does not match vault.lock (%s)" % h[:16], file=paths.EMU))
        else:
            diags.append(Diag("VAULT-MISSING", "no emulator at %s (show vault fetch)" % paths.EMU))
        for key, p in (("sdl3", paths.SDL3), ("bios", paths.BIOS)):
            want = t.get(key, {}).get("sha256")
            if not p.exists():
                diags.append(Diag("VAULT-MISSING", "%s missing (show vault fetch)" % p))
            elif sha256_file(p) != want:
                diags.append(Diag("VAULT-HASH", "%s does not match vault.lock" % key, file=p))
            else:
                ok.append("%s %s" % (key, p))
        sdk = t.get("psn00bsdk", {})
        if paths.PSN00BSDK.is_dir() and manifest_hash(tree_manifest(paths.PSN00BSDK)) == sdk.get("manifest_sha256"):
            ok.append("psn00bsdk %s (%d files)" % (paths.PSN00BSDK, sdk.get("files", 0)))
        else:
            diags.append(Diag("VAULT-HASH", "the PSn00bSDK tree differs from vault.lock (or is missing)",
                              file=paths.PSN00BSDK))
        tc = t.get("toolchain", {})
        files = sorted(_dpkg_files(TOOLCHAIN_PACKAGES), key=str)
        lines = ["%s  %s" % (sha256_file(f) if f.is_file() and not f.is_symlink() else "link", f) for f in files]
        if files and manifest_hash(lines) == tc.get("manifest_sha256"):
            ok.append("toolchain (%d files)" % len(files))
        else:
            diags.append(Diag("VAULT-HASH", "the mipsel toolchain differs from vault.lock (or dpkg does not "
                              "list it: a fetched toolchain is checked through the archive instead)"))
    if check_models:
        m = lock.get("models", {})
        r0 = m.get("root", "<scratch>/theme")
        root = paths.SCRATCH / r0[len("<scratch>/"):] if r0.startswith("<scratch>/") else Path(r0)
        bad = 0
        for r, h in m.get("files", {}).items():
            f = root / r
            if not f.exists():
                diags.append(Diag("VAULT-MISSING", "model file missing (fetched lazily by `show voice`)",
                                  file=f, severity="warning"))
            elif sha256_file(f) != h:
                bad += 1
                diags.append(Diag("VAULT-HASH", "model file does not match vault.lock", file=f))
        if not bad:
            ok.append("models (%d files)" % len(m.get("files", {})))
    if check_archives:
        vdir = Path(vault_dir or paths.VAULT_DIR)
        for name, a in sorted(lock.get("archives", {}).items()):
            try:
                _parts_stream(a, vdir)
                ok.append("archive %s (%d part(s), %.1f MB)" % (a["file"], len(a["parts"]), a["bytes"] / 1e6))
            except errors.ShowError as e:
                diags += e.diags
    return diags, ok
