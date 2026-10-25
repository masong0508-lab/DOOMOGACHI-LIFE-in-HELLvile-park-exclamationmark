"""test_wp1.py - WP1/WP0 acceptance tests (spec 16): AT1.1-AT1.4, AT0.3, the CLI.

    python3 -m pytest show/tests/engine/test_wp1.py            (all; ~3 min, two emulator runs)
    python3 -m pytest show/tests/engine/test_wp1.py -m "not slow"   (seconds)
    python3 show/tests/engine/test_wp1.py                       (same as the first, without pytest)

AT1.1 (check-game --full) is run by `show check-game --full` itself; here
only its path half runs (the full build takes minutes) unless SHOW_TEST_FULL=1.
"""
import os
import re
import shutil
import subprocess
import sys
import types
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools"))

from showtk import errors, paths                       # noqa: E402
from showtk.commands import build as build_cmd          # noqa: E402
from showtk.commands import check_game                  # noqa: E402

try:
    import pytest
    slow = pytest.mark.slow
except ImportError:                                     # plain `python3 test_wp1.py`
    pytest = None

    def slow(f):
        return f

WORK = paths.SHOW_WORK / "wp1-tests"


def _harness(exe_dir, script, out):
    out = Path(out)
    if out.exists():
        shutil.rmtree(out)
    p = subprocess.run([sys.executable, str(paths.GAME_HARNESS), "--cue", str(exe_dir / "show.cue"),
                        "--map", str(exe_dir / "show.map"), "--script", str(script), "--out", str(out),
                        "--timeout", "180"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert p.returncode == 0, p.stdout[-2000:]
    return (out / "tty.log").read_text(errors="replace")


# --- fast -----------------------------------------------------------------------
def test_error_format_matches_spec_13_4():
    d = errors.Diag("AUD-OVERRUN", "line co15_perfect (36 f) ends at +189, after the shot's tail hold starts "
                    "(+180, `button`). Move it to +141, use a shorter take, or set retime=\"take\" in takes.toml.",
                    file=paths.SHOW / "ep101/shots/co.shw", line=88, shot="co15", frame=153)
    assert d.format() == (
        "show/ep101/shots/co.shw:88: co15 +153 [AUD-OVERRUN] line co15_perfect (36 f) ends at +189, after the shot's\n"
        "  tail hold starts (+180, `button`). Move it to +141, use a shorter take, or set retime=\"take\" in takes.toml.")
    w = errors.Diag("TIME-READ", "short", severity="warning")
    assert w.format() == "[TIME-READ] warning: short"


def test_cli_autoloads_command_modules(tmp_path):
    """a new commands/<name>.py with register(subparsers) appears without touching cli.py"""
    from showtk import cli, commands
    (tmp_path / "zz_probe.py").write_text(
        "def register(sub):\n"
        "    p = sub.add_parser('zz-probe')\n"
        "    p.set_defaults(func=lambda a: 7)\n")
    (tmp_path / "zz_broken.py").write_text("raise RuntimeError('boom')\n")
    old = list(commands.__path__)
    commands.__path__.append(str(tmp_path))
    try:
        assert cli.main(["zz-probe"]) == 7
        ap, failed = cli.build_parser()
        assert ("zz_broken", "RuntimeError: boom") in failed
    finally:
        commands.__path__[:] = old
        for m in ("showtk.commands.zz_probe", "showtk.commands.zz_broken"):
            sys.modules.pop(m, None)


def test_check_game_paths_pass():
    """AT1.1, the per-commit half: nothing outside show/ changed"""
    diags, mb, n = check_game.path_check(paths.SHARED_REMOTE_REF)
    assert not diags, "\n".join(d.format() for d in diags)


def test_patches_apply_to_the_current_game():
    for f in ("portrait", "portrait_head"):
        out = WORK / ("ok_" + f + ".c")
        p = subprocess.run([sys.executable, str(paths.ENGINE / "apply_patch.py"), str(paths.CORE / "src" / (f + ".c")),
                            str(paths.ENGINE / "patches" / (f + ".c.patch")), str(out)],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert p.returncode == 0, p.stdout
        text = out.read_text(encoding="latin-1")
        assert text.count("#if DOOMO_SHOW") == (9 if f == "portrait" else 2)


def test_ram_report_within_spec_8_7():
    """AT1.2: elf2exe passed and the RAM report is inside the spec 8.7 budget"""
    ram = paths.build_dir() / "ram.txt"
    assert ram.exists(), "build first: show/tools/show build"
    text = ram.read_text()
    assert "verdict    PASS" in text, text
    left = int(re.search(r"left\s+(\d+) KB", text).group(1))
    need = int(re.search(r"needs\s+(\d+) KB", text).group(1))
    assert left >= need >= 128, text   # the player and its chunk buffer are linked (DECISIONS)
    assert (paths.build_dir() / "show.exe").stat().st_size < 2 * 1024 * 1024


def test_vault_hashes_match_lock():
    """AT0.3: every vault hash matches vault.lock (tools, models, archives)"""
    from showtk import vault
    diags, ok = vault.verify()
    errs = [d for d in diags if d.is_error]
    assert not errs, "\n".join(d.format() for d in errs)
    assert any(line.startswith("archive vault-emu") for line in ok), ok
    assert any(line.startswith("archive vault-sdk") for line in ok), ok


# --- slow (builds and emulator runs) ----------------------------------------------
@slow
def test_stale_patch_stops_the_show_build():
    """AT1.4: a patch that no longer applies stops the show build with the file name"""
    stale = WORK / "stale_src"
    if stale.exists():
        shutil.rmtree(stale)
    stale.mkdir(parents=True)
    src = (paths.CORE / "src" / "portrait.c").read_text(encoding="latin-1")
    # the game "changes" the line hunk (f) sits on (a copy; the game file is untouched)
    assert "    return steps_max() - steps_used;" in src
    (stale / "portrait.c").write_text(src.replace("    return steps_max() - steps_used;",
                                                  "    return steps_max() - steps_used - 0;"), encoding="latin-1")
    shutil.copy(paths.CORE / "src" / "portrait_head.c", stale / "portrait_head.c")
    bdir = WORK / "build-stale"
    if bdir.exists():
        shutil.rmtree(bdir)
    env = paths.env_for_build()
    p = subprocess.run(["cmake", "--preset", "ps1-show", "-B", str(bdir), "-DSHOW_PATCH_SRC_DIR=" + str(stale)],
                       cwd=str(paths.ENGINE), env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert p.returncode == 0, p.stdout
    p = subprocess.run(["cmake", "--build", str(bdir), "-j", "1", "--target", "show"], cwd=str(paths.ENGINE),
                       env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert p.returncode != 0
    assert "patch %s no longer applies to %s" % (paths.ENGINE / "patches" / "portrait.c.patch",
                                                 stale / "portrait.c") in p.stdout, p.stdout[-2000:]
    assert "hunk" in p.stdout
    assert not (bdir / "gen" / "portrait.c").exists()
    assert not (bdir / "show.exe").exists()
    shutil.rmtree(bdir)
    shutil.rmtree(stale)


@slow
def test_portrait_images_equal_the_games():
    """AT1.3: overrides at -1 -> the show's images equal the game's (20 roster looks x 11 expressions,
    busts + figures, + 4 chibi directions; CRC32 captured on the PS1)"""
    logs = {}
    for variant in ("selftest", "selftest-game"):
        bdir = build_cmd.build("ps1-show", variant, quiet=True)
        tty = _harness(bdir, HERE / "selftest.txt", WORK / ("run-" + variant))
        logs[variant] = tty
    rows = {}
    for v, tty in logs.items():
        rows[v] = re.findall(r"^selftest: ([PFC] \d\d \d\d [0-9a-f]{8})$", tty, re.M)
    assert len(rows["selftest"]) == 20 * 11 * 2 + 20 * 4, len(rows["selftest"])
    assert rows["selftest"] == rows["selftest-game"]
    assert len(set(r.split()[-1] for r in rows["selftest"])) > 300     # real, varied images
    m = re.search(r"override mouth differs (\d+)/20 restored (\d+)/20; blink differs (\d+)/20 restored (\d+)/20; "
                  r"chibi mouth differs (\d+)/20 restored (\d+)/20", logs["selftest"])
    assert m, logs["selftest"][-1500:]
    md, mr, bd, br, cd, cr = map(int, m.groups())
    assert md >= 15 and mr == 20, m.group(0)        # (b)+(d): the mouth changes, and -1 restores the original
    # (c)+(g): forced open vs forced shut differ (eye styles without a lid look the same both
    # ways), and -1 restores the clock's own phase
    assert bd >= 12 and br == 20, m.group(0)
    assert cr == 20, m.group(0)                     # (b'): the chibi key fold never serves a stale image
    (WORK / "at13_summary.txt").write_text("%d images equal; %s\n" % (len(rows["selftest"]), m.group(0)))


@slow
def test_boots_to_the_test_chart():
    tty = _harness(paths.build_dir(), HERE / "chart.txt", WORK / "run-chart")
    assert "show: init (show.exe" in tty
    assert "STUB:" not in tty
    assert "HARNESS: END" in tty


def test_full_gate_when_asked():
    if os.environ.get("SHOW_TEST_FULL") != "1":
        return
    assert not check_game.full_check(paths.SHARED_REMOTE_REF)


if __name__ == "__main__":
    WORK.mkdir(parents=True, exist_ok=True)
    fails = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and isinstance(fn, types.FunctionType):
            try:
                if "tmp_path" in fn.__code__.co_varnames[:fn.__code__.co_argcount]:
                    t = WORK / ("tmp_" + name)
                    t.mkdir(parents=True, exist_ok=True)
                    fn(t)
                else:
                    fn()
                print("PASS", name)
            except Exception as e:      # noqa: BLE001
                fails += 1
                print("FAIL", name, type(e).__name__, str(e)[:2000])
    sys.exit(1 if fails else 0)
