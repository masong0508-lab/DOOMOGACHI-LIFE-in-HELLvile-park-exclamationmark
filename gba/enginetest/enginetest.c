/* enginetest.c - ENGINE_TEST=1 build: boots straight into a scripted
 * first-person / third-person walk-through of a showcase map, measures
 * the frame rate and logs it to the mGBA debug log (owner: gba).
 *
 *   make -C gba engine-test  ->  gba/build/doomogachi_et.gba
 *
 * Tour (logged as "ET phase: <name>" so harness scripts can sync):
 *   own showcase map (enginetest/et_map.c): courtyard, path, hall door,
 *   skylight look-up, stairs, mezzanine look-down, third person, low
 *   corridor, hell room with demons, then the overworld view; then the
 *   core's Hell Breach maps (MAPID_VAPE_CELLAR, MAPID_CLOUD_NINE_SPIRE) if
 *   the registry has them: spin, look up/down, third-person orbit.
 * Any key takes manual control: D-pad walk/turn, L/R strafe, hold A +
 * UP/DOWN look, B jump, SELECT first/third person, START resumes the tour.
 * Holding SELECT at boot runs the flash save self-test first.
 */
#include "gba_plat.h"
#include "doomo/doomo.h"
#include "enginetest.h"

void doomo_palette_fallback(void);
extern uint32_t r3d_dbg[4];

/* ------------------------------------------------------------------------ */
typedef struct Key {
    int16_t x8, y8;        /* position in 1/8 cells */
    int16_t angle;         /* 0..4095 (unwrapped allowed) */
    int16_t pitch;
    uint8_t ticks;         /* time to reach this key from the previous */
    uint8_t cam;           /* CAM_* */
    const char *phase;     /* logged when the key starts (NULL = none) */
} Key;

#define C8(c) ((int16_t)((c) * 8))
static const Key tour[] = {
    { 76, 104, 3072, 0, 1, CAM_FIRST_PERSON, "courtyard" },
    { 76, 100, 3072 - 160, 10, 50, CAM_FIRST_PERSON, 0 },
    { 76, 92, 3072 + 200, 0, 50, CAM_FIRST_PERSON, 0 },
    { 76, 64, 3072, 0, 60, CAM_FIRST_PERSON, "path" },
    { 80, 60, 4096, 0, 30, CAM_FIRST_PERSON, 0 },
    { 124, 60, 4096, 0, 50, CAM_FIRST_PERSON, "hall_door" },
    { 152, 60, 4096 - 300, 0, 40, CAM_FIRST_PERSON, "hall" },
    { 156, 48, 4096 - 400, 0, 30, CAM_FIRST_PERSON, 0 },
    { 160, 46, 3072, 220, 40, CAM_FIRST_PERSON, "skylight" },
    { 164, 24, 4096, 0, 30, CAM_FIRST_PERSON, 0 },
    { 186, 22, 4096, 0, 25, CAM_FIRST_PERSON, "stairs" },
    { 224, 24, 4096 + 300, -40, 45, CAM_FIRST_PERSON, 0 },
    { 228, 36, 4096 + 1024 + 300, -170, 45, CAM_FIRST_PERSON, "mezzanine" },
    { 228, 36, 4096 + 1024 + 600, -120, 40, CAM_FIRST_PERSON, 0 },
    { 196, 66, 4096 + 1024 + 900, -20, 30, CAM_THIRD_PERSON, "third_person" },
    { 178, 76, 4096 + 1024 + 400, -20, 45, CAM_THIRD_PERSON, 0 },
    { 180, 82, 4096 + 1024, -20, 30, CAM_THIRD_PERSON, 0 },
    { 180, 100, 4096 + 1024, 0, 30, CAM_FIRST_PERSON, "corridor" },
    { 180, 116, 4096 + 1024, 0, 30, CAM_FIRST_PERSON, 0 },
    { 168, 124, 4096 + 1024 + 200, 0, 30, CAM_FIRST_PERSON, "hell_room" },
    { 156, 128, 4096 + 300, 30, 40, CAM_FIRST_PERSON, 0 },
    { 168, 140, 4096 + 100, 0, 40, CAM_THIRD_PERSON, "hell_third" },
    { 200, 156, 4096 - 500, 0, 50, CAM_THIRD_PERSON, 0 },
    { 200, 156, 4096 - 500, 0, 10, CAM_FIRST_PERSON, "end" },
};
#define TOUR_N ((int)(sizeof(tour) / sizeof(tour[0])))

typedef struct Prop {
    uint8_t x8, y8;        /* 1/8 cells */
    uint8_t spr, height;   /* world units tall */
    int8_t  z;             /* feet above the floor */
    uint8_t flags;
} Prop;

static const Prop props[] = {
    /* courtyard */
    { 100, 28, SPR_TREE, 112, 0, BB_SHADOW }, { 20, 92, SPR_TREE, 104, 0, BB_SHADOW },
    { 108, 116, SPR_TREE, 120, 0, BB_SHADOW }, { 52, 140, SPR_TREE, 112, 0, BB_SHADOW },
    { 68, 60, SPR_LAMP, 72, 0, 0 }, { 68, 28, SPR_LAMP, 72, 0, 0 },
    { 108, 80, SPR_FOUNTAIN, 56, -16, 0 },
    { 36, 84, SPR_FLOWER_BED, 16, 0, 0 }, { 92, 92, SPR_FLOWER_BED, 16, 0, 0 },
    { 60, 132, SPR_MAILBOX, 32, 0, BB_SHADOW }, { 28, 116, SPR_BENCH, 24, 0, BB_SHADOW },
    { 92, 52, SPR_SIGN, 40, 0, BB_SHADOW },
    /* hall */
    { 148, 76, SPR_BARREL, 32, 0, BB_SHADOW }, { 204, 76, SPR_BARREL, 32, 0, BB_SHADOW },
    { 236, 20, SPR_TORCH, 48, 0, BB_FULLBRIGHT }, { 220, 44, SPR_HEALTH_BIG, 16, 0, BB_SHADOW },
    { 172, 28, SPR_AMMO_SHELLS, 14, 0, BB_SHADOW }, { 196, 60, SPR_SOUL_ORB, 16, 40, BB_FULLBRIGHT },
    { 228, 28, SPR_KEY_RED, 14, 0, BB_FULLBRIGHT },
    /* hell room */
    { 164, 148, SPR_IMPLING, 44, 0, BB_SHADOW }, { 204, 164, SPR_BRUTE, 76, 0, BB_SHADOW },
    { 220, 132, SPR_PUFF_DEMON, 40, 36, BB_SHADOW }, { 152, 124, SPR_VAPE_WRAITH, 60, 8, BB_TRANSLUCENT },
    { 188, 140, SPR_SKULL_PILE, 20, 24, 0 }, { 228, 172, SPR_CORPSE_BRUTAL, 16, 0, 0 },
    { 236, 124, SPR_TORCH, 48, 0, BB_FULLBRIGHT }, { 140, 172, SPR_TORCH, 48, 0, BB_FULLBRIGHT },
    { 180, 172, SPR_HELLHOUND_PUP, 32, 0, BB_SHADOW },
};
#define PROPS_N ((int)(sizeof(props) / sizeof(props[0])))

/* ------------------------------------------------------------------------ */
static DoomoBillboard bb[PROPS_N + 4];
static int tour_on = 1, tour_k = 1, tour_t;
static int cam_mode = CAM_FIRST_PERSON;
static int32_t px, py, pz, pvz;          /* player feet (16.16)            */
static int pangle, ppitch;
static int on_ground = 1;
static int32_t eye_off = DOOMO_EYE_HEIGHT << 16;
static uint32_t ticks;
static int map_is_own = 1;
static int core_map_idx = -1;            /* -1 = own map */
static const char *phase = "boot";
static uint32_t fps_vbl0, fps_drawn0, fps10;
static uint32_t r3d_acc, r3d_n, r3d_max;
static int show_overworld;
static uint32_t ow_t;

static int floor_at(int32_t x, int32_t y)
{
    int cx = x >> 22, cy = y >> 22;
    const DoomoCell *c;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return 0;
    c = &doomo_map.cells[cy * doomo_map.w + cx];
    if (c->wall) return (c->ceil_h > c->floor_h ? c->ceil_h : c->floor_h) * DOOMO_HEIGHT_STEP;
    return map_floor_z(&doomo_map, cx, cy);
}

static int solid_at(int32_t x, int32_t y, int32_t z)
{
    int cx = x >> 22, cy = y >> 22;
    const DoomoCell *c;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return 1;
    c = &doomo_map.cells[cy * doomo_map.w + cx];
    if (c->wall) return c->ceil_h <= c->floor_h || (c->ceil_h * DOOMO_HEIGHT_STEP << 16) > z;
    return (map_floor_z(&doomo_map, cx, cy) << 16) > z ||
           (map_ceil_z(&doomo_map, cx, cy) << 16) < z;
}

static void set_phase(const char *p)
{
    phase = p;
    gba_logf("ET phase: %s (tick %u)", p, ticks);
}

static int lerp(int a, int b, int t, int n) { return a + (b - a) * t / n; }

static void tour_tick(void)
{
    const Key *a, *b;
    if (tour_k >= TOUR_N) {
        /* tour done: overworld, then core maps, then loop */
        return;
    }
    a = &tour[tour_k - 1];
    b = &tour[tour_k];
    if (tour_t == 0 && b->phase) set_phase(b->phase);
    tour_t++;
    px = (int32_t)lerp(a->x8, b->x8, tour_t, b->ticks) << 19;
    py = (int32_t)lerp(a->y8, b->y8, tour_t, b->ticks) << 19;
    /* sub-step smoothness: interpolate in 1/8 cell * 1/64 */
    px = ((int32_t)a->x8 << 19) + (int32_t)(((int64_t)(((int32_t)b->x8 - a->x8) << 19) * tour_t) / b->ticks);
    py = ((int32_t)a->y8 << 19) + (int32_t)(((int64_t)(((int32_t)b->y8 - a->y8) << 19) * tour_t) / b->ticks);
    pangle = lerp(a->angle, b->angle, tour_t, b->ticks) & 4095;
    ppitch = lerp(a->pitch, b->pitch, tour_t, b->ticks);
    cam_mode = b->cam;
    if (tour_t >= b->ticks) { tour_k++; tour_t = 0; }
}

static void manual_tick(uint32_t k)
{
    int32_t fx = fx_cos(pangle), fy = fx_sin(pangle);
    int32_t mvx = 0, mvy = 0, sp = 5 << 16;
    if (k & BTN_A) {
        if (k & BTN_UP) ppitch += 8;
        if (k & BTN_DOWN) ppitch -= 8;
        ppitch = DOOMO_CLAMP(ppitch, -256, 256);
    } else {
        if (k & BTN_UP) { mvx += FIXMUL(fx, sp); mvy += FIXMUL(fy, sp); }
        if (k & BTN_DOWN) { mvx -= FIXMUL(fx, sp); mvy -= FIXMUL(fy, sp); }
    }
    if (k & BTN_LEFT) pangle = (pangle - 48) & 4095;
    if (k & BTN_RIGHT) pangle = (pangle + 48) & 4095;
    if (k & BTN_L) { mvx += FIXMUL(fy, sp); mvy -= FIXMUL(fx, sp); }
    if (k & BTN_R) { mvx -= FIXMUL(fy, sp); mvy += FIXMUL(fx, sp); }
    if ((k & BTN_B) && on_ground) { pvz = 7 << 16; on_ground = 0; }
    /* collide per axis: blocked by walls and steps higher than STEP_UP */
    if (mvx) {
        int32_t nx = px + mvx + (mvx > 0 ? 12 << 16 : -(12 << 16));
        if (floor_at(nx, py) - (pz >> 16) <= DOOMO_STEP_UP && !solid_at(nx, py, pz + (40 << 16)))
            px += mvx;
    }
    if (mvy) {
        int32_t ny = py + mvy + (mvy > 0 ? 12 << 16 : -(12 << 16));
        if (floor_at(px, ny) - (pz >> 16) <= DOOMO_STEP_UP && !solid_at(px, ny, pz + (40 << 16)))
            py += mvy;
    }
}

static void physics_tick(void)
{
    int32_t fl = (int32_t)floor_at(px, py) << 16;
    if (!on_ground) {
        pz += pvz;
        pvz -= 1 << 15;
        if (pz <= fl) { pz = fl; pvz = 0; on_ground = 1; }
    } else if (pz < fl) {
        pz += (fl - pz + 3) / 4 + (1 << 14);        /* smooth step up */
        if (pz > fl) pz = fl;
    } else if (pz > fl) {
        pz -= 6 << 16;                              /* fall */
        if (pz < fl) pz = fl;
    }
}

/* ------------------------------------------------------------------------ */
static const DoomoImage *spr(int id, int frame)
{
    const DoomoImage *im = sprite_image(id, frame);
    return (im && im->pix && im->h) ? im : NULL;
}

static int build_sprites(int with_player)
{
    int i, n = 0;
    if (map_is_own) {
        for (i = 0; i < PROPS_N; i++) {
            const Prop *p = &props[i];
            DoomoBillboard *b = &bb[n];
            int frames = sprite_frames(p->spr);
            const DoomoImage *im = spr(p->spr, frames > 1 ? (int)((ticks / 8) % (frames > 2 ? 2 : frames)) : 0);
            int32_t x = (int32_t)p->x8 << 19, y = (int32_t)p->y8 << 19;
            if (!im) continue;
            b->x = x;
            b->y = y;
            b->z = ((int32_t)floor_at(x, y) + p->z) << 16;
            if (p->spr == SPR_PUFF_DEMON || p->spr == SPR_SOUL_ORB)
                b->z += fx_sin((int)(ticks * 40)) * 6;
            b->img = im;
            b->scale = (uint16_t)((p->height * 256) / im->h);
            b->flags = p->flags;
            b->light = (p->flags & BB_FULLBRIGHT) ? 0 : 0;
            n++;
        }
    }
    if (with_player) {
        const DoomoImage *im = spr(SPR_PET, (int)(ticks / 6) & 1);
        if (im) {
            DoomoBillboard *b = &bb[n];
            b->x = px; b->y = py; b->z = pz;
            b->img = im;
            b->scale = (uint16_t)((52 * 256) / im->h);
            b->flags = BB_SHADOW;
            b->light = 0;
            n++;
        }
    }
    return n;
}

static void third_person_camera(DoomoView3D *v)
{
    /* behind and above: back 136 units, right 8, up 34, tilted down so
     * the player stands in the lower middle of the view; pulled in when
     * the line from the player's head hits geometry */
    int32_t fx = fx_cos(pangle), fy = fx_sin(pangle);
    int32_t hx = px, hy = py, hz = pz + eye_off;
    int32_t bx = -FIXMUL(fx, 136 << 16) - FIXMUL(fy, 8 << 16);
    int32_t by = -FIXMUL(fy, 136 << 16) + FIXMUL(fx, 8 << 16);
    int32_t cz = hz + (34 << 16);
    int s, best = 0;
    for (s = 1; s <= 16; s++) {
        int32_t tx = hx + bx / 16 * s, ty = hy + by / 16 * s;
        int32_t tz = hz + (cz - hz) / 16 * s;
        if (solid_at(tx + (fx > 0 ? -(8 << 16) : (8 << 16)), ty, tz) ||
            solid_at(tx, ty, tz) || solid_at(tx, ty, tz + (6 << 16))) break;
        best = s;
    }
    if (best > 1) best--;
    v->x = hx + bx / 16 * best;
    v->y = hy + by / 16 * best;
    v->z = hz + (cz - hz) / 16 * best;
    v->pitch = (int16_t)DOOMO_CLAMP(v->pitch - 40, -256, 256);
    v->camera = CAM_THIRD_PERSON;
}

static void draw_hud(void)
{
    char buf[48];
    int i = 0, f = (int)fps10;
    const char *cam = cam_mode == CAM_THIRD_PERSON ? "3RD" : "1ST";
    const char *s;
    for (s = "ENGINE TEST "; *s; s++) buf[i++] = *s;
    for (s = cam; *s; s++) buf[i++] = *s;
    buf[i++] = ' ';
    if (f >= 100) buf[i++] = (char)('0' + f / 100);
    buf[i++] = (char)('0' + (f / 10) % 10);
    buf[i++] = '.';
    buf[i++] = (char)('0' + f % 10);
    for (s = "fps"; *s; s++) buf[i++] = *s;
    buf[i] = 0;
    plat_text(3, 3, buf, PAL_BLACK);
    plat_text(2, 2, buf, PAL_WHITE);
    plat_text(3, 152 - 1, phase, PAL_BLACK);
    plat_text(2, 152 - 2, phase, PAL_UI_GOLD);
    if (cam_mode == CAM_FIRST_PERSON) {
        plat_rect(119, 76, 2, 8, PAL_WHITE);
        plat_rect(116, 79, 8, 2, PAL_WHITE);
    }
}

static void fps_account(void)
{
    uint32_t dv = gba_vblanks - fps_vbl0;
    if (dv >= 120) {
        uint32_t dn = gba_mailbox.drawn - fps_drawn0;
        fps10 = dn * 597 / dv;            /* 59.73 Hz */
        gba_mailbox.fps10 = fps10;
        gba_logf("ET fps=%u.%u r3d avg=%uus max=%uus cols=%uus planes=%uus spr=%uus "
                 "planes=%u spans=%u ovf=%u [%s]",
                 fps10 / 10, fps10 % 10,
                 r3d_n ? (r3d_acc / r3d_n) * 10 / 168 : 0, r3d_max * 10 / 168,
                 r3d_stats.cycles_cols * 10 / 168, r3d_stats.cycles_planes * 10 / 168,
                 r3d_stats.cycles_sprites * 10 / 168, r3d_stats.planes, r3d_stats.spans,
                 r3d_stats.events_overflow, phase);
        gba_logf("ET dbg steps=%u facepx=%u skypx=%u flatpx=%u", r3d_dbg[0], r3d_dbg[1], r3d_dbg[2], r3d_dbg[3]);
        fps_vbl0 = gba_vblanks;
        fps_drawn0 = gba_mailbox.drawn;
        r3d_acc = r3d_n = r3d_max = 0;
    }
}

static void load_own_map(void)
{
    et_build_map(&doomo_map);
    map_is_own = 1;
    core_map_idx = -1;
}

static int load_core_map(int id)
{
    const DoomoMapDef *d = doomo_maps[id];
    int i;
    if (!d || !d->cells || !d->w) return 0;
    map_load(id);
    map_is_own = 0;
    core_map_idx = id;
    px = py = 0;
    for (i = 0; i < d->thing_count; i++) {
        if (d->things[i].type == THING_PLAYER_START) {
            px = ((int32_t)d->things[i].x * 64 + 32) << 16;
            py = ((int32_t)d->things[i].y * 64 + 32) << 16;
            pangle = d->things[i].angle * 16;
            break;
        }
    }
    if (!px) { px = 96 << 16; py = 96 << 16; }
    pz = (int32_t)floor_at(px, py) << 16;
    gba_logf("ET: core map %d '%s' %dx%d", id, d->name ? d->name : "?", d->w, d->h);
    return 1;
}

/* ------------------------------------------------------------------------ */
void enginetest_main(void)
{
    int save_test = (~REG_KEYINPUT & KEY_SELECT) != 0;
    uint32_t tour_end_tick = 0;
    int core_stage = 0;
    palette_init();
    if (doomo_colormap[15][2] != 2 || doomo_colormap[15][200] != 200) {
        gba_log("ET: colormap tables missing - building fallback (slow)");
        doomo_palette_fallback();
    }
    textures_init();
    sprites_init();
    video_load_palette();
    gba_log("ENGINE TEST: boot");
    et_div_test();
    if (save_test) et_save_test();
    load_own_map();
    px = (int32_t)tour[0].x8 << 19;
    py = (int32_t)tour[0].y8 << 19;
    pz = (int32_t)floor_at(px, py) << 16;
    pangle = tour[0].angle & 4095;
    set_phase(tour[0].phase);
    gba_pacing_reset();
    fps_vbl0 = gba_vblanks;
    fps_drawn0 = gba_mailbox.drawn;

    for (;;) {
        uint32_t k = gba_read_pad() & ~DOOMO_INPUT_SYNTH_XY;
        static uint32_t prevk;
        DoomoView3D v;
        int n, i;
        ticks++;
        plat_begin_frame();
        doomo_map.time++;
        if (map_is_own) {
            /* animate the showcase lift: up and down every 8 seconds */
        } else {
            map_tick(&doomo_map);
        }
        if (k && !tour_on) {
            /* manual */
        } else if (k & ~prevk & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_A |
                                 BTN_B | BTN_L | BTN_R)) {
            if (tour_on) gba_log("ET: manual control");
            tour_on = 0;
            show_overworld = 0;
        }
        if ((k & ~prevk & BTN_START) && !tour_on) {
            tour_on = 1;
            gba_log("ET: tour resumed");
        }
        if ((k & ~prevk & BTN_SELECT) && !tour_on)
            cam_mode = cam_mode == CAM_FIRST_PERSON ? CAM_THIRD_PERSON : CAM_FIRST_PERSON;
        prevk = k;

        if (tour_on) {
            if (map_is_own && tour_k < TOUR_N) {
                tour_tick();
                if (tour_k >= TOUR_N) tour_end_tick = ticks;
            } else if (map_is_own && !show_overworld && core_stage == 0) {
                show_overworld = 1;
                ow_t = 0;
                set_phase("overworld");
            } else if (show_overworld) {
                if (++ow_t > 150) {
                    show_overworld = 0;
                    core_stage = 1;
                }
            } else if (core_stage >= 1 && core_stage <= 2) {
                static uint32_t stage_t;
                if (core_map_idx < 0) {
                    int id = core_stage == 1 ? MAPID_VAPE_CELLAR : MAPID_CLOUD_NINE_SPIRE;
                    stage_t = 0;
                    if (!load_core_map(id)) {
                        core_stage++;
                        if (core_stage > 2) { gba_log("ET: no core maps yet"); core_stage = 3; }
                        continue;
                    }
                    set_phase(core_stage == 1 ? "core_vape_cellar" : "core_cloud_nine");
                }
                stage_t++;
                if (stage_t < 128) { pangle = (pangle + 32) & 4095; ppitch = 0; cam_mode = CAM_FIRST_PERSON; }
                else if (stage_t < 160) ppitch = (int)(stage_t - 128) * 7;
                else if (stage_t < 220) ppitch = 224 - (int)(stage_t - 160) * 7;
                else if (stage_t < 320) { ppitch = -20; cam_mode = CAM_THIRD_PERSON; pangle = (pangle + 20) & 4095; }
                else { core_stage++; core_map_idx = -1; if (core_stage > 2) core_stage = 3; }
            } else {
                /* loop the tour */
                load_own_map();
                tour_k = 1; tour_t = 0; core_stage = 0;
                (void)tour_end_tick;
            }
        } else {
            manual_tick(k);
        }
        physics_tick();

        if (show_overworld) {
            DoomoOverworldView ov;
            memset(&ov, 0, sizeof(ov));
            ov.map = &doomo_map;
            ov.cx = (int32_t)(4 * 64 + (int)ow_t * 4) << 16;
            ov.cy = (int32_t)(6 * 64 + (int)ow_t * 2) << 16;
            ov.vx = 0; ov.vy = 0; ov.vw = SCREEN_W; ov.vh = SCREEN_H;
            n = build_sprites(1);
            for (i = 0; i < n; i++) bb[i].scale = 256;    /* overworld: 1 texel = 1 px */
            ov.sprites = bb;
            ov.sprite_count = n;
            ov.daylight = 15;
            ov.time = (uint16_t)ticks;
            {
                uint32_t t0 = gba_cycles();
                plat_overworld(&ov);
                if (!gba_nodraw) {
                    uint32_t dt = gba_cycles() - t0;
                    r3d_acc += dt;
                    r3d_n++;
                    if (dt > r3d_max) r3d_max = dt;
                    draw_hud();
                }
            }
        } else {
            memset(&v, 0, sizeof(v));
            v.map = &doomo_map;
            v.angle = (int16_t)pangle;
            v.pitch = (int16_t)ppitch;
            v.vx = 0; v.vy = 0; v.vw = SCREEN_W; v.vh = SCREEN_H;
            v.time = (uint16_t)ticks;
            v.player_sprite = -1;
            if (cam_mode == CAM_THIRD_PERSON) {
                n = build_sprites(1);
                v.player_sprite = (int8_t)(n - 1);
                third_person_camera(&v);
            } else {
                n = build_sprites(0);
                v.x = px; v.y = py; v.z = pz + eye_off;
                v.bob = (int16_t)(tour_on ? (fx_sin((int)ticks * 180) * 2) >> 16 : 0);
                v.camera = CAM_FIRST_PERSON;
            }
            v.sprites = bb;
            v.sprite_count = n;
            plat_view3d(&v);
            if (!gba_nodraw) {
                r3d_acc += r3d_stats.cycles_total;
                r3d_n++;
                if (r3d_stats.cycles_total > r3d_max) r3d_max = r3d_stats.cycles_total;
                draw_hud();
            }
        }
        plat_end_frame();
        fps_account();
    }
}
