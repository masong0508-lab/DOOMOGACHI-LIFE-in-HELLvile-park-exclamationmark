/* lib_weapons.c - SCENE_LIB_WEAPONS: the Weapon Catalog (owner: library).
 *
 * Rivet's blueprints for every weld part: L/R picks the part list (frames,
 * barrels, cores, mods, sights, stocks, magazines, underbarrels, charms),
 * SELECT filters (all / unlocked / locked), START flips the preview between
 * a CUTE and a BRUTAL weld. Each part is shown on a blueprint welded onto
 * a showcase frame (weld_view_image), with its ratings - and, for parts,
 * how much better or worse they make the showcase weapon - a line of
 * flavour text and the materials it costs. Locked parts are silhouettes
 * with the hint from the unlocks registry.
 *
 * Only one view image (idle / fire) and one icon are asked for at a time,
 * so the action module's small sprite caches never thrash.
 */
#include "lib_int.h"

#define W (lib_w.wpn)
#define N_LISTS (CLIST_WCHARM - CLIST_WFRAME + 1)

static const char *const list_names[N_LISTS] = {
    "Frames", "Barrels", "Cores", "Mods", "Sights", "Stocks", "Magazines",
    "Underbarrels", "Charms"
};
static const char *const filter_names[3] = { "", "unlocked", "locked" };
static const uint8_t list_icons[N_LISTS] = {
    ICON_GUN, ICON_ARROW_R, ICON_BOLT, ICON_WRENCH, ICON_STAR, ICON_HOUSE,
    ICON_BOMB, ICON_ARROW_D, ICON_HEART
};
/* The frame each part list is shown welded onto. */
static const uint8_t showcase[N_LISTS] = {
    0, WFRAME_ASSAULT_RIFLE, WFRAME_SMG, WFRAME_ASSAULT_RIFLE, WFRAME_SNIPER,
    WFRAME_ASSAULT_RIFLE, WFRAME_LMG, WFRAME_ASSAULT_RIFLE, WFRAME_PISTOL
};

static const char *const fl_frame[] = {
    "Old reliable. Fits any paw, any purse.",
    "Six shots and one dramatic spin.",
    "Twice the pistol, twice the posing.",
    "A pistol that drank three espressos.",
    "Sprays like a very rude garden hose.",
    "The sensible cardigan of guns.",
    "Heavier rounds, heavier opinions.",
    "The magazine lives in its tail. Cute!",
    "Belt-fed. Belt-fed. Belt-fed. Belt-fed.",
    "Spin-up time: one prayer. Then: weather.",
    "For when they should know it was you.",
    "Reach out and boop someone. Hard.",
    "Goes through demons, walls and Mondays.",
    "Quiet, medieval and extremely pointy.",
    "Arrows! Like a hug, from far away.",
    "Chk-chk. The sound demons fear most.",
    "Two barrels. Zero subtlety. Pure Doom.",
    "A shotgun with no patience at all.",
    "Comes with a cowboy spin. Yeehaw!",
    "Lob, bounce, BOOM. Bank shots welcome.",
    "Point away from face. Then from friends.",
    "Toasts marshmallows and imps alike.",
    "Hums happily. Melts unhappily.",
    "Construction-grade crowd control.",
    "Big. Friendly. Gun. Two of those are lies.",
    "Rated for trees, gates and demon kings.",
    "Folded a thousand times. Cuts once.",
    "For trouble shaped like a nail.",
    "Free, reusable and always with you.",
    "Pop! A demon's worst nightmare: whimsy.",
    "Fires teddy bears at alarming speed.",
    "Every hit is a surprise party.",
    "Sweet tooth, sweeter swing.",
    "Shoots love. Love hurts. Demons most.",
    "Squeak! Bonk! Squeak! Repeat.",
    "You will be finding it for months.",
    "Bless it at church. Watch them hiss.",
    "Labels demons GOOD JOB at high speed.",
    "Knits a sweater around your problems.",
};
static const char *const fl_barrel[] = {
    "Factory fresh. Does the job.", "Snappy handling, shorter reach.",
    "More barrel, more reach, more swagger.", "Thick walls for thick rounds.",
    "Two holes. Twice the hello.", "Three barrels. Rivet said no. We did it.",
    "Twisty! Spins shots into a tight line.", "A shotgun's very best friend.",
    "Illegal in HaHaland. Popular in Hellvile.", "Shhh. Library approved.",
    "Every shot leaves the muzzle in love.", "Twinkle twinkle, little round.",
};
static const char *const fl_core[] = {
    "Classic lead. Polite, mostly.", "Pierces armour and excuses.",
    "Every bullet a tiny birthday.", "Pellets! A buffet of hurt.",
    "One big pellet. Very rude.", "Tiny darts in a big hurry.",
    "Bottled sunshine, but angry.", "Self-propelled consequences.",
    "Bouncy, bouncy... boom.", "Hammer optional.",
    "Magnetic, unstoppable and LOUD.", "Flammable feelings.",
    "Soapy, floaty, bonk.", "Sparkle damage over time.",
    "Blessed by Father Fuzz. Demons sizzle.", "Borrowed from below. Burns twice.",
    "Freezes demons mid-tantrum.", "Charms demons into a hug. Briefly.",
    "Bounces off walls and dignity.", "Sticky, gross and effective.",
    "A sugar rush in projectile form.", "Soft impact, hard feelings.",
};
static const char *const fl_mod[] = {
    "Plain and honest.", "More shots before the click.",
    "Pointy bits for close friends.", "A knife on a stick on a gun.",
    "+10 cute, +0 damage, +100 joy.", "Keeps barrels cool, tempers calm.",
    "Faster! Hotter! Warranty void!", "Shots bounce like happy puppies.",
    "Shots that know where you live.", "Three-round bursts in harmony.",
    "Hold the trigger. Keep holding.", "Hold... hold... RELEASE!",
    "Hits heal you. Vampires approve.", "Every shot squeaks. Every. One.",
};
static const char *const fl_sight[] = {
    "Squint and believe.", "A little red friend on target.",
    "A glowing ring floats on your foe.", "Close-ups of demons' worst angles.",
    "See a pimple across the park.", "Demons glow. So does your lunch.",
    "Aim with love.", "Points first, asks never.",
};
static const char *const fl_stock[] = {
    "Light and wild.", "Steady as a church pew.", "Tucks away like a nap.",
    "Soaks up recoil and arguments.", "Spooky, light, calcium-rich.",
    "A pillow for your shoulder.",
};
static const char *const fl_mag[] = {
    "Holds the usual.", "Holds the usual, plus extra.", "Round, heavy, glorious.",
    "Snaps in before you blink.", "Never-ending (until it ends).",
    "Feeds sweets to sweet weapons.",
};
static const char *const fl_under[] = {
    "Nothing down there. Breezy.", "Steadier aim, sweatier paws.",
    "A surprise under the surprise.", "A shotgun on your gun. Yes.",
    "See the demon. Regret seeing it.", "Brrrrm. Hugs with teeth.",
    "Blows bubbles while you work.",
};
static const char *const fl_charm[] = {
    "Undecorated. Stoic.", "A tiny bear rides along for luck.",
    "Grins at your enemies for you.", "Feeling lucky? It is.",
    "Love dangles off the trigger guard.", "It's watching. Always watching.",
    "Jingles when you sneak. Oops.", "Father Fuzz blessed it. Twice.",
};

typedef struct FlavourList { const char *const *s; uint8_t n; } FlavourList;
static const FlavourList flavours[N_LISTS] = {
    { fl_frame, DOOMO_COUNTOF(fl_frame) }, { fl_barrel, DOOMO_COUNTOF(fl_barrel) },
    { fl_core, DOOMO_COUNTOF(fl_core) }, { fl_mod, DOOMO_COUNTOF(fl_mod) },
    { fl_sight, DOOMO_COUNTOF(fl_sight) }, { fl_stock, DOOMO_COUNTOF(fl_stock) },
    { fl_mag, DOOMO_COUNTOF(fl_mag) }, { fl_under, DOOMO_COUNTOF(fl_under) },
    { fl_charm, DOOMO_COUNTOF(fl_charm) },
};

static const char *flavour(int list, int i)
{
    const FlavourList *f = &flavours[list];
    return (i >= 0 && i < f->n) ? f->s[i] : "Fresh from Rivet's bench.";
}

static int clist(int tab) { return CLIST_WFRAME + tab; }

/* ---- completion -------------------------------------------------------------- */
int lib_weapons_pct(void)
{
    int l, i, n = 0, have = 0, pct;
    for (l = 0; l < N_LISTS; l++) {
        int c = weld_part_count(clist(l));
        for (i = 0; i < c; i++) {
            n++;
            have += unlock_option_ok(clist(l), i);
        }
    }
    pct = lib_pct(have, n);
    lib_have = (int16_t)have;
    lib_total = (int16_t)n;
    if (pct >= 100) unlock_grant(LIB_UL_GUN_NERD);
    return pct;
}

/* ---- the view ------------------------------------------------------------------ */
static int part_of(int i) { return W.idx[i]; }

static void rebuild(void)
{
    int i, n = 0, c = weld_part_count(clist(lib_b.tab));
    for (i = 0; i < c && n < (int)sizeof(W.idx); i++) {
        int ok = unlock_option_ok(clist(lib_b.tab), i);
        if ((lib_b.filter % 3) == 1 && !ok) continue;
        if ((lib_b.filter % 3) == 2 && ok) continue;
        W.idx[n++] = (uint8_t)i;
    }
    lib_b.count = (int16_t)n;
    lib_clamp();
    W.w_for = -1;
}

/* Build the preview weapon (and the plain showcase it is compared with). */
static void build(int part)
{
    int key = (lib_b.toggle << 12) | (lib_b.tab << 8) | part;
    DoomoWeaponStats s;
    if (W.w_for == key) return;
    W.w_for = (int16_t)key;
    if (lib_b.tab == 0) {
        weld_default(&W.w, part, lib_b.toggle ? WSTYLE_BRUTAL : WSTYLE_CUTE);
        W.base = W.w;
    } else {
        weld_default(&W.base, showcase[lib_b.tab], lib_b.toggle ? WSTYLE_BRUTAL : WSTYLE_CUTE);
        W.w = W.base;
        weld_part_set(&W.w, clist(lib_b.tab), part);
    }
    W.w.quality = W.base.quality = 80;
    weld_stats(&W.w, &s);
    weld_ratings(&s, W.rate);
    weld_stats(&W.base, &s);
    weld_ratings(&s, W.base_rate);
}

/* ---- scene ------------------------------------------------------------------------- */
static uint8_t wpn_pct;

static void wpn_enter(int arg)
{
    lib_wing_enter(LIB_WING_WEAPONS, arg);
    if (lib_b.tab >= N_LISTS) lib_b.tab = 0;
    W.w_for = -1;
    rebuild();
    wpn_pct = (uint8_t)lib_weapons_pct();
}

static void wpn_update(void)
{
    switch (lib_nav(N_LISTS, 2)) {
    case LIBE_TAB: case LIBE_FILTER: rebuild(); break;
    case LIBE_START:
        lib_b.toggle ^= 1;
        audio_sfx(lib_b.toggle ? SFX_SHOTGUN : SFX_SQUEAK);
        break;
    case LIBE_TOGGLE:
        audio_sfx(lib_b.toggle ? SFX_SHOTGUN : SFX_SQUEAK);
        break;
    case LIBE_OPEN: case LIBE_FLIP:
        if (unlock_option_ok(clist(lib_b.tab), part_of(lib_b.sel))) audio_sfx(SFX_WELD);
        break;
    case LIBE_BACK: lib_wing_leave(); break;
    default: break;
    }
}

/* ---- drawing -------------------------------------------------------------------- */
static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int p = part_of(i), ok = unlock_option_ok(clist(lib_b.tab), p);
    char g[2], buf[40];
    (void)sel;
    g[0] = (char)list_icons[lib_b.tab];
    g[1] = 0;
    plat_text(x, y + 2, g, ok ? PAL_PAINT(PAINT_COBALT, 1) : st->text_dim);
    ui_text(x + 9, y + 2, lib_fit(weld_part_name(clist(lib_b.tab), p), w - 18, buf, sizeof(buf)),
            ok ? st->text : st->text_dim);
    if (!ok) lib_padlock(x + w - 8, y + 2, st->text_dim);
}

/* Blueprint paper with a grid; the weapon sits in the middle: its side
 * view (the menu icon, scaled up) or, with paw = 1, the first-person
 * sprite as you would hold it (idle / firing). */
static void blueprint(int x, int y, int w, int h, int scale, int paw, int fire)
{
    int gx, gy, ok, p;
    const DoomoImage *img;
    ui_round_rect(x, y, w, h, 3, PAL_RAMP(RAMP_SKY, 1));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, PAL_RAMP(RAMP_SKY, 2));
    for (gx = x + 6; gx < x + w - 2; gx += 8) plat_rect(gx, y + 2, 1, h - 4, PAL_RAMP(RAMP_SKY, 3));
    for (gy = y + 6; gy < y + h - 2; gy += 8) plat_rect(x + 2, gy, w - 4, 1, PAL_RAMP(RAMP_SKY, 3));
    plat_rect(x + 4, y + h - 5, 12, 1, PAL_RAMP(RAMP_SKY, 6));        /* scale bar */
    plat_rect(x + 4, y + h - 7, 1, 3, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(x + 15, y + h - 7, 1, 3, PAL_RAMP(RAMP_SKY, 6));
    if (lib_b.count <= 0) return;
    p = part_of(lib_b.sel);
    ok = unlock_option_ok(clist(lib_b.tab), p);
    build(p);
    img = paw ? weld_view_image(&W.w, fire ? 1 : 0) : weld_icon_image(&W.w);
    if (!img) return;
    if (paw) scale = 1;
    {
        int bob = paw ? 0 : ui_wave(lib_b.t, 60, 1);
        int ix = x + (w - img->w * scale) / 2, iy = y + (h - img->h * scale) / 2 + bob;
        if (paw) iy = y + h - img->h;                  /* held from below */
        if (ok) {
            if (!paw) {
                /* dimension lines like a real drawing */
                plat_rect(ix, iy + img->h * scale + 3, img->w * scale, 1, PAL_RAMP(RAMP_SKY, 6));
                plat_rect(ix, iy + img->h * scale + 1, 1, 5, PAL_RAMP(RAMP_SKY, 6));
                plat_rect(ix + img->w * scale - 1, iy + img->h * scale + 1, 1, 5, PAL_RAMP(RAMP_SKY, 6));
                plat_image(ix + scale, iy + scale, img, scale, IMG_SILHOUETTE, PAL_RAMP(RAMP_SKY, 1));
            }
            plat_image(ix, iy, img, scale, 0, 0);
            if (paw && fire) ui_sparkle(x + w / 2, y + 8, 10, PAL_RAMP(RAMP_CANDY, 7));
        } else {
            plat_image(ix, iy, img, scale, IMG_SILHOUETTE, PAL_RAMP(RAMP_SKY, 0));
            lib_padlock(x + w / 2 - 3, y + h / 2 - 4, PAL_UI_GOLD);
        }
    }
}

static void preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int p, ok, bh;
    char buf[48];
    ui_window(x, y, w, h, WIN_CREAM);
    if (lib_b.count <= 0 || plat_draw_skipped()) return;
    p = part_of(lib_b.sel);
    ok = unlock_option_ok(clist(lib_b.tab), p);
    bh = lib_big() ? 84 : 62;
    blueprint(x + 5, y + 5, w - 10, bh, lib_big() ? 4 : 3, 0, 0);
    ui_text_shadow(x + 6, y + bh + 8, lib_fit(weld_part_name(clist(lib_b.tab), p), w - 12, buf, sizeof(buf)),
                   st->accent, st->fill_lo);
    if (ok) {
        int lines = (y + h - (y + bh + 19) - 4) / FONT_LINE;
        if (lines > 0) {
            const char *f = flavour(lib_b.tab, p);
            int skip = 0, n = ui_wrap_skip(f, w - 12, lines);
            char fb[96];
            if (n > (int)sizeof(fb) - 1) n = (int)sizeof(fb) - 1;
            memcpy(fb, f + skip, (size_t)n);
            fb[n] = 0;
            ui_text_wrap(x + 6, y + bh + 19, w - 12, fb, st->text, 1);
        }
    } else {
        const DoomoUnlock *u = unlock_find(UL_CAT_WEAPON_PART, clist(lib_b.tab), p);
        ui_text_wrap(x + 6, y + bh + 19, w - 12, u ? u->how : "Locked", st->text_dim, 1);
    }
}

static void ratings(int x, int y, int w, int compare)
{
    static const char *const abbr[WRATE_COUNT] = {
        "DMG", "RATE", "RANGE", "AIM", "AMMO", "RELOAD", "GRIP", "MOVE"
    };
    const UiStyle *st = ui_style(WIN_CREAM);
    int k, lw = 38, bw = w - lw - (compare ? 20 : 0), step = lib_big() ? 12 : 9;
    for (k = 0; k < WRATE_COUNT; k++) {
        int yy = y + k * step, d = (int)W.rate[k] - (int)W.base_rate[k];
        uint8_t c = k == WRATE_DAMAGE ? PAL_PAINT(PAINT_CHERRY, 1) :
                    k == WRATE_RATE ? PAL_PAINT(PAINT_TANGERINE, 1) :
                    k == WRATE_RANGE ? PAL_PAINT(PAINT_SKY, 1) : PAL_PAINT(PAINT_TEAL, 1);
        ui_text(x, yy, abbr[k], st->text_dim);
        ui_bar(x + lw, yy + 1, bw, 6, W.rate[k], 100, c);
        if (compare && d) {
            char b[8];
            doomo_snprintf(b, sizeof(b), "%c%d", d > 0 ? '+' : '-', d > 0 ? d : -d);
            ui_text(x + w - ui_text_width(b), yy, b, d > 0 ? PAL_PAINT(PAINT_FOREST, 1) : PAL_PAINT(PAINT_BLOOD, 1));
        }
    }
}

static void detail(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15, big = lib_big();
    int p = part_of(lib_b.sel), ok = unlock_option_ok(clist(lib_b.tab), p);
    int bw = big ? 168 : 104, bh = big ? 112 : 58, bx = x + 6, by = y + 22;
    int fire = ((lib_b.dt / 5) % 6) == 1;
    char name[48], buf[96];
    DoomoWeaponStats s;
    if (!lib_detail_window(x, y, w, h, WIN_CREAM) || plat_draw_skipped()) return;
    build(p);
    weld_name(&W.w, name, sizeof(name));
    ui_text_shadow(x + 6, y + 5, lib_fit(weld_part_name(clist(lib_b.tab), p), w / 2, buf, sizeof(buf)),
                   st->accent, st->fill_lo);
    {
        const char *ln = list_names[lib_b.tab];
        ui_text(x + w - 8 - ui_text_width(ln), y + 5, ln, st->text_dim);
    }
    ui_text(x + 6, y + 13, lib_fit(ok ? name : "??? (locked)", w - 12, buf, sizeof(buf)), st->text_dim);
    blueprint(bx, by, bw, bh, big ? 4 : 3, lib_b.sub == 1, ok && fire);
    {
        static const char *const subs[2] = { "Plan", "In paw" };
        ui_tabs(bx + 4, by + bh - 1, subs, 2, lib_b.sub, WIN_CREAM);
    }
    if (!ok) {
        const DoomoUnlock *u = unlock_find(UL_CAT_WEAPON_PART, clist(lib_b.tab), p);
        int rx = bx + bw + 8;
        ui_text(rx, by, "LOCKED", PAL_PAINT(PAINT_BLOOD, 1));
        ui_text_wrap(rx, by + 12, x + w - rx - 6, u ? u->how : "Keep welding!", st->text, 1);
        lib_detail_end(x, y, w, h, WIN_CREAM);
        return;
    }
    ratings(bx + bw + 8, by, x + w - (bx + bw + 8) - 8, lib_b.tab != 0);
    weld_stats(&W.w, &s);
    {
        int ty = by + bh + 13, lines;
        doomo_snprintf(buf, sizeof(buf), "%s  %s  DPS %d", doomo_wclass_names[s.wclass % WCLASS_COUNT],
                       s.ammo_type < AMMO_COUNT ? doomo_ammo_names[s.ammo_type] : "Melee", weld_dps(&s));
        ui_text(x + 6, ty, buf, st->text);
        ty += FONT_LINE;
        lines = (y + h - 16 - ty) / FONT_LINE;
        if (lines > 0) {
            lib_b.scroll = 0;
            lib_text_block(x + 6, ty, w - 14, lines, flavour(lib_b.tab, p), st->accent, WIN_CREAM);
        }
        /* materials */
        {
            uint8_t items[8], counts[8];
            int n = weld_cost(&W.w, items, counts, 8), i, cx = x + 6;
            ui_text(cx, y + h - 13, "Weld:", st->text_dim);
            cx += 32;
            for (i = 0; i < n && cx < x + w - 30; i++) {
                char c[8];
                const DoomoItemInfo *it = (items[i] < ITEM_COUNT) ? &doomo_items[items[i]] : NULL;
                if (!it) continue;
                c[0] = (char)it->icon;
                c[1] = 0;
                plat_text(cx, y + h - 13, c, PAL_PAINT(PAINT_RUST, 1));
                doomo_snprintf(c, sizeof(c), "x%d", counts[i]);
                ui_text(cx + 7, y + h - 13, c, st->text);
                cx += 8 + ui_text_width(c) + 4;
            }
        }
    }
    lib_detail_end(x, y, w, h, WIN_CREAM);
}

static void wpn_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    char tabn[40];
    lib_chrome(LIB_WING_WEAPONS, wpn_pct);
    if (plat_draw_skipped()) return;
    y = 18;
    if (lib_b.filter % 3)
        doomo_snprintf(tabn, sizeof(tabn), "%s, %s", list_names[lib_b.tab], filter_names[lib_b.filter % 3]);
    else
        doomo_snprintf(tabn, sizeof(tabn), "%s", list_names[lib_b.tab]);
    y += lib_tab_pill(sw / 2, y, tabn, lib_b.count, lib_b.tab, N_LISTS, WIN_CREAM);
    {
        const char *sty = lib_b.toggle ? STR_ICON_SKULL " Brutal" : STR_ICON_HEART " Cute";
        ui_text_outline(6, 5, sty, PAL_WHITE, lib_b.toggle ? PAL_PAINT(PAINT_BLOOD, 0) : PAL_PAINT(PAINT_ROSE, 0));
    }
    lw = lib_big() ? 136 : 112;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_CREAM, row);
    preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    if (lib_b.detail) {
        lib_dim_for_detail();
        detail(sw, sh);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " next L/R view SEL style " STR_ICON_B " close", WIN_CREAM);
    } else {
        lib_hint(STR_ICON_A " open L/R part SEL filter START style", WIN_CREAM);
    }
}

const DoomoScene SCENE_LIB_WEAPONS = { "LIB_WEAPONS", wpn_enter, NULL, wpn_update, wpn_draw, 0 };
