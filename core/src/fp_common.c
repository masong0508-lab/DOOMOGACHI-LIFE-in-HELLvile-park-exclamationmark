/* fp_common.c - shared helpers of the free play screens (owner: freeplay).
 *
 * The living island (sim_tick + autosave), the HUD, chibis / portraits /
 * figures on a per-tick render budget (a chibi costs ~250k GBA cycles, a
 * portrait ~760k: new ones are rendered a few per tick and a soft plush
 * placeholder stands in meanwhile), meters, thought bubbles, particle
 * bursts, the island's own save chunk 'FPLY' and the strong
 * activity_launch() that opens the citizen picker for activities that
 * need a resident.
 */
#include "fp_int.h"

char fp_tmp[128];

/* ====================================================================== */
/* small helpers                                                            */
int fp_t(void) { return (int)doomo_frame_count(); }
int fp_sw(void) { return plat_screen_w(); }
int fp_sh(void) { return plat_screen_h(); }
int fp_big(void) { return plat_screen_h() >= 200; }
int fp_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int fp_strcasestr(const char *hay, const char *needle)
{
    int i, j;
    if (!needle || !*needle) return 1;
    if (!hay) return 0;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j] && hay[i + j]; j++)
            if (fp_lower((unsigned char)hay[i + j]) != fp_lower((unsigned char)needle[j])) break;
        if (!needle[j]) return 1;
    }
    return 0;
}

void fp_strcpy(char *dst, const char *src, int cap)
{
    int i;
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    for (i = 0; i < cap - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

int fp_is_freeplay(void) { return doomo_town.mode == TOWN_FREEPLAY; }

int fp_island_song(void)
{
    return town_is_night() ? SONG_HAHALAND_NIGHT : SONG_HAHALAND_DAY;
}

DoomoResident *fp_res(int id)
{
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return NULL;
    return &doomo_town.res[id];
}

int fp_living_status(int s)
{
    return s == RES_ALIVE || s == RES_POSSESSED || s == RES_SOUL_LOST || s == RES_GHOST;
}

int fp_alive(int id)
{
    const DoomoResident *r = fp_res(id);
    return r && fp_living_status(r->status);
}

static const char *const status_names[6] = {
    "Empty", "Alive", "Possessed", "Soul lost", "Dead", "Ghost"
};
const char *fp_status_name(int s) { return (s >= 0 && s < 6) ? status_names[s] : "?"; }

uint8_t fp_status_color(int s)
{
    switch (s) {
    case RES_ALIVE: return PP(PAINT_LIME, 0);
    case RES_POSSESSED: return PAL_UI_RED;
    case RES_SOUL_LOST: return PP(PAINT_GRAPE, 1);
    case RES_DEAD: return PAL_GRAY;
    case RES_GHOST: return PP(PAINT_CYAN, 0);
    default: return PAL_GRAY_L;
    }
}

uint8_t fp_status_icon(int s)
{
    switch (s) {
    case RES_ALIVE: return ICON_HEART;
    case RES_POSSESSED: return ICON_PENTAGRAM;
    case RES_SOUL_LOST: return ICON_FIRE;
    case RES_DEAD: return ICON_SKULL;
    case RES_GHOST: return ICON_GHOST;
    default: return ICON_HOUSE;
    }
}

int fp_mood_expr(const DoomoResident *r)
{
    if (!r) return EXPR_NEUTRAL;
    if (r->status == RES_DEAD) return EXPR_DEAD;
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) return EXPR_POSSESSED;
    switch (r->mood) {
    case MOOD_HAPPY: return EXPR_HAPPY;
    case MOOD_ECSTATIC: return EXPR_LOVE;
    case MOOD_SAD: case MOOD_LONELY: case MOOD_HUNGRY: case MOOD_FILTHY: return EXPR_SAD;
    case MOOD_ANGRY: case MOOD_CORRUPTED: return EXPR_ANGRY;
    case MOOD_LOVESICK: return EXPR_LOVE;
    case MOOD_SLEEPY: return EXPR_SLEEPY;
    case MOOD_SICK: return EXPR_SICK;
    case MOOD_SCARED: return EXPR_SHOCK;
    case MOOD_POSSESSED: return EXPR_POSSESSED;
    default: return EXPR_NEUTRAL;
    }
}

uint8_t fp_mood_icon(int mood)
{
    static const uint8_t ic[MOOD_COUNT] = {
        ICON_SMILE, ICON_SMILE, ICON_STAR, ICON_FROWN, ICON_FIRE, ICON_HEART,
        ICON_FOOD, ICON_ZZZ, ICON_CROSS, ICON_DROP, ICON_CLOUD, ICON_GHOST,
        ICON_PENTAGRAM, ICON_SKULL
    };
    return (mood >= 0 && mood < MOOD_COUNT) ? ic[mood] : ICON_SMILE;
}

uint8_t fp_mood_color(int mood)
{
    switch (mood) {
    case MOOD_HAPPY: return PP(PAINT_LIME, 0);
    case MOOD_ECSTATIC: return PAL_UI_GOLD;
    case MOOD_SAD: case MOOD_LONELY: return PP(PAINT_SKY, 0);
    case MOOD_ANGRY: return PP(PAINT_TANGERINE, 0);
    case MOOD_LOVESICK: return PP(PAINT_ROSE, 1);
    case MOOD_HUNGRY: return PP(PAINT_CARAMEL, 0);
    case MOOD_SLEEPY: return PP(PAINT_LAVENDER, 0);
    case MOOD_SICK: return PP(PAINT_CHARTREUSE, 0);
    case MOOD_FILTHY: return PP(PAINT_CHOCOLATE, 1);
    case MOOD_SCARED: return PP(PAINT_CYAN, 0);
    case MOOD_CORRUPTED: return PP(PAINT_VAPE_PURPLE, 0);
    case MOOD_POSSESSED: return PAL_UI_RED;
    default: return PP(PAINT_MINT, 0);
    }
}

uint8_t fp_rel_icon(int kind)
{
    static const uint8_t ic[REL_KIND_COUNT] = {
        ICON_SMILE, ICON_STAR, ICON_SPARK, ICON_HEART, ICON_HEART, ICON_BOLT,
        ICON_SKULL, ICON_HOUSE, ICON_FROWN
    };
    return (kind >= 0 && kind < REL_KIND_COUNT) ? ic[kind] : ICON_SMILE;
}

uint8_t fp_rel_color(int kind)
{
    switch (kind) {
    case REL_FRIEND: return PP(PAINT_TANGERINE, 1);
    case REL_BEST_FRIEND: return PAL_UI_GOLD;
    case REL_DATING: return PP(PAINT_BUBBLEGUM, 0);
    case REL_MARRIED: return PP(PAINT_CHERRY, 1);
    case REL_RIVAL: return PP(PAINT_TANGERINE, 0);
    case REL_NEMESIS: return PP(PAINT_GRAPE, 0);
    case REL_FAMILY: return PP(PAINT_FOREST, 1);
    case REL_EX: return PP(PAINT_SILVER, 0);
    default: return PP(PAINT_SKY, 0);
    }
}

/* ---- jobs & bios ---------------------------------------------------------- */
static const char *const hill_jobs[32] = {
    "Cupcake Tester", "Florist", "Mail Carrier", "Baker", "Nurse", "Gardener",
    "Toymaker", "Librarian", "Barista", "Painter", "Knitter", "Lifeguard",
    "Ice Cream Vendor", "Choir Singer", "Bus Driver", "Teacher", "Beekeeper",
    "Kite Maker", "Clown", "Plush Doctor", "Chef", "Poet", "Hairdresser",
    "Dog Walker", "Astronomer", "Yoga Coach", "Candle Maker", "Tailor",
    "Juggler", "Park Ranger", "Pastry Chef", "Dance Teacher"
};
static const char *const hell_jobs[32] = {
    "Imp Accountant", "Ghoul Landlord", "Bone Clerk", "Bouncer", "Bookie",
    "Vape Clerk", "Edgy Poet", "Cultist (Part-time)", "Sulfur Miner",
    "Fence", "Cursed Chef", "Goth DJ", "Gargoyle Model", "Lava Lifeguard",
    "Pit Plumber", "Skull Polisher", "Nightmare Writer", "Tax Demon",
    "Sewer Merchant", "Bat Wrangler", "Hex Tailor", "Coffin Carpenter",
    "Brimstone Baker", "Doom Barber", "Chain Smith", "Fire Juggler",
    "Debt Collector", "Haunt Consultant", "Spite Gardener", "Moat Keeper",
    "Ember Artist", "Gloom Merchant"
};

static int premade_here(int id)
{
    const DoomoResident *r = fp_res(id);
    const char *rn;
    if (!r || r->status == RES_EMPTY) return 0;
    if (doomo_town.population != TOWN_POP_PREMADE && doomo_town.mode != TOWN_STORY) return 0;
    rn = roster_name(id);
    return rn && rn[0] && strncmp(rn, r->name, DOOMO_NAME_LEN) == 0;
}

const char *fp_job(int id)
{
    const DoomoResident *r = fp_res(id);
    uint32_t h;
    if (!r || r->status == RES_EMPTY) return "";
    if (premade_here(id)) {
        const char *j = roster_job(id);
        if (j && j[0]) return j;
    }
    h = doomo_hash32((uint32_t)id * 2654435761u ^ r->like_hobby ^ ((uint32_t)r->catchphrase << 8));
    return (r->flags & RF_HELLBORN) ? hell_jobs[h & 31] : hill_jobs[h & 31];
}

const char *fp_bio(int id)
{
    static char buf[72];
    const DoomoResident *r = fp_res(id);
    if (!r || r->status == RES_EMPTY) return "";
    if (premade_here(id)) {
        const char *b = roster_bio(id);
        if (b && b[0]) return b;
    }
    if (r->flags & RF_CUSTOM)
        doomo_snprintf(buf, sizeof(buf), "Made with love. Into %s, scared of %s.",
                       doomo_hobby_names[r->like_hobby % HOBBY_COUNT],
                       doomo_fear_names[r->fear % FEAR_COUNT]);
    else
        doomo_snprintf(buf, sizeof(buf), "Loves %s. Can't stand %s.",
                       doomo_hobby_names[r->like_hobby % HOBBY_COUNT],
                       doomo_food_names[r->hate_food % FOOD_COUNT]);
    return buf;
}

int fp_floor(int id) { return (id < 0 ? 0 : id % DOOMO_DISTRICT_SIZE) / 10 + 1; }
int fp_room(int id) { return (id < 0 ? 0 : id % DOOMO_DISTRICT_SIZE) % 10 + 1; }

const char *fp_district_name(int d) { return d ? "Hellvile" : "Hillvile"; }
const char *fp_home_name(int d) { return d ? "Hellvile Pits" : "Hillvile Towers"; }

int fp_host_in_town(int roster_id)
{
    const DoomoResident *r = fp_res(roster_id);
    const char *rn = roster_name(roster_id);
    return r && r->status == RES_ALIVE && rn && rn[0] &&
           strncmp(rn, r->name, DOOMO_NAME_LEN) == 0;
}

const DoomoLook *fp_host_look(int roster_id)
{
    const DoomoResident *p;
    if (fp_host_in_town(roster_id)) return &doomo_town.res[roster_id].look;
    p = roster_premade(roster_id);
    if (p) return &p->look;
    return &doomo_town.player.look;
}

const char *fp_activity_name(int act)
{
    static const char *const n[ACT_COUNT] = {
        "At home", "Asleep", "At the park", "At the cafe", "Shopping", "At church",
        "At the workshop", "At the beach", "At a concert", "At the vape shop",
        "At the hospital", "Wandering", "Haunting"
    };
    return (act >= 0 && act < ACT_COUNT) ? n[act] : "Somewhere";
}

uint8_t fp_activity_icon(int act)
{
    static const uint8_t ic[ACT_COUNT] = {
        ICON_HOUSE, ICON_ZZZ, ICON_SPARK, ICON_FOOD, ICON_COIN, ICON_CROSS,
        ICON_WRENCH, ICON_DROP, ICON_NOTE, ICON_CLOUD, ICON_CROSS, ICON_ARROW_R,
        ICON_GHOST
    };
    return (act >= 0 && act < ACT_COUNT) ? ic[act] : ICON_HOUSE;
}

int fp_item_icon(int item)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return ICON_X;
    return doomo_items[item].icon;
}

/* ====================================================================== */
/* the island chunk 'FPLY'                                                   */
#define FP_FLOWER_BYTES (DOOMO_MAX_RESIDENTS / 4)
static struct {
    char     name[FP_ISLAND_NAME_LEN];
    uint32_t rent_day;
    uint8_t  flowers[FP_FLOWER_BYTES];      /* 2 bits per resident */
} fpx;
#define FPX_SIZE (FP_ISLAND_NAME_LEN + 4 + FP_FLOWER_BYTES)

static uint32_t fpx_size(void) { return FPX_SIZE; }

static uint32_t fpx_write(uint8_t *dst, uint32_t cap)
{
    if (cap < FPX_SIZE) return 0;
    memcpy(dst, fpx.name, FP_ISLAND_NAME_LEN);
    dst[16] = (uint8_t)(fpx.rent_day & 0xFF);
    dst[17] = (uint8_t)((fpx.rent_day >> 8) & 0xFF);
    dst[18] = (uint8_t)((fpx.rent_day >> 16) & 0xFF);
    dst[19] = (uint8_t)((fpx.rent_day >> 24) & 0xFF);
    memcpy(dst + 20, fpx.flowers, FP_FLOWER_BYTES);
    return FPX_SIZE;
}

static int fpx_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    int i;
    if (version < 1 || len < FPX_SIZE) return 0;
    memcpy(fpx.name, src, FP_ISLAND_NAME_LEN);
    fpx.name[FP_ISLAND_NAME_LEN - 1] = 0;
    for (i = 0; i < FP_ISLAND_NAME_LEN && fpx.name[i]; i++)
        if ((unsigned char)fpx.name[i] < 0x20 || (unsigned char)fpx.name[i] > 0x7E) fpx.name[i] = '?';
    fpx.rent_day = (uint32_t)src[16] | ((uint32_t)src[17] << 8) | ((uint32_t)src[18] << 16) |
                   ((uint32_t)src[19] << 24);
    memcpy(fpx.flowers, src + 20, FP_FLOWER_BYTES);
    return 1;
}

static void fpx_reset(void) { memset(&fpx, 0, sizeof(fpx)); }

static const DoomoSaveExt fp_ext = {
    SAVE_EXT_TAG('F', 'P', 'L', 'Y'), 1, fpx_size, fpx_write, fpx_read, fpx_reset
};
const DoomoSaveExt *const doomo_save_ext_freeplay = &fp_ext;

const char *fp_island_name(void) { return fpx.name; }
void fp_set_island_name(const char *s) { fp_strcpy(fpx.name, s, FP_ISLAND_NAME_LEN); }
const char *fp_island_title(char *buf, int cap)
{
    if (fpx.name[0]) doomo_snprintf(buf, cap, "%s", fpx.name);
    else doomo_snprintf(buf, cap, "Hellvile Park");
    return buf;
}
uint32_t fp_rent_day(void) { return fpx.rent_day; }
void fp_set_rent_day(uint32_t d) { fpx.rent_day = d; }
int fp_flowers(int id)
{
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    return (fpx.flowers[id >> 2] >> ((id & 3) * 2)) & 3;
}
void fp_set_flowers(int id, int n)
{
    int sh;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return;
    n = DOOMO_CLAMP(n, 0, 3);
    sh = (id & 3) * 2;
    fpx.flowers[id >> 2] = (uint8_t)((fpx.flowers[id >> 2] & ~(3 << sh)) | (n << sh));
}

/* ====================================================================== */
/* the living island                                                          */
static void fx_step(void);
static uint8_t autosave_bound;          /* this island owns slot 1 */

void fp_autosave_bind(int on) { autosave_bound = (uint8_t)on; }

void fp_live(void)
{
    if (fp_is_freeplay()) {
        uint32_t day0 = doomo_town.day;
        int h0 = town_hour(), mode = opt_get(OPT_AUTOSAVE);
        sim_tick();
        if (autosave_bound && mode &&
            (doomo_town.day != day0 || (mode == 2 && town_hour() != h0))) {
            if (save_game(1)) ui_toast(STR_ICON_HEART " Autosaved!");
            else ui_toast("Autosave failed!");
        }
    }
    fx_step();
}

void fp_new_island(int population, uint32_t seed)
{
    town_new(TOWN_FREEPLAY, seed, population);
    sim_init();
    fp_fx_clear();
    /* autosave only into an empty slot until the player saves by hand */
    autosave_bound = (uint8_t)!save_exists(1);
}

void fp_goto_island(int arg) { scene_set(&SCENE_ISLAND, arg); }

void fp_open_resident(int id)
{
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return;
    scene_push(&SCENE_RESIDENT, id);
}

/* ====================================================================== */
/* sorting (shell sort: n log^2 n, no recursion, no extra memory)            */
void fp_sort_u16(uint16_t *a, int n, int (*cmp)(int a, int b))
{
    static const int gaps[] = { 701, 301, 132, 57, 23, 10, 4, 1 };
    int g, i, j;
    for (g = 0; g < 8; g++) {
        int gap = gaps[g];
        for (i = gap; i < n; i++) {
            uint16_t v = a[i];
            for (j = i; j >= gap && cmp(a[j - gap], v) > 0; j -= gap) a[j] = a[j - gap];
            a[j] = v;
        }
    }
}

/* ====================================================================== */
/* drawing helpers                                                            */
void fp_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
{
    int r = DOOMO_MIN(h / 2, 4);
    if (plat_draw_skipped()) return;
    if (w <= 2 || h <= 2) { plat_rect(x, y, w, h, fill); return; }
    ui_round_rect(x, y, w, h, r, outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, r > 0 ? r - 1 : 0, fill);
    if (h >= 6) {
        int in = (r > 1 ? r - 1 : 1) + 1;
        plat_rect(x + in, y + 1, w - 2 * in, 1, hi);
    }
}

void fp_shadow(int x, int y, int w, int h)
{
    if (plat_draw_skipped() || w < 4 || h < 4) return;
    plat_shade_rect(x + 3, y + h, w - 4, 1, 9);
    plat_shade_rect(x + 5, y + h + 1, w - 8, 1, 11);
    plat_shade_rect(x + w, y + 3, 1, h - 4, 10);
}
void fp_icon(int x, int y, int icon, uint8_t color)
{
    char s[2];
    s[0] = (char)icon;
    s[1] = 0;
    plat_text(x + 1, y + 1, s, ui_darker(ui_darker(color)));
    plat_text(x, y, s, color);
}

void fp_ball(int cx, int cy, int r, uint8_t base)
{
    if (r < 1) r = 1;
    ui_fill_circle(cx, cy, r, ui_darker(ui_darker(base)));
    ui_fill_circle(cx, cy, r - 1 > 0 ? r - 1 : 1, base);
    if (r >= 3) {
        ui_fill_circle(cx - r / 3, cy - r / 3, r / 3 > 0 ? r / 3 : 1, ui_lighter(base));
        plat_rect(cx - r / 2, cy - r / 2, 1, 1, PAL_WHITE);
    }
}

void fp_bubble(int x, int y, int icon, uint8_t color, int phase)
{
    int b = ui_wave(phase, 40, 1), bx = x + 2, by = y - 16 + b;
    if (plat_draw_skipped()) return;
    /* the little trail of thought puffs */
    ui_fill_circle(x, y - 1, 1, PAL_RAMP(RAMP_STONE, 4));
    plat_rect(x, y - 1, 1, 1, PAL_WHITE);
    ui_fill_circle(x + 2, y - 4 + b / 2, 2, PAL_RAMP(RAMP_STONE, 4));
    ui_fill_circle(x + 2, y - 4 + b / 2, 1, PAL_WHITE);
    /* the bubble */
    ui_round_rect(bx - 1, by - 1, 15, 13, 5, PAL_RAMP(RAMP_STONE, 3));
    ui_round_rect(bx, by, 13, 11, 4, PAL_WHITE);
    plat_rect(bx + 2, by + 8, 9, 2, PAL_RAMP(RAMP_STONE, 6));
    plat_rect(bx + 3, by + 1, 3, 1, PAL_RAMP(RAMP_SKY, 7));
    fp_icon(bx + 3, by + 2, icon, color);
}

const char *const fp_stat_names[STAT_COUNT] = {
    "Food", "Joy", "Clean", "Energy", "Health", "Mind", "Dark", "Social"
};

uint8_t fp_stat_icon(int stat)
{
    static const uint8_t ic[STAT_COUNT] = {
        ICON_FOOD, ICON_SMILE, ICON_DROP, ICON_BOLT, ICON_HEART, ICON_STAR,
        ICON_PENTAGRAM, ICON_CLOUD
    };
    return (stat >= 0 && stat < STAT_COUNT) ? ic[stat] : ICON_STAR;
}

uint8_t fp_stat_color(int stat, int value)
{
    static const uint8_t base[STAT_COUNT] = {
        PP(PAINT_TANGERINE, 1), PP(PAINT_BUBBLEGUM, 1), PP(PAINT_SKY, 1),
        PP(PAINT_LEMON, 1), PP(PAINT_CHERRY, 1), PP(PAINT_LAVENDER, 1),
        PP(PAINT_VAPE_PURPLE, 1), PP(PAINT_MINT, 1)
    };
    if (stat == STAT_CORRUPTION) return value >= 70 ? PAL_UI_RED : base[stat];
    if (stat >= 0 && stat < STAT_COUNT && value < 25) return PP(PAINT_CHERRY, 0);
    return (stat >= 0 && stat < STAT_COUNT) ? base[stat] : PAL_UI_GOLD;
}

void fp_meter(int x, int y, int w, int icon, const char *label, int value, uint8_t fill,
              uint8_t text)
{
    int lw = 38;
    fp_icon(x, y, icon, fill);
    ui_text(x + 9, y, label, text);
    if (w - 9 - lw > 8) ui_bar(x + 9 + lw, y + 1, w - 9 - lw, 7, value, 100, fill);
}

int fp_hud(int style)
{
    const UiStyle *st = ui_style(style);
    int sw = fp_sw(), t = fp_t(), m, d, night = town_is_night();
    char clock[12], buf[40];
    int w, x = 3, y = 2;
    town_time_str(clock, sizeof(clock));
    sim_date(&m, &d);
    if (fp_big())
        doomo_snprintf(buf, sizeof(buf), "Day %d  %s %d %s  %s", (int)doomo_town.day + 1,
                       doomo_weekday_names[doomo_town.weekday % 7], d,
                       doomo_month_names[m % 12], clock);
    else
        doomo_snprintf(buf, sizeof(buf), "Day %d %s %s", (int)doomo_town.day + 1,
                       doomo_weekday_names[doomo_town.weekday % 7], clock);
    w = ui_text_width(buf) + 24;
    fp_pill(x, y, w, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
    /* sun / moon */
    if (night) {
        ui_fill_circle(x + 8, y + 6, 4, PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_circle(x + 10, y + 5, 3, st->title_bg);
        ui_sparkle(x + 4, y + 2, (t / 4) & 15, PAL_WHITE);
    } else {
        int r = 3 + (ui_wave(t, 60, 1) > 0);
        ui_fill_circle(x + 8, y + 6, r + 1, PAL_RAMP(RAMP_CANDY, 4));
        ui_fill_circle(x + 8, y + 6, r, PAL_UI_GOLD);
        plat_rect(x + 6, y + 4, 2, 1, PAL_WHITE);
    }
    ui_text_shadow(x + 16, y + 2, buf, st->title, st->outline);
    fp_money_pill(sw - 3, y, 1);
    return 16;
}

void fp_money_pill(int x, int y, int right_align)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    char buf[20];
    int w, t = fp_t();
    doomo_snprintf(buf, sizeof(buf), "H$ %u", (unsigned)doomo_town.money);
    w = ui_text_width(buf) + 20;
    if (right_align) x -= w;
    fp_pill(x, y, w, 12, st->fill_hi, PAL_RAMP(RAMP_CANDY, 2), PAL_WHITE);
    ui_fill_circle(x + 7, y + 6, 4, PAL_RAMP(RAMP_CANDY, 2));
    ui_fill_circle(x + 7, y + 6, 3, PAL_UI_GOLD);
    plat_rect(x + 7, y + 4, 1, 4, PAL_RAMP(RAMP_CANDY, 3));
    plat_rect(x + 5, y + 4, 1, 1, PAL_WHITE);
    if ((t / 6) % 24 == 0) ui_sparkle(x + 10, y + 2, (t / 2) & 15, PAL_WHITE);
    ui_text(x + 14, y + 2, buf, PAL_RAMP(RAMP_WOOD, 1));
}

void fp_panel(int x, int y, int w, int h, int style, const char *title)
{
    const UiStyle *st = ui_style(style);
    ui_window(x, y, w, h, style);
    if (title && title[0]) {
        int tw = ui_text_width(title) + 12;
        fp_pill(x + 6, y - 5, tw, 11, st->title_bg, st->outline, ui_lighter(st->title_bg));
        ui_text_shadow(x + 12, y - 3, title, st->title, st->outline);
    }
}

void fp_tag(int x, int y, const char *s, uint8_t fill, uint8_t text)
{
    int w = ui_text_width(s) + 8;
    fp_pill(x, y, w, 11, fill, ui_darker(ui_darker(fill)), ui_lighter(fill));
    ui_text(x + 4, y + 2, s, text);
}

/* Text clipped to [x0, x1): one plat_text per glyph that fits. */
static void text_clip(int x, int y, const char *s, uint8_t color, int x0, int x1)
{
    char g[2];
    g[1] = 0;
    for (; *s; s++, x += FONT_ADVANCE) {
        if (x < x0) continue;
        if (x + FONT_ADVANCE > x1) break;
        if (*s == ' ') continue;
        g[0] = *s;
        plat_text(x, y, g, color);
    }
}

void fp_marquee(int x, int y, int w, const char *s, uint8_t color, int t)
{
    int tw = (int)strlen(s) * FONT_ADVANCE;
    if (tw <= w) { ui_text(x, y, s, color); return; }
    {
        int span = tw - w + 24, off = (t / 2) % (span + 60);
        off = off < 30 ? 0 : off - 30;
        if (off > span - 24) off = span - 24;
        text_clip(x - off, y, s, color, x, x + w);
    }
}

void fp_ticker(int y, int style)
{
    static char line[300];
    static int built_at = -1000, built_n = -1;
    const UiStyle *st = ui_style(style);
    int sw = fp_sw(), t = fp_t(), n = sim_news_count(), i, len, x0 = 30, x1 = sw - 6;
    if (!opt_get(OPT_NEWS_TICKER)) return;
    if (n != built_n || t - built_at > 600) {
        int k = 0;
        line[0] = 0;
        for (i = 0; i < n && i < 4; i++) {
            const char *s = sim_news(i);
            if (!s) continue;
            if (k) { fp_strcpy(line + k, "   " STR_ICON_HEART "   ", (int)sizeof(line) - k); k += 7; }
            fp_strcpy(line + k, s, (int)sizeof(line) - k);
            k = (int)strlen(line);
        }
        if (!k) fp_strcpy(line, "All quiet on the island today...", sizeof(line));
        built_at = t;
        built_n = n;
    }
    fp_pill(2, y, sw - 4, 12, st->fill_hi, st->outline, PAL_WHITE);
    fp_pill(3, y + 1, 26, 10, PAL_UI_RED, PAL_UI_RED_D, PP(PAINT_ROSE, 2));
    ui_text(6, y + 2, "NEWS", PAL_WHITE);
    len = (int)strlen(line) * FONT_ADVANCE + 60;
    text_clip(x1 - (t % (len + (x1 - x0))), y + 2, line, st->text, x0, x1);
}

void fp_dots(int x, int y, uint8_t c)
{
    int i, t = fp_t();
    for (i = 0; i < 3; i++)
        plat_rect(x + i * 4, y - (ui_wave(t + i * 6, 24, 2) > 0), 2, 2, c);
}

/* ====================================================================== */
/* chibis / portraits / figures on a render budget                           */
#define MEMO_N 40
typedef struct { uint32_t key, hash, seen; } RenderMemo;
static RenderMemo memo[MEMO_N];
static uint32_t budget_frame = 0xFFFFFFFFu;
static int budget;

static int budget_cap(void)
{
    const char *n = plat_name();
    return (n && n[0] == 'G') ? 4 : 12;
}
static int budget_rate(void)
{
    const char *n = plat_name();
    return (n && n[0] == 'G') ? 1 : 6;
}

static void budget_tick(void)
{
    uint32_t f = doomo_frame_count();
    if (f != budget_frame) {
        budget += budget_rate();
        if (budget > budget_cap()) budget = budget_cap();
        budget_frame = f;
    }
}

static uint32_t look_hash(const DoomoLook *l, int status)
{
    const uint8_t *p = (const uint8_t *)l;
    uint32_t h = 2166136261u ^ (uint32_t)status;
    int i;
    for (i = 0; i < (int)sizeof(DoomoLook); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* 1 if this image may be drawn now (cached recently, or budget allows). */
static int memo_ok(uint32_t key, uint32_t hash, int cost)
{
    uint32_t now = doomo_frame_count();
    int i, oldest = 0;
    budget_tick();
    for (i = 0; i < MEMO_N; i++) {
        if (memo[i].key == key && memo[i].hash == hash && memo[i].seen &&
            now - memo[i].seen < 900) {
            memo[i].seen = now ? now : 1;
            return 1;
        }
        if (memo[i].seen < memo[oldest].seen) oldest = i;
    }
    if (budget < cost) return 0;
    budget -= cost;
    memo[oldest].key = key;
    memo[oldest].hash = hash;
    memo[oldest].seen = now ? now : 1;
    return 1;
}

static void placeholder(int cx, int cy, int r, uint8_t fur)
{
    int t = fp_t(), b = ui_wave(t + cx * 7, 30, 1);
    fp_ball(cx, cy + b, r, fur);
    plat_rect(cx - r / 3 - 1, cy + b - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(cx + r / 3, cy + b - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
    if (r >= 6) plat_rect(cx - 1, cy + b + 2, 2, 1, PP(PAINT_ROSE, 1));
}

int fp_chibi(int id, int x, int y, int dir, int frame, int flags)
{
    const DoomoResident *r = fp_res(id);
    uint32_t key;
    if (!r) return 0;
    key = 0x10000000u | ((uint32_t)id << 8) | ((uint32_t)(dir & 3) << 4) | (uint32_t)(frame & 15);
    if (memo_ok(key, look_hash(&r->look, r->status), 1)) {
        const DoomoImage *img = bodyspr_image(id, dir, frame);
        if (img) {
            plat_image(x, y, img, 1, flags, 0);
            return 1;
        }
    }
    placeholder(x + BODYSPR_W / 2, y + BODYSPR_H - 9, 6, look_skin(&r->look, 1));
    return 0;
}

void fp_portrait(int x, int y, int id, int scale, int expr, int style)
{
    const DoomoResident *r = fp_res(id);
    const UiStyle *st = ui_style(style);
    int w = PORTRAIT_W * scale + 6, h = PORTRAIT_H * scale + 6;
    uint32_t key;
    if (scale < 1) scale = 1;
    if (expr < 0) expr = fp_mood_expr(r);
    ui_round_rect(x, y, w, h, 4, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, st->fill_hi);
    plat_rect(x + 2, y + h * 2 / 3, w - 4, h / 3 - 2, st->fill);
    plat_checker(x + 2, y + h * 2 / 3 - 1, w - 4, 1, st->fill);
    ui_heart(x + w - 8, y + 8, 2, st->fill_lo);
    ui_heart(x + 7, y + h / 2, 2, st->fill_lo);
    if (!r) return;
    key = 0x20000000u | ((uint32_t)id << 8) | (uint32_t)(expr & 0xFF);
    if (memo_ok(key, look_hash(&r->look, r->status), 3)) {
        const DoomoImage *img = portrait_image(id, expr);
        if (img) plat_image(x + 3, y + 3, img, scale, 0, 0);
    } else {
        placeholder(x + w / 2, y + h / 2, 10 * scale, look_skin(&r->look, 1));
    }
}

void fp_figure(int x, int y, const DoomoLook *l, int status, int expr)
{
    uint32_t key;
    if (!l) return;
    key = 0x30000000u | ((uint32_t)status << 8) | (uint32_t)(expr & 0xFF);
    if (memo_ok(key, look_hash(l, status), 4)) {
        const DoomoImage *img = figure_image(l, status, expr);
        if (img) plat_image(x, y, img, 1, 0, 0);
    } else {
        placeholder(x + FIGURE_W / 2, y + FIGURE_H / 2, 12, look_skin(l, 1));
    }
}

/* ====================================================================== */
/* particles                                                                  */
#define FX_N 48
typedef struct { int16_t x, y, vx, vy; uint8_t kind, life, max, color; } Fx;
static Fx fx[FX_N];

void fp_fx_clear(void) { memset(fx, 0, sizeof(fx)); }

void fp_fx_burst(int x, int y, int kind, int n)
{
    static const uint8_t confetti[6] = {
        PP(PAINT_ROSE, 2), PP(PAINT_LEMON, 1), PP(PAINT_MINT, 1), PP(PAINT_SKY, 1),
        PP(PAINT_LAVENDER, 1), PP(PAINT_TANGERINE, 1)
    };
    int i, k = 0;
    for (i = 0; i < FX_N && k < n; i++) {
        Fx *f = &fx[i];
        if (f->life) continue;
        k++;
        f->kind = (uint8_t)kind;
        f->x = (int16_t)(x * 16 + rng_between(&doomo_rng, -40, 40));
        f->y = (int16_t)(y * 16 + rng_between(&doomo_rng, -24, 24));
        f->vx = (int16_t)rng_between(&doomo_rng, -18, 18);
        f->vy = (int16_t)rng_between(&doomo_rng, -30, -10);
        f->max = f->life = (uint8_t)rng_between(&doomo_rng, 24, 44);
        switch (kind) {
        case FX_HEART: f->color = PP(PAINT_ROSE, 1); break;
        case FX_STAR: case FX_COIN: f->color = PAL_UI_GOLD; break;
        case FX_SPARK: f->color = PAL_WHITE; break;
        case FX_NOTE: f->color = PP(PAINT_GRAPE, 1); break;
        case FX_STINK: f->color = PP(PAINT_CHARTREUSE, 0); f->vy = (int16_t)(f->vy / 2); break;
        case FX_TEAR: f->color = PP(PAINT_SKY, 1); f->vy = (int16_t)(f->vy / 3); break;
        case FX_PETAL: f->color = PP(PAINT_BUBBLEGUM, 2); f->vy = (int16_t)(f->vy / 4); break;
        case FX_SKULL: f->color = PAL_RAMP(RAMP_BONE, 6); break;
        case FX_BUBBLE: f->color = PP(PAINT_CYAN, 2); break;
        default: f->color = confetti[rng_range(&doomo_rng, 6)]; f->vy = (int16_t)(f->vy * 3 / 2); break;
        }
    }
}

static void fx_step(void)
{
    int i;
    for (i = 0; i < FX_N; i++) {
        Fx *f = &fx[i];
        if (!f->life) continue;
        f->life--;
        f->x = (int16_t)(f->x + f->vx);
        f->y = (int16_t)(f->y + f->vy);
        switch (f->kind) {
        case FX_HEART: case FX_NOTE: case FX_STINK: case FX_BUBBLE: case FX_SKULL:
            f->vx = (int16_t)(f->vx * 7 / 8 + ui_wave(f->life * 8 + i * 13, 64, 3));
            f->vy = (int16_t)(f->vy * 15 / 16 - 1);
            break;
        case FX_PETAL:
            f->vx = (int16_t)(ui_wave(f->life * 6 + i * 17, 48, 10));
            f->vy = (int16_t)DOOMO_MIN(f->vy + 1, 10);
            break;
        default:
            f->vx = (int16_t)(f->vx * 15 / 16);
            f->vy = (int16_t)(f->vy + 3);
            break;
        }
    }
}

void fp_fx_draw(void)
{
    int i;
    if (plat_draw_skipped()) return;
    for (i = 0; i < FX_N; i++) {
        const Fx *f = &fx[i];
        int x, y, age;
        char g[2];
        if (!f->life) continue;
        x = f->x / 16;
        y = f->y / 16;
        age = f->max - f->life;
        g[1] = 0;
        switch (f->kind) {
        case FX_HEART: ui_heart(x, y, age < 4 ? 2 : f->life < 8 ? 2 : 3, f->color); break;
        case FX_STAR: ui_star(x, y, f->life < 8 ? 2 : 3, f->color); break;
        case FX_SPARK: ui_sparkle(x, y, (age * 2) & 15, f->color); break;
        case FX_NOTE: g[0] = (char)ICON_NOTE; plat_text(x - 2, y - 4, g, f->color); break;
        case FX_SKULL: g[0] = (char)ICON_SKULL; plat_text(x - 2, y - 4, g, f->color); break;
        case FX_STINK:
            plat_rect(x, y, 1, 2, f->color);
            plat_rect(x + 1, y - 2, 1, 2, f->color);
            plat_rect(x, y - 4, 1, 2, f->color);
            break;
        case FX_TEAR:
            plat_rect(x, y, 2, 2, f->color);
            plat_rect(x, y - 1, 1, 1, PAL_WHITE);
            break;
        case FX_BUBBLE:
            ui_fill_circle(x, y, 2, f->color);
            plat_rect(x - 1, y - 1, 1, 1, PAL_WHITE);
            break;
        case FX_COIN:
            ui_fill_circle(x, y, 2, PAL_RAMP(RAMP_CANDY, 3));
            plat_rect(x - 1, y - 1, 2, 2, f->color);
            break;
        case FX_PETAL:
            plat_rect(x, y, 2, 1, f->color);
            plat_rect(x + 1, y + 1, 1, 1, ui_darker(f->color));
            break;
        default:
            plat_rect(x, y, (age / 3) & 1 ? 1 : 2, (age / 3) & 1 ? 2 : 1, f->color);
            break;
        }
    }
}

/* ====================================================================== */
/* activities                                                                 */
int fp_activity_visible(const DoomoActivity *a)
{
    if (!a || !a->scene || !a->name) return 0;
    if (doomo_town.mode == TOWN_STORY) return (a->flags & ACTF_STORY) != 0;
    return (a->flags & ACTF_FREEPLAY) != 0;
}

const char *fp_activity_reason(const DoomoActivity *a)
{
    if (!a) return "";
    if (activity_available(a)) return "";
    if (!fp_activity_visible(a)) return "Not on this island";
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter < a->min_chapter)
        return "Later in the story...";
    if (a->unlock_id != ACT_ALWAYS && !unlock_has(a->unlock_id)) {
        const DoomoUnlock *u = unlock_by_id(a->unlock_id);
        return u && u->how ? u->how : "Locked";
    }
    if ((a->flags & ACTF_NIGHT_ONLY) && !town_is_night()) return "Only at night";
    if ((a->flags & ACTF_DAY_ONLY) && town_is_night()) return "Only in the daytime";
    return "Not right now";
}

void fp_launch_with(const DoomoActivity *a, int id)
{
    if (!a || !a->scene) return;
    audio_sfx(SFX_SELECT);
    scene_push(a->scene, id);
}

static const DoomoActivity *pend_act;

static void act_picked(int id)
{
    const DoomoActivity *a = pend_act;
    pend_act = NULL;
    if (id >= 0 && a) fp_launch_with(a, id);
}

static int anyone_sleeping(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (fp_pick_match(PICK_SLEEPING, i)) return 1;
    return 0;
}

/* The strong definition: activities that need a resident first open the
 * citizen picker (the roster browser) and then push the scene with the
 * chosen id. Used by the island, the resident screen and the story. */
void activity_launch(const DoomoActivity *a)
{
    if (!a || !a->scene) return;
    if (a->flags & ACTF_NEEDS_RESIDENT) {
        int kind = PICK_LIVING;
        pend_act = a;
        if (a->location == ACTLOC_APARTMENTS && (a->flags & ACTF_NIGHT_ONLY) && anyone_sleeping())
            kind = PICK_SLEEPING;
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "%s: who?", a->name);
        fp_pick(fp_tmp, kind, -1, act_picked);
        return;
    }
    audio_sfx(SFX_SELECT);
    scene_push(a->scene, a->arg);
}
