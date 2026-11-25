#!/usr/bin/env python3
"""test_tools.py - unit tests of the PS1 build tools (owner: ps1).

    python3 ps1/tools/test_tools.py            (CI: the PS1 job runs it)

elf2exe.py (the stack reserve and the plan gate's WARNING) and
ram_ledger.py (the linker map reader) are tested on files made here;
strsec.py (the assembly rewrite that gives every table string a section
of its own) on assembly text and on real compiles; ovl_plan.py on a
made-up game of four scenes compiled with the MIPS compiler (which
callbacks, strings and parts of shared files may leave resident RAM).
Stdlib only; the tests that need the MIPS compiler are skipped without
one.
"""
import contextlib
import io
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import elf2exe          # noqa: E402
import ovl_plan         # noqa: E402
import partsbin         # noqa: E402
import ram_ledger       # noqa: E402
import strsec           # noqa: E402
import verify_game_disc # noqa: E402
from pathlib import Path

CROSS = next((p for p in ("mipsel-linux-gnu-", "mipsel-none-elf-") if shutil.which(p + "gcc")), None)
CFLAGS = ["-O1", "-G8", "-march=r3000", "-mabi=32", "-msoft-float", "-mno-abicalls", "-fno-pic",
          "-ffreestanding", "-fno-builtin", "-nostdlib", "-ffunction-sections", "-fdata-sections"]


def tiny_elf(end):
    """a 32-bit little-endian ELF with one loadable segment at 0x80010000
    whose image + BSS end at `end` (an address in RAM)"""
    ph = struct.pack("<IIIIIIII", 1, 0x1000, 0x80010000, 0x80010000, 16, end - 0x80010000, 7, 0x1000)
    eh = b"\x7fELF\x01\x01\x01" + bytes(9) + struct.pack(
        "<HHIIIIIHHHHHH", 2, 8, 1, 0x80010000, 52, 0, 0, 52, 32, 1, 40, 0, 0)
    blob = eh + ph
    return blob + bytes(0x1000 - len(blob)) + bytes(16)


def run_elf2exe(end, *extra, env=None):
    with tempfile.TemporaryDirectory() as d:
        src, dst = os.path.join(d, "a.elf"), os.path.join(d, "a.exe")
        with open(src, "wb") as f:
            f.write(tiny_elf(end))
        out = io.StringIO()
        old = dict(os.environ)
        os.environ.pop("GITHUB_ACTIONS", None)
        os.environ.update(env or {})
        try:
            with contextlib.redirect_stdout(out):
                rc = elf2exe.main(["elf2exe.py", src, dst] + list(extra))
        finally:
            os.environ.clear()
            os.environ.update(old)
        return rc, out.getvalue(), os.path.exists(dst)


class Elf2Exe(unittest.TestCase):
    TOP = 0x801FFFF0

    def test_room_above_the_gate_is_quiet(self):
        rc, out, made = run_elf2exe(self.TOP - 73728)
        self.assertEqual(rc, 0)
        self.assertTrue(made)
        self.assertNotIn("WARNING", out)
        self.assertIn("73728 B left below the stack top", out)
        # the line the show engine's ram_report.py reads keeps its shape
        self.assertRegex(out, r"size \d+ KB \(\+bss \d+ KB, ends 0x[0-9a-f]+, 72 KB left")

    def test_under_the_plan_gate_warns_but_passes(self):
        rc, out, made = run_elf2exe(self.TOP - 73724)
        self.assertEqual(rc, 0)
        self.assertTrue(made)
        self.assertIn("elf2exe: WARNING: only 73724 B left below the stack top: 4 B under the 72 KB plan gate", out)
        self.assertNotIn("::warning", out)

    def test_one_byte_under_the_plan_gate_warns(self):
        rc, out, made = run_elf2exe(self.TOP - 73727)
        self.assertEqual(rc, 0)
        self.assertTrue(made)
        self.assertIn("elf2exe: WARNING: only 73727 B left below the stack top: 1 B under the 72 KB plan gate", out)

    def test_the_warning_is_an_annotation_on_github(self):
        rc, out, _ = run_elf2exe(self.TOP - 70000, env={"GITHUB_ACTIONS": "true"})
        self.assertEqual(rc, 0)
        self.assertIn("::warning title=PS1 RAM under the plan gate::only 70000 B left", out)

    def test_the_gate_is_an_option(self):
        rc, out, _ = run_elf2exe(self.TOP - 70000, "--gate", "69000")
        self.assertEqual(rc, 0)
        self.assertNotIn("WARNING", out)

    def test_into_the_stack_reserve_fails(self):
        rc, out, made = run_elf2exe(0x801F0000 + 16)
        self.assertEqual(rc, 1)
        self.assertFalse(made)
        self.assertIn("16 B over the limit 0x801f0000", out)
        rc, _, made = run_elf2exe(0x801F0000)
        self.assertEqual(rc, 0)             # exactly at the limit still links
        self.assertTrue(made)


MAP = """\
Discarded input sections

 .text.dead     0x00000000       0x40 CMakeFiles/doomo_objs.dir/src/gfx.c.obj

Linker script and memory map

.text           0x80010000      0x300
                0x80010000                        __text_start = .
 .text.a        0x80010000      0x100 CMakeFiles/doomo_objs.dir/src/gfx.c.obj
                0x80010000                a
 .text.a_very_long_function_name_here
                0x80010100      0x200 CMakeFiles/doomo_objs.dir/x/core/src/ui.c.obj

.rodata         0x80010300      0x130
 .rodata.a.str1.4
                0x80010300       0x30 CMakeFiles/doomo_objs.dir/src/gfx.c.obj
                                 0x14 (size before relaxing)
 .rodata.str1.4.LC0
                0x80010330       0x20 CMakeFiles/doomo_objs.dir/x/core/src/ui.c.obj
 .rodata.tab    0x80010330      0x100 CMakeFiles/doomo_objs.dir/x/core/src/ui.c.obj
 *fill*         0x80010430        0x0

.data           0x80010430        0x8
 .data.d        0x80010430        0x8 /opt/psn00bsdk/lib/libpsn00b/release/libpsxgpu_exe_gprel.a(image.c.obj)

.bss            0x80010438     0x1000
 .bss.big       0x80010438     0x1000 CMakeFiles/doomo_objs.dir/src/gfx.c.obj

.borrow         0x80011438      0x800
                0x80011438                        __ovl_borrow = .
 .bss.lent      0x80011438      0x800 CMakeFiles/doomo_objs.dir/src/world.c.obj
                0x80011c38                        __ovl_slot = .

.ovl_01         0x80011c38     0x2000 load address 0x10000000
 .text.s        0x80011c38     0x2000 CMakeFiles/doomo_objs.dir/x/core/src/combat.c.obj

.ovl_02         0x80011438     0x2400 load address 0x10002000
 .text.h        0x80011438     0x2400 CMakeFiles/doomo_objs.dir/x/core/src/home.c.obj
                0x80014000                        __ovl_slot_end = .
                0x80014000                        _end = .
"""


class Ledger(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.map = os.path.join(self.dir.name, "x.ldmap")
        with open(self.map, "w") as f:
            f.write(MAP)
        self.ld = os.path.join(self.dir.name, "ovl.ld")
        with open(self.ld, "w") as f:
            f.write("\t\t.ovl_01 {\t/* combat */\n\t.ovl_02 __ovl_borrow : AT (0) {\t/* home */\n")

    def tearDown(self):
        self.dir.cleanup()

    def run_ledger(self, *extra):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = ram_ledger.main(["ram_ledger.py", self.map, "--ld", self.ld] + list(extra))
        return rc, out.getvalue()

    def test_map_reader(self):
        outs, syms = ram_ledger.parse_map(self.map)
        self.assertEqual(outs[".text"][:2], [0x80010000, 0x300])
        self.assertEqual([s[0] for s in outs[".text"][2]], [".text.a", ".text.a_very_long_function_name_here"])
        self.assertEqual(outs[".text"][2][1][2], 0x200)
        self.assertEqual(syms["_end"], 0x80014000)
        self.assertNotIn(".text.dead", [s[0] for o in outs.values() for s in o[2]])   # discarded: not in RAM

    def test_layout_and_objects(self):
        rc, out = self.run_ledger()
        self.assertEqual(rc, 0)
        self.assertIn("resident .text (code)  768", out)
        self.assertIn("48 B of it the merged string literals)  304", out)
        self.assertIn("overlay slot (after the .bss; the biggest group sets it)  9160", out)
        self.assertIn("left below the stack top (0x801FFFF0)  %d" % (0x801FFFF0 - 0x80014000), out)
        # gfx: 256 text + 4096 bss; the merged block's size is not its strings
        self.assertIn("  ps1/src/gfx  4352  256  0  0  0  4096  0", out)
        self.assertIn("  core/src/ui  800  512  256  32  0  0  0", out)
        self.assertIn("  ps1/src/world  2048  0  0  0  0  0  2048", out)
        self.assertIn("  sdk/libpsxgpu(image.c)  8  0  0  0  8  0  0", out)
        self.assertIn("  home  9216  0x80011438 (over .borrow)", out)
        self.assertIn("  combat  8192  0x80011c38", out)

    def test_gate_line_and_markdown(self):
        left = 0x801FFFF0 - 0x80014000
        rc, out = self.run_ledger("--gate", str(left + 100), "--md")
        self.assertEqual(rc, 0)
        self.assertIn("ram_ledger: WARNING: %d B left below the stack top, 100 B under" % left, out)
        self.assertIn("| ps1/src/gfx | 4352 | 256 | 0 | 0 | 0 | 4096 | 0 |", out)
        rc, out = self.run_ledger("--gate", str(left))
        self.assertNotIn("WARNING", out)


# ---- strsec.py -----------------------------------------------------------------
ASM = """\t.section .mdebug.abi32
\t.previous
\t.text
f:
\tjr\t$31
\t.section\t.rodata.f.str1.4,"aMS",@progbits,1
\t.align\t2
$LC0:
\t.ascii\t"in a function\\000"
\t.section\t.rodata.str1.4,"aMS",@progbits,1
\t.align\t2
$LC1:
\t.ascii\t"Plan\\000"
\t.align\t2
$LC2:
\t.ascii\t"a long one, "
\t.ascii\t"in two lines\\000"
\t.section\t.sdata.subs,"aw"
\t.align\t2
subs:
\t.word\t$LC1
\t.word\t$LC2
\t.section\t.rodata.str1.4
\t.align\t2
$LC3:
\t.ascii\t"Open\\000"
\t.text
"""


class StrSec(unittest.TestCase):
    def test_every_table_string_gets_its_own_section(self):
        new, moved = strsec.split_strings(ASM)
        self.assertEqual(moved, 3)
        lines = new.split("\n")
        for lab in ("LC1", "LC2", "LC3"):
            i = lines.index("$%s:" % lab)
            self.assertEqual(lines[i - 2], '\t.section\t.rodata.str1.4.%s,"aMS",@progbits,1' % lab)
            self.assertEqual(lines[i - 1], "\t.align\t2")
        # the generic section itself is gone, everything else is untouched and in order
        self.assertFalse([l for l in lines if re.match(r"\s*\.section\s+\.rodata\.str1\.4\s*(,|$)", l)])
        rest = [l for l in lines if ".rodata.str1.4." not in l]
        orig = [l for l in ASM.split("\n") if not re.match(r"\s*\.section\s+\.rodata\.str1\.4\s*(,|$)", l)]
        self.assertEqual(rest, orig)
        self.assertIn('\t.section\t.rodata.f.str1.4,"aMS",@progbits,1', lines)   # a function's strings stay
        self.assertEqual(lines[lines.index("$LC2:") + 2], '\t.ascii\t"in two lines\\000"')

    def test_a_file_without_table_strings_is_unchanged(self):
        text = '\t.text\nf:\n\tjr\t$31\n\t.section\t.rodata.f.str1.4,"aMS",@progbits,1\n$LC0:\n\t.ascii\t"x\\000"\n'
        self.assertEqual(strsec.split_strings(text), (text, 0))

    def test_layouts_it_does_not_know_are_left_alone(self):
        late = ASM + "\t.previous\n"
        self.assertEqual(strsec.split_strings(late), (late, 0))             # the section stack after strings
        odd = '\t.section\t.rodata.str1.4,"aMS",@progbits,1\n\t.ascii\t"no label\\000"\n$LC0:\n\t.ascii\t"x\\000"\n'
        self.assertEqual(strsec.split_strings(odd), (odd, 0))               # data before a label
        bare = '\t.section\t.rodata.str1.4\n$LC0:\n\t.ascii\t"x\\000"\n'
        self.assertEqual(strsec.split_strings(bare), (bare, 0))             # never declared with flags

    def test_an_empty_visit_is_kept(self):
        text = '\t.section\t.rodata.str1.4,"aMS",@progbits,1\n\t.align\t2\n\t.text\nf:\n'
        self.assertEqual(strsec.split_strings(text), (text, 0))

    def test_command_line(self):
        cmd = ["gcc", "-DX=1", "-I", "inc.c", "-O2", "-g", "-G8", "-march=r3000", "-mno-abicalls", "-fno-pic",
               "-Wa,--strip-local-absolute", "-MD", "-MT", "o/x.c.obj", "-MF", "o/x.c.obj.d", "-o", "o/x.c.obj",
               "-c", "src/x.c"]
        cc, args, ci, si, oi = strsec.parse(cmd)
        self.assertEqual((cc, args[ci], args[si], args[oi]), ("gcc", "-c", "src/x.c", "o/x.c.obj"))
        self.assertEqual(strsec.assembler_args(args),
                         ["-G8", "-march=r3000", "-mno-abicalls", "-fno-pic", "-Wa,--strip-local-absolute"])
        self.assertIsNone(strsec.parse(["gcc", "-o", "x.elf", "a.obj", "b.obj"]))        # a link
        self.assertIsNone(strsec.parse(["gcc", "-S", "-o", "x.s", "-c", "x.c"]))
        self.assertIsNone(strsec.parse(["gcc", "-o", "x.obj", "-c", "x.s"]))             # not C

    @unittest.skipUnless(CROSS, "no MIPS compiler")
    def test_real_compile(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "t.c")
            with open(src, "w") as f:
                f.write('static const char *const tabs[2] = { "Plan", "In paw" };\n'
                        'const char *const rows[3] = { "alpha", "beta", "gamma" };\n'
                        'const char *pick(int i) { return i > 5 ? "in the function" : tabs[i & 1]; }\n')
            cmd = [CROSS + "gcc"] + CFLAGS + ["-g", "-o", os.path.join(d, "t.obj"), "-c", src]
            old = os.environ.get("STRSEC_VERIFY")
            os.environ["STRSEC_VERIFY"] = "1"      # the two steps without the rewrite = the one-step object
            try:
                self.assertEqual(strsec.main(["strsec.py"] + cmd), 0)
            finally:
                os.environ.pop("STRSEC_VERIFY")
                if old is not None:
                    os.environ["STRSEC_VERIFY"] = old
            self.assertEqual(sorted(os.listdir(d)), ["t.c", "t.obj"])       # nothing left behind
            secs = subprocess.run([CROSS + "readelf", "-S", "-W", os.path.join(d, "t.obj")], check=True,
                                  capture_output=True, text=True).stdout
            own = re.findall(r"\.rodata\.str1\.4\.LC\d+\s+PROGBITS\s+\S+\s+\S+\s+\S+\s+01\s+AMS", secs)
            self.assertEqual(len(own), 5)                                   # 2 + 3 table strings
            self.assertIn(".rodata.pick.str1.4", secs)                      # the function's own stay together
            self.assertNotRegex(secs, r"\.rodata\.str1\.4\s")
            # a file without table strings: the object is the one-step compile's, byte for byte
            with open(src, "w") as f:
                f.write('const char *pick(int i) { return i ? "yes" : "no"; }\n')
            direct = os.path.join(d, "direct.obj")
            subprocess.run([CROSS + "gcc"] + CFLAGS + ["-g", "-o", direct, "-c", src], check=True)
            self.assertEqual(strsec.main(["strsec.py"] + cmd), 0)
            with open(direct, "rb") as a, open(os.path.join(d, "t.obj"), "rb") as b:
                self.assertEqual(a.read(), b.read())


# ---- ovl_plan.py on a made-up game -------------------------------------------
# Four scenes: FAMILY and the citizen PICKER (free play), ISLAND (island), all
# listed by the resident scene.c. main.c stands for the platform and the
# resident modals (ui_choice keeps the callback and calls it later).
SCENE_H = """
typedef struct DoomoScene { const char *name; void (*enter)(int); void (*leave)(void);
                            void (*update)(void); void (*draw)(void); unsigned char overlay; } DoomoScene;
void fp_pick(void (*done)(int));
void ui_choice(void (*done)(int));
extern int sink;
"""
FAKE = {
    "src/main.c": """#include "scene.h"
extern const DoomoScene *const doomo_all_scenes[];
int sink;
static void (*modal_done)(int);
void ui_choice(void (*done)(int)) { modal_done = done; }
int main(void)
{
    int i;
    for (i = 0; doomo_all_scenes[i]; i++) { doomo_all_scenes[i]->update(); if (modal_done) modal_done(i); }
    return sink;
}
""",
    # the lenders ovl_plan.py BORROW names
    "src/world.c": "unsigned char g_world_wtop[64], g_world_corner[64];\n",
    "src/view3d.c": "unsigned char s_vis[64], s_vlist[64];\n",
    "src/memcard.c": "unsigned char s_blk[64];\n",
    "core/src/combat_bot.c": "unsigned char first_dir[64], queue[64], target_bits[64];\n",
    "core/src/scene.c": """#include "scene.h"
extern const DoomoScene SCENE_FAMILY, SCENE_PICKER, SCENE_ISLAND;
const DoomoScene *const doomo_all_scenes[] = { &SCENE_FAMILY, &SCENE_PICKER, &SCENE_ISLAND, 0 };
""",
    "core/src/fp_roster.c": """#include "scene.h"
static void (*pick_done)(int);
void fp_pick(void (*done)(int)) { pick_done = done; }
static void picker_update(void) { if (pick_done) pick_done(7); }
const DoomoScene SCENE_PICKER = { "PICKER", 0, 0, picker_update, 0, 0 };
""",
    "core/src/fp_family.c": """#include "scene.h"
static void wz_neighbour(int id) { sink += id * 3; }    /* to fp_pick: PICK_CALLBACKS */
static void mem_flowers(int c) { sink += c * 5; }       /* to ui_choice: MODAL_CALLBACKS */
static void not_listed(int c) { sink += c * 7; }        /* to ui_choice, in no table */
static void fam_update(void) { fp_pick(wz_neighbour); ui_choice(mem_flowers); ui_choice(not_listed); }
const DoomoScene SCENE_FAMILY = { "FAMILY", 0, 0, fam_update, 0, 0 };
""",
    "core/src/fp_island.c": """#include "scene.h"
static void cb_treat(int id) { sink += id * 11; }       /* to fp_pick from ANOTHER group */
static void isl_update(void) { fp_pick(cb_treat); }
const DoomoScene SCENE_ISLAND = { "ISLAND", 0, 0, isl_update, 0, 0 };
""",
}


def compile_fake(root, sources, compiler=None):
    """the made-up game's objects, named as the PS1 build names them"""
    with open(os.path.join(root, "scene.h"), "w") as f:
        f.write(SCENE_H)
    objs = []
    for rel, text in sorted(sources.items()):
        src = os.path.join(root, rel)
        os.makedirs(os.path.dirname(src), exist_ok=True)
        with open(src, "w") as f:
            f.write(text)
        obj = src + ".obj"
        cmd = [CROSS + "gcc"] + CFLAGS + ["-I", root, "-o", obj, "-c", src]
        rc = compiler(cmd) if compiler else subprocess.run(cmd).returncode
        if rc:
            raise RuntimeError("cannot compile " + rel)
        objs.append(obj)
    return objs


def plan(root, objs):
    """run the planner; ({group or 'resident': {(object base, section)}}, its report)"""
    ld = os.path.join(root, "ovl.ld")
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = ovl_plan.main(["ovl_plan.py", "--readelf", CROSS + "readelf", "--out-ld", ld,
                            "--out-c", os.path.join(root, "ovl_table.c"), "--report"] + objs)
    if rc:
        raise RuntimeError("ovl_plan failed:\n" + out.getvalue())
    where = {}
    cur = None
    with open(ld) as f:
        for line in f:
            m = re.match(r"\s*\.ovl_\d+\s.*/\*\s*(\S+)\s*\*/", line)
            if m:
                cur = m.group(1)
            elif re.match(r"\s*\.(text|rodata) : \{", line):
                cur = "resident"
            elif re.match(r"\s*\}", line):
                cur = None
            m = re.match(r"\s*\*(\S+)\(([^)]*)\)\s*$", line)
            if m and cur and not m.group(1).startswith("("):
                for sec in m.group(2).split():
                    where.setdefault(cur, set()).add((ovl_plan.base(m.group(1)), sec))
    return where, out.getvalue()


@unittest.skipUnless(CROSS, "no MIPS compiler (mipsel-linux-gnu-gcc / mipsel-none-elf-gcc)")
class Planner(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.saved = (dict(ovl_plan.PICK_CALLBACKS), dict(ovl_plan.MODAL_CALLBACKS), ovl_plan.HOLD)
        ovl_plan.HOLD = ()          # the tables themselves; HOLD has its own tests (PlannerHold)

    def tearDown(self):
        ovl_plan.PICK_CALLBACKS.clear()
        ovl_plan.PICK_CALLBACKS.update(self.saved[0])
        ovl_plan.MODAL_CALLBACKS.clear()
        ovl_plan.MODAL_CALLBACKS.update(self.saved[1])
        ovl_plan.HOLD = self.saved[2]
        self.dir.cleanup()

    def test_callbacks_that_may_leave_resident_ram(self):
        where, _ = plan(self.dir.name, compile_fake(self.dir.name, FAKE))
        fp = where["freeplay"]
        # the scenes' own code is in their groups
        self.assertIn(("fp_family", ".text.fam_update"), fp)
        self.assertIn(("fp_roster", ".text.picker_update"), fp)
        self.assertIn(("fp_island", ".text.isl_update"), where["island"])
        # a modal's callback and a picker callback of the picker's own group go with the group
        self.assertIn(("fp_family", ".text.mem_flowers"), fp)
        self.assertIn(("fp_family", ".text.wz_neighbour"), fp)
        # a callback in no table is resident, and so is the island's picker callback
        self.assertIn(("fp_family", ".text.not_listed"), where["resident"])
        self.assertIn(("fp_island", ".text.cb_treat"), where["resident"])

    def test_a_picker_callback_of_another_group_stays_resident_even_if_listed(self):
        ovl_plan.PICK_CALLBACKS["fp_island"] = ("cb_treat",)
        where, report = plan(self.dir.name, compile_fake(self.dir.name, FAKE))
        self.assertIn("fp_island:.text.cb_treat is not in the picker's group; it stays resident", report)
        self.assertIn(("fp_island", ".text.cb_treat"), where["resident"])
        self.assertNotIn(("fp_island", ".text.cb_treat"), where["island"])

    def test_without_its_table_entry_a_picker_callback_is_resident(self):
        del ovl_plan.PICK_CALLBACKS["fp_family"]
        where, _ = plan(self.dir.name, compile_fake(self.dir.name, FAKE))
        self.assertIn(("fp_family", ".text.wz_neighbour"), where["resident"])
        self.assertNotIn(("fp_family", ".text.wz_neighbour"), where["freeplay"])

    def test_a_listed_callback_taken_by_another_group_stays_resident(self):
        fake = dict(FAKE)
        fake["core/src/fp_island.c"] = fake["core/src/fp_island.c"].replace(
            "static void isl_update(void) { fp_pick(cb_treat); }",
            "void wz_neighbour(int);\nstatic void isl_update(void) { fp_pick(cb_treat); fp_pick(wz_neighbour); }")
        fake["core/src/fp_family.c"] = fake["core/src/fp_family.c"].replace("static void wz_neighbour", "void wz_neighbour")
        where, report = plan(self.dir.name, compile_fake(self.dir.name, fake))
        self.assertIn("fp_family:.text.wz_neighbour is taken outside its group; it stays resident", report)
        self.assertIn(("fp_family", ".text.wz_neighbour"), where["resident"])


# the made-up FAMILY screen with two tables: `tabs` is 8 bytes, so -G8 puts it into .sdata, which is
# always resident (its two strings must stay); `rows` is plain const data of the group
FAMILY_TABLES = FAKE["core/src/fp_family.c"].replace(
    "static void fam_update(void) {",
    'static const char *const tabs[2] = { "Tab one", "Tab two" };\n'
    'static const char *const rows[4] = { "row alpha", "row beta", "row gamma", "row delta" };\n'
    "static void fam_update(void) { sink += tabs[sink & 1][0] + rows[sink & 3][0];")


def strsec_compiler(cmd):
    return strsec.main(["strsec.py"] + cmd)


@unittest.skipUnless(CROSS, "no MIPS compiler (mipsel-linux-gnu-gcc / mipsel-none-elf-gcc)")
class PlannerStrings(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.whole = (ovl_plan.STR_WHOLE, ovl_plan.HOLD)
        ovl_plan.HOLD = ()
        self.fake = dict(FAKE)
        self.fake["core/src/fp_family.c"] = FAMILY_TABLES

    def tearDown(self):
        ovl_plan.STR_WHOLE, ovl_plan.HOLD = self.whole
        self.dir.cleanup()

    def strings(self, where, key):
        return sorted(sec for obj, sec in where.get(key, ()) if obj == "fp_family" and ".str1." in sec)

    def test_one_string_section_per_file_is_resident_for_one_pointer(self):
        # what the plain compile gives: the scene's name and the .sdata table pin every table string
        where, _ = plan(self.dir.name, compile_fake(self.dir.name, self.fake))
        self.assertEqual(self.strings(where, "resident"), [".rodata.str1.4"])
        self.assertEqual(self.strings(where, "freeplay"), [])

    def test_split_strings_follow_their_readers(self):
        where, _ = plan(self.dir.name, compile_fake(self.dir.name, self.fake, strsec_compiler))
        self.assertEqual(len(self.strings(where, "freeplay")), 4)       # rows: only the group reads them
        self.assertEqual(len(self.strings(where, "resident")), 3)       # the name + the two .sdata labels
        self.assertTrue(all(ovl_plan.RX_STR.search(s) for s in self.strings(where, "freeplay")))

    def test_a_group_that_sets_the_slot_keeps_its_strings_together(self):
        ovl_plan.STR_WHOLE = ("freeplay",)
        where, _ = plan(self.dir.name, compile_fake(self.dir.name, self.fake, strsec_compiler))
        self.assertEqual(self.strings(where, "freeplay"), [])
        self.assertEqual(len(self.strings(where, "resident")), 7)


# a shared file (ovl_plan.py SHARED): the UI kit of the made-up game, used by both screen families,
# by the platform, and with a scene of its own
SHARED_FAKE = dict(FAKE)
SHARED_FAKE["core/src/ui.c"] = """#include "scene.h"
static const short island_tab[8] = { 3, 1, 4, 1, 5, 9, 2, 6 };
void ui_family_only(void) { sink += 2; }
void ui_island_only(void) { sink += island_tab[sink & 7]; }
void ui_both(void) { sink += 5; }
void ui_resident(void) { sink += 7; }
static void uix_update(void) { sink += 9; }
const DoomoScene SCENE_UIX = { "UIX", 0, 0, uix_update, 0, 0 };
"""
SHARED_FAKE["src/main.c"] = FAKE["src/main.c"].replace(
    "int main(void)\n{", "void ui_resident(void);\nint main(void)\n{\n    ui_resident();")
SHARED_FAKE["core/src/scene.c"] = FAKE["core/src/scene.c"].replace(
    "SCENE_ISLAND;", "SCENE_ISLAND, SCENE_UIX;").replace("&SCENE_ISLAND, 0", "&SCENE_ISLAND, &SCENE_UIX, 0")
SHARED_FAKE["core/src/fp_family.c"] = FAKE["core/src/fp_family.c"].replace(
    "static void fam_update(void) {",
    "void ui_family_only(void); void ui_both(void);\nstatic void fam_update(void) { ui_family_only(); ui_both();")
SHARED_FAKE["core/src/fp_island.c"] = FAKE["core/src/fp_island.c"].replace(
    "static void isl_update(void) {",
    "void ui_island_only(void); void ui_both(void);\nstatic void isl_update(void) { ui_island_only(); ui_both();")


@unittest.skipUnless(CROSS, "no MIPS compiler (mipsel-linux-gnu-gcc / mipsel-none-elf-gcc)")
class PlannerShared(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.saved = (ovl_plan.SHARED, ovl_plan.SHARED_NOT, ovl_plan.HOLD)
        ovl_plan.SHARED, ovl_plan.SHARED_NOT, ovl_plan.HOLD = r"ui$", (), ()

    def tearDown(self):
        ovl_plan.SHARED, ovl_plan.SHARED_NOT, ovl_plan.HOLD = self.saved
        self.dir.cleanup()

    def plan(self):
        return plan(self.dir.name, compile_fake(self.dir.name, SHARED_FAKE))

    def test_a_shared_files_sections_join_the_one_group_that_reaches_them(self):
        where, report = self.plan()
        self.assertIn(("ui", ".text.ui_family_only"), where["freeplay"])
        self.assertIn(("ui", ".text.ui_island_only"), where["island"])
        self.assertIn(("ui", ".rodata.island_tab"), where["island"])        # its table goes with it
        self.assertNotIn(("ui", ".text.ui_island_only"), where["freeplay"])  # one file, two groups
        self.assertNotIn(("ui", ".text.ui_family_only"), where["island"])
        # two groups, the platform, or a scene of the shared file itself: resident
        for sec in (".text.ui_both", ".text.ui_resident", ".text.uix_update"):
            self.assertIn(("ui", sec), where["resident"])
            self.assertNotIn(("ui", sec), where["freeplay"] | where["island"])
        self.assertIn("ovl_plan: shared code and data in island:", report)

    def test_a_file_that_is_not_listed_stays_resident(self):
        ovl_plan.SHARED = r"nothing$"
        where, report = self.plan()
        self.assertFalse([x for g in ("freeplay", "island") for x in where[g] if x[0] == "ui"])
        self.assertNotIn("shared code and data", report)

    def test_a_group_that_takes_none(self):
        ovl_plan.SHARED_NOT = ("freeplay",)
        where, _ = self.plan()
        self.assertIn(("ui", ".text.ui_family_only"), where["resident"])
        self.assertNotIn(("ui", ".text.ui_family_only"), where["freeplay"])
        self.assertIn(("ui", ".text.ui_island_only"), where["island"])


# HOLD: a group whose loads must not get longer takes nothing new. The made-up FAMILY screen with
# its tables (strings), its callbacks and the shared UI kit; the island next to it is not held.
HOLD_FAKE = dict(SHARED_FAKE)
HOLD_FAKE["core/src/fp_family.c"] = SHARED_FAKE["core/src/fp_family.c"].replace(
    "static void fam_update(void) {",
    'static const char *const tabs[2] = { "Tab one", "Tab two" };\n'
    'static const char *const rows[4] = { "row alpha", "row beta", "row gamma", "row delta" };\n'
    "static void fam_update(void) { sink += tabs[sink & 1][0] + rows[sink & 3][0];")


@unittest.skipUnless(CROSS, "no MIPS compiler (mipsel-linux-gnu-gcc / mipsel-none-elf-gcc)")
class PlannerHold(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.saved = (ovl_plan.SHARED, ovl_plan.SHARED_NOT, ovl_plan.STR_WHOLE, ovl_plan.HOLD,
                      dict(ovl_plan.HELD_CALLBACKS))
        ovl_plan.SHARED, ovl_plan.SHARED_NOT, ovl_plan.STR_WHOLE = r"ui$", (), ()
        ovl_plan.HELD_CALLBACKS.clear()
        ovl_plan.HELD_CALLBACKS.update({"fp_family": ("mem_flowers", "wz_neighbour")})

    def tearDown(self):
        ovl_plan.SHARED, ovl_plan.SHARED_NOT, ovl_plan.STR_WHOLE, ovl_plan.HOLD = self.saved[:4]
        ovl_plan.HELD_CALLBACKS.clear()
        ovl_plan.HELD_CALLBACKS.update(self.saved[4])
        self.dir.cleanup()

    def plan(self):
        return plan(self.dir.name, compile_fake(self.dir.name, HOLD_FAKE, strsec_compiler))[0]

    def strings(self, where, key):
        return [sec for obj, sec in where.get(key, ()) if obj == "fp_family" and ".str1." in sec]

    def test_a_held_group_takes_nothing_new(self):
        ovl_plan.HOLD = ("freeplay",)
        where = self.plan()
        fp = where["freeplay"]
        # its own scene code is in the group as ever
        self.assertIn(("fp_family", ".text.fam_update"), fp)
        # its reviewed callbacks, its table strings and the shared code only it calls stay resident
        for sec in (".text.mem_flowers", ".text.wz_neighbour"):
            self.assertIn(("fp_family", sec), where["resident"])
            self.assertNotIn(("fp_family", sec), fp)
        self.assertEqual(self.strings(where, "freeplay"), [])
        self.assertEqual(len(self.strings(where, "resident")), 7)
        self.assertIn(("ui", ".text.ui_family_only"), where["resident"])
        self.assertFalse([x for x in fp if x[0] == "ui"])
        # the group next to it is not held and still takes its part of the shared file
        self.assertIn(("ui", ".text.ui_island_only"), where["island"])

    def test_released_it_takes_all_of_it(self):
        ovl_plan.HOLD = ()
        where = self.plan()
        fp = where["freeplay"]
        for sec in (".text.mem_flowers", ".text.wz_neighbour"):
            self.assertIn(("fp_family", sec), fp)
        self.assertEqual(len(self.strings(where, "freeplay")), 4)
        self.assertIn(("ui", ".text.ui_family_only"), fp)

    def test_a_hold_on_another_group_changes_nothing_here(self):
        ovl_plan.HOLD = ("island",)
        where = self.plan()
        self.assertIn(("fp_family", ".text.mem_flowers"), where["freeplay"])
        self.assertIn(("ui", ".text.ui_family_only"), where["freeplay"])
        self.assertIn(("ui", ".text.ui_island_only"), where["resident"])

    def test_every_held_callback_is_in_a_reviewed_table(self):
        # HELD_CALLBACKS only holds back entries of the three callback tables
        held = self.saved[4]
        for obj, names in held.items():
            listed = (ovl_plan.LOCAL_CALLBACKS.get(obj, ()) + ovl_plan.MODAL_CALLBACKS.get(obj, ())
                      + ovl_plan.PICK_CALLBACKS.get(obj, ()))
            for n in names:
                self.assertIn(n, listed, "%s:%s is held but in no callback table" % (obj, n))



class PartsBin(unittest.TestCase):
    """partsbin.py (CR10): PARTS.BIN, the part scripts and packed icons the PS1 uploads to SPU RAM"""

    def make(self, sets, icons):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        index = "const PartSet *const doomo_part_scripts[%d] = {\n%s};\n" % (
            len(sets), "".join("    &doomo_parts_%s,\n" % n for n, _ in sets))
        open(os.path.join(d, "parts_index.c"), "w").write(index)
        for n, raw in sets:
            body = ",\n".join("0x%02x" % b for b in raw)
            open(os.path.join(d, "parts_%s.c" % n), "w").write(
                "static const uint8_t ops[] = {\n%s,\n};\n" % body)
        ofs = [0]
        for i in icons:
            ofs.append(ofs[-1] + len(i))
        flat = b"".join(icons)
        open(os.path.join(d, "part_icons_rle.c"), "w").write(
            "const uint32_t doomo_part_icon_rle_ofs[%d] = {%s};\nconst uint8_t doomo_part_icon_rle[%d] = {%s};\n"
            % (len(ofs), ",".join(map(str, ofs)), len(flat), ",".join(str(b) for b in flat)))
        out = os.path.join(d, "PARTS.BIN")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(partsbin.main(["partsbin", d, out]), 0)
        return open(out, "rb").read()

    def test_layout_sets_then_icons_each_on_64_bytes_in_whole_sectors(self):
        a, b = bytes(range(1, 71)), bytes([0xFF, 0, 0, 0])
        icons = [bytes([3, 1, 2, 3]) + bytes(40), bytes([2, 9, 8]) + bytes(77)]
        data = self.make([("hat", a), ("hair", b)], icons)
        self.assertEqual(data[:4], b"PRTS")
        n, _, total, _ = struct.unpack("<HHII", data[4:16])
        self.assertEqual(n, 2)
        self.assertEqual(len(data), 2048 + total)
        self.assertEqual(total % 2048, 0)
        recs = [struct.unpack("<II", data[16 + 8 * i:24 + 8 * i]) for i in range(3)]
        for ofs, _ in recs:
            self.assertEqual(ofs % 64, 0)
        body = data[2048:]
        self.assertEqual(body[recs[0][0]:recs[0][0] + recs[0][1]], a)
        self.assertEqual(body[recs[1][0]:recs[1][0] + recs[1][1]], b)
        self.assertEqual(body[recs[2][0]:recs[2][0] + recs[2][1]], b"".join(icons))
        self.assertTrue(recs[0][0] < recs[1][0] < recs[2][0])
        self.assertEqual(recs[0][0], 0)

    def test_a_set_order_follows_the_registry_not_the_file_names(self):
        data = self.make([("zeta", b"\x01\xFF\x00\x00"), ("alpha", b"\x02\xFF\x00\x00")], [bytes([1, 5, 0])])
        recs = [struct.unpack("<II", data[16 + 8 * i:24 + 8 * i]) for i in range(3)]
        self.assertEqual(data[2048 + recs[0][0]], 1)             # the registry's first set is zeta
        self.assertEqual(data[2048 + recs[1][0]], 2)

    def test_icon_offsets_that_disagree_with_the_bytes_are_refused(self):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        open(os.path.join(d, "parts_index.c"), "w").write(
            "const PartSet *const doomo_part_scripts[1] = {\n    &doomo_parts_a,\n};\n")
        open(os.path.join(d, "parts_a.c"), "w").write("static const uint8_t ops[] = {\n0xff,0,0,0,\n};\n")
        open(os.path.join(d, "part_icons_rle.c"), "w").write(
            "const uint32_t doomo_part_icon_rle_ofs[2] = {0,9};\nconst uint8_t doomo_part_icon_rle[3] = {1,2,3};\n")
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertNotEqual(partsbin.main(["partsbin", d, os.path.join(d, "P.BIN")]), 0)
        self.assertIn("offsets", err.getvalue())

    def test_the_real_generated_files_pack(self):
        gen = os.path.join(HERE, "..", "..", "core", "gen")
        if not os.path.exists(os.path.join(gen, "parts_index.c")):
            self.skipTest("no generated files")
        out = os.path.join(tempfile.mkdtemp(), "PARTS.BIN")
        self.addCleanup(shutil.rmtree, os.path.dirname(out))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(partsbin.main(["partsbin", gen, out]), 0)
        data = open(out, "rb").read()
        n = struct.unpack("<H", data[4:6])[0]
        idx = open(os.path.join(gen, "parts_index.c")).read()
        self.assertEqual(n, int(re.search(r"doomo_part_script_count = (\d+)", idx).group(1)))
        self.assertLessEqual(2048 + struct.unpack("<I", data[8:12])[0], 0x2E000)   # fits SPU RAM above the effects

class IntegratedDisc(unittest.TestCase):
    """Exercise the actual packaging gate over a small raw Mode 2 image."""
    @staticmethod
    def make_disc(files):
        def record(name, lba, length, directory=False):
            name = name.encode("ascii")
            row = bytearray(33 + len(name) + (len(name) % 2 == 0))
            row[0] = len(row)
            struct.pack_into("<I", row, 2, lba)
            struct.pack_into(">I", row, 6, lba)
            struct.pack_into("<I", row, 10, length)
            struct.pack_into(">I", row, 14, length)
            row[25] = 2 if directory else 0
            row[32] = len(name)
            row[33:33 + len(name)] = name
            return row

        def raw(data):
            sector = bytearray(2352)
            sector[:12] = b"\x00" + b"\xff" * 10 + b"\x00"
            sector[15] = 2
            sector[24:24 + len(data)] = data
            return bytes(sector)

        pvd = bytearray(2048)
        pvd[:7] = b"\x01CD001\x01"
        root = record("\x00", 17, 2048, True)
        pvd[156:156 + len(root)] = root
        entries = bytearray(root)
        entries += record("\x01", 17, 2048, True)
        payload = []
        lba = 18
        for name, body in files.items():
            entries += record(name + ";1", lba, len(body))
            for i in range(0, len(body), 2048):
                payload.append(raw(body[i:i + 2048]))
                lba += 1
        return bytes(16 * 2352) + raw(pvd) + raw(entries) + b"".join(payload)

    def test_full_disc_and_missing_or_corrupt_talk(self):
        files = {name: b"resource" for name in
                 ("SYSTEM.CNF", "OVL.BIN", "SFX.BIN", "SPRITES.BIN",
                  "PARTS.BIN", "TEXT.BIN", "README.TXT", "INTRO.STR")}
        files["DOOMO.EXE"] = b"PS-X EXE" + bytes(2050)
        files["TALK_EN.BIN"] = b"bank" * 1025
        with tempfile.TemporaryDirectory() as d:
            image, talk, exe = [Path(d) / name for name in ("game.bin", "talk.bin", "game.exe")]
            talk.write_bytes(files["TALK_EN.BIN"])
            exe.write_bytes(files["DOOMO.EXE"])
            image.write_bytes(self.make_disc(files))
            with contextlib.redirect_stdout(io.StringIO()):
                verify_game_disc.verify_game_disc(image, talk, exe, True)
                corrupt = dict(files)
                corrupt["TALK_EN.BIN"] = b"x" + files["TALK_EN.BIN"][1:]
                image.write_bytes(self.make_disc(corrupt))
                with self.assertRaisesRegex(ValueError, "disc payload mismatch"):
                    verify_game_disc.verify_game_disc(image, talk, exe, True)
                for missing in ("TALK_EN.BIN", "OVL.BIN", "TEXT.BIN"):
                    partial = {name: body for name, body in files.items() if name != missing}
                    image.write_bytes(self.make_disc(partial))
                    with self.assertRaisesRegex(ValueError, "full PS1 game disc missing"):
                        verify_game_disc.verify_game_disc(image, talk, exe, True)


if __name__ == "__main__":
    import warnings
    warnings.simplefilter("ignore", ResourceWarning)    # the tools read files in one expression
    unittest.main(warnings="ignore")
