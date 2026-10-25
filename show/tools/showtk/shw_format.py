"""shw_format.py - the EP###.SHW binary format, mirrored from
show/include/show/shw_format.h (spec 7.3). AT4.2 checks the two agree.

    from showtk import shw_format as F
    blob = F.write_episode(header_fields, shots)   # bytes
    ep = F.read_episode(blob)                      # dicts, round-trips

Every struct is a list of (field, ctype, count) in header order; pack() /
unpack() turn dicts into bytes and back. Constants (SOP_*, SHW_*) are read
from the header itself at import, so the two can never drift on a number.
"""
import re
import struct
import zlib
from pathlib import Path

HEADER = Path(__file__).resolve().parents[2] / "include" / "show" / "shw_format.h"

CTYPES = {"uint8_t": "B", "int8_t": "b", "uint16_t": "H", "int16_t": "h",
          "uint32_t": "I", "int32_t": "i"}

# (field, ctype, count) in the header's order; keep in lock-step with the .h
STRUCTS = {
    "ShwEpHeader": [
        ("magic", "uint32_t", 1), ("abi", "uint16_t", 1), ("nshots", "uint16_t", 1),
        ("fps", "uint8_t", 1), ("flags", "uint8_t", 1), ("canvas_w", "uint16_t", 1),
        ("canvas_h", "uint16_t", 1), ("canvas_y", "uint16_t", 1), ("seed", "uint32_t", 1),
        ("toolchain_hash", "uint8_t", 8), ("strtab_off", "uint32_t", 1),
        ("strtab_bytes", "uint32_t", 1), ("total_frames", "uint32_t", 1)],
    "ShwShotIndex": [
        ("start_frame", "uint32_t", 1), ("len", "uint16_t", 1), ("px", "uint8_t", 1),
        ("look", "uint8_t", 1), ("chunk_sector", "uint32_t", 1), ("chunk_bytes", "uint32_t", 1),
        ("base_shot", "uint16_t", 1), ("base_frame", "uint16_t", 1), ("replay_shot", "uint16_t", 1),
        ("replay_from", "uint16_t", 1), ("flags", "uint16_t", 1), ("name", "uint16_t", 1),
        ("seed", "uint32_t", 1), ("chunk_crc", "uint32_t", 1)],
    "ShwShotHdr": [
        ("magic", "uint32_t", 1), ("set_kind", "uint8_t", 1), ("plane_n", "uint8_t", 1),
        ("actor_n", "uint8_t", 1), ("prop_n", "uint8_t", 1), ("crowd_n", "uint8_t", 1),
        ("look_n", "uint8_t", 1), ("track_n", "uint8_t", 1), ("badge_est", "uint8_t", 1),
        ("ev_n", "uint16_t", 1), ("arg_n", "uint16_t", 1), ("camkey_n", "uint16_t", 1),
        ("stroke_n", "uint16_t", 1), ("texpage_n", "uint16_t", 1), ("clut_n", "uint16_t", 1),
        ("strings_bytes", "uint16_t", 1), ("set_name", "uint16_t", 1),
        ("off_looks", "uint32_t", 1), ("off_planes", "uint32_t", 1), ("off_actors", "uint32_t", 1),
        ("off_props", "uint32_t", 1), ("off_crowds", "uint32_t", 1), ("off_camkeys", "uint32_t", 1),
        ("off_events", "uint32_t", 1), ("off_args", "uint32_t", 1), ("off_tracks", "uint32_t", 1),
        ("off_strokes", "uint32_t", 1), ("off_strings", "uint32_t", 1),
        ("off_texpages", "uint32_t", 1), ("off_cluts", "uint32_t", 1), ("vram_plan", "uint16_t", 4),
        ("variant", "uint16_t", 1), ("pad", "uint16_t", 1)],
    "ShwLook": [("b", "uint8_t", 48)],
    "ShwPlane": [
        ("kind", "uint8_t", 1), ("order", "uint8_t", 1), ("flags", "uint8_t", 1), ("boil", "uint8_t", 1),
        ("par_q16", "int32_t", 1), ("w", "uint16_t", 1), ("h", "uint16_t", 1), ("tex", "uint16_t", 1),
        ("clut", "uint16_t", 1), ("x", "int16_t", 1), ("y", "int16_t", 1), ("arg", "uint32_t", 1)],
    "ShwActor": [
        ("kind", "uint8_t", 1), ("look", "uint8_t", 1), ("face", "uint8_t", 1), ("expr", "uint8_t", 1),
        ("idle", "uint8_t", 1), ("phase", "uint8_t", 1), ("px", "uint8_t", 1), ("state", "uint8_t", 1),
        ("x", "int16_t", 1), ("y", "int16_t", 1), ("name", "uint16_t", 1), ("roster", "uint16_t", 1)],
    "ShwProp": [
        ("plane", "uint8_t", 1), ("face", "uint8_t", 1), ("anchor", "uint8_t", 1), ("flags", "uint8_t", 1),
        ("x", "int16_t", 1), ("y", "int16_t", 1), ("art", "uint16_t", 1), ("tex", "uint16_t", 1)],
    "ShwCrowd": [
        ("look_base", "uint8_t", 1), ("n", "uint8_t", 1), ("lane", "uint8_t", 1), ("dir", "uint8_t", 1),
        ("kind", "uint8_t", 1), ("ripple_lo", "uint8_t", 1), ("ripple_hi", "uint8_t", 1),
        ("age", "uint8_t", 1), ("spacing_q8", "uint16_t", 1), ("from", "int16_t", 1),
        ("y", "int16_t", 1), ("cycle", "uint16_t", 1), ("speed_q16", "int32_t", 1),
        ("seed", "uint32_t", 1), ("clock_mask", "uint32_t", 1), ("gen", "uint8_t", 1),
        ("species", "uint8_t", 1), ("look_k", "uint8_t", 1), ("flags", "uint8_t", 1)],
    "ShwCamKey": [
        ("frame", "uint16_t", 1), ("mode", "uint8_t", 1), ("ease", "uint8_t", 1),
        ("x_q16", "int32_t", 1), ("y_q16", "int32_t", 1), ("vx_q16", "int32_t", 1),
        ("vy_q16", "int32_t", 1), ("scale_q8", "uint16_t", 1), ("over", "uint16_t", 1)],
    "ShwTrack": [("actor", "uint8_t", 1), ("kind", "uint8_t", 1), ("run_n", "uint16_t", 1),
                 ("off_runs", "uint32_t", 1)],
    "ShwRun": [("value", "uint8_t", 1), ("count", "uint8_t", 1)],
    "ShwStroke": [
        ("nib", "uint8_t", 1), ("width", "uint8_t", 1), ("flags", "uint8_t", 1), ("boil", "uint8_t", 1),
        ("pt_n", "uint16_t", 1), ("reveal_from", "uint16_t", 1), ("reveal_to", "uint16_t", 1),
        ("name", "uint16_t", 1), ("off_pts", "uint32_t", 1)],
    "ShwPoint": [("x", "int16_t", 1), ("y", "int16_t", 1)],
    "ShwTexPage": [
        ("w", "uint16_t", 1), ("h", "uint16_t", 1), ("bpp", "uint8_t", 1), ("comp", "uint8_t", 1),
        ("name", "uint16_t", 1), ("bytes", "uint32_t", 1), ("off", "uint32_t", 1)],
    "ShwClut": [("n", "uint16_t", 1), ("pad", "uint16_t", 1), ("off", "uint32_t", 1)],
    # beat.h, the engine's own record (not in shw_format.h)
    "BeatEv": [("at", "uint16_t", 1), ("anchor", "uint8_t", 1), ("op", "uint8_t", 1),
               ("a", "int16_t", 1), ("b", "int16_t", 1)],
}


def fmt(name):
    return "<" + "".join((str(n) if n > 1 else "") + CTYPES[t] for _f, t, n in STRUCTS[name])


SIZES = {name: struct.calcsize(fmt(name)) for name in STRUCTS}


def pack(name, d):
    vals = []
    for f, _t, n in STRUCTS[name]:
        v = d.get(f, 0)
        if n > 1:
            v = list(v) if v else []
            v = (v + [0] * n)[:n]
            vals += v
        else:
            vals.append(v)
    return struct.pack(fmt(name), *vals)


def unpack(name, blob, off=0):
    raw = struct.unpack_from(fmt(name), blob, off)
    d, i = {}, 0
    for f, _t, n in STRUCTS[name]:
        if n > 1:
            d[f] = list(raw[i:i + n])
        else:
            d[f] = raw[i]
        i += n
    return d


# ---- constants straight from the header ---------------------------------------
def _header_constants(text):
    c = {}
    src = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    for m in re.finditer(r"#define\s+(SHW_\w+|SOP_\w+)\s+(\(?[0-9xXa-fA-FuU \*\(\)]+\)?)\s*$", src, re.M):
        try:
            c[m.group(1)] = int(eval(m.group(2).replace("u", "").replace("U", "")))
        except Exception:
            pass
    for m in re.finditer(r"enum\s*\{(.*?)\}", src, re.S):
        cur = -1
        for item in m.group(1).split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                k, v = item.split("=", 1)
                v = v.strip().replace("u <<", " <<").replace("1u", "1")
                cur = int(eval(v, {}, c))
                k = k.strip()
            else:
                k = item
                cur += 1
            c[k] = cur
    return c


def header_structs(text=None):
    """{struct name: [(field, ctype, count)]} parsed from shw_format.h (AT4.2)"""
    text = text if text is not None else HEADER.read_text()
    src = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    out = {}
    for m in re.finditer(r"typedef struct (\w+)\s*\{(.*?)\}\s*(\w+)\s*;", src, re.S):
        fields = []
        for decl in m.group(2).split(";"):
            decl = decl.strip()
            if not decl:
                continue
            t, rest = decl.split(None, 1)
            for name in rest.split(","):
                name = name.strip()
                mm = re.match(r"(\w+)\s*\[(\d+)\]", name)
                fields.append((mm.group(1), t, int(mm.group(2))) if mm else (name, t, 1))
        out[m.group(3)] = fields
    return out


C = _header_constants(HEADER.read_text())
MAGIC, SHOT_MAGIC, ABI, SECTOR, NONE = (C["SHW_MAGIC"], C["SHW_SHOT_MAGIC"], C["SHW_ABI"],
                                        C["SHW_SECTOR"], C["SHW_NONE"])
SOP_FLAG = C["SHW_SOP_FLAG"]
CHUNK_MAX = 160 * 1024
SOP = {k: v for k, v in C.items() if k.startswith("SOP_")}


def align(n, a=4):
    return (n + a - 1) // a * a


# ---- the string table -----------------------------------------------------------
class StrTab:
    """NUL-terminated names; offset of each, de-duplicated"""

    def __init__(self):
        self.buf = bytearray()
        self.at = {}

    def add(self, s):
        if s is None:
            return NONE
        s = str(s)
        if s not in self.at:
            if len(self.buf) > 0xFFF0:
                raise ValueError("string table over 64 KB")
            self.at[s] = len(self.buf)
            self.buf += s.encode("utf-8") + b"\0"
        return self.at[s]

    def bytes(self):
        return bytes(self.buf) + b"\0" * (align(len(self.buf)) - len(self.buf))


def cstr(blob, off):
    if off == NONE:
        return None
    end = blob.index(b"\0", off)
    return blob[off:end].decode("utf-8")


# ---- one shot chunk -------------------------------------------------------------
SECTIONS = (  # (key in the shot dict, struct, ShwShotHdr count field, offset field)
    ("looks", "ShwLook", "look_n", "off_looks"),
    ("planes", "ShwPlane", "plane_n", "off_planes"),
    ("actors", "ShwActor", "actor_n", "off_actors"),
    ("props", "ShwProp", "prop_n", "off_props"),
    ("crowds", "ShwCrowd", "crowd_n", "off_crowds"),
    ("camkeys", "ShwCamKey", "camkey_n", "off_camkeys"),
    ("events", "BeatEv", "ev_n", "off_events"),
)


def write_chunk(shot):
    """shot: {'hdr': {...ShwShotHdr scalars...}, 'looks': [bytes48], 'planes': [dict], ...,
    'args': [int32], 'tracks': [{'actor','kind','runs':[(v,c)]}],
    'strokes': [{..., 'pts': [(x,y)]}], 'strings': bytes, 'texpages': [{..., 'data': bytes}],
    'cluts': [[bgr555, ...]]} -> bytes"""
    hdr = dict(shot.get("hdr", {}))
    hdr["magic"] = SHOT_MAGIC
    body = bytearray(b"\0" * SIZES["ShwShotHdr"])

    def here():
        while len(body) % 4:
            body.append(0)
        return len(body)

    for key, sname, nfield, ofield in SECTIONS:
        items = shot.get(key) or []
        hdr[nfield] = len(items)
        hdr[ofield] = here() if items else 0
        for it in items:
            body += (bytes(it) if sname == "ShwLook" else pack(sname, it))
    args = shot.get("args") or []
    hdr["arg_n"] = len(args)
    hdr["off_args"] = here() if args else 0
    for a in args:
        body += struct.pack("<i", a)
    # tracks: records, then the runs
    tracks = shot.get("tracks") or []
    hdr["track_n"] = len(tracks)
    hdr["off_tracks"] = here() if tracks else 0
    rec_at = len(body)
    body += b"\0" * (SIZES["ShwTrack"] * len(tracks))
    for i, t in enumerate(tracks):
        off = here()
        for v, c in t["runs"]:
            body += pack("ShwRun", {"value": v, "count": c})
        body[rec_at + i * 8: rec_at + i * 8 + 8] = pack(
            "ShwTrack", {"actor": t["actor"], "kind": t["kind"], "run_n": len(t["runs"]), "off_runs": off})
    strokes = shot.get("strokes") or []
    hdr["stroke_n"] = len(strokes)
    hdr["off_strokes"] = here() if strokes else 0
    rec_at = len(body)
    body += b"\0" * (SIZES["ShwStroke"] * len(strokes))
    for i, s in enumerate(strokes):
        off = here()
        for x, y in s["pts"]:
            body += pack("ShwPoint", {"x": x, "y": y})
        d = dict(s)
        d["pt_n"], d["off_pts"] = len(s["pts"]), off
        body[rec_at + i * 16: rec_at + i * 16 + 16] = pack("ShwStroke", d)
    strings = shot.get("strings") or b""
    hdr["strings_bytes"] = len(strings)
    hdr["off_strings"] = here() if strings else 0
    body += strings
    pages = shot.get("texpages") or []
    hdr["texpage_n"] = len(pages)
    hdr["off_texpages"] = here() if pages else 0
    rec_at = len(body)
    body += b"\0" * (SIZES["ShwTexPage"] * len(pages))
    for i, p in enumerate(pages):
        off = here()
        body += p["data"]
        d = dict(p)
        d["bytes"], d["off"] = len(p["data"]), off
        body[rec_at + i * 16: rec_at + i * 16 + 16] = pack("ShwTexPage", d)
    cluts = shot.get("cluts") or []
    hdr["clut_n"] = len(cluts)
    hdr["off_cluts"] = here() if cluts else 0
    rec_at = len(body)
    body += b"\0" * (SIZES["ShwClut"] * len(cluts))
    for i, cl in enumerate(cluts):
        off = here()
        body += struct.pack("<%dH" % len(cl), *cl)
        body[rec_at + i * 8: rec_at + i * 8 + 8] = pack("ShwClut", {"n": len(cl), "off": off})
    here()
    body[0:SIZES["ShwShotHdr"]] = pack("ShwShotHdr", hdr)
    return bytes(body)


def read_chunk(blob):
    hdr = unpack("ShwShotHdr", blob)
    if hdr["magic"] != SHOT_MAGIC:
        raise ValueError("not a SHOT chunk")
    shot = {"hdr": hdr}
    for key, sname, nfield, ofield in SECTIONS:
        n, off, sz = hdr[nfield], hdr[ofield], SIZES[sname]
        if sname == "ShwLook":
            shot[key] = [bytes(blob[off + i * sz: off + (i + 1) * sz]) for i in range(n)]
        else:
            shot[key] = [unpack(sname, blob, off + i * sz) for i in range(n)]
    shot["args"] = list(struct.unpack_from("<%di" % hdr["arg_n"], blob, hdr["off_args"])) if hdr["arg_n"] else []
    shot["tracks"] = []
    for i in range(hdr["track_n"]):
        t = unpack("ShwTrack", blob, hdr["off_tracks"] + i * 8)
        runs = [tuple(unpack("ShwRun", blob, t["off_runs"] + j * 2).values()) for j in range(t["run_n"])]
        shot["tracks"].append({"actor": t["actor"], "kind": t["kind"], "runs": runs})
    shot["strokes"] = []
    for i in range(hdr["stroke_n"]):
        s = unpack("ShwStroke", blob, hdr["off_strokes"] + i * 16)
        s["pts"] = [(p["x"], p["y"]) for p in (unpack("ShwPoint", blob, s["off_pts"] + j * 4)
                                               for j in range(s["pt_n"]))]
        shot["strokes"].append(s)
    shot["strings"] = bytes(blob[hdr["off_strings"]: hdr["off_strings"] + hdr["strings_bytes"]])
    shot["texpages"] = []
    for i in range(hdr["texpage_n"]):
        p = unpack("ShwTexPage", blob, hdr["off_texpages"] + i * 16)
        p["data"] = bytes(blob[p["off"]: p["off"] + p["bytes"]])
        shot["texpages"].append(p)
    shot["cluts"] = []
    for i in range(hdr["clut_n"]):
        c = unpack("ShwClut", blob, hdr["off_cluts"] + i * 8)
        shot["cluts"].append(list(struct.unpack_from("<%dH" % c["n"], blob, c["off"])))
    return shot


# ---- the episode ----------------------------------------------------------------
def write_episode(ep, shots, strtab):
    """ep: ShwEpHeader fields (magic/abi/nshots/strtab/total filled here);
    shots: [(ShwShotIndex fields, chunk bytes)]; strtab: StrTab -> bytes"""
    n = len(shots)
    st = strtab.bytes()
    head = SIZES["ShwEpHeader"] + n * SIZES["ShwShotIndex"]
    strtab_off = align(head)
    pos = align(strtab_off + len(st), SECTOR)
    idx_blobs, chunks = [], []
    for ix, chunk in shots:
        ix = dict(ix)
        ix["chunk_sector"] = pos // SECTOR
        ix["chunk_bytes"] = len(chunk)
        ix["chunk_crc"] = zlib.crc32(chunk) & 0xFFFFFFFF
        idx_blobs.append(pack("ShwShotIndex", ix))
        chunks.append((pos, chunk))
        pos = align(pos + len(chunk), SECTOR)
    e = dict(ep)
    e.update(magic=MAGIC, abi=ABI, nshots=n, strtab_off=strtab_off, strtab_bytes=len(st))
    e.setdefault("total_frames", sum(unpack("ShwShotIndex", b)["len"] for b in idx_blobs))
    out = bytearray(pack("ShwEpHeader", e))
    for b in idx_blobs:
        out += b
    out += b"\0" * (strtab_off - len(out))
    out += st
    for at, chunk in chunks:
        out += b"\0" * (at - len(out))
        out += chunk
    out += b"\0" * (align(len(out), SECTOR) - len(out))
    return bytes(out)


def read_episode(blob):
    ep = unpack("ShwEpHeader", blob)
    if ep["magic"] != MAGIC:
        raise ValueError("not an SHW1 file")
    if ep["abi"] != ABI:
        raise ValueError("SHW abi %d, this tool reads %d" % (ep["abi"], ABI))
    shots = []
    for i in range(ep["nshots"]):
        ix = unpack("ShwShotIndex", blob, SIZES["ShwEpHeader"] + i * SIZES["ShwShotIndex"])
        at = ix["chunk_sector"] * SECTOR
        chunk = blob[at: at + ix["chunk_bytes"]]
        if zlib.crc32(chunk) & 0xFFFFFFFF != ix["chunk_crc"]:
            raise ValueError("shot %d: chunk CRC mismatch" % i)
        shots.append((ix, read_chunk(chunk)))
    strtab = blob[ep["strtab_off"]: ep["strtab_off"] + ep["strtab_bytes"]]
    return ep, shots, strtab
