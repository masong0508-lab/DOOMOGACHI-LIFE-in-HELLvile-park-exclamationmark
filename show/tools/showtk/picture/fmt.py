"""fmt.py - the lane-2 record layouts, byte for byte the C structs in
show/include/show/show_{cam,fx,strokes,tilemap,text}.h (little-endian,
natural alignment, every size a multiple of 4). tests/engine/picture checks
each size against sizeof() from a host build of the headers.
"""
import struct

CAM_CU = 3600
Q16 = 65536

# ---- show_cam.h ---------------------------------------------------------------
CK_SET, CK_MOVE, CK_VEL, CK_CREEP = 0, 1, 2, 3
CA_X, CA_Y, CA_S = 1, 2, 4
CE = {"LINEAR": 0, "IN": 1, "OUT": 2, "INOUT": 3, "WHIP": 4}
CKF_WHIP = 1
CAMKEY = struct.Struct("<HHBBBBiii")              # 20
CAMSETUP = struct.Struct("<iiihh")                # 16
PLANECAM = struct.Struct("<hhhhhhHBB")            # 16

# ---- show_fx.h -------------------------------------------------------------------
LOOKS = {"pixel": 0, "crayon": 1, "sepia": 2, "lcd": 3, "hellify": 4}
MATTE = {"open": 0, "black": 1}
FXSETUP = struct.Struct("<BBBBBBBBIHH")           # 16
WIPE_SWEEP, WIPE_STACK = 0, 1
WIPE_B_RIGHT, WIPE_B_LEFT = 0, 1
WIPE_HOLD_END = 0xFFFF
WIPE = struct.Struct("<HHBBBBhBBBBBBB3x")         # 20

# ---- show_strokes.h ------------------------------------------------------------
SK_ALWAYS = 0xFFFF
SK_PEN_UP = -32768
SK_HIDDEN, SK_NOBOIL = 1, 2
STROKESET = struct.Struct("<HHIBB2x")             # 12
STROKE = struct.Struct("<HHHHBBBBBBH")            # 16
STROKEPT = struct.Struct("<hh")                   # 4

# ---- show_tilemap.h --------------------------------------------------------------
TM_TILE, TM_ATLAS, TM_CELL, TM_PER_ROW, TM_PER_ATLAS, TM_EMPTY = 16, 128, 18, 7, 49, 0xFFFF
TILEMAP = struct.Struct("<HHHBBIHH")              # 16

# ---- show_text.h -----------------------------------------------------------------
TS = {"type": 0, "pop": 1, "deadpan": 2, "sign": 3, "box": 4}
TF = {"game": 0, "child_hand": 1, "child": 1}
TA = {"left": 0, "center": 1, "right": 2}
TX_HOP, TX_SLOT, TX_NOOUTLINE = 1, 2, 4
TX_END = 0xFFFF
TEXTITEM = struct.Struct("<HHHHHHhhhhBBBBBBBB")   # 28
TEXTSET = struct.Struct("<HHI")                   # 8

SIZES = {"ShowCamKey": CAMKEY.size, "ShowCamSetup": CAMSETUP.size, "ShowPlaneCam": PLANECAM.size,
         "ShowFxSetup": FXSETUP.size, "ShowWipe": WIPE.size, "ShowStrokeSet": STROKESET.size,
         "ShowStroke": STROKE.size, "ShowStrokePt": STROKEPT.size, "ShowTilemap": TILEMAP.size,
         "ShowTextItem": TEXTITEM.size, "ShowTextSet": TEXTSET.size}

# ---- lane-2 show ops (show_l2.h) ---------------------------------------------------
SOP_L2_BASE = 0x0100
SOP_IRIS, SOP_BOIL, SOP_FADE_SET = 0x0100, 0x0101, 0x0102


def pad4(b):
    return b + b"\0" * (-len(b) % 4)


def camkey(t0, t1, kind, axis, a, b=0, c=0, ease=0, flags=0):
    return CAMKEY.pack(t0, t1, kind, axis, ease, flags, int(a), int(b), int(c))


def camsetup(x=0, y=0, s=Q16, cx=0, cy=0):
    return CAMSETUP.pack(int(x), int(y), int(s), cx, cy)


def planecam(par=(0, 1), par_y=None, at=(0, 0), spar_q8=256, noshake=0):
    py = par if par_y is None else par_y
    return PLANECAM.pack(par[0], par[1], py[0], py[1], at[0], at[1], spar_q8, noshake, 0)
