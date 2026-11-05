/* mslot.c - SCENE_MSLOT: a reserved mature scene slot (E2; pack.h, docs/STORY_ENGINE.md).
 * arg = the slot, 0..MSLOT_MAX - 1 (core/data/story/mslots.txt names them). A built-in script
 * reaches it with `push mslot N` (PEND_MSLOT); the result in var 0 is mslot_result().
 *
 *   - OPT_MATURE is "On + pack", a valid pack is open and it has a script for the slot: the scene
 *     runs the pack's script on the story VM. The built-in script that pushed us is waiting in
 *     the VM, so its state is saved first (vm_snapshot) and put back after (vm_restore).
 *   - anything else: the safe fade-to-black, then back (result 0).
 * Strong definition of the weak placeholder in weak_scenes.c. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/pack.h"
#include "doomo/story_vm.h"
#include "doomo/ending.h"
#include "doomo/camfx.h"
#include "doomo/palette.h"
#include "doomo/ui.h"
#include "doomo/txt.h"
#include "doomo/platform.h"
#include "doomo/plat_ext.h"

enum { MS_FALLBACK, MS_PLAY, MS_DONE };
#define FADE_FRAMES 12
#define HOLD_FRAMES 18

static struct {
    uint8_t  snap[VM_SNAP_MAX] __attribute__((aligned(8)));
    DoomoImage img;
    uint16_t t;
    uint8_t  mode, art;
} M;
static int g_result;

int mslot_result(void) { return g_result; }

static void ms_show(const StoryOp *o)
{
    uint32_t size = 0, cap = 0;
    const uint8_t *p;
    uint8_t *buf;
    int idx;
    M.art = 0;
    if (o->a == SHOW_OFF || o->a != PK_KIND_CG) return;      /* only stills are drawn here */
    idx = o->b;
    p = pack_art(PK_KIND_CG, PACK_UID(PK_KIND_CG, idx), &size);
    if (!p || size < (uint32_t)PACK_CG_W_THIS * PACK_CG_H_THIS) {
        buf = plat_mslot_buf(&cap);
        if (!buf || cap < (uint32_t)PACK_CG_W_THIS * PACK_CG_H_THIS) return;
        if (pack_art_read(PK_KIND_CG, PACK_UID(PK_KIND_CG, idx), buf, cap) != PACK_CG_W_THIS * PACK_CG_H_THIS) return;
        p = buf;
    }
    M.img.w = PACK_CG_W_THIS;
    M.img.h = PACK_CG_H_THIS;
    M.img.pix = p;
    M.img.uid = doomo_image_new_uid();
    M.art = 1;
}

static void ms_finish(void)
{
    vm_stop();
    vm_restore(M.snap);
    camfx_fade(0);                      /* a pack script may leave the screen faded */
    g_result = 1;
    M.mode = MS_DONE;
    scene_pop();
}

static void ms_enter(int arg)
{
    int pack = 0;
    memset(&M, 0, sizeof M);
    g_result = 0;
    if (opt_mature() == MATURE_PACK && pack_open() && arg >= 0 && arg < MSLOT_MAX)
{
        if (pack_streamed()) {                           /* E9: a disc pack: read the script into the port's buffer */
            uint32_t cap = 0;
            uint8_t *sb = plat_mslot_script_buf(&cap);
            pack = sb ? pack_script_bind(arg, sb, cap) : 0;
        } else {
            pack = pack_script(arg);
        }
    }
    M.mode = MS_FALLBACK;
    if (pack) {
        vm_snapshot(M.snap);
        vm_start(TXT_ID(pack, 1));
        if (vm_running()) M.mode = MS_PLAY;
        else vm_restore(M.snap);
    }
    if (M.mode == MS_FALLBACK) camfx_fade_to(16, FADE_FRAMES);
}

static void ms_leave(void)
{
    if (M.mode == MS_PLAY) { vm_stop(); vm_restore(M.snap); camfx_fade(0); }
    pack_script_release();                               /* E9: the streamed script buffer is only ours while we run */
    M.mode = MS_DONE;
}

static void ms_update(void)
{
    int guard = 32;
    M.t++;
    camfx_tick();                                        /* the fade clock is this scene's (camfx.h) */
    if (M.mode == MS_FALLBACK) {
        if (M.t == FADE_FRAMES + HOLD_FRAMES) camfx_fade_to(0, FADE_FRAMES);
        if (M.t >= 2 * FADE_FRAMES + HOLD_FRAMES + 1) { M.mode = MS_DONE; scene_pop(); }
        return;
    }
    if (M.mode != MS_PLAY) return;
    while (guard-- > 0) {
        int r = vm_tick();
        if (r == VM_EXT) { const StoryOp *o = vm_ext_op(); if (o && o->op == OP_SHOW) ms_show(o); continue; }
        if (r == VM_PUSHED) { vm_resume(0); continue; }   /* a pack never pushes (PACK_OPS); be safe */
        if (r == VM_IDLE) { if (!ui_dialog_active()) ms_finish(); return; }
        return;                                           /* VM_RUN: waiting */
    }
}

static void ms_draw(void)
{
    if (M.mode == MS_PLAY) {                  /* the fallback leaves the scene below showing, fading */
        plat_clear(PAL_BLACK);
        if (M.art) plat_image(0, 0, &M.img, 1, 0, 0);
    }
    camfx_apply();
}

const DoomoScene SCENE_MSLOT = { "MSLOT", ms_enter, ms_leave, ms_update, ms_draw, 1 };
