/* overworld_spawn.c - the running-gag spawner and the wrongness table
 * (owner: S1 M5; docs/design/story/SPEC.md sec.7 S1, C5 (the file is named
 * overworld_* so it sits in the story overlay group on the PS1),
 * story_final.md sec.5-6 and 15.8, comic_timing.md sec.5.2 / 6.3).
 *
 * THE SPAWNER. A ROM table {gag, weight, cooldown_min, chapter_mask,
 * night_only}; one gag per 20-40 s of walking at most, nothing for 5-15 s
 * after a big beat (ow_spawn_quiet), nothing while a beat script or a
 * dialogue runs, and per-gag cooldowns in minutes that live in
 * STRY.gag_cool[] so a reload does not fire everything again. Picks go
 * through a local xorshift seeded from the town seed (deterministic, no
 * wall clock). The gags:
 *   STARER          a citizen goes perfectly still and stares at you; stand
 *                   still for R21 and they wave (a hop and a heart)
 *   CROWN_CLOUD     a little cloud with a crown drifts over the rooftops
 *   PET_BEEP        the pet says "beep." during a sincere (HEART) line
 *   DAYS_SINCE      the "DAYS SINCE LAST FIRE" board by Rivet's flips 7 - 0
 *   GRUMBLE_BACKS_IN in Hellvile, Grumbleguts backs into frame rear first,
 *                   record scratch, a wink, and backs out (max once / 4 min)
 *
 * THE WRONGNESS TABLE (W1-W16) is the same mechanism with "noticed" bits
 * (SF_WRONG(w), copied to STRY.wrong): a row fires once, when you are
 * within its radius on its map in its chapters (and, for OWW_STILL rows,
 * standing still for R21), `delay` frames late (W1: R8), with its sound,
 * a one-frame palette lerp (W13: lilac), a deadpan caption and/or a VM
 * script. The rows are content (S2, ch1/ ch2/) through the weak
 * doomo_ow_wrong_rows registry, read through a volatile pointer.
 *
 * State: OwScratch.spawn (a cache; a scratch claim resets the cadence).
 */
#include "overworld_int.h"
#include "doomo/camfx.h"
#include "doomo/txt.h"

/* the default gag table (chapter_mask bit c = chapter c, bit 5 = after) */
static const OwGagRow gag_rows_default[] = {
    { GAG_STARER,           3, 2, 0x06, 0 },
    { GAG_CROWN_CLOUD,      2, 3, 0x06, 0 },
    { GAG_PET_BEEP,         2, 5, 0x3E, 0 },
    { GAG_DAYS_SINCE,       2, 4, 0x04, 0 },
    { GAG_GRUMBLE_BACKS_IN, 2, 4, 0x3C, 0 },
    { GAG_NONE, 0, 0, 0, 0 }
};
__attribute__((weak)) const OwGagRow *const doomo_ow_gag_rows = gag_rows_default;
__attribute__((weak)) const OwWrongRow *const doomo_ow_wrong_rows = 0;
#define REG(p) (*(const void *const volatile *)&(p))

static const OwWrongRow *wrong_override;    /* tests */
static int wrong_override_n;

void ow_wrong_rows_override(const OwWrongRow *rows, int n) { wrong_override = rows; wrong_override_n = n; }

static const OwWrongRow *wrong_rows(int *n)
{
    const OwWrongRow *r;
    if (wrong_override) { *n = wrong_override_n; return wrong_override; }
    r = (const OwWrongRow *)REG(doomo_ow_wrong_rows);
    *n = 0;
    if (r) while (r[*n].map != 0xFF) (*n)++;      /* rows end with map 0xFF */
    return r;
}

static const OwGagRow *gag_rows(void)
{
    const OwGagRow *r = (const OwGagRow *)REG(doomo_ow_gag_rows);
    return r ? r : gag_rows_default;
}

#define SP (ow_scr->spawn)
#define CADENCE_MIN 600                         /* 20 s */
#define CADENCE_SPAN 600                        /* .. 40 s */
#define MINUTE 1800

static uint32_t rng_state;
static uint32_t srand32(void)
{
    if (!rng_state) rng_state = doomo_town.seed ^ 0x6A6E5E5Du ^ doomo_town.day * 0x9E3779B9u;
    if (!rng_state) rng_state = 1;
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

void ow_spawn_reset(void)
{
    memset(&ow_scr->spawn, 0, sizeof(ow_scr->spawn));
    SP.npc = -1;
    rng_state = 0;
    SP.next = (uint16_t)(CADENCE_MIN + srand32() % CADENCE_SPAN);
}

void ow_spawn_quiet(int frames)
{
    if (!ow_scratch_mine()) return;
    if (frames > SP.next) SP.next = (uint16_t)DOOMO_MIN(frames, 0xFFFF);
}

static int chapter_bit(void)
{
    int c = story_chapter();
    return 1 << DOOMO_CLAMP(c, 0, 5);
}

/* Cooldowns in minutes, saved in STRY.gag_cool[] (SF sec.17.2), so a
 * reload does not fire everything again (--- S2: moved into STRY --- ). */
static uint8_t *cool(int gag)
{
    return gag > 0 && gag <= 8 ? &story_stry()->gag_cool[gag - 1] : NULL;
}
static uint8_t pal_frame;                        /* a one-frame palette lerp is up */

/* where the pet stands (overworld.c keeps it in ow.petx/pety) */
static int pet_here(void) { return pet_exists() || inv_count(ITEM_DOOMOGACHI_EGG); }

/* the workshop's door cell on this map, if any */
static int workshop_door(int *cx, int *cy)
{
    int x, y;
    for (y = 0; y < doomo_map.h; y++)
        for (x = 0; x < doomo_map.w; x++) {
            const DoomoCell *c = map_cell(&doomo_map, x, y);
            if (c->wall && c->tag == STORY_TAG_DOOR + PLACE_WORKSHOP) { *cx = x; *cy = y; return 1; }
        }
    return 0;
}

static int near_cell(int cx, int cy, int r)
{
    int32_t dx = (ow.px - ((int32_t)cx * OW_CELL + OW_CELL / 2)) / OWQ;
    int32_t dy = (ow.py - ((int32_t)cy * OW_CELL + OW_CELL / 2)) / OWQ;
    int32_t rr = (int32_t)r * DOOMO_CELL_UNITS;
    return dx * dx + dy * dy <= rr * rr;
}

/* can this gag run here and now? */
static int gag_ok(int gag)
{
    int cx, cy;
    switch (gag) {
    case GAG_STARER: {
        int i;
        for (i = 0; i < ow_npc_count; i++) {
            const OwNpc *n = &ow_npcs[i];
            if (n->kind == NK_RESIDENT && !(n->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY)) &&
                !(n->sflags & NSF_SCRIPTED) && near_cell(n->x / OW_CELL, n->y / OW_CELL, 6)) return 1;
        }
        return 0; }
    case GAG_CROWN_CLOUD: return ow.map == MAPID_HAHALAND;
    case GAG_PET_BEEP: return 0;                    /* fires under dialogue only */
    case GAG_DAYS_SINCE: return workshop_door(&cx, &cy) && near_cell(cx, cy, 4);
    case GAG_GRUMBLE_BACKS_IN: {
        const DoomoResident *r = town_res(ROSTER_ID_GRUMBLEGUTS);
        return ow.map == MAPID_HELLVILE && r && (r->status == RES_ALIVE) && ownpc_find(ROSTER_ID_GRUMBLEGUTS) < 0 &&
               !story_flag(SF_GRUMBLE_DEAD);
    }
    default: return 0;
    }
}

static void gag_start(int gag)
{
    int i;
    SP.gag = (uint8_t)gag;
    SP.live = 1;
    SP.t = 0;
    SP.npc = -1;
    switch (gag) {
    case GAG_STARER:
        for (i = 0; i < ow_npc_count; i++) {
            OwNpc *n = &ow_npcs[i];
            if (n->kind == NK_RESIDENT && !(n->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY)) &&
                !(n->sflags & NSF_SCRIPTED) && near_cell(n->x / OW_CELL, n->y / OW_CELL, 6)) {
                SP.npc = n->id;
                ownpc_act(n->id, NACT_FACE, DOOMO_PLAYER_ID);
                ownpc_act(n->id, NACT_STILL, 0);
                n->state = NS_IDLE;
                break;
            }
        }
        break;
    case GAG_CROWN_CLOUD:
        SP.x = ow.camx - 140 * 4 * OWQ;
        SP.y = ow.camy - 70 * 4 * OWQ;
        break;
    case GAG_DAYS_SINCE: {
        int cx = 0, cy = 0;
        workshop_door(&cx, &cy);
        SP.x = (int32_t)cx * OW_CELL + OW_CELL / 2;
        SP.y = (int32_t)cy * OW_CELL;
        break; }
    case GAG_GRUMBLE_BACKS_IN: {
        /* he backs in from the right edge of the view, rear first */
        int cx = (int)((ow.camx + 70 * 4 * OWQ) / OW_CELL), cy = (int)(ow.py / OW_CELL);
        int k = ownpc_cast(ROSTER_ID_GRUMBLEGUTS, (cx & 0xFF) | ((cy & 0xFF) << 8), NPCF_FIXED);
        if (k < 0) { SP.live = 0; return; }
        SP.npc = ROSTER_ID_GRUMBLEGUTS;
        ownpc_act(SP.npc, NACT_TURN, 2);                       /* faces away: rear to camera */
        ownpc_act(SP.npc, NACT_WALK_TO, ((cx - 2) & 0xFF) | ((cy & 0xFF) << 8));
        break; }
    default: SP.live = 0; return;
    }
    {
        uint8_t *c = cool(gag);
        const OwGagRow *r = gag_rows();
        for (; r->gag; r++) if (r->gag == gag && c) *c = r->cooldown_min;
    }
}

static void gag_end(void)
{
    if (SP.gag == GAG_STARER && SP.npc >= 0) ownpc_act(SP.npc, NACT_THAW, 0);
    if (SP.gag == GAG_GRUMBLE_BACKS_IN && SP.npc >= 0) ownpc_cast(SP.npc, 0, OWC_REMOVE);
    SP.live = 0;
    SP.gag = 0;
    SP.npc = -1;
    SP.next = (uint16_t)(CADENCE_MIN + srand32() % CADENCE_SPAN);
}

static void gag_tick(void)
{
    int t = SP.t++;
    switch (SP.gag) {
    case GAG_STARER:
        if (SP.npc < 0 || ownpc_find(SP.npc) < 0) { gag_end(); return; }
        if (!ow.moving) SP.x++;
        else SP.x = 0;
        if (SP.x == RUNG_R21) {                                 /* you stood still: they wave */
            ownpc_act(SP.npc, NACT_THAW, 0);
            ownpc_act(SP.npc, NACT_HOP, 0);
            ownpc_act(SP.npc, NACT_EMOTE, EMO_HEART);
            SP.t = 200;
        }
        if (t > 240) gag_end();
        break;
    case GAG_CROWN_CLOUD:
        SP.x += 2 * OWQ;                                         /* a slow drift */
        if (t > 300) gag_end();
        break;
    case GAG_DAYS_SINCE:
        if (t == 30 || t == 30 + RUNG_R13) audio_sfx_synth(SFX_TINK);
        if (t > 120) gag_end();
        break;
    case GAG_GRUMBLE_BACKS_IN:
        if (t == 12) { audio_sfx_synth(SFX_SCRATCH); ow_cam(CAM_BRAKE, 4); }   /* the rear arrives */
        if (t == 12 + RUNG_R13) { ownpc_act(SP.npc, NACT_TURN, 0); ownpc_act(SP.npc, NACT_EMOTE, EMO_HEART); }
        if (t == 60) {
            int k = ownpc_find(SP.npc);
            if (k >= 0) {
                int cx = ow_npcs[k].x / OW_CELL + 3, cy = ow_npcs[k].y / OW_CELL;
                ownpc_act(SP.npc, NACT_WALK_TO, (cx & 0xFF) | ((cy & 0xFF) << 8));
            }
        }
        if (t > 120) gag_end();
        break;
    default: gag_end(); break;
    }
}

/* ---- wrongness -------------------------------------------------------------- */
static uint8_t still_t;

static int wrong_noticed(int w) { return story_flag(SF_WRONG(w & 15)); }

static void wrong_fire(const OwWrongRow *r)
{
    char line[200];
    if (r->sfx) {
        if (r->sfx >= SFX_SYNTH_FIRST) audio_sfx_synth(r->sfx);
        else audio_sfx(r->sfx);
    }
    if (r->pal) { camfx_pal_lerp(r->pal, 16); pal_frame = 1; }   /* one frame: off next tick */
    if (r->caption != TXT_NONE && txt_get(r->caption, line, (int)sizeof(line)) > 0)
        ui_say_ex(-1, EXPR_NEUTRAL, DL_DEADPAN, line);
    if (r->script) vm_start(r->script);
}

static int wrong_check(void)
{
    int n, i, chap = chapter_bit(), night = town_is_night();
    const OwWrongRow *rows = wrong_rows(&n);
    if (!rows) return 0;
    still_t = ow.moving ? 0 : (uint8_t)DOOMO_MIN(still_t + 1, 255);
    for (i = 0; i < n; i++) {
        const OwWrongRow *r = &rows[i];
        if (r->map != ow.map || !(r->chapter_mask & chap) || wrong_noticed(r->w)) continue;
        if ((r->flags & OWW_NIGHT) && !night) continue;
        if ((r->flags & OWW_DAY) && night) continue;
        if ((r->flags & OWW_STILL) && still_t < RUNG_R21) continue;
        if (!near_cell(r->x, r->y, DOOMO_MAX(1, r->r))) continue;
        story_set_flag(SF_WRONG(r->w & 15), 1);
        SP.wrong = (uint8_t)(i + 1);
        SP.wrong_t = r->delay;
        return 1;
    }
    return 0;
}

void ow_spawn_update(void)
{
    if (!ow_scratch_mine()) return;
    if (pal_frame) { pal_frame = 0; camfx_pal_lerp(PAL_LERP_NONE, 0); }
    /* cooldown minutes tick while you walk around */
    if (++SP.minute >= MINUTE) {
        int g;
        SP.minute = 0;
        for (g = 1; g <= 8; g++) { uint8_t *c = cool(g); if (c && *c) (*c)--; }
    }
    if (SP.wrong) {
        if (SP.wrong_t) SP.wrong_t--;
        else {
            int n;
            const OwWrongRow *rows = wrong_rows(&n);
            if (rows && SP.wrong - 1 < n) wrong_fire(&rows[SP.wrong - 1]);
            SP.wrong = 0;
        }
    }
    if (SP.live) { gag_tick(); return; }
    if (beat_running() || story_talking() || ui_dialog_active()) return;
    if (SP.next) { SP.next--; return; }
    if (wrong_check()) {
        SP.next = (uint16_t)(CADENCE_MIN + srand32() % CADENCE_SPAN);
        return;
    }
    {   /* a weighted pick among the gags that may run now */
        const OwGagRow *r = gag_rows();
        int total = 0, pick, chap = chapter_bit(), night = town_is_night();
        const OwGagRow *q;
        for (q = r; q->gag; q++) {
            uint8_t *c = cool(q->gag);
            if (!(q->chapter_mask & chap) || (q->night_only && !night) || (c && *c) || !gag_ok(q->gag)) continue;
            total += q->weight;
        }
        if (!total) { SP.next = 90; return; }
        pick = (int)(srand32() % (uint32_t)total);
        for (q = r; q->gag; q++) {
            uint8_t *c = cool(q->gag);
            if (!(q->chapter_mask & chap) || (q->night_only && !night) || (c && *c) || !gag_ok(q->gag)) continue;
            if ((pick -= q->weight) < 0) { gag_start(q->gag); break; }
        }
    }
}

/* Under a dialogue box (called from the overworld's draw timers, which run
 * even while SCENE_DIALOG is on top): the pet's "beep." on a sincere line. */
static uint16_t beep_t;
static uint16_t beep_line;
void ow_spawn_under_dialog(void)
{
    UiDialogTop top;
    uint8_t *c = cool(GAG_PET_BEEP);
    if (beep_t) { beep_t--; return; }
    if (!ow_scratch_mine() || beat_running() || !pet_here() || (c && *c)) return;
    if (!(chapter_bit() & 0x3E) || !ui_dialog_top(&top) || !(top.flags & DL_HEART)) return;
    if (top.serial == beep_line || top.shown < 4) return;
    beep_line = top.serial;
    if (srand32() % 3u) return;
    beep_t = 30;
    audio_music_duck(140, 13);
    audio_sfx_synth(SFX_BEEP);
    if (c) *c = 5;
}

/* screen-space parts of the gags (after the world, before the HUD) */
void ow_spawn_draw(void)
{
    int sx, sy;
    if (!ow_scratch_mine()) return;
    if (beep_t && pet_here()) {
        int s = tw_tab(&TW_POP_BUBBLE, 30 - beep_t);
        owfx_world_to_screen(ow.petx, ow.pety, &sx, &sy);
        if (s > 120) {
            ui_round_rect(sx + 4, sy - 34, 34, 13, 4, PAL_WHITE);
            ui_text(sx + 8, sy - 31, "beep.", PAL_BLACK);
        }
    }
    if (!SP.live) return;
    switch (SP.gag) {
    case GAG_CROWN_CLOUD: {
        static const int8_t px[5] = { -9, -4, 2, 7, 0 }, py[5] = { 2, -2, -3, 1, 3 };
        int i, bob = ui_wave(SP.t, 60, 2);
        owfx_world_to_screen(SP.x, SP.y, &sx, &sy);
        for (i = 0; i < 5; i++) ui_fill_circle(sx + px[i], sy + py[i] + bob, 6, PAL_RAMP(RAMP_VAPE, 6));
        for (i = 0; i < 4; i++) ui_fill_circle(sx + px[i] - 1, sy + py[i] - 2 + bob, 3, PAL_RAMP(RAMP_VAPE, 7));
        /* the crown, slightly crooked */
        plat_rect(sx - 5, sy - 10 + bob, 11, 3, PAL_UI_GOLD);
        plat_rect(sx - 5, sy - 13 + bob, 2, 3, PAL_UI_GOLD);
        plat_rect(sx - 1, sy - 14 + bob, 2, 4, PAL_UI_GOLD);
        plat_rect(sx + 3, sy - 13 + bob, 2, 3, PAL_UI_GOLD);
        plat_rect(sx - 1, sy - 9 + bob, 2, 1, PAL_UI_RED);
        break; }
    case GAG_DAYS_SINCE: {
        static const char *const days[3] = { "7", "-", "0" };
        int k = SP.t < 30 ? 0 : SP.t < 30 + RUNG_R13 ? 1 : 2;
        int age = k == 0 ? SP.t : k == 1 ? SP.t - 30 : SP.t - 30 - RUNG_R13;
        int s = k ? tw_tab(&TW_SLAM_COUNTER, age) : 256;
        owfx_world_to_screen(SP.x, SP.y, &sx, &sy);
        ui_round_rect(sx - 40, sy - 40, 80, 24, 3, PAL_RAMP(RAMP_WOOD, 2));
        ui_round_rect(sx - 39, sy - 39, 78, 22, 3, PAL_UI_CREAM);
        ui_text(sx - 36, sy - 37, "DAYS SINCE LAST", PAL_RAMP(RAMP_WOOD, 1));
        ui_text(sx - 22, sy - 28, "FIRE:", PAL_UI_RED_D);
        ui_text(sx + 12, sy - 28 - (s - 256) / 32, days[k], PAL_UI_RED);
        break; }
    default: break;
    }
}

int ow_spawn_debug(int set_next)
{
    if (!ow_scratch_mine()) return -1;
    if (set_next >= 0) SP.next = (uint16_t)set_next;
    return SP.live ? SP.gag : 0;
}
