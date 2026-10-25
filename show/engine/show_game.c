/* show_game.c - doomo_init / doomo_frame for the show: boot, the episode
 * index, the shot loop and the mailbox (owner: ENGINE lane 1; spec 4.1, 8.1).
 *
 * ps1/src/main.c is linked unchanged: plat_init_hw(); doomo_init(); then per
 * tick hcmd_poll(), pad_read(), plat_begin_frame(), doomo_frame(b),
 * plat_end_frame(). This file also stands in for everything game.c and
 * scene.c would define, so neither (nor any scene) is linked:
 *   doomo_frame_count, doomo_initialized, doomo_image_new_uid (the PS1 VRAM
 *   image cache keys on it), and no-op scene_set / scene_push / scene_top /
 *   scene_by_name (hcmd.c's weak references to them do NOT resolve to NULL
 *   at link time: measured, spec 15.1).
 *
 * The tick (show_api.h has the full call order for modules):
 *   1. a seek request from Lua (g_show_mbox.req_shot/req_frame) binds that
 *      shot and pre-rolls its logic to the requested frame, drawing skipped;
 *      otherwise the next frame (the next shot after the last frame);
 *   2. logic: camfx_tick, beat_tick (BT_EXT -> show_ext), module ticks;
 *   3. draw: planes, actors, crowds, FX, text-under, tint, camfx_apply,
 *      text, HUD, matte bars;
 *   4. P1, P4 and stats to g_show_mbox.
 * With no episode on the disc the player draws the test chart (AT2.1).
 * Nothing here reads a wall clock: every seed is the episode's and shot's.
 */
#include <stdio.h>
#include <string.h>
#include "doomo/scene.h"
#include "show_core.h"

ShowShot g_show_shot = { -1, NULL, NULL, 0, 1, 0, 0 };
int g_show_drawing;

static uint32_t s_frames;
static uint8_t  s_inited;
static int      s_state = SHOW_ST_BOOT;
static int      s_last;                 /* last shot to play             */
static BeatScript s_beat;

int  show_mbox_take_seek(int *shot, int *frame);
void show_mbox_publish(int shot, int frame, uint32_t ep_frame, const char *id, uint32_t p1);
void show_ext_draw_tint(void);
const ShwShotHdr *show_chunk_hdr(void);

uint32_t doomo_image_new_uid(void)
{
    /* game.c's scheme: uids below 0x01000000 are free for const images */
    static uint32_t next = 0x01000000u;
    if (++next == 0) next = 0x01000000u;
    return next;
}

uint32_t doomo_frame_count(void) { return s_frames; }
int doomo_initialized(void) { return s_inited; }

/* the four scene entry points hcmd.c references (no scenes in the show) */
void scene_set(const DoomoScene *s, int arg) { (void)s; (void)arg; }
void scene_push(const DoomoScene *s, int arg) { (void)s; (void)arg; }
const DoomoScene *scene_top(void) { return 0; }
const DoomoScene *scene_by_name(const char *name) { (void)name; return 0; }

/* the shake is a camera offset in the show (spec 2.3): camfx_apply's whole-
 * frame draw offset is switched off (-Wl,--wrap=plat_post_offset), so the
 * matte bars and the frame edge never move; show_cam applies camfx_offset()
 * to the planes instead */
void __wrap_plat_post_offset(int dx, int dy);
void __wrap_plat_post_offset(int dx, int dy) { (void)dx; (void)dy; }

const ShowShot *show_shot(void) { return &g_show_shot; }
int show_drawing(void) { return g_show_drawing; }
uint32_t show_episode_frame(void)
{
    return g_show_shot.ix ? g_show_shot.ix->start_frame + (uint32_t)(g_show_shot.t < 0 ? 0 : g_show_shot.t) : 0;
}

#if SHOW_RAM_KB > 2048
/* M0-T5: does the machine have 8 MB? On 2 MB the bus mirrors every 2 MB, so a
 * write 2 MB above a word lands on the word itself; on 8 MB it does not. */
static volatile uint32_t s_ram_probe;
static void show_ram_probe(void)
{
    volatile uint32_t *hi = (volatile uint32_t *)((uintptr_t)&s_ram_probe + 0x200000u);
    uint32_t keep = *hi;
    s_ram_probe = 0x11111111u;
    *hi = 0x5A5A5A5Au;
    printf("show: ram probe %s (word %08x, +2MB %08x)\n",
           s_ram_probe == 0x5A5A5A5Au ? "2MB-mirrored" : "8MB", (unsigned)s_ram_probe, (unsigned)*hi);
    *hi = keep;
}
#endif

void doomo_init(void)
{
#if SHOW_RAM_KB > 2048
    show_ram_probe();
#endif
    printf("show: init (show.exe %s, canvas %dx%d at y %d, api %d, %d modules)\n",
           SHOW_BUILD_ID, SHOW_CANVAS_W, SHOW_CANVAS_H, SHOW_CANVAS_Y, SHOW_API_VERSION,
           show_module_count());
    palette_init();
    textures_init();
    sprites_init();
    rng_seed(&doomo_rng, SHOW_SEED);
    opt_defaults();
    show_chart_init();
#ifdef SHOW_SELFTEST
    show_selftest_init();
#endif
    s_frames = 0;
    s_inited = 1;
}

/* ---- the shot ---------------------------------------------------------- */
static void logic_step(int t)
{
    int r, guard = 256, i, n = show_module_count();
    g_show_shot.t = t;
    camfx_tick();
    do {
        r = beat_tick();
        if (r == BT_EXT) show_ext_event(beat_ext_event());
    } while (r == BT_EXT && guard-- > 0);
    for (i = 0; i < n; i++) {
        const ShowModule *m = show_module(i);
        if (m->tick) m->tick(t);
    }
}

/* bind a shot and run its logic up to (not including) `frame`; returns the
 * frame to make next, or -1 */
static int bind_shot(int shot, int frame)
{
    const ShwEpHeader *eh = show_episode();
    const ShwShotIndex *ix = show_shot_index(shot);
    const ShwShotHdr *hd;
    const BeatEv *ev;
    int i, n, nev, f;
    if (ix) {                    /* a load error names this shot, frame 0 (AT3.4) */
        g_show_shot.ix = ix;
        g_show_shot.index = shot;
        g_show_shot.t = 0;
    }
    if (!ix || !show_load_chunk(shot)) {
        s_state = SHOW_ST_ERROR;
        g_show_mbox.state = SHOW_ST_ERROR;
        return -1;
    }
    hd = show_chunk_hdr();
    g_show_shot.index = shot;
    g_show_shot.ix = ix;
    g_show_shot.hdr = hd;
    g_show_shot.len = ix->len;
    g_show_shot.px = ix->px ? ix->px : 1;
    g_show_shot.seed = eh->seed ^ ix->seed;
    g_show_shot.t = -(int)ix->preroll;
    g_show_drawing = 0;

    show_purity_reset();
    show_images_prep();
    show_stage_bind();
    show_stage_prep();
    n = show_module_count();
    for (i = 0; i < n; i++) {
        const ShowModule *m = show_module(i);
        if (m->prep) m->prep(&g_show_shot);
    }
    ev = (const BeatEv *)show_chunk_find(SHWR_BEAT, &nev, NULL);
    s_beat.ev = ev;
    s_beat.n = (uint16_t)(ev ? nev : 0);
    s_beat.nlabels = hd->nlabels;
    if (ev && nev > 0) {
        if (!beat_script_valid(&s_beat)) show_error("SHW-BEAT", "beat script not valid (sorted? end?)");
        beat_start(&s_beat);
    } else {
        beat_start(NULL);
    }
    /* pre-roll (base=/replay= resolved by the compiler) and the seek frames */
    if (frame >= ix->len) frame = ix->len - 1;
    if (frame < 0) frame = 0;
    for (f = -(int)ix->preroll; f < frame; f++) logic_step(f);
    return frame;
}

static void draw_layer(int layer, int t)
{
    int i, n = show_module_count();
    for (i = 0; i < n; i++) {
        const ShowModule *m = show_module(i);
        if (m->draw) m->draw(layer, t);
    }
}

static void matte(void)
{
    plat_rect(0, 0, SHOW_CANVAS_W, SHOW_CANVAS_Y, PAL_BLACK);
    plat_rect(0, SHOW_CANVAS_Y + SHOW_CANVAS_H, SHOW_CANVAS_W,
              240 - SHOW_CANVAS_Y - SHOW_CANVAS_H, PAL_BLACK);
}

static void draw_frame(int t)
{
    g_show_drawing = 1;
    plat_clear(g_show_shot.hdr ? g_show_shot.hdr->bg : PAL_BLACK);
    show_stage_draw(t);
    draw_layer(SHOW_L_PLANES, t);
    draw_layer(SHOW_L_ACTORS, t);
    draw_layer(SHOW_L_CROWDS, t);
    draw_layer(SHOW_L_FX, t);
    draw_layer(SHOW_L_TEXT_UNDER, t);
    show_ext_draw_tint();
    camfx_apply();
    draw_layer(SHOW_L_TEXT, t);
    draw_layer(SHOW_L_HUD, t);
    matte();
}

static void boot_episode(void)
{
    const ShwEpHeader *eh;
    if (!show_load_episode(g_show_mbox.req_episode)) {
        s_state = SHOW_ST_CHART;
    } else {
        eh = show_episode();
        g_show_mbox.nshots = eh->nshots;
        g_show_mbox.total_frames = eh->total_frames;
        s_state = SHOW_ST_PLAY;
        g_show_shot.index = -1;
        if (g_show_mbox.req_shot < 0) {         /* nobody steering: play from 0 */
            g_show_mbox.req_shot = 0;
            g_show_mbox.req_frame = 0;
        }
    }
    g_show_mbox.state = (uint32_t)s_state;
}

static int s_next;                       /* the shot frame this tick makes */

static void play_tick(void)
{
    int shot, frame;
    const ShwEpHeader *eh = show_episode();
    if (show_mbox_take_seek(&shot, &frame)) {
        s_last = g_show_mbox.req_last_shot >= 0 ? g_show_mbox.req_last_shot : eh->nshots - 1;
        s_state = SHOW_ST_PLAY;
        if ((s_next = bind_shot(shot, frame)) < 0) return;
    } else if (s_state == SHOW_ST_PLAY && s_next >= g_show_shot.len) {
        if (g_show_shot.index + 1 > s_last || g_show_shot.index + 1 >= eh->nshots)
            s_state = SHOW_ST_DONE;
        else if ((s_next = bind_shot(g_show_shot.index + 1, 0)) < 0)
            return;
    }
    g_show_mbox.state = (uint32_t)s_state;
    if (s_state != SHOW_ST_PLAY) {
        g_show_drawing = 0;
        plat_clear(PAL_BLACK);
        show_mbox_publish(-1, -1, 0, NULL, 0);
        return;
    }
    show_p4_reset_frame();
    logic_step(s_next);
    show_p4_fire_due(s_next);
    show_stats_mark_update();
    draw_frame(s_next);
    show_stats_mark_draw();
    show_mbox_publish(g_show_shot.index, s_next, g_show_shot.ix->start_frame + (uint32_t)s_next,
                      g_show_shot.ix->id, show_p1_frame());
    g_show_drawing = 0;
    s_next++;
}

void doomo_frame(uint32_t buttons)
{
    (void)buttons;              /* the show is never steered by the pad */
    s_frames++;
#ifdef SHOW_SELFTEST
    if (show_selftest_frame(s_frames))
        return;
#endif
    show_stats_begin_frame();
    if (s_state == SHOW_ST_BOOT) boot_episode();
    if (s_state == SHOW_ST_CHART) {
        show_chart_draw(s_frames);
        return;
    }
    play_tick();
}
