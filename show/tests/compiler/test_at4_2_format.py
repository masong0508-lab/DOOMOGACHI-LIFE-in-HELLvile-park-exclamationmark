"""AT4.2: shw_format.py round-trips with shw_format.h.

1. The struct tables in shw_format.py equal the ones parsed from the header.
2. The host gcc compiles the header; sizeof/offsetof of every field match.
3. A file written by Python is read by a C program through the header's
   structs and dumped; the dump equals Python's own reading.
4. Python write -> read -> write is byte-identical.
"""
import os
import shutil
import subprocess
import sys
import tempfile

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "show", "tools"))

from showtk import shw_format as F  # noqa: E402

INC = os.path.join(ROOT, "show", "include")


def sample_episode():
    st = F.StrTab()
    look = bytes(range(48))
    shot = {
        "hdr": {"set_kind": 1, "badge_est": 0, "set_name": st.add("park_storybook"), "vram_plan": [2, 1, 0, 1]},
        "looks": [look, bytes(48)],
        "planes": [{"kind": 1, "order": 0, "par_q16": 3277, "w": 352, "h": 240, "tex": 0, "clut": 0, "x": -16, "y": -30}],
        "actors": [{"kind": 1, "look": 0, "face": 1, "expr": 1, "idle": 0, "phase": 39, "px": 1, "x": 196, "y": 96,
                    "name": st.add("mayor"), "roster": 0}],
        "props": [{"plane": 0, "face": 0, "anchor": 1, "x": 160, "y": 92, "art": st.add("gen:bandstand"), "tex": F.NONE}],
        "crowds": [{"look_base": 1, "n": 6, "lane": 0, "dir": 0, "kind": 2, "ripple_lo": 0, "ripple_hi": 3, "age": 2,
                    "spacing_q8": 26 << 8, "from": -32, "y": 118, "cycle": st.add("walk4"), "speed_q16": 120149,
                    "seed": 301, "clock_mask": 0x3F, "gen": 1, "species": 3, "look_k": 0, "flags": 1}],
        "camkeys": [{"frame": 0, "mode": 1, "x_q16": -4915200, "vx_q16": 54613, "scale_q8": 256}],
        "events": [{"at": 0, "anchor": 0, "op": 19, "a": 1, "b": 5},
                   {"at": 0, "anchor": 0, "op": 33, "a": F.SOP_FLAG | F.SOP["SOP_AUDIO"], "b": 0},
                   {"at": 180, "anchor": 0, "op": 0, "a": 0, "b": 0}],
        "args": [1, 3, -7, 123456],
        "tracks": [{"actor": 0, "kind": 0, "runs": [(0, 10), (5, 3), (0, 255)]}],
        "strokes": [{"nib": 9, "width": 2, "flags": 0, "boil": 2, "reveal_from": 0, "reveal_to": 24,
                     "name": st.add("circle_midsummer"), "pts": [(1, 2), (-3, 4), (5, -6)]}],
        "strings": b"Every Midsummer night.\0It was called {d}the Mingle.\0\0",
        "texpages": [{"w": 4, "h": 2, "bpp": 8, "comp": 0, "name": st.add("paper"), "data": bytes(range(8))}],
        "cluts": [[0, 0x7FFF, 0x1234]],
    }
    chunk = F.write_chunk(shot)
    ix = {"start_frame": 90, "len": 180, "px": 1, "look": 1, "base_shot": F.NONE, "base_frame": 0,
          "replay_shot": F.NONE, "replay_from": 0, "flags": (0 << 8), "name": st.add("co02"), "seed": 0xD00D ^ 2}
    ix2 = dict(ix, start_frame=270, len=120, px=2, name=st.add("co03"), base_shot=0, base_frame=90)
    ep = {"fps": 30, "flags": 0, "canvas_w": 320, "canvas_h": 180, "canvas_y": 30, "seed": 0xD00D,
          "toolchain_hash": list(range(8))}
    return F.write_episode(ep, [(ix, chunk), (ix2, F.write_chunk({"hdr": {}}))], st)


def py_dump(blob):
    ep, shots, _st = F.read_episode(blob)
    lines = []

    def emit(name, d):
        for f, _t, n in F.STRUCTS[name]:
            v = d[f]
            lines.append("%s.%s=%s" % (name, f, ",".join(str(x) for x in v) if n > 1 else v))
    emit("ShwEpHeader", ep)
    for ix, sh in shots:
        emit("ShwShotIndex", ix)
        emit("ShwShotHdr", sh["hdr"])
        for key, sname, _n, _o in F.SECTIONS:
            for it in sh[key]:
                if sname == "ShwLook":
                    lines.append("ShwLook.b=" + ",".join(str(x) for x in it))
                else:
                    emit(sname, it)
        for a in sh["args"]:
            lines.append("arg=%d" % a)
    return "\n".join(lines) + "\n"


C_SRC = r'''
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "show/shw_format.h"
typedef struct BeatEv { uint16_t at; uint8_t anchor; uint8_t op; int16_t a, b; } BeatEv;
static unsigned char *B;
#define P(S, f) printf(#S "." #f "=%ld\n", (long)(s->f))
#define PA(S, f, n) do { printf(#S "." #f "="); for (int i_ = 0; i_ < (n); i_++) printf(i_ ? ",%ld" : "%ld", (long)s->f[i_]); printf("\n"); } while (0)
static void ep(const ShwEpHeader *s) { P(ShwEpHeader,magic); P(ShwEpHeader,abi); P(ShwEpHeader,nshots); P(ShwEpHeader,fps); P(ShwEpHeader,flags); P(ShwEpHeader,canvas_w); P(ShwEpHeader,canvas_h); P(ShwEpHeader,canvas_y); P(ShwEpHeader,seed); PA(ShwEpHeader,toolchain_hash,8); P(ShwEpHeader,strtab_off); P(ShwEpHeader,strtab_bytes); P(ShwEpHeader,total_frames); }
static void ix(const ShwShotIndex *s) { P(ShwShotIndex,start_frame); P(ShwShotIndex,len); P(ShwShotIndex,px); P(ShwShotIndex,look); P(ShwShotIndex,chunk_sector); P(ShwShotIndex,chunk_bytes); P(ShwShotIndex,base_shot); P(ShwShotIndex,base_frame); P(ShwShotIndex,replay_shot); P(ShwShotIndex,replay_from); P(ShwShotIndex,flags); P(ShwShotIndex,name); P(ShwShotIndex,seed); P(ShwShotIndex,chunk_crc); }
static void hd(const ShwShotHdr *s) { P(ShwShotHdr,magic); P(ShwShotHdr,set_kind); P(ShwShotHdr,plane_n); P(ShwShotHdr,actor_n); P(ShwShotHdr,prop_n); P(ShwShotHdr,crowd_n); P(ShwShotHdr,look_n); P(ShwShotHdr,track_n); P(ShwShotHdr,badge_est); P(ShwShotHdr,ev_n); P(ShwShotHdr,arg_n); P(ShwShotHdr,camkey_n); P(ShwShotHdr,stroke_n); P(ShwShotHdr,texpage_n); P(ShwShotHdr,clut_n); P(ShwShotHdr,strings_bytes); P(ShwShotHdr,set_name); P(ShwShotHdr,off_looks); P(ShwShotHdr,off_planes); P(ShwShotHdr,off_actors); P(ShwShotHdr,off_props); P(ShwShotHdr,off_crowds); P(ShwShotHdr,off_camkeys); P(ShwShotHdr,off_events); P(ShwShotHdr,off_args); P(ShwShotHdr,off_tracks); P(ShwShotHdr,off_strokes); P(ShwShotHdr,off_strings); P(ShwShotHdr,off_texpages); P(ShwShotHdr,off_cluts); PA(ShwShotHdr,vram_plan,4); }
static void lk(const ShwLook *s) { PA(ShwLook,b,48); }
static void pl(const ShwPlane *s) { P(ShwPlane,kind); P(ShwPlane,order); P(ShwPlane,flags); P(ShwPlane,boil); P(ShwPlane,par_q16); P(ShwPlane,w); P(ShwPlane,h); P(ShwPlane,tex); P(ShwPlane,clut); P(ShwPlane,x); P(ShwPlane,y); P(ShwPlane,arg); }
static void ac(const ShwActor *s) { P(ShwActor,kind); P(ShwActor,look); P(ShwActor,face); P(ShwActor,expr); P(ShwActor,idle); P(ShwActor,phase); P(ShwActor,px); P(ShwActor,state); P(ShwActor,x); P(ShwActor,y); P(ShwActor,name); P(ShwActor,roster); }
static void pr(const ShwProp *s) { P(ShwProp,plane); P(ShwProp,face); P(ShwProp,anchor); P(ShwProp,flags); P(ShwProp,x); P(ShwProp,y); P(ShwProp,art); P(ShwProp,tex); }
static void cr(const ShwCrowd *s) { P(ShwCrowd,look_base); P(ShwCrowd,n); P(ShwCrowd,lane); P(ShwCrowd,dir); P(ShwCrowd,kind); P(ShwCrowd,ripple_lo); P(ShwCrowd,ripple_hi); P(ShwCrowd,age); P(ShwCrowd,spacing_q8); P(ShwCrowd,from); P(ShwCrowd,y); P(ShwCrowd,cycle); P(ShwCrowd,speed_q16); P(ShwCrowd,seed); P(ShwCrowd,clock_mask); P(ShwCrowd,gen); P(ShwCrowd,species); P(ShwCrowd,look_k); P(ShwCrowd,flags); }
static void ck(const ShwCamKey *s) { P(ShwCamKey,frame); P(ShwCamKey,mode); P(ShwCamKey,ease); P(ShwCamKey,x_q16); P(ShwCamKey,y_q16); P(ShwCamKey,vx_q16); P(ShwCamKey,vy_q16); P(ShwCamKey,scale_q8); P(ShwCamKey,over); }
static void be(const BeatEv *s) { P(BeatEv,at); P(BeatEv,anchor); P(BeatEv,op); P(BeatEv,a); P(BeatEv,b); }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    B = malloc(n); if (fread(B, 1, n, f) != (size_t)n) return 2; fclose(f);
    const ShwEpHeader *e = (const ShwEpHeader *)B; ep(e);
    for (int i = 0; i < e->nshots; i++) {
        const ShwShotIndex *x = (const ShwShotIndex *)(B + sizeof(ShwEpHeader) + i * sizeof(ShwShotIndex)); ix(x);
        unsigned char *c = B + (size_t)x->chunk_sector * SHW_SECTOR;
        const ShwShotHdr *h = (const ShwShotHdr *)c; hd(h);
        for (int k = 0; k < h->look_n; k++) lk((const ShwLook *)(c + h->off_looks) + k);
        for (int k = 0; k < h->plane_n; k++) pl((const ShwPlane *)(c + h->off_planes) + k);
        for (int k = 0; k < h->actor_n; k++) ac((const ShwActor *)(c + h->off_actors) + k);
        for (int k = 0; k < h->prop_n; k++) pr((const ShwProp *)(c + h->off_props) + k);
        for (int k = 0; k < h->crowd_n; k++) cr((const ShwCrowd *)(c + h->off_crowds) + k);
        for (int k = 0; k < h->camkey_n; k++) ck((const ShwCamKey *)(c + h->off_camkeys) + k);
        for (int k = 0; k < h->ev_n; k++) be((const BeatEv *)(c + h->off_events) + k);
        for (int k = 0; k < h->arg_n; k++) printf("arg=%d\n", ((const int32_t *)(c + h->off_args))[k]);
    }
    return 0;
}
'''


def offsets_src():
    lines = ['#include <stdio.h>', '#include <stddef.h>', '#include "show/shw_format.h"', 'int main(void) {']
    for name, fields in F.STRUCTS.items():
        if name == "BeatEv":
            continue
        lines.append('printf("%s sizeof %%zu\\n", sizeof(%s));' % (name, name))
        for f, _t, _n in fields:
            lines.append('printf("%s.%s %%zu\\n", offsetof(%s, %s));' % (name, f, name, f))
    lines.append("return 0; }")
    return "\n".join(lines)


def py_offsets():
    import struct
    out = []
    for name, fields in F.STRUCTS.items():
        if name == "BeatEv":
            continue
        out.append("%s sizeof %d" % (name, F.SIZES[name]))
        off = 0
        for f, t, n in fields:
            out.append("%s.%s %d" % (name, f, off))
            off += struct.calcsize("<" + F.CTYPES[t]) * n
    return "\n".join(out) + "\n"


def test_tables_match_header():
    hs = F.header_structs()
    for name, fields in F.STRUCTS.items():
        if name == "BeatEv":
            continue
        assert hs.get(name) == fields, name
    assert set(hs) == set(F.STRUCTS) - {"BeatEv"}


def test_python_roundtrip_bytes():
    blob = sample_episode()
    ep, shots, st = F.read_episode(blob)
    sh = shots[0][1]
    assert sh["strokes"][0]["pts"] == [(1, 2), (-3, 4), (5, -6)]
    assert sh["tracks"][0]["runs"] == [(0, 10), (5, 3), (0, 255)]
    assert sh["cluts"] == [[0, 0x7FFF, 0x1234]]
    assert F.cstr(st, shots[1][0]["name"]) == "co03"
    # re-write from what was read: identical bytes
    st2 = F.StrTab()
    st2.buf = bytearray(st.rstrip(b"\0") + b"\0")
    again = F.write_episode({k: ep[k] for k in ("fps", "flags", "canvas_w", "canvas_h", "canvas_y", "seed",
                                                "toolchain_hash")},
                            [(ix, F.write_chunk(sh)) for ix, sh in shots], st2)
    assert again == blob


@pytest.mark.skipif(not shutil.which("gcc"), reason="no host gcc")
def test_c_header_agrees():
    blob = sample_episode()
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "off.c")
        open(src, "w").write(offsets_src())
        exe = os.path.join(d, "off")
        subprocess.run(["gcc", "-std=c99", "-Wall", "-Werror", "-I", INC, src, "-o", exe], check=True)
        assert subprocess.run([exe], capture_output=True, text=True, check=True).stdout == py_offsets()
        src = os.path.join(d, "dump.c")
        open(src, "w").write(C_SRC)
        exe = os.path.join(d, "dump")
        subprocess.run(["gcc", "-std=c99", "-Wall", "-I", INC, src, "-o", exe], check=True)
        path = os.path.join(d, "EP.SHW")
        open(path, "wb").write(blob)
        c_out = subprocess.run([exe, path], capture_output=True, text=True, check=True).stdout
        assert c_out == py_dump(blob)
