/* test_ow_script.c - overworld NPC / camera scripting, the border record
 * scratch, the running-gag spawner and the wrongness table (S1 M5; SPEC
 * sec.7 S1, SF sec.15.8, CT sec.6.3). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"
#include "doomo/txt.h"
#include "../src/overworld_int.h"
#include "../src/audio_int.h"

static void frame(uint32_t b)
{
    plat_begin_frame();
    doomo_frame(b);
    plat_end_frame();
}

/* a story overworld on a quiet beat, dialogue closed */
static void on_map(int beat)
{
    int i;
    test_reset();
    scene_set(&SCENE_STORY, STORY_ARG_DEBUG_BEAT + beat);
    for (i = 0; i < 400 && scene_top() != &SCENE_STORY; i++) frame(i & 1 ? BTN_A : 0);
    for (i = 0; i < 10; i++) frame(0);
}

static int ambient(void)
{
    int i;
    for (i = 0; i < ow_npc_count; i++)
        if (ow_npcs[i].kind == NK_RESIDENT && !(ow_npcs[i].flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY | NPCF_FIXED)))
            return i;
    return -1;
}

TEST(ow_script_walk_to_and_freeze)
{
    int k, i, id, cx, cy;
    int32_t x0, y0;
    on_map(BEAT_C1_FRIENDS);
    CHECK(scene_top() == &SCENE_STORY);
    k = ambient();
    CHECK(k >= 0);
    id = ow_npcs[k].id;
    cx = ow_npcs[k].x / OW_CELL;
    cy = ow_npcs[k].y / OW_CELL;
    /* FREEZE: an ambient citizen stops wandering */
    CHECK(ownpc_act(id, NACT_FREEZE, 0));
    k = ownpc_find(id);
    x0 = ow_npcs[k].x; y0 = ow_npcs[k].y;
    for (i = 0; i < 300; i++) frame(0);
    k = ownpc_find(id);
    CHECK(k >= 0);
    CHECK(ow_npcs[k].x == x0 && ow_npcs[k].y == y0);
    CHECK_EQ(ownpc_clock(&ow_npcs[k]), 0);
    ownpc_act(id, NACT_THAW, 0);
    /* WALK_TO a neighbouring walkable cell */
    {
        int tx = -1, ty = -1, dx, dy;
        for (dy = -2; dy <= 2 && tx < 0; dy++)
            for (dx = -2; dx <= 2; dx++)
                if ((dx || dy) && ow_walkable_cell(cx + dx, cy + dy, ow_floor_h(cx, cy)) &&
                    (dx == 0 || dy == 0)) { tx = cx + dx; ty = cy + dy; break; }
        CHECK(tx >= 0);
        CHECK(ownpc_act(id, NACT_WALK_TO, tx | (ty << 8)));
        CHECK(ownpc_acting(id));
        for (i = 0; i < 240 && ownpc_acting(id); i++) frame(0);
        k = ownpc_find(id);
        CHECK(!ownpc_acting(id));
        CHECK_EQ(ow_npcs[k].x / OW_CELL, tx);
        CHECK_EQ(ow_npcs[k].y / OW_CELL, ty);
    }
}

TEST(ow_script_ripple_and_double_take)
{
    int k, i, staggered = 0;
    on_map(BEAT_C1_FRIENDS);
    k = ambient();
    CHECK(k >= 0);
    CHECK(ownpc_act(ow_npcs[k].id, NACT_RIPPLE, 30));
    CHECK_EQ(ow_npcs[k].emote_t, 60);              /* the first bangs at once */
    for (i = 0; i < ow_npc_count; i++)
        if (i != k && ow_npcs[i].emote_t > 60) {
            CHECK_EQ((ow_npcs[i].emote_t - 60) % 3, 0);  /* 3 frames apart */
            staggered++;
        }
    CHECK(staggered >= 1);
    /* the double-take: R3, the head turn, then "!" + tik */
    k = ambient();
    ownpc_act(ow_npcs[k].id, NACT_THAW, 0);
    CHECK(ownpc_act(ow_npcs[k].id, NACT_DOUBLE_TAKE, 0));
    for (i = 0; i < 6; i++) frame(0);
    k = ownpc_find(ow_npcs[k].id);
    CHECK(ow_npcs[k].emote_f == EMO_BANG);
}

TEST(ow_script_camera_pans_in_12_frames)
{
    int i, cx, cy;
    on_map(BEAT_C1_FRIENDS);
    cx = (int)(ow.px / OW_CELL) + 3;
    cy = (int)(ow.py / OW_CELL) + 2;
    ow_cam(CAM_PAN_TO, cx | (cy << 8));
    for (i = 0; i < 12; i++) frame(0);
    CHECK_EQ(ow.camx, (int32_t)cx * OW_CELL + OW_CELL / 2);
    CHECK_EQ(ow.camy, (int32_t)cy * OW_CELL + OW_CELL / 2);
    ow_cam(CAM_BRAKE, 8);
    {
        int32_t dx, dy;
        frame(0);
        ow_cam_offset(&dx, &dy);
        CHECK(dx != 0);                             /* the brake overshoots */
        for (i = 0; i < 60; i++) frame(0);
        ow_cam_offset(&dx, &dy);
        CHECK_EQ(dx, 0);                            /* and settles */
    }
    ow_cam(CAM_FOLLOW, 0);
    for (i = 0; i < 40; i++) frame(0);
    CHECK(ow.camx > ow.px - 4 * OW_CELL && ow.camx < ow.px + 4 * OW_CELL);
}

TEST(ow_script_border_record_scratch)
{
    int i, gated = 0;
    on_map(BEAT_C4_GATE);
    if (ow.map != MAPID_HAHALAND) ow_enter_map(MAPID_HAHALAND, 30, 30);
    for (i = 0; i < 5; i++) frame(0);
    ow_enter_map(MAPID_HELLVILE, 3, 27);
    CHECK(audio_music_current() != SONG_HELLVILE);   /* the old song tape-stops */
    for (i = 0; i < 18; i++) { frame(0); if (audio_fx_music_gated()) gated = 1; }
    CHECK(gated);                                     /* 14 frames of silence */
    for (i = 0; i < 4; i++) frame(0);
    CHECK_EQ(audio_music_current(), SONG_HELLVILE);   /* then the theme on the downbeat */
}

TEST(ow_script_spawner_rate_limit_and_wrongness)
{
    static const OwWrongRow rows[1] = {
        { 0, MAPID_HAHALAND, 0, 0, 3, 0x3E, 0, RUNG_R8, SFX_TINK, 0, TXT_NONE, 0 } };
    OwWrongRow row = rows[0];
    int i, fired = -1, gag_frames = 0;
    on_map(BEAT_C1_FRIENDS);
    CHECK(ow.map == MAPID_HAHALAND);
    /* nothing fires before the cadence (>= 20 s) */
    ow_spawn_debug(600);
    for (i = 0; i < 590; i++) { frame(0); if (ow_spawn_debug(-1) > 0) gag_frames++; }
    CHECK_EQ(gag_frames, 0);
    /* a wrongness row under the player: noticed once, R8 late */
    row.x = (uint8_t)(ow.px / OW_CELL);
    row.y = (uint8_t)(ow.py / OW_CELL);
    ow_wrong_rows_override(&row, 1);
    CHECK(!story_flag(SF_WRONG(0)));
    ow_spawn_debug(0);
    frame(0);
    CHECK(story_flag(SF_WRONG(0)));
    ow_spawn_quiet(0);
    for (i = 0; i < 2000; i++) { frame(0); if (fired < 0 && ow_spawn_debug(-1) > 0) fired = i; }
    ow_wrong_rows_override(NULL, 0);
    CHECK(fired >= 590);                              /* the cadence after a wrongness */
    /* never while a dialogue is open */
    for (i = 0; i < 400 && ow_spawn_debug(-1) > 0; i++) frame(0);
    CHECK_EQ(ow_spawn_debug(-1), 0);
    ow_spawn_debug(0);
    ui_say(-1, "A long line that keeps the box open for a little while.");
    for (i = 0; i < 20; i++) frame(0);
    CHECK_EQ(ow_spawn_debug(-1), 0);
}
