/* credits.c - SCENE_CREDITS: the parade of all 1000 citizens through the
 * united Hellvile Park (owner: story).
 *
 * The citizens march in pairs along the Parade Road, one Hillvile citizen
 * and one Hellvile citizen side by side (ids n and 500 + n), led by you
 * and your Doomogachi. The story cast cheers from the roadside, confetti
 * falls, fireworks pop, the names of every pair scroll by in a ticker at
 * the bottom and the team credits fade in and out at the top. Citizens
 * who passed away march too, translucent, in loving memory. Hold A to fast
 * forward, START skips to the end card.
 */
#include "overworld_int.h"

#define RANKS 500
#define RANK_TICKS 11                 /* a new pair every 11 ticks          */
#define MARCH_SPEED 6                 /* world units per tick               */
#define ROAD_Y (47 * DOOMO_CELL_UNITS + 40)
#define START_X (2 * DOOMO_CELL_UNITS)
#define END_X (62 * DOOMO_CELL_UNITS)
#define CR_BB OW_BB_MAX

static uint32_t ct;                   /* parade ticks                      */
static uint32_t draw_t;
static uint8_t ended, end_t;
/* the overworld's billboard array is free while the parade runs */
#define bb ow_bb
#define nbb ow_bb_count
static int16_t ticker_rank;

typedef struct { const char *head; const char *lines; } CreditCard;
static const CreditCard cards[] = {
    { "DOOMOGACHI LIFE", "in HELLvile Park!" },
    { "A Dippinn Productions game", "Made with love, plush stuffing\nand an alarming amount of vapor" },
    { "Starring", "@P\nand @E the Doomogachi" },
    { "Mayor Gigglesworth", "Mayor Giggs\nwho laughed at every speech" },
    { "Nana Pudding", "Head Baker, keeper of keys\nand muffins" },
    { "Father Fuzz", "Stuffed with love\nand a little buckwheat" },
    { "Rivet", "Head Welder, runaway imp\nand very good daughter" },
    { "Pip & Mr. Flops", "Bravest kid in HaHaland\n(and their bunny)" },
    { "Mother Nox", "High Priestess of Hellvile\nand potluck legend" },
    { "Vex", "Former vape tycoon\nnow selling plain water" },
    { "Lord Vapula", "Duke of the Ninth Cloud\n(now pocket-sized)" },
    { "Game Design & Story", "The Doomogachi Team" },
    { "Music & Sound", "30 Hz chiptunes:\ncute, then METAL, then cute" },
    { "Shooter & Welding", "Every gun you could weld\nand a bubble wand" },
    { "Citizens", "500 from Hillvile\n500 from Hellvile\nall of them marching" },
    { "Real hardware", "Game Boy Advance cartridge\nPlayStation disc" },
    { "Special thanks", "Everyone who ever\ntore down a wall" },
    { "And you", "Thank you for playing!" },
};
#define NCARDS ((int)(sizeof(cards) / sizeof(cards[0])))

static uint32_t total_ticks(void)
{
    return (uint32_t)(RANKS + 2) * RANK_TICKS + (END_X - START_X) / MARCH_SPEED;
}

static void cr_enter(int arg)
{
    (void)arg;
    ct = 0;
    draw_t = 0;
    ended = 0;
    end_t = 0;
    ticker_rank = -1;
    map_load(MAPID_PARK_UNITED);
    audio_music(SONG_CREDITS);
}

static void finish(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

static void cr_update(void)
{
    int steps = btn_down(BTN_A) ? 6 : 1, i;
    if (ended) {
        end_t++;
        if ((end_t > 30 && (btn_pressed(BTN_A) || btn_pressed(BTN_START))) || end_t > 240) finish();
        return;
    }
    if (btn_pressed(BTN_START)) { ended = 1; audio_music_once(SONG_VICTORY); return; }
    for (i = 0; i < steps; i++) {
        ct++;
        if ((ct % 300) == 150) audio_sfx(SFX_CONFETTI);
    }
    if (ct >= total_ticks()) { ended = 1; audio_music_once(SONG_VICTORY); }
}

/* World x of rank r's marchers right now (rank -1 is you). */
static int32_t rank_x(int r)
{
    int32_t start = (int32_t)(r + 1) * RANK_TICKS;
    if ((int32_t)ct < start) return -1;
    return START_X + ((int32_t)ct - start) * MARCH_SPEED;
}

static void add(int32_t x, int32_t y, int z, const DoomoImage *img, int flags)
{
    DoomoBillboard *b;
    if (!img || nbb >= CR_BB) return;
    b = &bb[nbb++];
    b->x = INT2FIX(x);
    b->y = INT2FIX(y);
    b->z = INT2FIX(z);
    b->img = img;
    b->scale = 256;
    b->flags = (uint8_t)flags;
    b->light = 1;
}

static int32_t camera_x(void)
{
    int32_t lead = rank_x(-1), cx;
    int32_t hw = plat_screen_w() * 2;
    cx = DOOMO_CLAMP(lead - hw / 2, START_X + hw, 34 * DOOMO_CELL_UNITS);
    return cx;
}

static void marcher(int id, int32_t x, int32_t y, uint32_t t)
{
    const DoomoResident *r = town_res(id);
    int fl = BB_SHADOW, hop;
    if (!r || r->status == RES_EMPTY) return;
    if (r->status == RES_DEAD || r->status == RES_GHOST) fl |= BB_TRANSLUCENT;
    hop = (int)((t + (uint32_t)id * 7u) % 16u);
    hop = hop < 8 ? hop : 16 - hop;
    add(x, y, hop, bodyspr_image(id, 2, (int)(((t / 4) + (uint32_t)id) & 3)), fl);
}

static void build(void)
{
    int32_t camx = camera_x(), vx0 = camx - plat_screen_w() * 2 - 64, vx1 = camx + plat_screen_w() * 2 + 64;
    int r, first, last, i;
    nbb = 0;
    /* you + your pet lead the parade */
    {
        int32_t x = rank_x(-1);
        if (x >= vx0 && x <= vx1 && x <= END_X) {
            marcher(DOOMO_PLAYER_ID, x, ROAD_Y, ct);
            if (pet_exists()) add(x + 26, ROAD_Y + 10, (int)((ct / 3) & 3) * 2, sprite_image(SPR_PET, (int)(ct / 6) & 3), BB_SHADOW);
        }
    }
    /* only the ranks on screen */
    first = (int)(((int32_t)ct - (vx1 - START_X) / MARCH_SPEED) / RANK_TICKS) - 2;
    last = (int)(((int32_t)ct - (vx0 - START_X) / MARCH_SPEED) / RANK_TICKS) + 1;
    if (first < 0) first = 0;
    if (last >= RANKS) last = RANKS - 1;
    for (r = first; r <= last; r++) {
        int32_t x = rank_x(r);
        if (x < 0 || x > END_X || x < vx0 || x > vx1) continue;
        marcher(r, x, ROAD_Y - 14, ct);
        marcher(DOOMO_DISTRICT_SIZE + r, x - 8, ROAD_Y + 14, ct);
    }
    /* the cast cheers from the roadside */
    {
        static const int16_t fans[10] = { ID_MAYOR, ID_MOTHER_NOX, ID_NANA, ID_FUZZ, ID_RIVET,
                                          ID_ANVIL, ID_PIP, ID_SOOTKIN, ID_VEX, ID_TALLYWORN };
        for (i = 0; i < 10; i++) {
            int32_t x = camx - plat_screen_w() * 2 + 40 + i * (plat_screen_w() * 4 - 80) / 9;
            int jump = (int)(((ct + (uint32_t)i * 9) / 3) % 12);
            jump = jump < 6 ? jump * 3 : (12 - jump) * 3;
            {
                const DoomoResident *f = town_res(fans[i]);
                if (f && f->status != RES_EMPTY && f->status != RES_DEAD)
                    add(x, ROAD_Y - 92 - (i & 1) * 20, jump, bodyspr_image(fans[i], 0, 0), BB_SHADOW);
            }
        }
    }
}

static void draw_ticker(int sw, int sh)
{
    int y = sh - 15, r, x;
    char buf[48];
    int32_t mid = camera_x();
    /* the rank closest to the middle of the screen */
    r = (int)(((int32_t)ct - (mid - START_X) / MARCH_SPEED) / RANK_TICKS) - 1;
    if (r >= 0 && r < RANKS) ticker_rank = (int16_t)r;
    ui_round_rect(4, y - 2, sw - 8, 14, 4, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    ui_round_rect(5, y - 1, sw - 10, 12, 3, PAL_UI_CREAM);
    if (ticker_rank < 0) {
        ui_text_center(sw / 2, y + 1, "Here comes the parade!", PAL_RAMP(RAMP_PASTEL_PINK, 1));
        return;
    }
    {
        const DoomoResident *a = town_res(ticker_rank), *b = town_res(DOOMO_DISTRICT_SIZE + ticker_rank);
        doomo_snprintf(buf, sizeof(buf), "%s " STR_ICON_HEART " %s",
                       (a && a->status != RES_EMPTY) ? a->name : "...",
                       (b && b->status != RES_EMPTY) ? b->name : "...");
    }
    x = 10;
    plat_text(x, y + 1, STR_ICON_STAR, PAL_UI_GOLD);
    ui_text(x + 10, y + 1, buf, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    doomo_snprintf(buf, sizeof(buf), "%d/1000", DOOMO_MIN(1000, (ticker_rank + 1) * 2));
    ui_text(sw - 10 - ui_text_width(buf), y + 1, buf, PAL_GRAY_D);
}

static void draw_card(int sw)
{
    uint32_t per = total_ticks() / NCARDS, k = ct / per, local = ct % per;
    char head[64], lines[160];
    int y = 6, s;
    if (k >= (uint32_t)NCARDS) k = NCARDS - 1;
    if (local > per - 20 || local < 12) return;            /* gaps between cards */
    story_fmt(head, sizeof(head), cards[k].head);
    story_fmt(lines, sizeof(lines), cards[k].lines);
    s = (k == 0 && ui_text_big_width(head, 2) < sw - 8) ? 2 : 1;
    if (s == 2) ui_text_big((sw - ui_text_big_width(head, 2)) / 2, y, head, 2, PAL_WHITE, PAL_UI_PINK_D);
    else ui_title(sw / 2, y + 2, head, WIN_CUTE);
    {
        const char *p = lines;
        int ly = y + (s == 2 ? 22 : 18);
        char one[48];
        while (*p) {
            int n = 0;
            while (p[n] && p[n] != '\n' && n < 47) { one[n] = p[n]; n++; }
            one[n] = 0;
            ui_text_shadow((sw - ui_text_width(one)) / 2, ly, one, PAL_WHITE, PAL_BLACK);
            ly += FONT_LINE;
            p += n;
            if (*p == '\n') p++;
        }
    }
}

static void cr_draw(void)
{
    DoomoOverworldView v;
    int sw = plat_screen_w(), sh = plat_screen_h(), i;
    draw_t++;
    if (plat_draw_skipped()) return;
    if (ended) {
        int s = sh >= 200 ? 3 : 2;
        ui_backdrop(WIN_CUTE, draw_t);
        plat_checker(sw / 2, 0, sw - sw / 2, sh, PAL_RAMP(RAMP_VAPE, 3));
        ui_text_big((sw - ui_text_big_width("THE END", s)) / 2, sh / 2 - 30, "THE END", s, PAL_WHITE, PAL_UI_PINK_D);
        ui_text_center(sw / 2, sh / 2 + 4, "Everyone is together in Hellvile Park.", PAL_UI_CREAM);
        for (i = 0; i < 8; i++)
            ui_heart(20 + i * (sw - 40) / 7, sh - 30 + ui_wave((int)draw_t + i * 9, 40, 4), 4, PAL_PAINT((i * 5) & 31, 2));
        if (end_t > 30 && ((draw_t / 16) & 1)) ui_text_center(sw / 2, sh - 14, STR_ICON_A " Continue", PAL_WHITE);
        return;
    }
    build();
    memset(&v, 0, sizeof(v));
    v.map = &doomo_map;
    v.cx = INT2FIX(camera_x());
    v.cy = INT2FIX(ROAD_Y - 60);
    v.vw = (int16_t)sw;
    v.vh = (int16_t)sh;
    v.sprites = bb;
    v.sprite_count = nbb;
    v.daylight = (uint8_t)(ct < total_ticks() / 2 ? 13 : 9);
    v.time = (uint16_t)draw_t;
    plat_overworld(&v);
    /* confetti */
    for (i = 0; i < 30; i++) {
        int x = (int)(((uint32_t)i * 53u + draw_t * (1u + (uint32_t)i % 3u)) % (uint32_t)sw);
        int y = (int)(((uint32_t)i * 29u + draw_t * (2u + (uint32_t)i % 4u)) % (uint32_t)sh);
        plat_rect(x + ui_wave((int)draw_t + i * 11, 40, 3), y, (draw_t + (uint32_t)i) & 8 ? 2 : 1, 2, PAL_PAINT((i * 7) & 31, 2));
    }
    /* fireworks */
    for (i = 0; i < 2; i++) {
        uint32_t t2 = (draw_t + (uint32_t)i * 40) % 80, k;
        int fx = (int)((((draw_t + (uint32_t)i * 40) / 80) * 97u + (uint32_t)i * 61u) % (uint32_t)(sw - 40)) + 20;
        int fy = 34 + i * 16;
        if (t2 < 40)
            for (k = 0; k < 12; k++) {
                int a = (int)k * 341;
                plat_rect(fx + FIX2INT(fx_cos(a) * (int)t2), fy + FIX2INT(fx_sin(a) * (int)t2) + (int)t2 / 8, 2, 2,
                          PAL_PAINT((int)(k + (uint32_t)i * 9) & 31, t2 > 28 ? 0 : 2));
            }
    }
    draw_card(sw);
    draw_ticker(sw, sh);
}

const DoomoScene SCENE_CREDITS = {
    "CREDITS", cr_enter, NULL, cr_update, cr_draw, 0
};
