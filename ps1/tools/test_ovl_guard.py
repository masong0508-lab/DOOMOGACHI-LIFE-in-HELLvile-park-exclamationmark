#!/usr/bin/env python3
"""test_ovl_guard.py - unit tests of ovl_plan.py's platform groups and guarded
calls (owner: ps1; HOUSE CHARACTERS FLAT, docs/PS1.md "No 3D models in houses").

    python3 ps1/tools/test_ovl_guard.py

A made-up game is compiled with the MIPS compiler and planned: the resident
view3d.c calls model.c's model_player_draw behind ovl_code_in(), a combat
scene draws the view. The model must leave resident RAM for the combat
group, and the plan must fail when anything else (house code, say) would
call it. Stdlib only; skipped without a MIPS compiler.
"""
import contextlib
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ovl_plan         # noqa: E402

CROSS = next((p for p in ("mipsel-linux-gnu-", "mipsel-none-elf-") if shutil.which(p + "gcc")), None)
CFLAGS = ["-O1", "-G8", "-march=r3000", "-mabi=32", "-msoft-float", "-mno-abicalls", "-fno-pic",
          "-ffreestanding", "-fno-builtin", "-nostdlib", "-ffunction-sections", "-fdata-sections"]

SCENE_H = """
typedef struct DoomoScene { const char *name; void (*enter)(int); void (*leave)(void);
                            void (*update)(void); void (*draw)(void); unsigned char overlay; } DoomoScene;
void plat_view3d(int third_person);
int ovl_code_in(int code);
int model_player_draw(int x);
int prim_box(int x);
extern int sink;
"""
GAME = {
    "src/main.c": """#include "scene.h"
extern const DoomoScene *const doomo_all_scenes[];
int sink;
int main(void)
{
    int i;
    for (i = 0; doomo_all_scenes[i]; i++) doomo_all_scenes[i]->draw();
    return sink;
}
""",
    "src/overlay.c": """#include "scene.h"
extern const unsigned char ps1_ovl_code_grp[];
int ovl_code_in(int code) { return !ps1_ovl_code_grp[code]; }
""",
    # resident: the 3D view calls the model behind the guard; it also lends its tables (BORROW)
    "src/view3d.c": """#include "scene.h"
unsigned char s_vis[64], s_vlist[64];
void plat_view3d(int third_person)
{
    if (third_person && ovl_code_in(0) && model_player_draw(third_person)) return;
    sink++;
}
""",
    "src/model.c": """#include "scene.h"
static int walk_phase;
static __attribute__((noinline)) int limb(int x) { return prim_box(x) + walk_phase++; }
int model_player_draw(int x) { return limb(x) + limb(x + 1); }
""",
    "src/prims.c": """#include "scene.h"
int prim_box(int x) { sink += x; return 1; }
""",
    # the other lenders ovl_plan.py BORROW names
    "src/world.c": "unsigned char g_world_wtop[64], g_world_corner[64];\n",
    "src/memcard.c": "unsigned char s_blk[64];\n",
    "core/src/combat_bot.c": "unsigned char first_dir[64], queue[64], target_bits[64];\n",
    "core/src/scene.c": """#include "scene.h"
extern const DoomoScene SCENE_COMBAT, SCENE_PET;
const DoomoScene *const doomo_all_scenes[] = { &SCENE_COMBAT, &SCENE_PET, 0 };
""",
    "core/src/combat.c": """#include "scene.h"
static void combat_draw(void) { plat_view3d(1); }
const DoomoScene SCENE_COMBAT = { "COMBAT", 0, 0, 0, combat_draw, 0 };
""",
    "core/src/pet.c": """#include "scene.h"
static void pet_draw(void) { sink += 2; }
const DoomoScene SCENE_PET = { "PET", 0, 0, 0, pet_draw, 0 };
""",
}


def plan(files, guarded=None):
    """compile the made-up game and run the planner: (rc, report, ld script, C table)"""
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "scene.h"), "w") as f:
            f.write(SCENE_H)
        objs = []
        for rel, text in sorted(files.items()):
            src = os.path.join(d, rel)
            os.makedirs(os.path.dirname(src), exist_ok=True)
            with open(src, "w") as f:
                f.write(text)
            obj = src + ".obj"
            subprocess.run([CROSS + "gcc"] + CFLAGS + ["-I", d, "-c", src, "-o", obj], check=True)
            objs.append(obj)
        ld, c = os.path.join(d, "ovl.ld"), os.path.join(d, "ovl_table.c")
        out = io.StringIO()
        old = ovl_plan.GUARDED
        if guarded is not None:
            ovl_plan.GUARDED = guarded
        try:
            with contextlib.redirect_stdout(out):
                rc = ovl_plan.main(["ovl_plan.py", "--readelf", CROSS + "readelf", "--out-ld", ld,
                                    "--out-c", c, "--report"] + objs)
        finally:
            ovl_plan.GUARDED = old
        ldt = ct = ""                     # a failed plan writes neither
        if os.path.exists(ld):
            with open(ld) as f:
                ldt = f.read()
        if os.path.exists(c):
            with open(c) as f:
                ct = f.read()
        return rc, out.getvalue(), ldt, ct


def group_sections(ld, group):
    """the input sections the linker script puts into `group`'s overlay"""
    m = re.search(r"\.ovl_\d+ (?:__ovl_borrow : AT \([^\n]*\) )?\{\t/\* %s \*/\n(.*?)\n\t*\}" % re.escape(group), ld, re.S)
    return m.group(1) if m else ""


def code_grp(c):
    m = re.search(r"ps1_ovl_code_grp\[\] = \{ ([^}]*) \}", c)
    return [int(x) for x in m.group(1).split(",")]


def group_id(c, group):
    names = re.search(r"ps1_ovl_name\[\] = \{\n\s*(.*)\n\};", c).group(1)
    return [n.strip().strip('"') for n in names.split(",")].index(group)


@unittest.skipUnless(CROSS, "no MIPS compiler")
class Guarded(unittest.TestCase):
    def test_the_model_joins_the_combat_group(self):
        rc, rep, ld, c = plan(GAME)
        self.assertEqual(rc, 0, rep)
        combat = group_sections(ld, "combat")
        # the guarded function and what only it reaches (its static helper) are in the group
        self.assertIn(".text.model_player_draw", combat)
        self.assertIn(".text.limb", combat)
        # the table the run-time guard reads names the combat group
        self.assertEqual(code_grp(c), [group_id(c, "combat")])
        self.assertIn("guarded call view3d -> model:model_player_draw: in combat", rep)
        # prims.c is no PLAT_GROUPS file: resident, and so is the caller
        self.assertNotIn("prims.c.obj", combat)
        self.assertNotIn("view3d.c.obj", combat)
        # the resident .text names model.c's sections one by one and lists none of the moved ones
        res = ld[ld.index("\t.text : {"):ld.index("\t.ctors")]
        self.assertIn("EXCLUDE_FILE(", res)
        self.assertNotIn(".text.model_player_draw", res)

    def test_without_the_guard_entry_the_model_stays_resident(self):
        # the same game, the call not listed: resident code reaches the model, so it must stay
        rc, rep, ld, c = plan(GAME, guarded=())
        self.assertEqual(rc, 0, rep)
        self.assertNotIn("model.c.obj", group_sections(ld, "combat"))
        self.assertEqual(code_grp(c), [0])              # an empty table is still valid C

    def test_a_second_resident_caller_fails_the_plan(self):
        files = dict(GAME)
        files["src/etest.c"] = '#include "scene.h"\nvoid etest_tick(void) { sink += model_player_draw(3); }\n'
        files["src/main.c"] = GAME["src/main.c"].replace("return sink;", "etest_tick(); return sink;").replace(
            "int sink;", "int sink;\nvoid etest_tick(void);")
        rc, rep, ld, c = plan(files)
        self.assertEqual(rc, 1, rep)
        self.assertIn("ERROR: model:model_player_draw must be in the combat group", rep)
        self.assertIn("called by etest:.text.etest_tick", rep)

    def test_an_escaping_address_fails_the_plan(self):
        files = dict(GAME)
        files["src/view3d.c"] = GAME["src/view3d.c"] + "int (*model_hook)(int);\nvoid hook(void) { model_hook = model_player_draw; }\n"
        files["src/main.c"] = GAME["src/main.c"].replace("return sink;", "hook(); return sink;").replace(
            "int sink;", "int sink;\nvoid hook(void);")
        rc, rep, ld, c = plan(files)
        self.assertEqual(rc, 1, rep)
        self.assertIn("ERROR: model:model_player_draw must be in the combat group", rep)
        self.assertIn("its address is taken", rep)

    def test_house_code_calling_the_model_fails_the_plan(self):
        # the owner's rule (no 3D character models inside houses): the home group cannot draw the chibi
        files = dict(GAME)
        files["core/src/home.c"] = """#include "scene.h"
static void home_draw(void) { sink += model_player_draw(2); }
const DoomoScene SCENE_HOME = { "HOME", 0, 0, 0, home_draw, 0 };
"""
        files["core/src/scene.c"] = GAME["core/src/scene.c"].replace("SCENE_PET;", "SCENE_PET, SCENE_HOME;").replace(
            "&SCENE_PET, 0", "&SCENE_PET, &SCENE_HOME, 0")
        rc, rep, ld, c = plan(files)
        self.assertEqual(rc, 1, rep)
        self.assertIn("ERROR: model:model_player_draw must be in the combat group", rep)
        self.assertIn("called by home:.text.home_draw", rep)
        # the same house without the call plans fine, the model in the combat group
        files["core/src/home.c"] = files["core/src/home.c"].replace("model_player_draw(2)", "2")
        rc, rep, ld, c = plan(files)
        self.assertEqual(rc, 0, rep)
        self.assertIn(".text.model_player_draw", group_sections(ld, "combat"))
        self.assertNotIn("model.c.obj", group_sections(ld, "home"))

    def test_the_real_lists_name_the_model(self):
        # the game's own entry: view3d.c (resident) -> model.c in the combat group; ps1.h OVL_CODE_MODEL is 0
        self.assertEqual(ovl_plan.PLAT_GROUPS.get("model"), "combat")
        self.assertEqual(ovl_plan.GUARDED[0], ("view3d", "model", "model_player_draw"))
        with open(os.path.join(HERE, "..", "src", "ps1.h")) as f:
            self.assertRegex(f.read(), r"enum \{ OVL_CODE_MODEL \};")


if __name__ == "__main__":
    unittest.main()
