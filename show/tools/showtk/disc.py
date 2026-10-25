"""disc.py - a show disc image (show.exe + SHOW/EP###.SHW) in seconds (spec 8.11:
"disc images rebuild in seconds"). Owner: ENGINE.

    from showtk import disc
    cue = disc.make(exe_dir, {"EP101.SHW": shw_path}, out_dir)

mkpsxiso writes <out_dir>/show.bin + show.cue from an XML made here (the
same layout as show/engine/iso_show.xml). The image's timestamps differ
between builds; nothing compares it (the frames are hashed instead).
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from . import errors, paths

_XML = """<?xml version="1.0" encoding="utf-8"?>
<iso_project>
	<track type="data">
		<identifiers system="PLAYSTATION" volume="DOOMO_SHOW" volume_set="DOOMO_SHOW"
		             publisher="HELLVILE_PARK_HOMEBREW" data_preparer="MKPSXISO" application="PLAYSTATION"/>
		<directory_tree>
			<file name="SYSTEM.CNF" type="data" source="{cnf}" />
			<file name="SHOW.EXE"   type="data" source="{exe}" />
			<dir name="SHOW">
{files}			</dir>
			<dummy sectors="1024"/>
		</directory_tree>
	</track>
</iso_project>
"""


def make(exe_dir: Path, files: dict, out_dir: Path) -> Path:
    """Build out_dir/show.bin + show.cue; returns the .cue path."""
    exe_dir, out_dir = Path(exe_dir), Path(out_dir)
    exe = exe_dir / "show.exe"
    if not exe.exists():
        raise errors.ShowError(errors.Diag("DISC", f"{paths.rel(exe)} not built (run `show build`)"))
    out_dir.mkdir(parents=True, exist_ok=True)
    rows = "".join(f'\t\t\t\t<file name="{n}" type="data" source="{Path(p).resolve()}" />\n'
                   for n, p in sorted(files.items()))
    xml = out_dir / "disc.xml"
    xml.write_text(_XML.format(cnf=(paths.ENGINE / "system.cnf").resolve(), exe=exe.resolve(),
                               files=rows))
    tool = shutil.which("mkpsxiso") or "/opt/psn00bsdk/bin/mkpsxiso"
    p = subprocess.run([tool, "-y", "-q", "-o", str(out_dir / "show.bin"), "-c",
                        str(out_dir / "show.cue"), str(xml)], cwd=out_dir,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode != 0:
        raise errors.ShowError(errors.Diag("DISC", "mkpsxiso failed:\n" + p.stdout[-1500:]))
    return out_dir / "show.cue"
