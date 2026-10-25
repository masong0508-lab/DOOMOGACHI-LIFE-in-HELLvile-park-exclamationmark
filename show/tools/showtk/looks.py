"""looks.py - the cast and crowd looks for the LOOK chunk (spec 7.3, 9.1).

The ART track owns the data (series/cast.shw, series/looks/*.shw) and its
parser/packer (showtk.art.cast.lookdata.Bible). This module asks the Bible
for what the compiler needs and caches it in show/build/cache/, keyed by the
content of those files and the game headers, so a recompile does not pay
for the Bible (about 1 s) when nothing in the cast changed.

    cast(series_dir) -> Cast  (.chars {id: dict}, .looks {id: 48 bytes|None},
                               .kset(id) -> [48 bytes], .errors [str])
"""
import hashlib
import json
from pathlib import Path

from . import paths

VERSION = 2


def _inputs(series):
    files = []
    s = Path(series)
    if (s / "cast.shw").exists():
        files.append(s / "cast.shw")
    files += sorted((s / "looks").glob("*.shw")) if (s / "looks").is_dir() else []
    h = hashlib.sha256(b"looks%d" % VERSION)
    for f in files:
        h.update(str(f.name).encode() + b"\0" + f.read_bytes())
    for hdr in ("catalog.h", "resident.h", "palette.h"):
        p = paths.CORE / "include" / "doomo" / hdr
        if p.exists():
            h.update(p.read_bytes())
    return h.hexdigest()[:24], files


class Cast:
    def __init__(self, chars, looks, ksets, errors, series):
        self.chars = chars
        self.looks = looks
        self._ksets = ksets
        self.errors = errors
        self.series = series

    def kset(self, kid):
        return [bytes.fromhex(b) for b in self._ksets.get(kid, [])] if kid in self._ksets else None

    def has_kset(self, kid):
        return kid in self._ksets


def _opt(c, k, d=None):
    v = c.opts.get(k)
    return v[0] if v else d


def cast(series=None):
    series = Path(series or paths.SERIES)
    key, files = _inputs(series)
    if not files:
        return Cast({}, {}, {}, [], series)
    cp = paths.BUILD / "cache" / ("cast-%s.json" % key)
    try:
        d = json.loads(cp.read_text())
        return Cast(d["chars"], {k: (bytes.fromhex(v) if v else None) for k, v in d["looks"].items()},
                    d["ksets"], d["errors"], series)
    except (OSError, ValueError, KeyError):
        pass
    from .art.cast import lookdata
    b = lookdata.Bible(str(series))
    errors = list(b.errors)
    chars, looks, ksets = {}, {}, {}
    for cid in b.order:
        c = b.chars[cid]
        tags = [t for t in (_opt(c, "tags", "") or "").split(",") if t]
        kinds = [t for t in (_opt(c, "kinds", "") or "").split(",") if t]
        roster = _opt(c, "roster")
        chars[cid] = {"id": cid, "age": _opt(c, "age"), "tags": tags, "kinds": kinds,
                      "voice": _opt(c, "voice"), "phase": int(_opt(c, "phase", "0") or 0),
                      "idle": _opt(c, "idle", "on"), "state": _opt(c, "state", "normal"),
                      "roster": int(roster) if roster not in (None, "") else None,
                      "register": _opt(c, "register", "pixel"), "first": _opt(c, "first"),
                      "name": _opt(c, "name", cid), "sprite": _opt(c, "sprite"),
                      "file": c.where.split(":")[0] if isinstance(c.where, str) else None}
        try:
            lb = b.char_look_bytes(cid)
        except Exception as e:          # a look the Bible could not resolve
            errors.append("char %s: %s" % (cid, e))
            lb = None
        looks[cid] = lb
    for kid, ks in b.ksets.items():
        try:
            ksets[kid] = [lb.hex() for _label, lb in b.kset_looks(kid)]
        except Exception as e:
            errors.append("kset %s: %s" % (kid, e))
    try:
        cp.parent.mkdir(parents=True, exist_ok=True)
        cp.write_text(json.dumps({"chars": chars, "looks": {k: (v.hex() if v else None) for k, v in looks.items()},
                                  "ksets": ksets, "errors": errors}))
    except OSError:
        pass
    return Cast(chars, looks, ksets, errors, series)
