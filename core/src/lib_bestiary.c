/* lib_bestiary.c - SCENE_LIB_BESTIARY: every demon from the Gate (owner:
 * library). A list of the eleven demon kinds by category with a live
 * preview; the detail page is a specimen jar where the demon walks,
 * attacks, flinches and dies on a loop (cute confetti or brutal gibs -
 * SELECT toggles), next to its lore, weaknesses and real combat numbers.
 * Demons nobody has sighted yet are black silhouettes.
 */
#include "lib_int.h"
#include "combat_int.h"

#define N_DEMONS LIBRARY_DEMONS

enum { BK_CRITTER, BK_FLIER, BK_BRUISER, BK_BOSS };

typedef struct BeastLore {
    uint8_t     threat;     /* 1..5 skulls                                  */
    uint8_t     kind;       /* BK_*                                         */
    const char *nick;
    const char *attack;
    const char *habitat;
    const char *lore;
    const char *cute;       /* cute weakness                                */
    const char *brutal;     /* brutal weakness                              */
} BeastLore;

static const BeastLore lore[N_DEMONS] = {
    { 1, BK_CRITTER, "the Pocket Devil", "Fireballs, hug-claws",
      "Vape Cellar; everywhere after the Breach",
      "Hatched from a cinder in Vapula's ashtray, an Impling is the size of a loaf of "
      "bread and twice as crusty. It giggles before it throws a fireball. It giggles "
      "after. Some say it giggles in its sleep, in your attic, right above your bed.",
      "Tummy rubs, bubbles, and being called \"little guy\".",
      "Any shotgun. Honestly, a firm shove." },
    { 2, BK_FLIER, "Brimstone Blast, flavoured", "Vape bolts, chilling drift",
      "Cloud Nine Vapor's back room, foggy alleys",
      "A discarded flavour cartridge that grew a grudge. It drifts through walls "
      "trailing the smell of grape candy and burnt hair. If one whispers your name, "
      "do not answer: it is collecting voices for its choir.",
      "Fresh air, a big friendly fan, a spritz of holy water.",
      "Plasma. It hates being split into little wisps." },
    { 3, BK_FLIER, "the Floating Grudge", "Vape bolts, one big chomp",
      "High ceilings, open skies, your peripheral vision",
      "One enormous eye inside a ball of fluff, like a dandelion that hates you. It is "
      "slow, it is patient, and it remembers every face it has ever bitten. The kids "
      "call it Mister Puffball. It lets them. For now.",
      "Glitter in the eye. It blinks for ages.",
      "Rockets. Pop goes the puffball." },
    { 2, BK_CRITTER, "Too Fast, Too Floofy", "Sprint, lunge, chomp",
      "Hellvile's dog park, anywhere with a ball",
      "A good boy from a very bad place. It fetches anything - sticks, balls, the "
      "femurs of the unwary - and brings them back wagging. Its bark sounds like a "
      "smoke alarm. So does its bite, but wetter.",
      "Belly rubs, squeaky toys, \"who's a good boy?\"",
      "A chainsaw. It will still try to fetch it." },
    { 3, BK_CRITTER, "the Rehearsed Dodger", "Homing orbs, cartwheels",
      "The circus of the damned, haunted stages",
      "A skeleton who joined the circus of the damned and never stopped practising. "
      "It juggles homing orbs, dodges on cue and bows to applause that isn't there. "
      "Its jokes are terrible. It laughs anyway, with no lungs, which is worse.",
      "Applause. It stops mid-fight to take a bow.",
      "Hitscan. It can't dodge what it can't see coming." },
    { 2, BK_CRITTER, "the Gurgling Gourmet", "Arcing slime globs",
      "Bins, sewers, the cafe's back door",
      "Eats everything: bins, benches, a bicycle once. Then it burps it back at you as "
      "a warm green glob. Squishy, smelly and weirdly persistent, like a cold. Never "
      "let it near Brimstone Bites. It has a reservation.",
      "Breath mints. It is very self-conscious.",
      "Flamethrower. It crisps up nicely." },
    { 3, BK_FLIER, "Wakes Up Grumpy", "Dive bombs, stone claws",
      "Church roofs, clock towers, high ledges",
      "A stone bat that naps on church roofs and wakes up furious about it. It dives "
      "out of the sky claws-first with its morning breath on. The cherubs once tried "
      "to keep one as a pet. We don't talk about the cherubs any more.",
      "A warm blanket and a lullaby.",
      "Sledgehammer. It is basically gravel already." },
    { 4, BK_BRUISER, "Oink.exe", "Burst fire from a distance",
      "Hellvile's factories, the Spire's walkways",
      "Half pig, half arm cannon, all attitude. It keeps its distance, fires in bursts "
      "and squeals in binary. Its curly little tail is reinforced titanium. Its eyes "
      "glow red when it is happy, which is never.",
      "Truffles. It cannot resist a truffle.",
      "A railgun right through the circuits." },
    { 5, BK_BRUISER, "Never Skips Leg Day", "Haymakers, acid spit",
      "Hell Breach arenas, anywhere with a door to kick",
      "A wall of furious muscle that punches first and spits acid second. It cannot "
      "read, so it eats books instead; this library has banned it twice. When it "
      "roars, your whole spine hears it. Deep down it just wants a hug. A very, very "
      "strong hug.",
      "A plush the size of a car. It will hug it for hours.",
      "The BFG. Everything else just makes it angrier." },
    { 4, BK_BRUISER, "Eight Legs, One Cloud", "Vape spray volleys, scuttling",
      "Lamp posts, rafters, the underside of bridges",
      "It spins webs of solid vapour between lamp posts and waits upside down, "
      "humming. Eight legs, one fluffy cloud of a body and far too many eyes, all "
      "blinking at different times. Never walk under a lamp that smells of grape.",
      "Yarn. It cannot stop knitting once it starts.",
      "Auto shotgun. Leg day - for you." },
    { 5, BK_BOSS, "Duke of the Ninth Cloud", "Smoke rings, vape storms, minions",
      "Cloud Nine Spire, the top floor, the throne of fog",
      "Ruler of the vape-gate, tyrant of Hellvile and inventor of Unholy Grape: three "
      "phases of vapour and spite wearing a crown of smoke rings. He kept five hundred "
      "citizens prisoner to blow his clouds. He believes this was a good business "
      "plan. It was not.",
      "Being ignored. He simply cannot stand it.",
      "Everything you've got. Three times." },
};

static const char *const kind_names[] = { "All Demons", "Critters", "Fliers", "Bruisers", "Boss" };
static const char *const filter_names[] = { "", "Sighted", "Not sighted", "Defeated" };
#define N_TABS 5
#define N_FILTERS 4

static int demon_of(int i) { return THING_IMPLING + lib_w.best.idx[i]; }
/* cached on entry: the registry lookups are too slow to repeat per frame */
static int seen(int t) { return (lib_w.best.seen >> (t - THING_IMPLING)) & 1; }
static int slain(int t) { return (lib_w.best.defeated >> (t - THING_IMPLING)) & 1; }

static void cache_flags(void)
{
    int t;
    lib_w.best.seen = lib_w.best.defeated = 0;
    for (t = THING_IMPLING; t <= THING_LORD_VAPULA; t++) {
        if (library_seen(t)) lib_w.best.seen |= (uint16_t)(1u << (t - THING_IMPLING));
        if (library_defeated(t)) lib_w.best.defeated |= (uint16_t)(1u << (t - THING_IMPLING));
    }
    lib_w.best.pct = (uint8_t)lib_bestiary_pct();
}

static void rebuild(void)
{
    int i, n = 0;
    for (i = 0; i < N_DEMONS; i++) {
        int t = THING_IMPLING + i;
        if (lib_b.tab > 0 && lore[i].kind != lib_b.tab - 1) continue;
        switch (lib_b.filter % N_FILTERS) {
        case 1: if (!seen(t)) continue; break;
        case 2: if (seen(t)) continue; break;
        case 3: if (!slain(t)) continue; break;
        default: break;
        }
        lib_w.best.idx[n++] = (uint8_t)i;
    }
    lib_b.count = (int16_t)n;
    lib_clamp();
}

int lib_bestiary_pct(void)
{
    return lib_pct(library_seen_count(), N_DEMONS);
}

static void best_enter(int arg)
{
    lib_wing_enter(LIB_WING_BESTIARY, arg);
    if (lib_b.demo) {
        /* a few sightings so the book has something in it */
        lib_bit_set(LIB_UL_SEEN(THING_IMPLING));
        lib_bit_set(LIB_UL_SEEN(THING_PUFF_DEMON));
        lib_bit_set(LIB_UL_SEEN(THING_HELLHOUND_PUP));
        lib_bit_set(LIB_UL_SEEN(THING_BRUTE));
        lib_bit_set(LIB_UL_SEEN(THING_LORD_VAPULA));
    }
    if (lib_b.tab >= N_TABS) lib_b.tab = 0;
    lib_b.toggle = opt_get(OPT_GORE) == GORE_BRUTAL;
    cache_flags();
    rebuild();
}

static void best_update(void)
{
    switch (lib_nav(N_TABS, 3)) {
    case LIBE_TAB: case LIBE_FILTER: rebuild(); break;
    case LIBE_BACK: lib_wing_leave(); break;
    case LIBE_OPEN: case LIBE_FLIP: {
        int t = demon_of(lib_b.sel);
        if (seen(t)) audio_sfx(lore[t - THING_IMPLING].threat >= 4 ? SFX_DEMON_ROAR : SFX_DEMON_PAIN);
        break; }
    case LIBE_TOGGLE:
        audio_sfx(lib_b.toggle ? SFX_SPLAT : SFX_CONFETTI);
        break;
    default: break;
    }
}

/* ---- drawing ---------------------------------------------------------------- */
static void skulls(int x, int y, int n, int max)
{
    lib_pips(x, y, n, max, ICON_SKULL, PAL_PAINT(PAINT_BLOOD, 1), PAL_RAMP(RAMP_STONE, 5));
}

static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int t = demon_of(i), sn = seen(t);
    const char *name = sn ? combat_demon_name(t) : "? ? ? ? ?";
    uint8_t c = sn ? st->text : st->text_dim;
    (void)sel;
    plat_text(x, y + 2, sn ? STR_ICON_SKULL : "?", sn ? PAL_PAINT(PAINT_BLOOD, 1) : st->text_dim);
    ui_text(x + 9, y + 2, name, c);
    if (slain(t)) plat_text(x + w - 7, y + 2, STR_ICON_CHECK, PAL_PAINT(PAINT_FOREST, 1));
}

/* Gore / death effect over a dying demon. */
static void death_fx(int cx, int cy, int k, int brutal)
{
    int spr, f;
    const DoomoImage *img;
    if (k < 0 || k > 11) return;
    f = k / 3;
    if (brutal) spr = (k & 4) ? SPR_GIBS : SPR_BLOOD_SPRAY;
    else spr = (k & 4) ? SPR_HEARTS : SPR_CONFETTI;
    img = sprite_image(spr, f);
    if (img) plat_image(cx - img->w / 2, cy - img->h / 2, img, 1, 0, 0);
    if (!brutal) {
        img = sprite_image(SPR_STUFFING, f);
        if (img) plat_image(cx - img->w / 2 + 6, cy - img->h / 2 + 4, img, 1, 0, 0);
    }
}

/* The demon on stage: feet at (cx, ground). anim = ticks for the loop,
 * mode 0 = walk only (preview), 1 = the full show (detail page). */
static const char *draw_demon(int t, int cx, int ground, int anim, int mode, int brutal, int known,
                              int scale)
{
    const DemonInfo *d = DEMON(t);
    int floats = (sprite_flags(d->spr) & SPRF_FLOATS) != 0;
    int phase = anim % 150, frame = DFRAME_WALK0, shake = 0, flash = 0, lift = 0;
    const char *label = "Prowling";
    const DoomoImage *img;
    if (mode == 0) phase = anim % 60;
    if (phase < 60) {
        frame = ((phase / 8) & 1) ? DFRAME_WALK1 : DFRAME_WALK0;
    } else if (phase < 80) {
        frame = DFRAME_ATTACK;
        shake = (phase & 2) ? 1 : -1;
        label = "Attack!";
    } else if (phase < 96) {
        frame = DFRAME_PAIN;
        flash = phase < 84;
        label = "Ouch!";
    } else {
        int k = (phase - 96) / 6;
        frame = DFRAME_DEATH0 + DOOMO_MIN(k, 3);
        label = brutal ? "Gibbed!" : "Poof! Bye-bye!";
    }
    if (floats && phase < 96) lift = (8 + ui_wave(anim, 40, 3)) * scale;
    img = sprite_image(d->spr, frame);
    if (!img) return label;
    /* soft contact shadow */
    ui_fill_ellipse(cx, ground - 1, img->w * scale / 3 + 2, 2, PAL_RAMP(RAMP_STONE, 1));
    if (!known) {
        plat_image(cx - img->w * scale / 2, ground - img->h * scale - lift, img, scale, IMG_SILHOUETTE,
                   PAL_RAMP(RAMP_STONE, 1));
        return "???";
    }
    shake *= scale;
    if (flash) plat_image(cx - img->w * scale / 2 + shake, ground - img->h * scale - lift, img, scale,
                          IMG_SILHOUETTE, PAL_WHITE);
    else plat_image(cx - img->w * scale / 2 + shake, ground - img->h * scale - lift, img, scale, 0, 0);
    if (mode == 1 && phase >= 96) death_fx(cx, ground - 18 * scale, (phase - 96) / 3, brutal);
    return label;
}

static void preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int t, i, sn, cx, ground;
    const BeastLore *bl;
    char buf[48];
    ui_window(x, y, w, h, WIN_CREAM);
    if (lib_b.count <= 0 || plat_draw_skipped()) return;
    t = demon_of(lib_b.sel);
    i = t - THING_IMPLING;
    bl = &lore[i];
    sn = seen(t);
    ui_text_shadow(x + 6, y + 4, lib_fit(sn ? combat_demon_name(t) : "Unknown demon", w - 12, buf, sizeof(buf)),
                   st->accent, st->fill_lo);
    ui_text(x + 6, y + 13, lib_fit(sn ? bl->nick : "Not yet sighted", w - 12, buf, sizeof(buf)), st->text_dim);
    /* little stage: a dark arch with a spotlight */
    {
        int sx = x + 6, sy = y + 24, sw2 = w - 12, sh2 = h - 50;
        ui_round_rect(sx, sy, sw2, sh2, 4, PAL_RAMP(RAMP_BLOOD, 0));
        ui_round_rect(sx + 1, sy + 1, sw2 - 2, sh2 - 2, 3, PAL_RAMP(RAMP_STONE, 1));
        ui_fill_ellipse(sx + sw2 / 2, sy + sh2 - 6, sw2 / 3, 4, PAL_RAMP(RAMP_STONE, 2));
        cx = sx + sw2 / 2;
        ground = sy + sh2 - 5;
        /* tall sprites are drawn as they are: the stage clips nothing */
        draw_demon(t, cx, ground, lib_b.rest + lib_b.t, 0, lib_b.toggle, sn, 1);
        if (!sn) ui_text_big(cx - 5, sy + 6 + ui_wave(lib_b.t, 30, 2), "?", 2, PAL_WHITE, PAL_RAMP(RAMP_STONE, 0));
    }
    if (sn) {
        ui_text(x + 6, y + h - 22, "Threat", st->text_dim);
        skulls(x + 46, y + h - 22, bl->threat, 5);
    } else {
        ui_text(x + 6, y + h - 22, "Threat  ?????", st->text_dim);
    }
    if (slain(t)) lib_stamp(x + w / 2, y + h - 9, "SLAIN", PAL_PAINT(PAINT_BLOOD, 1));
    else if (sn) lib_stamp(x + w / 2, y + h - 9, "AT LARGE", PAL_PAINT(PAINT_COBALT, 1));
}

static void detail(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15;
    int t = demon_of(lib_b.sel), i = t - THING_IMPLING, sn = seen(t);
    const BeastLore *bl = &lore[i];
    const DemonInfo *d = DEMON(t);
    int jw = lib_big() ? 120 : 92, jx = x + 5, jy = y + 5, jh = h - 10;
    int rx = jx + jw + 6, rw = x + w - rx - 6, ry = y + 5;
    char buf[96];
    const char *label;
    if (!lib_detail_window(x, y, w, h, WIN_CREAM) || plat_draw_skipped()) return;
    /* the specimen jar */
    ui_round_rect(jx, jy + 6, jw, jh - 6, 8, PAL_RAMP(RAMP_SKY, 2));
    ui_round_rect(jx + 1, jy + 7, jw - 2, jh - 8, 7, lib_b.toggle ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 2));
    plat_rect(jx + 2, jy + jh - 18, jw - 4, 14, lib_b.toggle ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 1));
    plat_checker(jx + 2, jy + jh - 19, jw - 4, 1, lib_b.toggle ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 1));
    /* lid */
    ui_round_rect(jx + 6, jy, jw - 12, 8, 2, PAL_RAMP(RAMP_STEEL, 3));
    plat_rect(jx + 8, jy + 1, jw - 16, 1, PAL_RAMP(RAMP_STEEL, 6));
    /* rising bubbles in the jar */
    {
        int b;
        for (b = 0; b < 5; b++) {
            uint32_t hs = doomo_hash32((uint32_t)b * 131u + 9u);
            int span = jh - 30, by = jy + jh - 20 - (int)((lib_b.t * (2 + b % 3) / 3 + hs) % (uint32_t)span);
            int bx = jx + 8 + (int)(hs % (uint32_t)(jw - 16)) + ui_wave(lib_b.t + b * 17, 40, 2);
            ui_fill_circle(bx, by, 1 + (b & 1), lib_b.toggle ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_VAPE, 4));
            plat_rect(bx - 1, by - 1, 1, 1, PAL_WHITE);
        }
    }
    {
        const DoomoImage *f0 = sprite_image(d->spr, 0);
        int scale = (f0 && f0->w * 2 + 12 <= jw && f0->h * 2 + 40 <= jh) ? 2 : 1;
        label = draw_demon(t, jx + jw / 2, jy + jh - 8, lib_b.dt, 1, lib_b.toggle, sn, scale);
    }
    /* glass shine on top of everything */
    plat_rect(jx + 4, jy + 14, 2, jh / 3, PAL_WHITE);
    plat_rect(jx + 4, jy + 16 + jh / 3, 2, 4, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(jx + jw - 6, jy + 12, 1, jh / 4, PAL_RAMP(RAMP_SKY, 6));
    /* label tag */
    doomo_snprintf(buf, sizeof(buf), "No.%02d", i + 1);
    ui_round_rect(jx + 4, jy + 10, ui_text_width(buf) + 6, 11, 2, PAL_UI_CREAM);
    ui_text(jx + 7, jy + 12, buf, PAL_RAMP(RAMP_WOOD, 2));
    if (sn) ui_text_center(jx + jw / 2, jy + jh - 14, label, PAL_WHITE);
    /* the page */
    if (!sn) {
        ui_text_shadow(rx, ry, "Unknown demon", st->accent, st->fill_lo);
        ui_text_wrap(rx, ry + 14, rw, "Not yet sighted. Spot one in a Hell Breach or in the Arena "
                     "and this page will fill itself in. (Quillby's quill is enchanted.)", st->text, 1);
        doomo_snprintf(buf, sizeof(buf), "Rumoured: %s", bl->habitat);
        ui_text_wrap(rx, ry + 14 + 6 * FONT_LINE, rw, buf, st->text_dim, 1);
        lib_detail_end(x, y, w, h, WIN_CREAM);
        return;
    }
    ui_text_shadow(rx, ry, combat_demon_name(t), st->accent, st->fill_lo);
    ui_text(rx, ry + 10, lib_fit(bl->nick, rw, buf, sizeof(buf)), st->text_dim);
    skulls(rx, ry + 20, bl->threat, 5);
    if (bl->kind == BK_BOSS) ui_star(rx + 40, ry + 23, 4, PAL_UI_GOLD);
    {
        static const char *const subs[3] = { "Lore", "Weak", "Stats" };
        int ty = ry + 32;
        ui_tabs(rx, ty, subs, 3, lib_b.sub, WIN_CREAM);
        ty += 15;
        {
            int lines = (y + h - 22 - ty) / FONT_LINE;
            if (lines < 2) lines = 2;
            if (lib_b.sub == 0) {
                lib_text_block(rx, ty, rw - 6, lines, bl->lore, st->text, WIN_CREAM);
            } else if (lib_b.sub == 1) {
                char wbuf[200];
                doomo_snprintf(wbuf, sizeof(wbuf), STR_ICON_HEART " Cute: %s\n" STR_ICON_SKULL " Brutal: %s",
                               bl->cute, bl->brutal);
                lib_text_block(rx, ty, rw - 6, lines, wbuf, st->text, WIN_CREAM);
            } else {
                char v[24];
                int ly = ty;
                lib_b.scroll_max = 0;
                doomo_snprintf(v, sizeof(v), "%d", d->hp);
                lib_kv(rx, ly, rw - 4, "Health", v, st->text_dim, st->text); ly += FONT_LINE;
                doomo_snprintf(v, sizeof(v), "%s", d->speed >= 40 ? "Zoomy" : d->speed >= 28 ? "Brisk" :
                               d->speed >= 20 ? "Steady" : "Lumbering");
                lib_kv(rx, ly, rw - 4, "Speed", v, st->text_dim, st->text); ly += FONT_LINE;
                doomo_snprintf(v, sizeof(v), "%d", d->melee_dmg);
                lib_kv(rx, ly, rw - 4, "Bite", v, st->text_dim, st->text); ly += FONT_LINE;
                if (d->pk) {
                    doomo_snprintf(v, sizeof(v), "%d x%d", d->shot_dmg, d->burst ? d->burst : 1);
                    lib_kv(rx, ly, rw - 4, "Shots", v, st->text_dim, st->text); ly += FONT_LINE;
                }
                if (ly + 2 * FONT_LINE <= y + h - 22) {
                    doomo_snprintf(buf, sizeof(buf), "Attack: %s", bl->attack);
                    ly += FONT_LINE * ui_text_wrap(rx, ly, rw - 4, buf, st->text, 1);
                }
                if (ly + 2 * FONT_LINE <= y + h - 22) {
                    doomo_snprintf(buf, sizeof(buf), "Found: %s", bl->habitat);
                    ui_text_wrap(rx, ly, rw - 4, buf, st->text_dim, 1);
                }
            }
        }
    }
    if (slain(t)) lib_stamp(rx + rw / 2, y + h - 12, "SLAIN " STR_ICON_CHECK, PAL_PAINT(PAINT_BLOOD, 1));
    else lib_stamp(rx + rw / 2, y + h - 12, "AT LARGE", PAL_PAINT(PAINT_COBALT, 1));
    lib_detail_end(x, y, w, h, WIN_CREAM);
}

static void best_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    char tabn[24];
    lib_chrome(LIB_WING_BESTIARY, lib_w.best.pct);
    if (plat_draw_skipped()) return;
    y = 18;
    if (lib_b.filter % N_FILTERS)
        doomo_snprintf(tabn, sizeof(tabn), "%s: %s", kind_names[lib_b.tab] + (lib_b.tab == 0 ? 4 : 0),
                       filter_names[lib_b.filter % N_FILTERS]);
    else
        doomo_snprintf(tabn, sizeof(tabn), "%s", kind_names[lib_b.tab]);
    y += lib_tab_pill(sw / 2, y, tabn, lib_b.count, lib_b.tab, N_TABS, WIN_CREAM);
    lw = lib_big() ? 136 : 104;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_CREAM, row);
    preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    if (lib_b.detail) {
        plat_shade_rect(0, 0, sw, sh, 9);
        detail(sw, sh);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " page  L/R tab  SEL gore  " STR_ICON_B " close", WIN_CREAM);
    } else {
        lib_hint(STR_ICON_A " open  L/R kind  SEL filter  " STR_ICON_B " back", WIN_CREAM);
    }
}

const DoomoScene SCENE_LIB_BESTIARY = { "LIB_BESTIARY", best_enter, NULL, best_update, best_draw, 0 };
