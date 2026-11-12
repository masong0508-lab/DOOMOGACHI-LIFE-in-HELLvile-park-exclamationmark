#!/usr/bin/env python3
"""test_tools.py - unit tests of the PS1 build tools (owner: ps1).

    python3 ps1/tools/test_tools.py            (CI: the PS1 job runs it)

elf2exe.py (the stack reserve and the plan gate's WARNING) and
ram_ledger.py (the linker map reader) are tested on files made here;
ovl_plan.py on a made-up game of four scenes compiled with the MIPS
compiler (which callbacks may leave resident RAM). Stdlib only; the
tests that need the MIPS compiler are skipped without one.
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
import ram_ledger       # noqa: E402

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
        self.saved = (dict(ovl_plan.PICK_CALLBACKS), dict(ovl_plan.MODAL_CALLBACKS))

    def tearDown(self):
        ovl_plan.PICK_CALLBACKS.clear()
        ovl_plan.PICK_CALLBACKS.update(self.saved[0])
        ovl_plan.MODAL_CALLBACKS.clear()
        ovl_plan.MODAL_CALLBACKS.update(self.saved[1])
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


if __name__ == "__main__":
    import warnings
    warnings.simplefilter("ignore", ResourceWarning)    # the tools read files in one expression
    unittest.main(warnings="ignore")
