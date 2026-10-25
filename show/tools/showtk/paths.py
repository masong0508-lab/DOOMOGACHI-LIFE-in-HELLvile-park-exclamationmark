"""paths.py - every fixed location the show's tools use, in one place.

Repo paths are derived from this file (show/tools/showtk/paths.py), so the
tools work from any working directory and any clone. Machine paths (the
emulator, BIOS, SDK) and the scratch area can be moved with environment
variables, so a fresh container restored by `show vault fetch` needs no
code change.

    SHOW_SCRATCH   scratch root (vault/, show_lib/, show_out/, show_work/)
    SHOW_EMU       pcsx-redux binary          (default /opt/ps1emu/pcsx-redux)
    SHOW_BIOS      BIOS image                 (default /opt/ps1emu/openbios.bin)
    PSN00BSDK_LIBS libpsn00b dir              (default /opt/psn00bsdk/lib/libpsn00b)
"""
import os
from pathlib import Path

# --- the repository --------------------------------------------------------
SHOWTK = Path(__file__).resolve().parent            # show/tools/showtk
TOOLS = SHOWTK.parent                               # show/tools
SHOW = TOOLS.parent                                 # show/
REPO = SHOW.parent                                  # repo root
ENGINE = SHOW / "engine"                            # the stand-alone CMake project
BUILD = SHOW / "build"                              # gitignored
SERIES = SHOW / "series"
VAULT_LOCK = SHOW / "vault.lock"
COMMANDS = SHOWTK / "commands"                      # one file per subcommand

# read-only game pieces the show uses
CORE = REPO / "core"
PS1 = REPO / "ps1"
GAME_HARNESS = PS1 / "harness" / "run.py"
DOCS_SHOW = REPO / "docs" / "show"

# the directories a show commit may never touch (spec 4.4); in practice the
# gate allows nothing outside show/ at all
GAME_DIRS = ("core", "gba", "ps1", "tools", "docs/show", "trailer")
SHOW_PREFIX = "show/"

# the branch every workflow merges into (the brief)
SHARED_BRANCH = "claude/nice-allen-gr6czm"
SHARED_REMOTE_REF = "origin/" + SHARED_BRANCH

PRESETS = ("ps1-show", "ps1-show-8mb")


def build_dir(preset="ps1-show"):
    """the CMake binary dir of a show preset (CMakePresets.json binaryDir)"""
    return BUILD / preset


def episode_dir(ep):
    return SHOW / ep


def episode_build(ep):
    return BUILD / ep


# --- this machine ------------------------------------------------------------
_SESSION_SCRATCH = Path("/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/"
                        "ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad")


def _scratch_default():
    if _SESSION_SCRATCH.is_dir():
        return _SESSION_SCRATCH
    return BUILD / "scratch"


SCRATCH = Path(os.environ.get("SHOW_SCRATCH") or _scratch_default())
VAULT_DIR = SCRATCH / "vault"          # `show vault pack` writes here
SHOW_LIB = SCRATCH / "show_lib"        # large reusable media (<kind>/)
SHOW_OUT = SCRATCH / "show_out"        # creator-facing samples (<folder>/)
SHOW_WORK = SCRATCH / "show_work"      # per-package scratch (<package>/)
THEME = SCRATCH / "theme"              # the audio/voice sources (READ-ONLY)

EMU = Path(os.environ.get("SHOW_EMU") or "/opt/ps1emu/pcsx-redux")
BIOS = Path(os.environ.get("SHOW_BIOS") or "/opt/ps1emu/openbios.bin")
PSN00BSDK_LIBS = Path(os.environ.get("PSN00BSDK_LIBS") or "/opt/psn00bsdk/lib/libpsn00b")
PSN00BSDK = PSN00BSDK_LIBS.parent.parent           # /opt/psn00bsdk
SDL3 = Path("/usr/local/lib/libSDL3.so.0.2.10")


def rel(p):
    """a path for messages: repo-relative when inside the repo"""
    p = Path(p)
    try:
        return str(p.resolve().relative_to(REPO))
    except ValueError:
        return str(p)


def env_for_build():
    """the environment the PSn00bSDK CMake toolchain needs"""
    env = dict(os.environ)
    env.setdefault("PSN00BSDK_LIBS", str(PSN00BSDK_LIBS))
    return env
