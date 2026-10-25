"""shw_format.py - the EP###.SHW format, mirrored field for field from
show/include/show/shw_format.h (spec 7.3). Owner: ENGINE.

The C header is the source of truth; tests/compiler/test_shw_format.py
(AT4.2) parses the header's structs and enums and checks this module
against them. Change both in one commit.

Writing an episode:

    from showtk import shw_format as F
    ch = F.Chunk(shot=0, bg=F.PAL_BLACK)
    ch.planes.append(F.plane_bands(id=1, order=0, bands=[(0, 140), (90, 141)]))
    ch.add_beat([(0, 0, F.BOP_END, 0, 0)])
    ep = F.Episode(episode=101, seed=0x101)
    ep.add_shot("co01", len=120, chunk=ch, px=1, seed=1)
    open("EP101.SHW", "wb").write(ep.build())

Reading one back (tests, verify): F.read_episode(data) -> dict.
"""
from __future__ import annotations

import hashlib
import struct
import zlib
from dataclasses import dataclass, field

SHW_MAGIC = 0x31574853
SHW_SHOT_MAGIC = 0x544F4853
SHW_ABI = 1
SHW_SECTOR = 2048
SHW_CHUNK_MAX = 160 * 1024
SHW_ID_LEN = 8

# struct formats (little-endian, no implicit padding: sizes are asserted)
EP_HEADER = struct.Struct("<IHHBBHHHIHH8sIIIIIIII")          # 64
SHOT_INDEX = struct.Struct("<IHBBIIHHHHHHI8sII")             # 48
SHOT_HDR = struct.Struct("<IHHIBBBB4HII")                    # 32
REC = struct.Struct("<HHI")                                  # 8
PLANE = struct.Struct("<BBHhHhhhhhhHH")                      # 24
IMAGE = struct.Struct("<HHBBHI")                             # 12
SLUG = struct.Struct("<HHHBBIHH")                            # 16
AUDIO_K = struct.Struct("<HHI")                              # 8
BEATEV = struct.Struct("<HBBhh")                             # 8 (core/include/doomo/beat.h)
BAND = struct.Struct("<hBB")                                 # 4
assert (EP_HEADER.size, SHOT_INDEX.size, SHOT_HDR.size, REC.size, PLANE.size, IMAGE.size,
        SLUG.size, AUDIO_K.size, BEATEV.size) == (64, 48, 32, 8, 24, 12, 16, 8, 8)

# ShwShotIndex.flags
SHWF_SLUG, SHWF_HOLDLAST, SHWF_3D = 1, 2, 4
SHWB_PS1_LEGAL, SHWB_EMU_ONLY = 0, 1

# record types (ADD-ONLY; keep in step with the header)
SHWR = dict(
    NONE=0, LOOK=1, PLANE=2, BEAT=3, ARGS=4, STRINGS=5, IMAGE=6, SLUG=7, AUDIO_K=8, CLUT=9,
    CAMSETUP=16, CAMKEY=17, STROKES=18, TILEMAP=19, TEXT=20, FX=21, FONT=22,
    ACTOR=32, TRACK=33, CROWD=34, PROP=35,
    OVERWORLD=48, ROOM=49, VIEW3D=50, HUD=51, RAIL=52,
    PRIV_L2=0x100, PRIV_L3=0x200, PRIV_L1=0x300,
)
globals().update({"SHWR_" + k: v for k, v in SHWR.items()})

SHWP = dict(NONE=0, BANDS=1, IMAGE=2, TILEMAP=3, STROKES=4, GRAIN=5, VIEW3D=6, OVERWORLD=7,
            ROOM=8, FILL=9, COUNT=16)
globals().update({"SHWP_" + k: v for k, v in SHWP.items()})
SHWPF_NOSHAKE, SHWPF_HIDDEN, SHWPF_TILE_X, SHWPF_TILE_Y = 1, 2, 4, 8
SHWI_RAW, SHWI_RLE = 0, 1

SHOW_OP_FLAG = 0x4000
SOP = dict(NOP=0x000, PLANE_SHOW=0x001, PLANE_HIDE=0x002, MARK=0x003, FADE_TO=0x004,
           PAL_TO=0x005, IRIS=0x006, TEXT_BASE=0x040, FX_BASE=0x060, CAM_BASE=0x080,
           ACTOR_BASE=0x0A0, CROWD_BASE=0x0C0, HOST_BASE=0x100)
globals().update({"SOP_" + k: v for k, v in SOP.items()})

# the beat ops the player sees (core/include/doomo/beat.h enum order)
_BOPS = ("END LABEL WAIT_A HOLD SAY CAPTION POP EXPR ACT FREEZE THAW PANIC HITSTOP SHAKE PUNCH "
         "ZOOM FLASH FADE IRIS_OUT IRIS_IN CUT INSERT SFX MUSIC MUSIC_MUTE MUSIC_DUCK TAPESTOP FLAG "
         "STILL TWITCH NPC CAM TINT PAL_LERP NOBUTTON SHOT HEADTURN COUNT").split()
BOP = {n: i for i, n in enumerate(_BOPS)}
globals().update({"BOP_" + k: v for k, v in BOP.items()})

PAL_BLACK = 1          # core palette.h: 0 transparent, 1 black, 2 white
PAL_WHITE = 2


def pal_ramp(ramp: int, shade: int) -> int:
    """PAL_RAMP(r, shade) from palette.h."""
    return 136 + ramp * 8 + shade


def _pad4(b: bytes) -> bytes:
    return b + b"\0" * (-len(b) % 4)


def show_op(op: int, b: int = 0, at: int = 0, anchor: int = 0) -> tuple:
    """A show op as a BeatEv tuple (at, anchor, op, a, b)."""
    return (at, anchor, BOP_SHOT, SHOW_OP_FLAG | op, b)


# ---- planes ---------------------------------------------------------------
def plane(kind, payload: bytes, *, id=0, order=0, flags=0, par=(1, 1, 1, 1), at=(0, 0),
          spar_q8=256) -> bytes:
    payload = _pad4(payload)
    return PLANE.pack(kind, flags, PLANE.size + len(payload), order, id, par[0], par[1], par[2],
                      par[3], at[0], at[1], spar_q8, 0) + payload


def plane_bands(bands, **kw) -> bytes:
    """bands: [(y, colour), ...] top to bottom, y in plane px."""
    body = struct.pack("<HH", len(bands), 0) + b"".join(BAND.pack(y, c, 0) for y, c in bands)
    return plane(SHWP_BANDS, body, **kw)


def plane_fill(color, **kw) -> bytes:
    return plane(SHWP_FILL, struct.pack("<BBBB", color, 0, 0, 0), **kw)


def plane_image(image, scale=0, flip=0, **kw) -> bytes:
    return plane(SHWP_IMAGE, struct.pack("<HBB", image, scale, flip), **kw)


def plane_grain(color, density, seed, boil=0, **kw) -> bytes:
    return plane(SHWP_GRAIN, struct.pack("<BBBBI", color, density, boil, 0, seed), **kw)


def image(w, h, pix: bytes, id=0, rle=None) -> bytes:
    """An 8bpp image (index 0 transparent). RLE is chosen when it is smaller."""
    assert len(pix) == w * h
    if rle is None or rle:
        out = bytearray()
        i = 0
        while i < len(pix):
            j = i
            while j < len(pix) and j - i < 255 and pix[j] == pix[i]:
                j += 1
            out += bytes((j - i, pix[i]))
            i = j
        if rle or len(out) < len(pix):
            data, comp = bytes(out), SHWI_RLE
        else:
            data, comp = pix, SHWI_RAW
    else:
        data, comp = pix, SHWI_RAW
    data = _pad4(data)
    return IMAGE.pack(w, h, comp, 0, id, IMAGE.size + len(data)) + data


# ---- a shot chunk ---------------------------------------------------------
@dataclass
class Chunk:
    shot: int = 0
    bg: int = PAL_BLACK
    set_kind: int = 0
    badge: int = SHWB_PS1_LEGAL
    nlabels: int = 0
    vram_plan: tuple = (0, 0, 0, 0)
    planes: list = field(default_factory=list)          # bytes from plane_*()
    images: list = field(default_factory=list)          # bytes from image()
    records: dict = field(default_factory=dict)         # type -> (n, payload bytes)
    strings: bytearray = field(default_factory=bytearray)

    def add(self, rtype: int, n: int, payload: bytes):
        assert rtype not in self.records, f"record type {rtype} twice in one chunk"
        self.records[rtype] = (n, bytes(payload))

    def add_beat(self, events, nlabels=None):
        """events: (at, anchor, op, a, b) tuples; sorted by (anchor, at) here."""
        ev = sorted(events, key=lambda e: (e[1], e[0]))
        if not ev or ev[-1][2] != BOP_END:
            last = max([e[0] for e in ev if e[1] == 0] or [0])
            ev.append((last, 0, BOP_END, 0, 0))
        self.add(SHWR_BEAT, len(ev), b"".join(BEATEV.pack(*e) for e in ev))
        if nlabels is not None:
            self.nlabels = nlabels

    def add_args(self, words):
        self.add(SHWR_ARGS, len(words), struct.pack(f"<{len(words)}i", *words))

    def str(self, s: str) -> int:
        """Intern a string; returns its offset in SHWR_STRINGS."""
        b = s.encode("latin-1", "replace") + b"\0"
        i = self.strings.find(b)
        if i >= 0 and (i == 0 or self.strings[i - 1] == 0):
            return i
        off = len(self.strings)
        self.strings += b
        return off

    def add_slug(self, title, line1="", line2="", beat=0, act=0, bg=PAL_BLACK, fg=PAL_WHITE):
        t, a, b = self.str(title), self.str(line1), self.str(line2)
        self.add(SHWR_SLUG, 1, SLUG.pack(t, a, b, bg, fg, beat, act, 0))

    def add_audio_k(self, items):
        """items: [(frame, k), ...]"""
        self.add(SHWR_AUDIO_K, len(items), b"".join(AUDIO_K.pack(f, 0, k) for f, k in items))

    def build(self) -> bytes:
        recs = dict(self.records)
        if self.planes:
            recs[SHWR_PLANE] = (len(self.planes), b"".join(self.planes))
        if self.images:
            recs[SHWR_IMAGE] = (len(self.images), b"".join(self.images))
        if self.strings:
            recs[SHWR_STRINGS] = (len(self.strings), bytes(self.strings))
        body = b""
        for rtype in sorted(recs):
            n, pay = recs[rtype]
            body += REC.pack(rtype, n, len(pay)) + _pad4(pay)
        crc = zlib.crc32(body) & 0xFFFFFFFF
        total = SHOT_HDR.size + len(body)
        if total > SHW_CHUNK_MAX:
            raise ValueError(f"shot chunk {total} B > {SHW_CHUNK_MAX} B (spec 7.3)")
        hdr = SHOT_HDR.pack(SHW_SHOT_MAGIC, len(recs), self.shot, total, self.set_kind, self.badge,
                            self.bg, self.nlabels, *self.vram_plan, crc, 0)
        return hdr + body


# ---- the episode file -------------------------------------------------------
@dataclass
class Episode:
    episode: int = 101
    seed: int = 0x00101101
    toolchain: bytes = b"showc-v0"
    shots: list = field(default_factory=list)

    def add_shot(self, id, len, chunk, px=1, seed=0, flags=0, preroll=0, beat=0, look=0,
                 base=(0xFFFF, 0), replay=(0xFFFF, 0)):
        if isinstance(chunk, Chunk):
            chunk.shot = self._n()
            chunk = chunk.build()
        self.shots.append(dict(id=id, len=len, chunk=chunk, px=px, seed=seed, flags=flags,
                               preroll=preroll, beat=beat, look=look, base=base, replay=replay))

    def _n(self):
        return self.shots.__len__()

    def build(self) -> bytes:
        n = len(self.shots)
        index_off = EP_HEADER.size
        strtab_off = index_off + n * SHOT_INDEX.size
        strtab = b""
        head_end = strtab_off + len(strtab)
        sector = (head_end + SHW_SECTOR - 1) // SHW_SECTOR
        chunks, index, start = b"", b"", 0
        for s in self.shots:
            c = s["chunk"]
            secs = (len(c) + SHW_SECTOR - 1) // SHW_SECTOR
            sid = s["id"].encode()[:SHW_ID_LEN].ljust(SHW_ID_LEN, b"\0")
            index += SHOT_INDEX.pack(start, s["len"], s["px"], s["look"], sector, len(c),
                                     s["base"][0], s["base"][1], s["replay"][0], s["replay"][1],
                                     s["flags"], s["preroll"], s["seed"], sid, s["beat"], 0)
            chunks += c + b"\0" * (secs * SHW_SECTOR - len(c))
            sector += secs
            start += s["len"]
        th = hashlib.sha256(self.toolchain).digest()[:8]
        head_wo_size = (index_off, strtab_off, len(strtab), start)
        body_len = ((head_end + SHW_SECTOR - 1) // SHW_SECTOR) * SHW_SECTOR + len(chunks)
        hdr = EP_HEADER.pack(SHW_MAGIC, SHW_ABI, n, 30, 1, 320, 180, 30, self.seed, self.episode, 0,
                             th, *head_wo_size, body_len, 0, 0, 0)
        head = hdr + index + strtab
        head += b"\0" * (-len(head) % SHW_SECTOR)
        return head + chunks


def read_episode(data: bytes) -> dict:
    """Parse an episode file (header, index, and each chunk's record list)."""
    h = EP_HEADER.unpack_from(data, 0)
    keys = ("magic abi nshots fps px_canvas canvas_w canvas_h y_off seed episode pad "
            "toolchain_hash index_off strtab_off strtab_bytes total_frames file_bytes r0 r1 r2").split()
    hd = dict(zip(keys, h))
    assert hd["magic"] == SHW_MAGIC, "not an SHW1 file"
    shots = []
    ik = ("start_frame len px look chunk_sector chunk_bytes base_shot base_frame replay_shot "
          "replay_from flags preroll seed id beat reserved").split()
    for i in range(hd["nshots"]):
        s = dict(zip(ik, SHOT_INDEX.unpack_from(data, hd["index_off"] + i * SHOT_INDEX.size)))
        s["id"] = s["id"].rstrip(b"\0").decode()
        off = s["chunk_sector"] * SHW_SECTOR
        c = data[off:off + s["chunk_bytes"]]
        ch = SHOT_HDR.unpack_from(c, 0)
        s["chunk_hdr"] = dict(zip(("magic nrec shot bytes set_kind badge bg nlabels v0 v1 v2 v3 "
                                   "crc reserved").split(), ch))
        recs, p = {}, SHOT_HDR.size
        for _ in range(ch[1]):
            t, n, b = REC.unpack_from(c, p)
            recs[t] = (n, c[p + REC.size:p + REC.size + b])
            p += REC.size + b + (-b % 4)
        s["records"] = recs
        shots.append(s)
    hd["shots"] = shots
    return hd
