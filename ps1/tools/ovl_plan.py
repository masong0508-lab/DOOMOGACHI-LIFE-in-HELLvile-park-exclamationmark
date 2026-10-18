#!/usr/bin/env python3
"""ovl_plan.py - split the PS1 game's scene code into CD-loaded overlays
(owner: ps1). Stdlib only, deterministic.

    ovl_plan.py --readelf R --out-ld F.ld --out-c F.c [--report] OBJ...

The whole EXE (code, const data, BSS) must fit in the console's 2 MB of
RAM, and the game's scenes no longer do (docs/PS1.md section 3). Most of
that code only runs while its own scene is on screen, so it is linked
into overlays: groups of input sections that share one address range
(two "slots", see src/overlay.c) and are read from the disc when a scene
of the group is entered.

What may go into an overlay is decided from the relocations of the
object files (built with -ffunction-sections -fdata-sections), not by
hand:

  * every .text.* / .rodata.* section of a group's objects (GROUPS below)
    is a candidate; .data / .bss always stay resident;
  * a section is RESIDENT if resident code or data can reach it. The
    roots are everything outside the groups (platform, foundation, the
    SDK) and every section whose address escapes into memory at run
    time (a function or table address taken by code: a callback that
    resident code may call later). The only edges that are NOT followed
    are the four function pointers of a scene's `const DoomoScene`
    (SCENE_*), because scene.c calls those through plat_scene_code(),
    which loads the scene's overlay first;
  * the rest of a group is reachable only through its scenes' entry
    points; a section that two groups need is made resident.

Outputs: a linker script fragment (OVERLAY statement with one output
section per group, inserted before .ctors) and a C table mapping scene
structs to overlay ids (src/overlay.c reads it).
"""
import argparse
import os
import collections
import re
import subprocess
import sys

# object basename (without .c.obj) -> group; first match wins
GROUPS = [
    ("creator",  r"creator(_draw|_data)?$"),
    ("studio",   r"studio(_\w+)?$"),
    ("concert",  r"concert_\w+$"),
    ("dream",    r"dream(_gfx)?$"),
    ("exorcism", r"exorcism(_gfx|_scene)?$"),
    ("pet",      r"pet(_gfx|_scene)?$"),
    ("fishcook", r"fish_\w+$|cook_\w+$"),
    ("minigame", r"mg(_\w+)?$"),
    ("library",  r"lib_\w+$"),
    ("combat",   r"levels(_\w+)?$|arena(_modes)?$|combat(_\w+)?$|actor(_\w+)?$|weld_gfx$"),
    ("workshop", r"workshop$|weld_game$"),
    ("island",   r"fp_island(_gfx)?$"),
    ("freeplay", r"fp_(apartments|graveyard|newisland|news|options|resident|roster|saveload|shop|title|unlocks)$"),
    # the story's cutscenes with their Grumbleguts renderer are a group of
    # their own: story.c only pushes SCENE_CUTSCENE (a scene edge), so the
    # story group stays under the slot with them out
    ("cutscene", r"cutscene(_\w+)?$|story_feast$|grumbleguts_gfx$"),   # S2: cutscene_ch12
    # --- S2 (for H3, SPEC C4/C5): the story's minigames (SCENE_STORY_MG, its
    # S4 / S5 registry rows, SCENE_STORY_TIMING) are scene edges from the
    # story, so they are a group of their own and the story group stays in
    # its slot ---
    ("storymg",  r"story_mg(_\w+)?$|story_timing$"),
    # --- end S2 ---
    ("story",    r"story(_\w+)?$|overworld(_\w+)?$|credits$"),
    ("gallery",  r"ui_gallery$"),
    # --- H4 (for H3, SPEC C3/C6): the dream house; house_api.c stays resident ---
    ("home",     r"home(_\w+)?$|room_ref$|furn_data$"),
    # --- end H4 ---
]
OVL_KINDS = (".text", ".rodata")

# Callbacks that never escape their group: functions whose address is taken
# only to be called back SYNCHRONOUSLY by code of the same file, before that
# code returns (a draw function handed to a cache builder, a per-item hook
# of a loop), and never stored where resident code could call it later.
# Every entry is reviewed by hand; the planner still checks that each taker
# of the address is in the function's own group, and anything a resident
# taker reaches stays resident through the normal closure, so a wrong entry
# can only cost RAM, never put a live callback into an unloaded slot -
# except for the one thing the planner cannot see: a pointer passed on to
# resident code and stored. Do not list a function whose pointer goes to a
# ui_* / scene / save callback, a registry or any variable.
#   object base -> function names
LOCAL_CALLBACKS = {
    # grumbleguts_gfx.c: fig_fn goes to gg_build / gg_blit (the layer cache
    # renders it at once); hang_direct / hang_blit go to lantern_string,
    # which calls them per lantern. All three in the cutscene group.
    "grumbleguts_gfx": ("fig_fn", "hang_direct", "hang_blit"),
    # story_feast.c: lantern_fn goes to gg_cache_image (renders at once).
    "story_feast": ("lantern_fn",),
    # the Library wings: row goes to lib_list(), which draws the visible
    # rows with it and returns (lib_common.c).
    "lib_almanac": ("row",), "lib_bestiary": ("row",), "lib_gallery": ("row",),
    "lib_items": ("row",), "lib_jukebox": ("row",), "lib_trophies": ("row",),
    "lib_weapons": ("row",),
    # fp_roster.c: cmp_res is the comparator of fp_sort_u16() (a shell sort).
    "fp_roster": ("cmp_res",),
}


def run(cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def base(path):
    b = path.rsplit("/", 1)[-1]
    for ext in (".c.obj", ".s.obj", ".obj", ".o"):
        if b.endswith(ext):
            return b[: -len(ext)]
    return b


def group_of(path):
    # --- H4 (for H3, SPEC C3): the generated catalog joins the home group ---
    if "/core/gen/" in path and base(path) == "furn_data":
        return "home"
    # --- end H4 ---
    if "/core/src/" not in path:
        return None                     # platform, generated data, SDK
    b = base(path)
    for g, rx in GROUPS:
        if re.fullmatch(rx, b):
            return g
    return None


class Obj:
    def __init__(self, path, readelf):
        self.path = path
        self.secs = {}      # index -> (name, size, alloc)
        self.syms = []      # (name, ndx, bind, type, value)
        self.rels = collections.defaultdict(list)   # target sec name -> [(type, symidx)]
        out = run([readelf, "-S", "-s", "-r", "-W", path])
        mode = None
        cur = None
        for line in out.splitlines():
            if line.startswith("Section Headers:"):
                mode = "S"; continue
            if line.startswith("Symbol table"):
                mode = "s"; continue
            if line.startswith("Relocation section"):
                mode = "r"
                cur = re.search(r"'\.rel(\S*)'", line).group(1)
                continue
            if mode == "S":
                m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)\s+[0-9a-f]+\s+(\S*)", line)
                if m:
                    self.secs[int(m.group(1))] = (m.group(2), int(m.group(3), 16), "A" in m.group(4))
            elif mode == "s":
                m = re.match(r"\s*(\d+):\s+([0-9a-f]+)\s+\d+\s+(\w+)\s+(\w+)\s+\w+\s+(\w+)\s*(\S*)", line)
                if m:
                    ndx = int(m.group(5)) if m.group(5).isdigit() else m.group(5)
                    self.syms.append((m.group(6), ndx, m.group(4), m.group(3), int(m.group(2), 16)))
            elif mode == "r":
                m = re.match(r"([0-9a-f]+)\s+[0-9a-f]+\s+(R_MIPS_\w+)\s+[0-9a-f]+\s+(\S+)", line)
                if m:
                    self.rels[cur].append((m.group(2), m.group(3)))
        self.secname = {i: s[0] for i, s in self.secs.items()}
        # symbol name -> section name (local + global); section symbols too
        self.symsec = {}
        self.globals = {}
        self.weak = set()
        for name, ndx, bind, typ, val in self.syms:
            if not isinstance(ndx, int) or ndx == 0:
                continue
            sec = self.secname.get(ndx)
            if sec is None:
                continue
            if typ == "SECTION":
                self.symsec.setdefault(sec, sec)
            if name:
                self.symsec.setdefault(name, sec)
                if bind in ("GLOBAL", "WEAK"):
                    self.globals[name] = sec
                    if bind == "WEAK":
                        self.weak.add(name)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--readelf", required=True)
    ap.add_argument("--out-ld")
    ap.add_argument("--out-c")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("objs", nargs="+")
    a = ap.parse_args(argv[1:])

    objs = [Obj(p, a.readelf) for p in a.objs]
    # global symbol -> (obj index, section); strong wins over weak
    gdef = {}
    for oi, o in enumerate(objs):
        for name, sec in o.globals.items():
            if name not in gdef or (name not in o.weak and gdef[name][2]):
                gdef[name] = (oi, sec, name in o.weak)

    def node_size(n):
        o = objs[n[0]]
        for s in o.secs.values():
            if s[0] == n[1]:
                return s[1]
        return 0

    alloc = set()
    for oi, o in enumerate(objs):
        for name, size, al in o.secs.values():
            if al and size and not name.startswith((".pdr", ".MIPS", ".reginfo", ".gnu.attributes")):
                alloc.add((oi, name))

    # edges: node -> [(target node, reloc type)]
    edges = collections.defaultdict(list)
    for oi, o in enumerate(objs):
        for sec, rels in o.rels.items():
            src = (oi, sec)
            if src not in alloc:
                continue
            for typ, sym in rels:
                if sym in o.symsec and (sym not in o.globals or sym not in gdef or gdef[sym][0] == oi):
                    tgt = (oi, o.symsec[sym])
                elif sym in gdef:
                    tgt = (gdef[sym][0], gdef[sym][1])
                else:
                    continue            # SDK libraries (resident anyway)
                if tgt != src:
                    edges[src].append((tgt, typ))

    def grp(n):
        o = objs[n[0]]
        g = group_of(o.path)
        if g is None or not n[1].startswith(OVL_KINDS):
            return None
        return g

    # scene structs: sections named .rodata.SCENE_* (their function
    # pointers are the cut edges); they stay resident themselves
    scene_secs = {n for n in alloc if re.fullmatch(r"\.rodata\.SCENE_\w+", n[1])}

    def is_code(n):
        return n[1].startswith(".text")

    # escaping addresses: a code section takes the address of a function
    # (HI16/LO16, not a jal) -> that function may be called by anyone later
    escaped = set()
    takers = collections.defaultdict(set)
    for src, lst in edges.items():
        if not is_code(src):
            continue
        for tgt, typ in lst:
            if typ in ("R_MIPS_HI16", "R_MIPS_LO16", "R_MIPS_32") and is_code(tgt):
                escaped.add(tgt)
                takers[tgt].add(src)
    # LOCAL_CALLBACKS (above): not escaped when every taker is in their group
    for f in sorted(escaped):
        o = objs[f[0]]
        names = LOCAL_CALLBACKS.get(base(o.path), ())
        if not names or f[1] not in tuple(".text." + n for n in names):
            continue
        g = group_of(o.path)
        if g and all(group_of(objs[t[0]].path) == g and t[1].startswith(OVL_KINDS) for t in takers[f]):
            escaped.discard(f)
        else:
            print("ovl_plan: %s:%s is taken outside its group; it stays resident" % (base(o.path), f[1]))


    def closure_all(start):
        seen = set(start)
        todo = list(start)
        while todo:
            n = todo.pop()
            for t, _ in edges.get(n, ()):
                if t not in seen:
                    seen.add(t)
                    todo.append(t)
        return seen

    def closure(start, follow_scene_ptrs):
        # A scene struct's code pointers are not followed (scene.c loads the
        # scene's group before calling them); its data (the name string,
        # read by scene_by_name() at any time) is.
        seen = set(start)
        todo = list(start)
        while todo:
            n = todo.pop()
            cut = n in scene_secs and not follow_scene_ptrs
            for t, _ in edges.get(n, ()):
                if cut and is_code(t):
                    continue
                if t not in seen:
                    seen.add(t)
                    todo.append(t)
        return seen

    # what the final link keeps (--gc-sections): everything reachable from
    # main() and the constructor tables
    entry = {(gdef["main"][0], gdef["main"][1])}
    entry |= {n for n in alloc if n[1].startswith((".ctors", ".dtors"))}
    live = closure_all(entry)
    roots = {n for n in live if grp(n) is None} | (set() if os.environ.get("OVL_NOESC") else (escaped & live))
    resident = closure(roots, False)
    # A scene's entry points run with its group loaded; everything they
    # reach that is not resident must be of that same group. Whatever two
    # groups (or a resident scene) reach becomes resident, and so does all
    # it reaches; repeat until stable.
    scene_live = sorted(n for n in scene_secs if n in live)
    while True:
        owner = {}
        clash = set()
        for s in scene_live:
            g = grp(s)
            for t, _ in edges.get(s, ()):
                if t in resident or not is_code(t):
                    continue
                if g is None:
                    clash.add(t)
                    continue
                for r in closure({t}, False):
                    if r in resident:
                        continue
                    if grp(r) != g or owner.get(r, g) != g:
                        clash.add(r)
                    else:
                        owner[r] = g
        if not clash:
            break
        resident |= closure(clash, False)

    why = os.environ.get("OVL_WHY_OBJ")
    if why:
        # reverse edges; BFS from roots recording parents
        par = {}
        todo = [r for r in roots]
        for r in roots:
            par[r] = None
        while todo:
            n = todo.pop(0)
            if n in scene_secs:
                continue
            for t, _ in edges.get(n, ()):
                if t not in par:
                    par[t] = n
                    todo.append(t)
        shown = collections.Counter()
        for n in sorted(resident, key=lambda x: -node_size(x)):
            if base(objs[n[0]].path) != why or n not in live:
                continue
            chain = []
            m = n
            while m is not None and len(chain) < 8:
                chain.append("%s:%s%s" % (base(objs[m[0]].path), m[1], "*" if m in escaped else ""))
                m = par.get(m)
            print("why %6d %s" % (node_size(n), " <- ".join(chain)))
    groups = collections.defaultdict(list)
    for r, g in owner.items():
        groups[g].append(r)
    for g in groups:
        groups[g] = sorted(n for n in groups[g] if n in live)

    names = sorted(groups)
    gid = {g: i + 1 for i, g in enumerate(names)}
    in_ovl = {}
    for g in names:
        for n in groups[g]:
            in_ovl[n] = g
    grp_objs = [oi for oi, o in enumerate(objs) if group_of(o.path)]

    def pat(oi):
        # the object as the link command names it: a unique path suffix
        p = objs[oi].path
        k = p.find("/core/")
        if k < 0:
            k = p.rfind("/")
        return "*" + p[k:]

    def secs_of(oi, kinds, want_ovl):
        out = []
        for name, size, al in objs[oi].secs.values():
            if not al or not name.startswith(kinds):
                continue
            if ((oi, name) in in_ovl) == want_ovl:
                out.append(name)
        return sorted(set(out))

    if a.out_ld:
        L = []
        L.append("/* generated by ps1/tools/ovl_plan.py - do not edit. The PSn00bSDK")
        L.append(" * exe.ld with the scene overlays (docs/PS1.md section 3). */")
        L.append("ENTRY(_start)")
        L.append("MEMORY {")
        L.append("\tAPP_RAM (rwx) : ORIGIN = 0x80010000, LENGTH = 0x7f0000")
        L.append("}")
        L.append("SECTIONS {")
        excl = " ".join(pat(oi) for oi in grp_objs)
        for out, kinds, extra in ((".text", (".text",), "\t\t*(.plt .MIPS.stubs)"),
                                  (".rodata", (".rodata",), None)):
            L.append("\t%s : {" % out)
            if out == ".text":
                L.append("\t\t__text_start = .;")
            L.append("\t\tEXCLUDE_FILE(%s) *(%s %s.* .gnu.linkonce.%s.*)" % (
                excl, out, out, "t" if out == ".text" else "r"))
            for oi in grp_objs:
                ss = secs_of(oi, kinds, False)
                if ss:
                    L.append("\t\t%s(%s)" % (pat(oi), " ".join(ss)))
            if extra:
                L.append(extra)
            L.append("\t} > APP_RAM")
        L.append("""\t.ctors : ALIGN(16) {
\t\t__CTOR_LIST__ = .;
\t\tLONG(((__CTOR_END__ - __CTOR_LIST__) / 4) - 2)
\t\tKEEP(*(SORT(.ctors.*)))
\t\tKEEP(*(.ctors))
\t\tLONG(0)
\t\t__CTOR_END__ = .;
\t} > APP_RAM
\t.dtors : ALIGN(16) {
\t\t__DTOR_LIST__ = .;
\t\tLONG(((__DTOR_END__ - __DTOR_LIST__) / 4) - 2)
\t\tKEEP(*(SORT(.dtors.*)))
\t\tKEEP(*(.dtors))
\t\tLONG(0)
\t\t__DTOR_END__ = .;
\t} > APP_RAM
\t.data : {
\t\t*(.data .data.* .gnu.linkonce.d.*)
\t} > APP_RAM
\tHIDDEN(_gp = ALIGN(16) + 0x7ff0);
\t.sdata : {
\t\t*(.sdata .sdata.* .gnu.linkonce.s.*)
\t} > APP_RAM
\t. = ALIGN((. != 0) ? 4 : 1);
\t__bss_start = .;
\t.sbss (NOLOAD) : {
\t\t*(.sbss .sbss.* .gnu.linkonce.sb.*)
\t\t*(.scommon)
\t} > APP_RAM
\t.bss (NOLOAD) : {
\t\t*(.bss .bss.* .gnu.linkonce.b.*)
\t\t*(COMMON)
\t} > APP_RAM
\t. = ALIGN(16);
\t__bss_end = .;
\t/* the overlay slot: every group is linked here; src/overlay.c reads
\t * the one a scene needs from OVL.BIN on the disc (load addresses are
\t * outside RAM: elf2exe.py leaves them out of the EXE) */
\t__ovl_slot = .;
\tOVERLAY __ovl_slot : NOCROSSREFS AT (0x10000000) {""")
        for g in names:
            L.append("\t\t.ovl_%02d {\t/* %s */" % (gid[g], g))
            for oi in sorted({n[0] for n in groups[g]}):
                ss = secs_of(oi, (".text", ".rodata"), True)
                if ss:
                    L.append("\t\t\t%s(%s)" % (pat(oi), " ".join(ss)))
            L.append("\t\t\t. = ALIGN(4);")
            L.append("\t\t}")
        L.append("""\t} > APP_RAM
\t/* whole sectors: the CD reads 2048-byte units into the slot */
\t. = __ovl_slot + ((. - __ovl_slot + 2047) & ~2047);
\t__ovl_slot_end = .;
\t_end = .;
\t.dummy (NOLOAD) : {
\t\tKEEP(*(.dummy))
\t} > APP_RAM
\t/DISCARD/ : {
\t\t*(.note.* .gnu_debuglink .gnu.lto_*)
\t\t*(.MIPS.abiflags)
\t}
}""")
        with open(a.out_ld, "w") as f:
            f.write("\n".join(L) + "\n")

    if a.out_c:
        rows = []
        for sct in scene_live:
            g = grp(sct)
            sg = 0
            for t, _ in edges.get(sct, ()):
                if t in in_ovl:
                    if sg and gid[in_ovl[t]] != sg:
                        sys.exit("ovl_plan: scene %s spans two overlays" % sct[1])
                    sg = gid[in_ovl[t]]
            if sg:
                rows.append((sct[1][len(".rodata."):], sg))
        C = ["/* generated by ps1/tools/ovl_plan.py - do not edit */",
             '#include "doomo/scene.h"', ""]
        for sym, _ in rows:
            C.append("extern const DoomoScene %s;" % sym)
        C.append("")
        C.append("const DoomoScene *const ps1_ovl_scene[] = {")
        for sym, _ in rows:
            C.append("    &%s," % sym)
        C.append("    0\n};")
        C.append("const unsigned char ps1_ovl_scene_grp[] = {")
        C.append("    " + ", ".join(str(g) for _, g in rows) + (", 0" if rows else "0"))
        C.append("};")
        C.append("const unsigned ps1_ovl_ngroups = %d;" % len(names))
        C.append("const char *const ps1_ovl_name[] = {")
        C.append('    "-", ' + ", ".join('"%s"' % g for g in names))
        C.append("};")
        with open(a.out_c, "w") as f:
            f.write("\n".join(C) + "\n")

    if a.report:
        rs = sum(node_size(n) for n in resident if n in live and n[1].startswith(OVL_KINDS))
        print("ovl_plan: resident text+rodata %d KB" % (rs // 1024))
        tot = 0
        for g in sorted(groups):
            sz = sum(node_size(n) for n in groups[g])
            tot += sz
            print("ovl_plan:   %-10s %4d KB  %d sections" % (g, sz // 1024, len(groups[g])))
        print("ovl_plan: overlays total %d KB" % (tot // 1024))
        if os.environ.get("OVL_WHY"):
            c = collections.Counter()
            for n in resident:
                if n in live and grp(n) is not None:
                    c[base(objs[n[0]].path)] += node_size(n)
            for k, v in c.most_common(40):
                print("ovl_plan:   resident from group objs: %6d %s" % (v, k))
            c = collections.Counter()
            for n in resident:
                if n in live and n[1].startswith(OVL_KINDS) and grp(n) is None:
                    c[base(objs[n[0]].path)] += node_size(n)
            for k, v in c.most_common(60):
                print("ovl_plan:   resident core: %6d %s" % (v, k))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
