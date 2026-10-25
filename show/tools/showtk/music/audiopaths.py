"""Where the audio tools read and write (WP10; SHOW_ENGINE_SPEC.md sec.4.1, 10.7).

Everything the music and SFX tools touch is resolved here, so a fresh
container only has to set environment variables:

    DOOMO_SCRATCH     the session scratchpad (models, kit assets, the theme)
    DOOMO_SHOW_LIB    large reusable media: <lib>/music, <lib>/sfx (not git)
    DOOMO_SHOW_OUT    creator-facing previews: <out>/music
    DOOMO_KIT_ASSETS  the kit's licensed samples (theme/kit/assets; vault-audio)
    DOOMO_THEME_DIR   the "Doomtastic Times" masters (docs/show/theme/ once it is
                      published, else the scratchpad's theme/final)

The source tree is located from this file: show/tools/showtk/music/.
"""
from __future__ import annotations

import os
from pathlib import Path

HERE = Path(__file__).resolve()
SHOWTK = HERE.parents[1]                 # show/tools/showtk
SHOW = HERE.parents[3]                   # show/
REPO = HERE.parents[4]                   # the repository root

_DEFAULT_SCRATCH = ("/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/"
                    "ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad")


def _env_path(name: str, default: Path) -> Path:
    v = os.environ.get(name)
    return Path(v) if v else default


SCRATCH = _env_path("DOOMO_SCRATCH", Path(_DEFAULT_SCRATCH))
LIB = _env_path("DOOMO_SHOW_LIB", SCRATCH / "show_lib")
OUT = _env_path("DOOMO_SHOW_OUT", SCRATCH / "show_out")
WORK = _env_path("DOOMO_SHOW_WORK", SCRATCH / "show_work" / "WP10")
LIB_MUSIC = LIB / "music"
LIB_SFX = LIB / "sfx"
OUT_MUSIC = OUT / "music"
KIT_ASSETS = _env_path("DOOMO_KIT_ASSETS", SCRATCH / "theme" / "kit" / "assets")
KIT_CACHE = _env_path("DOOMO_KIT_CACHE", LIB_MUSIC / ".kitcache")
AUDIO = SHOW / "audio"                   # the vendored kit (show/audio/kit)
SERIES = SHOW / "series"
LIB_SRC = SHOW / "lib"                   # show/lib/music (score cues), show/lib/licences


def theme_dir() -> Path:
    """docs/show/theme/ when the lyrics-and-vocal workflow has published it, else
    the scratchpad's current master (theme/final)."""
    v = os.environ.get("DOOMO_THEME_DIR")
    if v:
        return Path(v)
    pub = REPO / "docs" / "show" / "theme"
    if pub.is_dir() and any(pub.glob("*main_title*")):
        return pub
    return SCRATCH / "theme" / "final"


def episode_dir(ep: str) -> Path:
    return SHOW / ep


def ensure(*dirs: Path) -> None:
    for d in dirs:
        d.mkdir(parents=True, exist_ok=True)
