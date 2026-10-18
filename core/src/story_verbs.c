/* story_verbs.c - the side-mission verbs and the story minigame shell
 * (owner: S2; docs/design/story/SPEC.md sec.7 S2, C12, C14; story_final.md
 * sec.14 verbs; the language is in docs/STORY_ENGINE.md).
 *
 * VERBS (`verb KIND [WHO] [cell=x,y] [fragile] then=SCRIPT` in a script):
 * one runs at a time on the overworld while the VM stays free, so the plot
 * can still talk; when it ends, SCRIPT runs with v1 = the result.
 *
 *   escort   WHO follows you to `cell`; they wait when you get ahead, and
 *            are lost if you leave them far behind for too long (1 / 0)
 *   find     walk onto the hidden `cell`; the HUD says warmer / colder
 *            as you get closer (1)
 *   photo    press A with WHO in front of you, 2-6 cells away: 2 framed
 *            (centred on your line of sight), 1 in the frame, 0 nothing
 *            (try again)
 *   deliver  carry something to WHO: you walk slower; `fragile` also
 *            spills when you run (2 perfect, 1 delivered with spills)
 *
 * The minigame shell SCENE_STORY_MG is story_mg.c.
 *
 * Story group code on the PS1 (the name matches the story regex): every
 * entry point is called from SCENE_STORY.
 */
#include "story_int.h"
#include "overworld_int.h"
#include "doomo/camfx.h"

static int iabs(int v) { return v < 0 ? -v : v; }

/* ---- the rules (pure; tests) ---------------------------------------------------------- */
/* frames and distances live here, in one table (SPEC risk 6: no literal
 * timing in story code outside tables) */
static const struct {
    uint8_t escort_wait, escort_far;      /* cells: the follower waits / is falling behind */
    uint8_t escort_lost_ticks;            /* ticks far behind before they are lost      */
    uint8_t photo_near, photo_far;        /* cells                                       */
    uint8_t heat_hot, heat_warm, heat_cool;   /* cells (max of |dx|, |dy|)               */
    uint8_t walk_q8, fragile_q8;          /* walking speed while delivering              */
} VT = { 3, 9, 150, 2, 6, 2, 5, 9, 200, 160 };

int story_escort_judge(int dist_cells, int far_ticks)
{
    if (far_ticks >= VT.escort_lost_ticks) return ESC_LOST;
    if (dist_cells > VT.escort_far) return ESC_FAR;
    if (dist_cells > VT.escort_wait) return ESC_WAIT;
    return ESC_FOLLOW;
}

/* facing 0 down 1 left 2 right 3 up (the overworld's) */
int story_photo_judge(int px, int py, int facing, int tx, int ty)
{
    static const int8_t fx[4] = { 0, -1, 1, 0 }, fy[4] = { 1, 0, 0, -1 };
    int f = facing & 3, along = (tx - px) * fx[f] + (ty - py) * fy[f];
    int side = fx[f] ? ty - py : tx - px;
    if (side < 0) side = -side;
    if (along < VT.photo_near || along > VT.photo_far) return 0;
    if (side * 2 > along) return 0;                 /* outside the ~53 deg frame */
    return side == 0 ? 2 : 1;
}

int story_find_heat(int dx, int dy)
{
    int d;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    d = dx > dy ? dx : dy;
    if (d == 0) return 4;                            /* found                     */
    if (d <= VT.heat_hot) return 3;
    if (d <= VT.heat_warm) return 2;
    if (d <= VT.heat_cool) return 1;
    return 0;
}

/* ---- the running verb ----------------------------------------------------------------- */
static struct {
    uint8_t  kind, live, heat, spills, ran;
    int16_t  who;
    uint8_t  cx, cy, fragile;
    uint16_t then, far_t, t;
} V;

static int follower(void) { return V.who >= 0 ? ownpc_find(V.who) : -1; }

static void verb_end(int result)
{
    uint16_t then = V.then;
    V.live = 0;
    if (then) story_conv_arg(then, 0, result);
    ow_refresh_cast();
}

void story_verb_begin(int kind, int who, int arg, int then)
{
    memset(&V, 0, sizeof(V));
    V.kind = (uint8_t)kind;
    V.who = (int16_t)who;
    V.cx = (uint8_t)(arg & 0xFF);
    V.cy = (uint8_t)((arg >> 8) & 0x7F);
    V.fragile = (uint8_t)((arg & VERB_ARG_FRAGILE) != 0);
    V.then = (uint16_t)then;
    V.heat = 0xFF;
    if (kind < 0 || kind >= VERB_COUNT) return;
    V.live = 1;
    if (story_debug_autowin_on()) {                  /* scripted playthroughs */
        verb_end(kind == VERB_PHOTO || kind == VERB_DELIVER ? 2 : 1);
        return;
    }
    if (kind == VERB_ESCORT && follower() < 0) {
        int px, py;
        if (ow_player_cell(&px, &py)) ownpc_cast(who, (px + 1) | (py << 8), NPCF_MAIN);
    }
    audio_sfx(SFX_SELECT);
}

int story_verb_active(void) { return V.live ? V.kind : -1; }
void story_verb_cancel(void) { V.live = 0; }

void story_verbs_tick(void)
{
    int px, py;
    if (!V.live || !ow_player_cell(&px, &py)) return;
    V.t++;
    if (V.kind == VERB_ESCORT) {
        int k = follower(), nx, ny, d, s;
        if (k < 0) { verb_end(0); return; }
        nx = (int)(ow_npcs[k].x / OW_CELL);
        ny = (int)(ow_npcs[k].y / OW_CELL);
        d = DOOMO_MAX(iabs(nx - px), iabs(ny - py));
        V.far_t = (uint16_t)(d > VT.escort_far ? V.far_t + 1 : 0);
        s = story_escort_judge(d, V.far_t);
        if (s == ESC_LOST) {
            ui_toast(STR_ICON_HEART " You lost them...");
            verb_end(0);
            return;
        }
        /* the follower walks to the cell behind you (re-aimed twice a second) */
        if (s == ESC_FOLLOW && d > 1 && (V.t & 15) == 0) ownpc_act(V.who, NACT_WALK_TO, px | (py << 8));
        if (s == ESC_FAR && (V.t & 63) == 0) ui_toast("Wait up! (they're falling behind)");
        if (iabs(nx - V.cx) <= 1 && iabs(ny - V.cy) <= 1 && d <= VT.escort_wait) {
            ui_toast(STR_ICON_CHECK " Made it!");
            verb_end(1);
        }
    } else if (V.kind == VERB_DELIVER && V.fragile) {
        if (btn_down(BTN_B) && (btn_down(BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN))) {
            if (!V.ran) {
                V.ran = 1;
                V.spills++;
                audio_sfx_synth(SFX_BOING);
                ui_toast("Careful! It's fragile!");
            }
        } else {
            V.ran = 0;
        }
    }
}

void story_verbs_cell(int cx, int cy)
{
    int h;
    if (!V.live || V.kind != VERB_FIND) return;
    h = story_find_heat(cx - V.cx, cy - V.cy);
    if (h == 4) {
        audio_sfx(SFX_SPARKLE);
        ui_toast(STR_ICON_SPARK " Found it!");
        verb_end(1);
        return;
    }
    if (h != V.heat) {
        static const char *const msg[4] = { "Cold...", "Getting warmer...", "Warmer!", "Hot! It's right here somewhere!" };
        if (V.heat != 0xFF) ui_toast(msg[h]);
        V.heat = (uint8_t)h;
    }
}

int story_verbs_press_a(void)
{
    int px, py, k, r;
    if (!V.live || V.kind != VERB_PHOTO || !ow_player_cell(&px, &py)) return 0;
    k = follower();
    if (k < 0) return 0;
    /* only with the subject on screen: elsewhere A talks and opens doors */
    if (iabs((int)(ow_npcs[k].x / OW_CELL) - px) > VT.photo_far + 2 ||
        iabs((int)(ow_npcs[k].y / OW_CELL) - py) > VT.photo_far + 2) return 0;
    r = story_photo_judge(px, py, ow_player_facing(), (int)(ow_npcs[k].x / OW_CELL), (int)(ow_npcs[k].y / OW_CELL));
    audio_sfx_synth(SFX_SHWIP);
    camfx_flash(PAL_WHITE, 10, 4);
    if (!r) {
        ui_toast("*click* ...just a picture of the sky.");
        return 1;
    }
    ui_toast(r == 2 ? STR_ICON_STAR " Perfectly framed!" : STR_ICON_CHECK " Got the shot!");
    verb_end(r);
    return 1;
}

int story_verbs_npc(int id)
{
    if (!V.live || V.kind != VERB_DELIVER || id != V.who) return 0;
    audio_sfx(SFX_HAPPY);
    ui_toast(V.spills ? STR_ICON_CHECK " Delivered! (a little spilled)" : STR_ICON_STAR " Delivered perfectly!");
    verb_end(V.spills ? 1 : 2);
    return 1;
}

int story_verbs_walk_q8(void)
{
    if (!V.live || V.kind != VERB_DELIVER) return 256;
    return V.fragile ? VT.fragile_q8 : VT.walk_q8;
}

const char *story_verbs_objective(void)
{
    static char buf[64];
    const DoomoResident *r;
    if (!V.live) return NULL;
    r = town_res(V.who);
    switch (V.kind) {
    case VERB_ESCORT:
        doomo_snprintf(buf, sizeof(buf), "Walk %s there (stay close!)", r ? r->name : "them");
        break;
    case VERB_FIND:
        doomo_snprintf(buf, sizeof(buf), "Find it! %s", V.heat == 3 ? "(hot!)" : V.heat == 2 ? "(warmer)" : "");
        break;
    case VERB_PHOTO:
        doomo_snprintf(buf, sizeof(buf), "Photo of %s: face them, step back, A", r ? r->name : "them");
        break;
    default:
        doomo_snprintf(buf, sizeof(buf), "Bring it to %s%s", r ? r->name : "them", V.fragile ? " (don't run!)" : "");
        break;
    }
    return buf;
}
