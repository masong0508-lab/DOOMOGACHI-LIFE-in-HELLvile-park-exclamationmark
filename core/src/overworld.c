/* overworld.c - SCENE_STORY: the open-world overworld of Story Mode
 * (owner: story). Rendered by plat_overworld() (GBA/native: top-down
 * tiles; PS1: real 3D terrain), with every citizen, prop, marker and the
 * player as billboards, then particles and the HUD in 2D on top.
 *
 * The player walks (D-pad, hold B to run) with box collision against
 * walls, water, lava, ledges taller than DOOMO_STEP_UP (stairs and ramps
 * are fine), props and citizens. A talks to the citizen in front of you,
 * reads signs, picks flowers, snaps photos; walking into a door (a wall
 * cell tagged STORY_TAG_DOOR + PLACE_*) enters the place; walking onto a
 * CELL_TRIGGER cell fires story_trigger(tag). START opens the pause panel,
 * SELECT toggles the mini-map.
 */
#include "overworld_int.h"
#include "audio_int.h"
#include "doomo/camfx.h"
#include "doomo/txt.h"

OwState ow;
int ow_bb_count;                        /* ow_bb = the shared doomo_frame_bb */

/* ---- the overworld's part of the scene scratch (overworld_int.h) ---------------------- */
#define OW_SCRATCH_TAG SAVE_EXT_TAG('O', 'W', 'L', 'D')
OwScratch *ow_scr;

int ow_scratch_mine(void)
{
    return ow_scr && scene_scratch_mine(OW_SCRATCH_TAG);
}

int ow_scratch_claim(void)
{
    if (ow_scratch_mine()) return 0;
    ow_scr = (OwScratch *)scene_scratch(OW_SCRATCH_TAG, (uint32_t)sizeof(OwScratch));
    memset(ow_scr, 0, sizeof(*ow_scr));
    ow_npc_count = 0;
    ow_prop_count = 0;
    owfx_scratch_lost();
    ownpc_acts_reset();                     /* --- S1 M5: script slots, camera, spawner --- */
    ow_scr->cam.mode = CAM_FOLLOW;
    tw_spring_init(&ow_scr->cam.brake, TW_BRAKE, 0);
    ow_spawn_reset();
    return 1;
}

/* May the overworld write its scratch now? Only while it is the scene
 * running (or drawing), or when the scratch is still its own: story
 * callbacks can run under other scenes, whose scratch must stay intact. */
static int ow_scratch_usable(void)
{
    if (ow_scratch_mine()) return 1;
    if (scene_top() != &SCENE_STORY) return 0;
    ow_scratch_claim();
    return 1;
}

/* Back from a scene that took the scratch: props + cast again. */
static void ow_scratch_ensure(void)
{
    if (ow_scratch_claim() && ow.started && map_current_id() == ow.map) {
        ownpc_load_props();
        ownpc_spawn_all();
    }
}

/* R17: the script extension area (Feast guests, the Say It walk). */
void *ow_scratch_ext(void) { return ow_scratch_mine() ? ow_scr->ext : NULL; }

static const int8_t dir_dx[4] = { 0, -1, 1, 0 };
static const int8_t dir_dy[4] = { 1, 0, 0, -1 };

#define PR (9 * OWQ)                    /* player half-width (feet box)     */
#define WALK_SPEED (4 * OWQ + 8)
#define RUN_SPEED (8 * OWQ)

uint32_t ow_time(void) { return ow.t; }
int ow_active(void) { return ow.started && !ow.visit; }
void ow_shake(int t) { ow.shake = (uint8_t)DOOMO_MAX(ow.shake, t); }
void ow_flash(int color, int t) { ow.flash = (uint8_t)t; ow.flash_col = (uint8_t)color; }
void ow_banner(const char *text) { ow.banner = text; ow.banner_t = 100; }

int ow_player_cell(int *cx, int *cy)
{
    if (cx) *cx = ow.px / OW_CELL;
    if (cy) *cy = ow.py / OW_CELL;
    return ow.map;
}

void ow_sync_to_save(void)
{
    DoomoStoryState *s = &doomo_town.story;
    if (ow.visit || !ow.started) return;
    s->map = ow.map;
    s->x = (int16_t)(ow.px / OWQ);
    s->y = (int16_t)(ow.py / OWQ);
    s->facing = ow.dir;
}

/* ---- map queries --------------------------------------------------------------- */
int ow_floor_h(int cx, int cy)
{
    return map_cell_safe(&doomo_map, cx, cy)->floor_h;
}

int ow_walkable_cell(int cx, int cy, int from_fh)
{
    const DoomoCell *c;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return 0;
    c = map_cell(&doomo_map, cx, cy);
    if (c->wall) return 0;
    if (c->flags & (CELL_WATER | CELL_LAVA)) return 0;
    if (c->floor_h - from_fh > 3 || from_fh - c->floor_h > 3) return 0;
    return 1;
}

int ow_prop_blocks(int32_t x, int32_t y, int r)
{
    int i;
    int32_t ux = x / OWQ, uy = y / OWQ;
    for (i = 0; i < ow_prop_count; i++) {
        const OwProp *p = &ow_props[i];
        int32_t dx, dy, rr;
        if (!p->r) continue;
        dx = ux - p->x;
        dy = uy - p->y;
        if (dx > 64 || dx < -64 || dy > 64 || dy < -64) continue;
        rr = p->r + r;
        if (dx * dx + dy * dy * 2 < rr * rr) return 1;      /* squashed: feet */
    }
    return 0;
}

static int can_stand(int32_t x, int32_t y, int from_fh)
{
    int k;
    for (k = 0; k < 4; k++) {
        int32_t qx = x + ((k & 1) ? PR : -PR);
        int32_t qy = y + ((k & 2) ? PR / 2 : -PR);
        if (qx < 0 || qy < 0) return 0;
        if (!ow_walkable_cell(qx / OW_CELL, qy / OW_CELL, from_fh)) return 0;
    }
    if (ow_prop_blocks(x, y, 6)) return 0;
    if (ownpc_blocks(x, y, -1)) return 0;
    return 1;
}

/* ---- music ------------------------------------------------------------------------ */
int ow_music_for_map(int map)
{
    int b = story_beat();
    switch (map) {
    case MAPID_HAHALAND_INVADED: return SONG_INVASION;
    case MAPID_HELLVILE: return SONG_HELLVILE;
    case MAPID_PARK_UNITED: return SONG_FESTIVAL;
    default:
        if (!ow.visit && (b == BEAT_C2_SCREAM || b == BEAT_C2_FUZZ || b == BEAT_C2_EXORCISE)) return SONG_OMINOUS;
        if (!ow.visit && b == BEAT_C1_WALL) return SONG_OMINOUS;
        return town_is_night() ? SONG_HAHALAND_NIGHT : SONG_HAHALAND_DAY;
    }
}

static void restore_style(void)
{
    ui_dialog_style(ow.map == MAPID_HAHALAND_INVADED ? WIN_HELL :
                    ow.map == MAPID_HELLVILE ? WIN_DARK : WIN_CUTE);
}

/* ---- entering a map ---------------------------------------------------------------- */
static int find_open_near(int *cx, int *cy)
{
    int r, dx, dy, fh = ow_floor_h(*cx, *cy);
    if (ow_walkable_cell(*cx, *cy, fh)) return 1;
    for (r = 1; r < 6; r++)
        for (dy = -r; dy <= r; dy++)
            for (dx = -r; dx <= r; dx++) {
                int x = *cx + dx, y = *cy + dy;
                if (x < 0 || y < 0 || x >= doomo_map.w || y >= doomo_map.h) continue;
                if (ow_walkable_cell(x, y, ow_floor_h(x, y))) { *cx = x; *cy = y; return 1; }
            }
    return 0;
}

static void place_player(int32_t x, int32_t y)
{
    int i;
    ow.px = x;
    ow.py = y;
    ow.camx = x;
    ow.camy = y;
    ow.petx = x - OW_CELL / 2;
    ow.pety = y;
    for (i = 0; i < 16; i++) { ow.trail_x[i] = x; ow.trail_y[i] = y; }
    ow.last_cx = (int8_t)(x / OW_CELL);
    ow.last_cy = (int8_t)(y / OW_CELL);
}

static void load_map_only(int map_id)
{
    if (map_id < 0 || map_id >= MAPID_COUNT) map_id = MAPID_HAHALAND;
    ow.map = (uint8_t)map_id;
    map_load(map_id);
    if (ow_scratch_usable()) ownpc_load_props();   /* else when we resume */
}

/* --- S1 M5: the border record scratch (CT sec.6.3): Hillvile -> Hellvile is a
 * tape-stop (5 f), 14 frames of silence, then the Hellvile theme on the
 * downbeat, with the camera braking. --- */
static uint8_t border_t;                 /* frames until the theme comes in */

static int side_of(int map)
{
    return map == MAPID_HELLVILE ? 2 : (map == MAPID_HAHALAND || map == MAPID_HAHALAND_INVADED) ? 1 : 0;
}

void ow_enter_map(int map_id, int cx, int cy)
{
    int crossing = ow.started && side_of(ow.map) == 1 && side_of(map_id) == 2;
    load_map_only(map_id);
    if (!find_open_near(&cx, &cy)) { cx = doomo_map.w / 2; cy = doomo_map.h / 2; }
    place_player((int32_t)cx * OW_CELL + OW_CELL / 2, (int32_t)cy * OW_CELL + OW_CELL / 2);
    ow.fade = 12;
    ow.region = NULL;
    if (ow_scratch_usable()) ownpc_spawn_all();
    if (crossing) {
        audio_sfx_synth(SFX_SCRATCH);
        audio_tapestop(5);
        border_t = 5 + 14;
        ow_cam(CAM_BRAKE, 8);
        ow_spawn_quiet(300);
    } else {
        border_t = 0;
        audio_music(ow_music_for_map(ow.map));
    }
    restore_style();
    if (!ow.visit) {
        doomo_town.story.map = (uint8_t)ow.map;
        ow_sync_to_save();
    }
    {
        const char *t = map_title(ow.map);
        if (t && t[0]) ow_banner(t);
    }
}

void ow_refresh_cast(void)
{
    if (ow.started && map_current_id() == ow.map && ow_scratch_usable()) ownpc_spawn_all();
}

static void continue_on_saved_map(void)
{
    DoomoStoryState *s = &doomo_town.story;
    int map = s->map < MAPID_COUNT ? s->map : MAPID_HAHALAND;
    int cx, cy;
    if (map == MAPID_VAPE_CELLAR || map == MAPID_CLOUD_NINE_SPIRE || map == MAPID_ARENA)
        map = story_chapter() == STORY_CH3 ? MAPID_HAHALAND_INVADED : MAPID_HAHALAND;
    load_map_only(map);
    cx = s->x / DOOMO_CELL_UNITS;
    cy = s->y / DOOMO_CELL_UNITS;
    if (s->x <= 0 && s->y <= 0) { cx = 34; cy = 26; }
    if (find_open_near(&cx, &cy) && cx == s->x / DOOMO_CELL_UNITS && cy == s->y / DOOMO_CELL_UNITS)
        place_player((int32_t)s->x * OWQ, (int32_t)s->y * OWQ);
    else
        place_player((int32_t)cx * OW_CELL + OW_CELL / 2, (int32_t)cy * OW_CELL + OW_CELL / 2);
    ow.dir = s->facing & 3;
    ow.fade = 15;
    if (ow_scratch_usable()) ownpc_spawn_all();
    audio_music(ow_music_for_map(ow.map));
    restore_style();
}

/* ---- scene ---------------------------------------------------------------------------- */
static void start_story(void)
{
    story_new_game();
    ow.started = 1;
    ow_enter_map(MAPID_HAHALAND, 34, 26);
    ow.dir = 3;
}

static void story_enter(int arg)
{
    memset(&ow, 0, sizeof(ow));
    camfx_reset();                                 /* --- S1 M5 --- */
    story_text_prefetch();                         /* S2: the text packs (PS1: TEXT.BIN) */
    border_t = 0;
    ow_scratch_claim();
    ow.near_npc = -1;
    ow.near_prop = -1;
    ow.near_spot = -1;
    owfx_init();
    if (arg >= STORY_ARG_VISIT_HAHALAND && arg <= STORY_ARG_VISIT_PARK) {
        static const uint8_t maps[3] = { MAPID_HAHALAND, MAPID_HELLVILE, MAPID_PARK_UNITED };
        static const uint8_t sx[3] = { 34, 3, 16 }, sy[3] = { 26, 27, 26 };
        int k = arg - STORY_ARG_VISIT_HAHALAND;
        ow.visit = 1;
        ow.started = 1;
        ow_enter_map(maps[k], sx[k], sy[k]);
        return;
    }
    if (arg >= STORY_ARG_DEBUG_BEAT && arg < STORY_ARG_DEBUG_NIGHT + BEAT_ALL_COUNT &&   /* S2: + the new ids */
        (arg - STORY_ARG_DEBUG_BEAT) % 100 < BEAT_ALL_COUNT) {
        int beat = (arg - STORY_ARG_DEBUG_BEAT) % 100;
        town_new(TOWN_STORY, 0xD0D0u + (uint32_t)beat, TOWN_POP_PREMADE);
        sim_init();
        start_story();
        story_set_flag(SF_TITLE_CH1, 1);
        story_debug_jump(story_beat_chapter(beat), beat);
        if (arg >= STORY_ARG_DEBUG_NIGHT) doomo_town.minute = 21 * 60 + 30;
        else doomo_town.minute = 10 * 60;
        audio_music(ow_music_for_map(ow.map));
        return;
    }
    if (arg == STORY_ARG_NEW) {
        const DoomoResident *p;
        if (doomo_town.mode != TOWN_STORY) {
            town_new(TOWN_STORY, doomo_hash32(plat_entropy() + 0x5107u), TOWN_POP_PREMADE);
            sim_init();
        }
        p = town_res(DOOMO_PLAYER_ID);
        load_map_only(MAPID_HAHALAND);
        place_player(34 * OW_CELL + OW_CELL / 2, 26 * OW_CELL + OW_CELL / 2);
        if (!p || p->status == RES_EMPTY) {
            story_push_scene(&SCENE_CREATOR, -3, PEND_CREATOR, CTX_BEAT, 0);
            return;
        }
        start_story();
        return;
    }
    if (doomo_town.story.chapter == STORY_CH_NONE) {
        if (doomo_town.mode != TOWN_STORY) {
            town_new(TOWN_STORY, doomo_hash32(plat_entropy() + 0x5107u), TOWN_POP_PREMADE);
            sim_init();
        }
        start_story();
        return;
    }
    sim_init();
    ow.started = 1;
    story_rearm_possessors();                      /* S2: a load re-arms the authored possessors */
    continue_on_saved_map();
}

static void story_leave(void)
{
    ow_sync_to_save();
}

/* Back on top after a pushed scene. */
static void after_subscene(void)
{
    if (map_current_id() != ow.map) {
        int32_t x = ow.px, y = ow.py;
        load_map_only(ow.map);
        ow.px = x;
        ow.py = y;
    }
    if (ow_scratch_usable()) ownpc_spawn_all();
    audio_music(ow_music_for_map(ow.map));
    restore_style();
    ow.fade = 8;
    input_eat();
}

/* Plot moments that start by themselves. Returns 1 if one started. */
static int auto_events(void)
{
    int b = story_beat();
    if (b == BEAT_C1_ARRIVE) {
        if (!story_flag(SF_TITLE_CH1)) {
            story_set_flag(SF_TITLE_CH1, 1);
            story_push_cutscene(CUT_CHAPTER1, CTX_BEAT, BEAT_C1_ARRIVE);
            return 1;
        }
        story_set_beat(BEAT_C1_MAYOR);
        return 1;
    }
    if (b == BEAT_C3_ERUPTION && ow.map != MAPID_HAHALAND_INVADED) {
        ow_enter_map(MAPID_HAHALAND_INVADED, ow.px / OW_CELL, ow.py / OW_CELL);
        return 0;
    }
    if (b == BEAT_C1_MAYOR && !story_flag(SF_MAP_HINT) && ow.t > 20) {
        story_set_flag(SF_MAP_HINT, 1);
        ui_toast(STR_ICON_A " Talk   START Menu   B Run");
        return 0;
    }
    if (b >= BEAT_C1_PIP && b <= BEAT_C1_FRIENDS && pet_exists() && !story_flag(SF_PET_HINT)) {
        char buf[48];
        story_set_flag(SF_PET_HINT, 1);
        story_fmt(buf, sizeof(buf), STR_ICON_HEART " @E follows you around!");
        ui_toast(buf);
        return 0;
    }
    if (b == BEAT_C3_NESTS && !story_flag(SF_INVASION_HINT) && ow.t > 30) {
        story_set_flag(SF_INVASION_HINT, 1);
        ui_toast(STR_ICON_SKULL " Walk into a nest to fight!");
        return 0;
    }
    return 0;
}

/* ---- player ---------------------------------------------------------------------- */
static void enter_door(int place)
{
    ow.push_t = 0;
    if (ow.visit) story_place_menu(place);
    else story_enter_place(place);
}

static int door_in_front(void)
{
    int32_t fx = ow.px + dir_dx[ow.dir] * (PR + 10 * OWQ);
    int32_t fy = ow.py + dir_dy[ow.dir] * (PR + 10 * OWQ) - (ow.dir == 3 ? 4 * OWQ : 0);
    const DoomoCell *c;
    if (fx < 0 || fy < 0) return 0;
    c = map_cell_safe(&doomo_map, fx / OW_CELL, fy / OW_CELL);
    if (c->wall && c->tag >= STORY_TAG_DOOR && c->tag < STORY_TAG_DOOR + PLACE_COUNT)
        return c->tag - STORY_TAG_DOOR;
    return 0;
}

static void on_new_cell(int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
    const char *rg = story_region_name(ow.map, cx, cy);
    if (rg && rg[0] && rg != ow.region) {
        if (ow.region) ow_banner(rg);
        ow.region = rg;
    }
    if (ow.visit) return;
    if ((c->flags & CELL_TRIGGER) && c->tag) story_trigger_ow(c->tag);
    story_verbs_cell(cx, cy);                      /* S2: the find verb */
    if (!story_talking()) side_trigger_spot(ow.map, cx, cy, 0);
}

static void player_update(void)
{
    int dx = 0, dy = 0, speed, moved = 0, fh;
    int32_t nx, ny;
    if (btn_down(BTN_LEFT)) dx = -1;
    if (btn_down(BTN_RIGHT)) dx = 1;
    if (btn_down(BTN_UP)) dy = -1;
    if (btn_down(BTN_DOWN)) dy = 1;
    ow.running = btn_down(BTN_B) ? 1 : 0;
    speed = ow.running ? RUN_SPEED : WALK_SPEED;
    if (dx && dy) speed = speed * 181 / 256;
    if (!ow.visit) speed = speed * story_verbs_walk_q8() / 256;   /* S2: carrying a delivery */
    if (dx) ow.dir = dx < 0 ? 1 : 2;
    else if (dy) ow.dir = dy < 0 ? 3 : 0;
    fh = ow_floor_h(ow.px / OW_CELL, ow.py / OW_CELL);
    if (dx) {
        nx = ow.px + dx * speed;
        if (can_stand(nx, ow.py, fh)) { ow.px = nx; moved = 1; }
    }
    if (dy) {
        ny = ow.py + dy * speed;
        if (can_stand(ow.px, ny, fh)) { ow.py = ny; moved = 1; }
    }
    ow.moving = (uint8_t)moved;
    if (moved) ow.anim++;
    else ow.anim = 0;
    /* pushing into a door opens it */
    if ((dx || dy) && !moved) {
        int place = door_in_front();
        if (place) {
            if (++ow.push_t >= 6) { enter_door(place); return; }
        } else {
            ow.push_t = 0;
        }
    } else {
        ow.push_t = 0;
    }
    /* trail for the pet */
    if (moved && (ow.t & 1)) {
        ow.trail_i = (uint8_t)((ow.trail_i + 1) & 15);
        ow.trail_x[ow.trail_i] = ow.px;
        ow.trail_y[ow.trail_i] = ow.py;
    }
    {
        int cx = ow.px / OW_CELL, cy = ow.py / OW_CELL;
        if (cx != ow.last_cx || cy != ow.last_cy) {
            ow.last_cx = (int8_t)cx;
            ow.last_cy = (int8_t)cy;
            on_new_cell(cx, cy);
        }
    }
}

/* What A would do right now (also drives the HUD prompt). */
static void find_interactables(void)
{
    int i, best = -1;
    int32_t bestd = (int32_t)(46 * OWQ) * (46 * OWQ);
    ow.near_npc = -1;
    ow.near_prop = -1;
    ow.near_spot = -1;
    ow.near_door = (int16_t)door_in_front();
    for (i = 0; i < ow_npc_count; i++) {
        const OwNpc *n = &ow_npcs[i];
        int32_t ddx = n->x - ow.px, ddy = n->y - ow.py, d;
        if (n->kind != NK_RESIDENT) continue;
        if (ddx > 64 * OWQ || ddx < -64 * OWQ || ddy > 64 * OWQ || ddy < -64 * OWQ) continue;
        d = ddx * ddx + ddy * ddy;
        /* prefer whoever is in front */
        if (ddx * dir_dx[ow.dir] + ddy * dir_dy[ow.dir] < -4 * OWQ) d = d * 3;
        if (d < bestd) { bestd = d; best = i; }
    }
    ow.near_npc = (int16_t)best;
    if (best < 0) {
        int32_t pd = (int32_t)(40 * OWQ) * (40 * OWQ);
        for (i = 0; i < ow_prop_count; i++) {
            const OwProp *p = &ow_props[i];
            int32_t ddx, ddy, d;
            if (p->type != THING_PROP_SIGN && p->type != THING_PROP_FLOWERS &&
                p->type != THING_PROP_MAILBOX && p->type != THING_PROP_FOUNTAIN) continue;
            ddx = (int32_t)p->x * OWQ - ow.px;
            ddy = (int32_t)p->y * OWQ - ow.py;
            if (ddx > 80 * OWQ || ddx < -80 * OWQ || ddy > 80 * OWQ || ddy < -80 * OWQ) continue;
            d = ddx * ddx + ddy * ddy;
            if (p->type == THING_PROP_FOUNTAIN) d /= 3;
            if (d < pd) { pd = d; ow.near_prop = (int16_t)i; }
        }
    }
    if (!ow.visit) {
        SideSpot sp[8];
        int n = side_spots(ow.map, sp, 8), cx = ow.px / OW_CELL, cy = ow.py / OW_CELL;
        for (i = 0; i < n; i++) {
            int ddx = cx - sp[i].x, ddy = cy - sp[i].y;
            if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1) { ow.near_spot = (int16_t)i; break; }
        }
    }
}

static void interact_prop(int i)
{
    const OwProp *p = &ow_props[i];
    int cx = p->x / DOOMO_CELL_UNITS, cy = p->y / DOOMO_CELL_UNITS;
    switch (p->type) {
    case THING_PROP_SIGN: {
        const char *t = place_sign_text(ow.map, cx, cy);
        story_say(-1, (t && t[0]) ? t : "A cute little sign. Somebody drew a heart on it.");
        break; }
    case THING_PROP_FLOWERS:
        story_pick_flowers(ow.map, cx, cy);
        break;
    case THING_PROP_MAILBOX:
        story_say(-1, ow.map == MAPID_HAHALAND && cx == 5 && cy == 7 ?
                  "Your mailbox! A letter: \"Welcome, neighbour! Love, everyone.\" Aww." :
                  "Somebody else's mail. Snooping is not cute. (It's a catalogue for hats.)");
        break;
    case THING_PROP_FOUNTAIN:
        audio_sfx(SFX_SPLASH);
        story_say(-1, ow.map == MAPID_HAHALAND_INVADED ? "The fountain bubbles red and growls at you." :
                  "The fountain giggles. You toss in a coin and make a wish. It giggles harder.");
        break;
    default: break;
    }
}

int ow_player_facing(void) { return ow.dir; }      /* S2: the photo verb */

static void press_a(void)
{
    if (!ow.visit && story_verbs_press_a()) return; /* S2: the photo verb */
    if (ow.near_npc >= 0) {
        OwNpc *n = &ow_npcs[ow.near_npc];
        ownpc_face_player(ow.near_npc);
        n->state = NS_TALK;
        n->timer = 60;
        audio_sfx(SFX_BLIP);
        if (ow.visit) talk_ambient(n->id);
        else {
            const DoomoResident *r = town_res(n->id);
            if (r && r->status == RES_POSSESSED && !story_debug_autowin_on()) {
                /* the main plot / side quests may want this one */
                int b = story_beat();
                story_interact_npc(n->id);
                if (!story_talking() && story_beat() == b) talk_ambient(n->id);
            } else {
                story_interact_npc(n->id);
            }
        }
        return;
    }
    if (ow.near_spot >= 0 && !ow.visit) {
        side_trigger_spot(ow.map, ow.px / OW_CELL, ow.py / OW_CELL, 1);
        return;
    }
    if (ow.near_door) { enter_door(ow.near_door); return; }
    if (ow.near_prop >= 0) { interact_prop(ow.near_prop); return; }
}

/* ---- S1 M5: the camera field (pan, creep, brake; zoom through camfx) ---- */
void ow_cam(int mode, int arg)
{
    OwCam *c;
    if (!ow_scratch_mine()) return;
    c = &ow_scr->cam;
    switch (mode) {
    case CAM_PAN_TO:
        c->mode = CAM_PAN_TO;
        c->pan_t = 0;
        c->fx = ow.camx; c->fy = ow.camy;
        c->tx = (int32_t)(arg & 0xFF) * OW_CELL + OW_CELL / 2;
        c->ty = (int32_t)((arg >> 8) & 0xFF) * OW_CELL + OW_CELL / 2;
        break;
    case CAM_FOLLOW:
        c->mode = CAM_FOLLOW;
        c->creep_q8 = 0;
        c->creep = 0;
        break;
    case CAM_BRAKE:
        camfx_shake(SHK_BRAKE);
        tw_spring_punch(&c->brake, (int32_t)(arg ? arg : 6) * 16);
        break;
    case CAM_CREEP: c->creep_q8 = (int16_t)arg; break;
    case CAM_ZOOM:
        if (arg == 256 || arg <= 0) camfx_zoom_snap(256, 0);
        else { camfx_zoom_snap(arg, RUNG_R13); camfx_flash(PAL_WHITE, 10, 2); audio_sfx_synth(SFX_WHOOSH); }
        break;
    case CAM_RESET:
    default:
        c->mode = CAM_FOLLOW;
        c->creep_q8 = 0;
        c->creep = 0;
        tw_spring_init(&c->brake, TW_BRAKE, 0);
        camfx_zoom_snap(256, 0);
        break;
    }
}

void ow_cam_offset(int32_t *dx, int32_t *dy)
{
    *dx = *dy = 0;
    if (!ow_scratch_mine()) return;
    *dx = ow_scr->cam.brake.e * (OWQ / 16) + (ow_scr->cam.creep >> 8) * OWQ;
}

static void camera_update(void)
{
    int32_t tx = ow.px, ty = ow.py - 8 * OWQ;
    if (ow_scratch_mine()) {
        OwCam *c = &ow_scr->cam;
        tw_spring_step(&c->brake);
        c->creep += c->creep_q8;
        if (c->mode == CAM_PAN_TO) {
            /* a comic camera sits, then moves quickly: 12 frames of smoothstep */
            fix16 k;
            if (c->pan_t < 12) c->pan_t++;
            k = tw_smooth(tw_t(c->pan_t, 12));
            ow.camx = tw_lerp(c->fx, c->tx, k);
            ow.camy = tw_lerp(c->fy, c->ty, k);
            return;
        }
    }
    int32_t maxx = (int32_t)doomo_map.w * OW_CELL, maxy = (int32_t)doomo_map.h * OW_CELL;
    int32_t hw = (int32_t)plat_screen_w() * 4 * OWQ / 2, hh = (int32_t)plat_screen_h() * 4 * OWQ / 2;
    ow.camx += (tx - ow.camx) / 4;
    ow.camy += (ty - ow.camy) / 4;
    if (maxx > hw * 2) ow.camx = DOOMO_CLAMP(ow.camx, hw, maxx - hw);
    if (maxy > hh * 2) ow.camy = DOOMO_CLAMP(ow.camy, hh, maxy - hh);
}

static void pet_update(void)
{
    int k = (ow.trail_i + 16 - 6) & 15;
    int32_t tx = ow.trail_x[k], ty = ow.trail_y[k];
    ow.petx += (tx - ow.petx) / 5;
    ow.pety += (ty - ow.pety) / 5;
}

/* ---- pause panel ------------------------------------------------------------------- */
enum { PM_RESUME, PM_LOG, PM_PET, PM_ARMORY, PM_MAP, PM_OPTIONS, PM_SAVE, PM_TITLE, PM_COUNT };
const char *const ow_pause_names[PM_COUNT] = {
    "Resume", "Quest Log", "Doomogachi", "Armory", "Map", "Options", "Save", "Title Screen"
};
int ow_pause_count(void) { return PM_COUNT; }

static void title_yes(int yes)
{
    if (yes) { ow_sync_to_save(); scene_set(&SCENE_TITLE, 0); }
}

static void pause_activate(int sel)
{
    switch (sel) {
    case PM_RESUME: ow.pause = 0; break;
    case PM_LOG: ow.pause = 0; story_push_scene(&SCENE_QUESTLOG, 0, PEND_OTHER, CTX_FREE, 0); break;
    case PM_PET:
        if (!pet_exists()) { ui_toast("No Doomogachi yet!"); audio_sfx(SFX_ERROR); break; }
        ow.pause = 0;
        story_push_scene(&SCENE_PET, 0, PEND_OTHER, CTX_FREE, 0);
        break;
    case PM_ARMORY:
        if (!story_flag(SF_WEAPON_CHOSEN)) { ui_toast("Rivet hasn't welded you anything yet!"); audio_sfx(SFX_ERROR); break; }
        ow.pause = 0;
        story_push_scene(&SCENE_WORKSHOP, 0, PEND_OTHER, CTX_FREE, 0);
        break;
    case PM_MAP: ow.pause = 0; story_push_scene(&SCENE_QUESTLOG, 3, PEND_OTHER, CTX_FREE, 0); break;
    case PM_OPTIONS: ow.pause = 0; story_push_scene(&SCENE_OPTIONS, 0, PEND_OTHER, CTX_FREE, 0); break;
    case PM_SAVE:
        ow_sync_to_save();
        if (ow.visit) { ui_toast("Strolls aren't saved"); break; }
        if (save_game(0)) { ui_toast(STR_ICON_HEART " Game saved!"); audio_sfx(SFX_SELECT); }
        else { ui_toast(save_last_error()[0] ? save_last_error() : "Couldn't save"); audio_sfx(SFX_ERROR); }
        break;
    case PM_TITLE:
        ui_confirm(ow.visit ? "Leave and go back?" : "Return to the title? Unsaved progress is lost.", title_yes);
        break;
    default: break;
    }
}

static void pause_update(void)
{
    ow.pause_t++;
    if (btn_pressed(BTN_B) || btn_pressed(BTN_START)) { ow.pause = 0; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_UP)) { ow.pause_sel = (uint8_t)((ow.pause_sel + PM_COUNT - 1) % PM_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { ow.pause_sel = (uint8_t)((ow.pause_sel + 1) % PM_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A)) { audio_sfx(SFX_SELECT); pause_activate(ow.pause_sel); }
}

/* ---- update / draw ------------------------------------------------------------------ */
static int music_tick;

static void story_update(void)
{
    ow.t++;
    camfx_tick();                                  /* --- S1 M5 --- */
    ow_scratch_ensure();
    if (story_pending.kind == PEND_CREATOR) {
        const DoomoResident *p = town_res(DOOMO_PLAYER_ID);
        memset(&story_pending, 0, sizeof(story_pending));
        if (!p || p->status == RES_EMPTY || !p->name[0]) { scene_set(&SCENE_TITLE, 0); return; }
        start_story();
        return;
    }
    if (!ow.started) return;
    if (story_pending.kind != PEND_NONE) {
        after_subscene();
        story_resume();
        return;
    }
    if (map_current_id() != ow.map) after_subscene();
    story_vm_pump();                               /* the story VM (S2; SPEC C6 host) */
    story_vm_minute();
    story_poll_callbacks();                        /* conversation callbacks by id */
    story_trigger_poll();
    story_verbs_tick();                            /* S2: escort / delivery */
    story_objective_refresh();                     /* S2: the HUD's resident copy */
    if (scene_top() != &SCENE_STORY) return;
    sim_tick();
    owfx_update();
    if (border_t && --border_t == 0) audio_music(ow_music_for_map(ow.map));   /* on the downbeat */
    if (++music_tick >= 90 && !border_t && !audio_fx_music_gated()) {
        music_tick = 0;
        if (audio_music_current() != ow_music_for_map(ow.map)) audio_music(ow_music_for_map(ow.map));
    }
    if (ow_beat_live()) ow_beat_step();            /* --- S1 M5: the overworld hosts a beat --- */
    if (!ow.visit && auto_events()) return;
    if (ow.pause) { pause_update(); return; }
    if (btn_pressed(BTN_START)) {
        ow.pause = 1;
        ow.pause_t = 0;
        ow.pause_sel = 0;
        audio_sfx(SFX_SELECT);
        return;
    }
    if (btn_pressed(BTN_SELECT)) { ow.minimap ^= 1; audio_sfx(SFX_CURSOR); }
    player_update();
    if (scene_top() != &SCENE_STORY) return;       /* a door opened a scene */
    pet_update();
    ownpc_update();
    ownpc_acts_update();                           /* --- S1 M5 --- */
    camera_update();
    find_interactables();
    if (!ow.visit) {
        ownpc_double_take_check();
        ow_spawn_update();
    }
    if (btn_pressed(BTN_A)) { press_a(); return; }
    /* chapter 3: walking into a demon nest */
    if (!ow.visit && ow.map == MAPID_HAHALAND_INVADED && story_beat() == BEAT_C3_NESTS && !story_talking()) {
        int nest = ownpc_nest_near(ow.px, ow.py);
        if (nest >= 0) {
            ow_shake(8);
            ow_flash(PAL_UI_RED, 4);
            script_nest_encounter(nest);
        }
    }
}

static int32_t view_shake(int k)
{
    if (!ow.shake) return 0;
    return (int32_t)(((ow.t * (k ? 7 : 13)) % 7) - 3) * OWQ * 2;
}

static void add_bb(int32_t x, int32_t y, int z, const DoomoImage *img, int flags, int light)
{
    DoomoBillboard *b;
    if (!img || ow_bb_count >= OW_BB_MAX) return;
    b = &ow_bb[ow_bb_count++];
    b->x = INT2FIX(x / OWQ) + (fix16)((x % OWQ) << 12);
    b->y = INT2FIX(y / OWQ) + (fix16)((y % OWQ) << 12);
    b->z = INT2FIX(z);
    b->img = img;
    b->scale = 256;
    b->flags = (uint8_t)flags;
    b->light = (uint8_t)light;
}

void ow_add_billboard(int32_t x, int32_t y, int z, const DoomoImage *img, int flags, int light)
{
    add_bb(x, y, z, img, flags, light);
}

static void build_billboards(void)
{
    int32_t vx0 = ow.camx - 640 * OWQ, vx1 = ow.camx + 640 * OWQ;
    int32_t vy0 = ow.camy - 700 * OWQ, vy1 = ow.camy + 520 * OWQ;
    int i, cx, cy, z, frame;
    uint32_t t = doomo_frame_count();
    ow_bb_count = 0;
    /* props */
    for (i = 0; i < ow_prop_count; i++) {
        const OwProp *p = &ow_props[i];
        int32_t x = (int32_t)p->x * OWQ, y = (int32_t)p->y * OWQ;
        int spr, fl = BB_SHADOW;
        if (x < vx0 || x > vx1 || y < vy0 || y > vy1) continue;
        switch (p->type) {
        case THING_PROP_TREE: spr = SPR_TREE; break;
        case THING_PROP_LAMP: spr = SPR_LAMP; break;
        case THING_PROP_BENCH: spr = SPR_BENCH; break;
        case THING_PROP_FOUNTAIN: spr = SPR_FOUNTAIN; break;
        case THING_PROP_BARREL: spr = SPR_BARREL; break;
        case THING_PROP_SKULLS: spr = SPR_SKULL_PILE; break;
        case THING_PROP_TORCH: spr = SPR_TORCH; fl = BB_FULLBRIGHT; break;
        case THING_PROP_SIGN: spr = SPR_SIGN; break;
        case THING_PROP_FLOWERS: spr = SPR_FLOWER_BED; fl = 0; break;
        case THING_PROP_MAILBOX: spr = SPR_MAILBOX; break;
        case THING_VAPE_PORTAL: spr = SPR_VAPE_PORTAL; fl = BB_FULLBRIGHT; break;
        default: continue;
        }
        cx = p->x / DOOMO_CELL_UNITS;
        cy = p->y / DOOMO_CELL_UNITS;
        z = ow_floor_h(cx, cy) * DOOMO_HEIGHT_STEP;
        frame = sprite_frames(spr) > 1 ? (int)((t / 5 + (uint32_t)i) % (uint32_t)sprite_frames(spr)) : 0;
        add_bb(x, y + 8 * OWQ, z, sprite_image(spr, frame), fl, 0);
    }
    ownpc_add_billboards(vx0, vy0, vx1, vy1);
    /* side quest spots */
    if (!ow.visit) {
        SideSpot sp[8];
        int n = side_spots(ow.map, sp, 8);
        for (i = 0; i < n; i++) {
            int32_t x = (int32_t)sp[i].x * OW_CELL + OW_CELL / 2, y = (int32_t)sp[i].y * OW_CELL + OW_CELL / 2;
            z = ow_floor_h(sp[i].x, sp[i].y) * DOOMO_HEIGHT_STEP + 8 + (int)(ui_wave((int)t, 30, 6));
            add_bb(x, y, z, sprite_image(SPR_SPARKLE, (int)(t / 3) & 3), BB_FULLBRIGHT, 4);
            add_bb(x, y, z + 40, owfx_marker(NPCF_SIDE, (int)t), BB_FULLBRIGHT, 2);
        }
    }
    /* the pet */
    if (!ow.visit) {
        const DoomoImage *pimg = NULL;
        if (pet_exists()) pimg = sprite_image(SPR_PET, (int)(t / 6) & 3);
        else if (inv_count(ITEM_DOOMOGACHI_EGG)) pimg = sprite_image(SPR_EGG, (int)(t / 12) & 1);
        if (pimg) {
            cx = ow.petx / OW_CELL;
            cy = ow.pety / OW_CELL;
            z = ow_floor_h(cx, cy) * DOOMO_HEIGHT_STEP;
            if (ow.moving) z += (int)(((t * 2) & 7) < 4 ? ((t * 2) & 7) : 8 - ((t * 2) & 7)) * 3;
            add_bb(ow.petx, ow.pety + 2 * OWQ, z, pimg, BB_SHADOW | (ow.petx > ow.px ? BB_FLIPX : 0), 0);
        }
    }
    /* the player */
    {
        int f = ow.moving ? (int)((ow.anim / (ow.running ? 3 : 5)) & 3) : 0;
        cx = ow.px / OW_CELL;
        cy = ow.py / OW_CELL;
        z = ow_floor_h(cx, cy) * DOOMO_HEIGHT_STEP;
        if (!ow.moving && ((t / 20) & 1)) z += 2;        /* breathing */
        add_bb(ow.px, ow.py, z, bodyspr_image(DOOMO_PLAYER_ID, ow.dir, f), BB_SHADOW, 1);
    }
}

void ow_view_fill(DoomoOverworldView *v)
{
    int b = story_beat(), lvl = 0, tint = TINT_NONE;
    memset(v, 0, sizeof(*v));
    v->map = &doomo_map;
    {
        int32_t ox, oy;
        ow_cam_offset(&ox, &oy);                   /* --- S1 M5: brake + creep --- */
        v->cx = INT2FIX((ow.camx + ox + view_shake(0)) / OWQ);
        v->cy = INT2FIX((ow.camy + oy + view_shake(1)) / OWQ);
    }
    v->vx = 0;
    v->vy = 0;
    v->vw = (int16_t)plat_screen_w();
    v->vh = (int16_t)plat_screen_h();
    v->sprites = ow_bb;
    v->sprite_count = ow_bb_count;
    v->daylight = (uint8_t)owfx_daylight();
    v->time = (uint16_t)doomo_frame_count();
    if (ow.map == MAPID_HAHALAND_INVADED) {
        tint = TINT_RED;
        lvl = 2 + (ui_wave((int)doomo_frame_count(), 90, 2) + 2) / 3;
    } else if (ow.map == MAPID_HELLVILE) {
        tint = TINT_PURPLE;
        lvl = 4;
    } else if (ow.map == MAPID_HAHALAND && !ow.visit) {
        /* the purple fog leaks over the Wall in chapters 1-2 */
        int cx = ow.px / OW_CELL;
        if (b < BEAT_C3_ERUPTION && cx >= 44) { tint = TINT_PURPLE; lvl = (cx - 42) / 2 + (b >= BEAT_C2_VAPOR ? 1 : 0); }
        else if (b >= BEAT_C2_VAPOR && b < BEAT_C3_ERUPTION) { tint = TINT_PURPLE; lvl = 1; }
    }
    v->tint = (uint8_t)tint;
    v->tint_level = (uint8_t)DOOMO_CLAMP(lvl, 0, 8);
}

static void story_draw(void)
{
    DoomoOverworldView v;
    int sw = plat_screen_w(), sh = plat_screen_h();
    if (!ow.started) { plat_clear(PAL_UI_PINK); return; }
    /* a scene popped this tick may have taken the scratch (before the
     * draw-skip test, so every platform rebuilds on the same tick) */
    ow_scratch_ensure();
    /* timers run with the picture, also under dialogue boxes */
    if (ow.fade) ow.fade--;
    if (ow.flash) ow.flash--;
    if (ow.shake) ow.shake--;
    if (ow.banner_t) ow.banner_t--;
    if (!ow.visit && ui_dialog_active()) ow_spawn_under_dialog();   /* --- S1 M5: "beep." --- */
    if (plat_draw_skipped()) return;
    build_billboards();
    ow_view_fill(&v);
    plat_overworld(&v);
    owfx_draw_world();
    if (!ow.visit) ow_spawn_draw();                /* --- S1 M5 --- */
    camfx_apply();                                 /* --- S1 M5: the world shakes, the HUD does not (PS1) --- */
    owfx_draw_hud();
    if (ow.pause) owfx_draw_pause();
    if (ow.flash) plat_checker(0, 0, sw, sh, ow.flash_col);
    if (ow.fade) plat_shade_rect(0, 0, sw, sh, DOOMO_CLAMP(15 - ow.fade, 0, 15));
}

const DoomoScene SCENE_STORY = {
    "STORY", story_enter, story_leave, story_update, story_draw, 0
};

/* ---- S1 M5: the overworld as a beat host (SPEC C6). The scheduler is
 * resident; the host kinds come back here: NPC / ACT acts, CAM moves, the
 * SAY / CAPTION lines (decoded from the packs into the dialogue queue) and
 * the iris around the player. ------------------------------------------------ */
static uint8_t ow_beat_on;

void ow_run_beat(const BeatScript *s)
{
    beat_start(s);
    ow_beat_on = beat_running() ? 1 : 0;
    ow_spawn_quiet(150);
}

int ow_beat_live(void) { return ow_beat_on; }

static void ow_beat_event(const BeatEv *e)
{
    char line[200];
    switch (e->op) {
    case BOP_NPC: ownpc_act(e->a, e->b & 0xFF, (e->b >> 8) & 0xFF); break;
    case BOP_ACT: ownpc_act(e->a, e->b, 0); break;
    case BOP_CAM: ow_cam(e->a, e->b); break;
    case BOP_SAY: case BOP_CAPTION:
        if (txt_get((uint16_t)e->b, line, (int)sizeof(line)) >= 0)
            ui_say_ex(e->op == BOP_SAY ? e->a : -1, EXPR_NEUTRAL, 0, line);
        break;
    case BOP_IRIS_OUT: case BOP_IRIS_IN: {
        int sx, sy;
        owfx_world_to_screen(ow.px, ow.py, &sx, &sy);
        if (e->op == BOP_IRIS_OUT) camfx_iris_out(sx, sy - 12, 18 + ((e->b >> 7) & 63) * 2, (e->b >> 6) & 1, e->b & 63);
        else camfx_iris_in(sx, sy - 12, 18, (e->b >> 6) & 1, e->b & 63);
        break; }
    case BOP_EXPR: case BOP_POP: case BOP_SHOT: case BOP_CUT: case BOP_INSERT: case BOP_TWITCH:
    case BOP_TINT: case BOP_HEADTURN:
        if (e->op == BOP_HEADTURN) ownpc_act(e->a, NACT_HEAD_TURN, e->b);
        if (e->op == BOP_TWITCH) ownpc_act(e->a, e->b ? NACT_TWITCH : NACT_THAW, 0);
        break;
    default: break;
    }
}

void ow_beat_step(void)
{
    int r, guard = 32;
    if (btn_pressed(BTN_A)) beat_press_a();
    do {
        r = beat_tick();
        if (r == BT_EXT) ow_beat_event(beat_ext_event());
    } while (r == BT_EXT && guard-- > 0);
    if (r == BT_DONE) { ow_beat_on = 0; ow_spawn_quiet(300); }
}
