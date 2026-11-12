#!/usr/bin/env python3
"""test_tools.py - unit tests of the PS1 build tools (owner: ps1).

    python3 ps1/tools/test_tools.py            (CI: the PS1 job runs it)

elf2exe.py (the stack reserve and the plan gate's WARNING) and
ram_ledger.py (the linker map reader) are tested on files made here. Stdlib
only; the tests that need the MIPS compiler are skipped without one.
"""
import contextlib
import io
import os
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import elf2exe          # noqa: E402
import ram_ledger       # noqa: E402


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


if __name__ == "__main__":
    import warnings
    warnings.simplefilter("ignore", ResourceWarning)    # the tools read files in one expression
    unittest.main(warnings="ignore")
