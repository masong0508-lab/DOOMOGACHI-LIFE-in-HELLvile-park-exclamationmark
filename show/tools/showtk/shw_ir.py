"""shw_ir.py - the compiler's in-memory shot representation (ENGINE, WP4).

showc resolves every shot into plain dicts and lists (planes, props, actors,
crowds, camera keys, show ops by NAME, strings, tracks); emit.py turns that
into the player's file format (shw_format.py / shw_format.h, owned by the
player). Nothing here is written to disk as is, so this module can change
freely; the on-disk contract is shw_format.h.
"""
NONE = 0xFFFF
CHUNK_MAX = 160 * 1024

# shot flags kept by the compiler (emit maps them to ShwShotIndex.flags,
# the PIC1 fx setup and the timeline)
C = {
    "SHW_SHOT_SLUG": 1 << 0,
    "SHW_SHOT_INSERT": 1 << 1,
    "SHW_SHOT_MATTE_BLACK": 1 << 2,
    "SHW_SHOT_TEXT_OFF": 1 << 3,
    "SHW_SHOT_GORE_BRUTAL": 1 << 4,
    "SHW_SHOT_FADE_IN": 1 << 5,
    "SHW_SHOT_PROVISIONAL": 1 << 6,
}


class StrTab:
    """NUL-terminated names, de-duplicated; offsets are only meaningful in memory."""

    def __init__(self):
        self.buf = bytearray()
        self.at = {}
        self.by_off = {}

    def add(self, s):
        if s is None:
            return NONE
        s = str(s)
        if s not in self.at:
            self.at[s] = len(self.buf)
            self.by_off[len(self.buf)] = s
            self.buf += s.encode("utf-8") + b"\0"
        return self.at[s]

    def get(self, off):
        return self.by_off.get(off)
