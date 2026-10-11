/* test_concert.c - the Concert Hall + Dreams expansion (owner: concert). */
#include <stdlib.h>
#include "test.h"
#include "../src/concert_int.h"

static void premade_island(void)
{
    town_new(TOWN_FREEPLAY, 777, TOWN_POP_PREMADE);
    sim_init();
}

static void run(uint32_t b, int n)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}

static void tap(uint32_t b) { run(b, 1); run(0, 2); }

static int printable(const char *s)
{
    for (; *s; s++) if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x9F) return 0;
    return 1;
}

/* ---- songs ---------------------------------------------------------------- */
TEST(concert_song_is_deterministic)
{
    ConcertSong a, b;
    premade_island();
    concert_song_make(&a, 42, 3, 1);
    concert_song_make(&b, 42, 3, 1);
    CHECK_EQ_U(concert_song_hash(&a), concert_song_hash(&b));
    concert_song_make(&b, 42, 3, 2);
    CHECK(concert_song_hash(&a) != concert_song_hash(&b));
    concert_song_make(&b, 43, 3, 1);
    CHECK(concert_song_hash(&a) != concert_song_hash(&b));
    CHECK(a.bpm >= concert_genres[3].bpm - 4 && a.bpm <= concert_genres[3].bpm + 4);
}

TEST(concert_lyrics_fit_and_are_personal)
{
    int id, g;
    premade_island();
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id += 13) {
        const DoomoResident *r = town_res(id);
        for (g = 0; g < MUSIC_COUNT; g += 5) {
            ConcertSong s;
            int l;
            concert_song_make(&s, id, g, id & 3);
            CHECK_MSG(s.title[0] && strlen(s.title) <= CONCERT_TITLE_LEN - 1 && printable(s.title),
                      "title of %d: \"%s\"", id, s.title);
            for (l = 0; l < CONCERT_LINES; l++) {
                uint8_t st[CONCERT_SYL_MAX];
                int n = concert_syllables(s.lines[l], st, CONCERT_SYL_MAX);
                CHECK_MSG(s.lines[l][0] && strlen(s.lines[l]) <= CONCERT_LINE_LEN - 1 && printable(s.lines[l]),
                          "line %d of %d: \"%s\"", l, id, s.lines[l]);
                CHECK_MSG(n >= 1 && n <= CONCERT_SYL_MAX, "syllables of \"%s\": %d", s.lines[l], n);
            }
            /* verse 1 introduces the singer by name */
            CHECK_MSG(strstr(s.lines[0], r->name) != NULL, "intro \"%s\" lacks %s", s.lines[0], r->name);
            CHECK_EQ(s.edgy, RES_DISTRICT(id) || (r->flags & RF_HELLBORN) ? 1 : 0);
        }
    }
}

TEST(concert_syllables_split_words)
{
    uint8_t st[CONCERT_SYL_MAX];
    CHECK_EQ(concert_syllables("cupcakes", st, CONCERT_SYL_MAX), 2);
    CHECK_EQ(st[1], 3);                                  /* cup|cakes */
    CHECK_EQ(concert_syllables("Ate my heart", st, CONCERT_SYL_MAX), 3);
    CHECK_EQ(concert_syllables("Little old me", st, CONCERT_SYL_MAX), 4);
    CHECK_EQ(concert_syllables("", st, CONCERT_SYL_MAX), 0);
    CHECK_EQ(concert_syllables("la la la la la la la la la la la la la la la la la la la la la la",
                               st, CONCERT_SYL_MAX), CONCERT_SYL_MAX);
}

TEST(concert_band_names_and_reviews)
{
    char buf[32];
    int s;
    premade_island();
    concert_band_name(42, 0, 1, buf, sizeof(buf));
    CHECK(buf[0] && strlen(buf) <= 18 && printable(buf));
    concert_band_name(666, 14, 7, buf, sizeof(buf));
    CHECK(buf[0] && strlen(buf) <= 18);
    for (s = 1; s <= 5; s++) {
        CHECK(concert_review(s, 0, 0, 3)[0]);
        CHECK(concert_review(s, 14, 1, 3)[0]);
    }
}

/* ---- the act -------------------------------------------------------------- */
TEST(concert_choir_of_25)
{
    ConcertCast c;
    int i, j, sec[SECTION_COUNT] = { 0 };
    uint32_t all;
    premade_island();
    concert_cast_make(&c, 77, CMODE_CHOIR, 25, CHOIR_SECTIONS);
    CHECK_EQ(c.n, 25);
    CHECK_EQ(c.ids[0], 77);
    all = (1u << c.n) - 1u;
    for (i = 0; i < c.n; i++) {
        const DoomoResident *r = town_res(c.ids[i]);
        CHECK(r && r->status != RES_EMPTY && r->status != RES_DEAD);
        for (j = 0; j < i; j++) CHECK(c.ids[i] != c.ids[j]);
        CHECK(c.section[i] < SECTION_COUNT);
        sec[c.section[i]]++;
    }
    for (i = 0; i < SECTION_COUNT; i++) CHECK(sec[i] >= 5);
    for (i = 0; i < CONCERT_LINES; i++) CHECK(c.line_mask[i] && (c.line_mask[i] & ~all) == 0);
    for (i = 4; i < 8; i++) CHECK_EQ_U(c.line_mask[i], all);          /* everyone sings the chorus */
    CHECK_EQ_U(c.line_mask[0], 1u);                                    /* the lead starts solo */
    CHECK_EQ(concert_mask_count(c.line_mask[2]), 2);                   /* a duet */
}

TEST(concert_band_has_four_mates)
{
    ConcertCast c;
    int i;
    premade_island();
    concert_cast_make(&c, 13, CMODE_BAND, 0, 0);
    CHECK_EQ(c.nband, BAND_COUNT);
    for (i = 0; i < BAND_COUNT; i++) {
        CHECK(c.band[i] != CONCERT_NOBODY && c.band[i] != 13);
        CHECK(town_res(c.band[i])->status != RES_EMPTY);
    }
    concert_cast_make(&c, 13, CMODE_SOLO, 0, 0);
    CHECK_EQ(c.n, 1);
    CHECK_EQ(c.band[0], CONCERT_NOBODY);
}

TEST(concert_cast_on_a_lonely_island)
{
    ConcertCast c;
    int id = town_alloc(DISTRICT_HILLVILE);
    DoomoResident *r = town_res(id);
    CHECK(id >= 0);
    resident_generate(r, 5);
    concert_cast_make(&c, id, CMODE_CHOIR, 25, CHOIR_RISERS);
    CHECK_EQ(c.n, 1);                              /* nobody else to sing */
    CHECK_EQ_U(c.line_mask[4], 1u);
}

/* ---- the player ----------------------------------------------------------- */
TEST(concert_player_plays_and_sings)
{
    static ConcertSong s;
    static ConcertCast c;
    ConcertPlayer p;
    int ticks = 0, onsets = 0, bars = 0, ev, lines = 0, last_line = -1;
    uint32_t calls0 = nat_tone_calls;
    premade_island();
    concert_song_make(&s, 1, 0, 0);
    concert_cast_make(&c, 1, CMODE_CHOIR, 6, CHOIR_RISERS);
    concert_play_start(&p, &s, &c, 0);
    do {
        ev = concert_play_tick(&p);
        ticks++;
        if (ev & PEV_ONSET) onsets++;
        if (ev & PEV_BAR) bars++;
        if (p.line >= 0 && p.line != last_line) { lines++; last_line = p.line; }
    } while (!(ev & PEV_END) && ticks < 5000);
    CHECK(ev & PEV_END);
    CHECK_EQ(bars, SONG_BARS);
    /* 20 bars of 16 steps at bpm: 450/bpm ticks per step */
    CHECK(ticks >= SONG_BARS * 16 * 450 / (s.bpm + 1) - 2 && ticks <= SONG_BARS * 16 * 450 / (s.bpm - 1) + 4);
    CHECK(onsets > 40);
    CHECK_EQ(lines, 16);                           /* 4 + 4 + 4 + 4 sung lines */
    CHECK(nat_tone_calls > calls0 + 100);
    /* the encore is chorus + outro, faster */
    concert_play_start(&p, &s, &c, 1);
    bars = 0;
    do { ev = concert_play_tick(&p); if (ev & PEV_BAR) bars++; } while (!(ev & PEV_END));
    CHECK_EQ(bars, SONG_ENCORE_BARS);
}

/* ---- the Music Studio export -------------------------------------------- */
TEST(concert_export_to_song_format)
{
    static ConcertSong s;
    static ConcertCast c;
    static DoomoSong d;
    int p, t, st, i;
    premade_island();
    concert_song_make(&s, 77, 10, 0);
    concert_cast_make(&c, 77, CMODE_CHOIR, 12, CHOIR_PYRAMID);
    concert_song_export(&s, &c, &d);
    CHECK(d.flags & SONGF_VALID);
    CHECK_STR(d.title, s.title);
    CHECK(d.bpm >= 60 && d.bpm <= 220 && d.key < 12 && d.scale < SONG_SCALE_COUNT);
    CHECK(d.order_len >= 1 && d.order_len <= SONG_ORDER_MAX);
    for (i = 0; i < d.order_len; i++) CHECK(d.order[i] < SONG_PATTERNS);
    for (p = 0; p < SONG_PATTERNS; p++)
        for (t = 0; t < SONG_TRACKS; t++)
            for (st = 0; st < SONG_STEPS; st++) {
                int v = d.notes[p][t][st];
                if (t == 3) CHECK(v < SONG_DRUM_COUNT);
                else CHECK(v < 128);
            }
    for (i = 0; i < SONG_LYRIC_LINES; i++) CHECK_STR(d.lyrics[i], s.lines[i]);
    CHECK_EQ(d.singer[0], 77);
    CHECK_EQ(d.singer[11], c.ids[11]);
    CHECK_EQ(d.singer[12], SONG_NO_SINGER);
    CHECK_EQ_U(d.line_singers[4], c.line_mask[4]);
    CHECK_EQ(d.choir_layout, CHOIR_PYRAMID);
    CHECK_EQ_U(d.seed, s.seed);
    /* the tune survives: the chorus' first note is the lead track's */
    CHECK_EQ(d.notes[2][0][0] >= 2 ? d.notes[2][0][0] : 0, s.mel[1][0] >= 2 ? s.mel[1][0] : 0);
}

/* ---- singles ---------------------------------------------------------------- */
TEST(concert_singles_chart)
{
    ConcertSingle s;
    CHECK_EQ(concert_single_count(), 0);
    CHECK(!concert_single_get(0, &s));
    concert_single_save(10, 1, 0);
    concert_single_save(20, 2, 1);
    concert_single_save(30, 3, 2);
    concert_single_save(40, 4, 3);
    concert_single_save(50, 5, 0);                 /* #4 (10) drops off */
    CHECK_EQ(concert_single_count(), 4);
    CHECK(concert_single_get(0, &s) && s.singer == 50 && s.genre == 5 && s.take == 0);
    CHECK(concert_single_get(3, &s) && s.singer == 20);
    concert_single_save(30, 3, 2);                 /* back to #1, no duplicate */
    CHECK_EQ(concert_single_count(), 4);
    CHECK(concert_single_get(0, &s) && s.singer == 30 && s.take == 2);
    CHECK(concert_single_get(1, &s) && s.singer == 50);
    concert_single_remove(0);
    CHECK_EQ(concert_single_count(), 3);
    CHECK(concert_single_get(0, &s) && s.singer == 50);
    concert_single_save(DOOMO_PLAYER_ID, 15, 3);   /* the player can sing too */
    CHECK(concert_single_get(0, &s) && s.singer == DOOMO_PLAYER_ID && s.genre == 15 && s.take == 3);
    CHECK(!concert_single_unpack(0xFFFF, &s));
}

/* ---- registration ----------------------------------------------------------- */
TEST(concert_unlocks_registered)
{
    int i, n = 0;
    for (i = 0; doomo_unlocks_concert[i].name; i++, n++) {
        const DoomoUnlock *u = &doomo_unlocks_concert[i];
        CHECK(u->id >= UL_CONCERT_FIRST && u->id < UL_DREAM_JOURNAL);
        CHECK(unlock_by_id(u->id) == u);           /* unique across every table */
        CHECK(u->how && u->how[0]);
    }
    CHECK_EQ(n, UL_CONCERT_LAST - UL_CONCERT_FIRST);
    for (i = UL_DREAM_JOURNAL; i < UL_DREAM_JOURNAL + DREAM_VIGNETTES; i++) CHECK(unlock_by_id(i) == NULL);
    CHECK(concert_stage_unlocked(STAGE_SUGAR_BOWL));
    CHECK(!concert_stage_unlocked(STAGE_HELLFIRE));
    progress_add(PROG_CONCERTS, 5);
    CHECK(concert_stage_unlocked(STAGE_HELLFIRE));
    CHECK(unlock_has(UL_PART_BAND_TEE));
    CHECK(scene_by_name("CONCERT") == &SCENE_CONCERT);
    CHECK(scene_by_name("DREAM") == &SCENE_DREAM);
}

TEST(concert_activities_registered)
{
    const DoomoActivity *a[16];
    int n, i, concert = 0, parade = 0, dream = 0;
    doomo_town.minute = 12 * 60;
    n = activities_at(ACTLOC_CONCERT_HALL, a, 16);
    for (i = 0; i < n; i++) {
        if (a[i]->scene == &SCENE_CONCERT && (a[i]->flags & ACTF_NEEDS_RESIDENT)) concert = 1;
        if (a[i]->scene == &SCENE_CONCERT && a[i]->arg == CONCERT_ARG_PARADE) parade = 1;
        CHECK(strlen(a[i]->desc) <= 40);
    }
    CHECK(concert && parade);
    n = activities_at(ACTLOC_APARTMENTS, a, 16);
    for (i = 0; i < n; i++) if (a[i]->scene == &SCENE_DREAM) dream = 1;
    CHECK(!dream);                                  /* dreams are for the night */
    doomo_town.minute = 23 * 60;
    n = activities_at(ACTLOC_APARTMENTS, a, 16);
    for (i = 0; i < n; i++) if (a[i]->scene == &SCENE_DREAM) dream = 1;
    CHECK(dream);
}

/* ---- dreams ------------------------------------------------------------------ */
TEST(dream_plan_is_deterministic)
{
    uint8_t a[DREAM_SHOW], b[DREAM_SHOW];
    int na, nb, i, j, id, nightmares = 0;
    premade_island();
    na = dream_plan(42, 7, a, 0);
    nb = dream_plan(42, 7, b, 0);
    CHECK_EQ(na, nb);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    for (i = 0; i < DREAM_SHOW; i++) {
        CHECK(a[i] < DREAM_VIGNETTES);
        for (j = 0; j < i; j++) CHECK(a[i] != a[j]);
    }
    CHECK(dream_plan(42, 7, a, 1));                 /* forced */
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id += 3) nightmares += dream_plan(id, 3, a, 0);
    CHECK(nightmares > 5 && nightmares < 120);      /* rare */
    town_res(42)->status = RES_POSSESSED;
    CHECK(dream_plan(42, 7, a, 0));                 /* the possessed always do */
}

TEST(dream_captions_mix_the_sleepers_life)
{
    DreamCast c;
    char buf[112];
    int v, split;
    premade_island();
    dream_cast(42, &c);
    for (v = 0; v < DREAM_VIGNETTES; v++) {
        split = dream_caption(v, &c, 0, buf, sizeof(buf));
        CHECK_MSG(split > 4 && split < (int)strlen(buf) && printable(buf), "vignette %d: %s", v, buf);
        CHECK(strlen(dream_vignette_names[v]) > 0);
    }
    dream_caption(2, &c, 0, buf, sizeof(buf));
    CHECK(strstr(buf, town_res(42)->name) != NULL);
    dream_caption(0, &c, 0, buf, sizeof(buf));
    {
        char food[32];
        int i;
        for (i = 0; doomo_food_names[c.food][i] && i < 31; i++)
            food[i] = (char)(doomo_food_names[c.food][i] | 0x20);
        food[i] = 0;
        CHECK_MSG(strstr(buf, food) != NULL, "%s / %s", buf, food);
    }
    split = dream_caption(5, &c, 1, buf, sizeof(buf));
    CHECK(strstr(buf, "AAAH") != NULL);
}

TEST(dream_journal_fills)
{
    CHECK_EQ(dream_journal_count(), 0);
    CHECK(dream_journal_mark(3));
    CHECK(!dream_journal_mark(3));
    CHECK(dream_journal_has(3));
    CHECK(!dream_journal_has(4));
    CHECK_EQ(dream_journal_count(), 1);
    CHECK_EQ(ui_toast_pending(), 0);                /* raw bits: no toast */
    CHECK_EQ(unlock_owned_count(), 0);
}

/* ---- scenes ------------------------------------------------------------------- */
TEST(concert_scene_full_show)
{
    DoomoResident *r;
    int happy0, i;
    premade_island();
    r = town_res(42);
    r->stat[STAT_HAPPY] = 40;
    happy0 = r->stat[STAT_HAPPY];
    scene_set(&SCENE_CONCERT, 42);
    run(0, 5);
    tap(BTN_A);                                    /* start the show */
    for (i = 0; i < 3000 && scene_top() == &SCENE_CONCERT; i++) {
        run((i % 9) == 0 ? BTN_A : 0, 1);          /* cheer along */
        if (progress_get(PROG_CONCERTS)) break;
    }
    CHECK_EQ(progress_get(PROG_CONCERTS), 1);
    CHECK(r->stat[STAT_HAPPY] > happy0);
    CHECK(unlock_has(UL_PART_BAND_TEE));
    run(0, 30);
    tap(BTN_A);                                    /* Save Single */
    CHECK_EQ(concert_single_count(), 1);
    CHECK(unlock_has(UL_PART_SPARKLE_EYES));
}

TEST(concert_scene_parade_and_empty)
{
    scene_set(&SCENE_CONCERT, CONCERT_ARG_PARADE);
    run(0, 10);
    CHECK(scene_top() == &SCENE_CONCERT);
    tap(BTN_B);
    CHECK(scene_top() != &SCENE_CONCERT);
    /* an empty slot: no crash, straight to the parade */
    scene_set(&SCENE_CONCERT, 5);
    run(0, 5);
    CHECK(scene_top() == &SCENE_CONCERT);
}

TEST(dream_scene_nightmare_choices)
{
    DoomoResident *r;
    int i;
    premade_island();
    r = town_res(2);
    r->stat[STAT_CORRUPTION] = 10;
    r->stat[STAT_ENERGY] = 80;
    /* let them sleep: corruption rises */
    scene_set(&SCENE_DREAM, DREAM_ARG_NIGHTMARE - 2);
    run(0, 12);
    tap(BTN_A);
    for (i = 0; i < 800 && !unlock_has(UL_PART_WAVY_MOUTH); i++) run(i % 40 == 39 ? BTN_A : 0, 1);
    run(0, 15);
    tap(BTN_RIGHT);                                 /* "Let them sleep" */
    tap(BTN_A);
    CHECK(unlock_has(UL_PART_DROOPY_EYES));
    CHECK(unlock_has(UL_PART_WAVY_MOUTH));
    CHECK_EQ(r->stat[STAT_CORRUPTION], 16);
    CHECK(dream_journal_count() >= 1);
    /* wake them up instead: a hug, a little tired */
    scene_set(&SCENE_DREAM, DREAM_ARG_NIGHTMARE - 3);
    r = town_res(3);
    r->stat[STAT_ENERGY] = 80;
    r->stat[STAT_CORRUPTION] = 10;
    run(0, 12);
    tap(BTN_A);
    for (i = 0; i < 900 && r->stat[STAT_ENERGY] == 80; i++) run(i % 40 == 39 ? BTN_A : 0, 1);
    CHECK_EQ(r->stat[STAT_ENERGY], 70);
    CHECK_EQ(r->stat[STAT_CORRUPTION], 10);
    CHECK(unlock_has(UL_PART_MESSY_HAIR));
}

/* DOOMO_LYRICS=1 tests/ concert_lyrics_dump: print a few songs */
TEST(concert_lyrics_dump)
{
    static const int ids[] = { 0, 1, 13, 42, 77, 111, 250, 499, 500, 666, 777, 999 };
    int i, l;
    if (!getenv("DOOMO_LYRICS")) return;
    premade_island();
    for (i = 0; i < DOOMO_COUNTOF(ids); i++) {
        ConcertSong s;
        const DoomoResident *r = town_res(ids[i]);
        int take;
        for (take = 0; take < 2; take++) {
            concert_song_make(&s, ids[i], r->like_music, take);
            printf("\n== %s (%d) - \"%s\" [%s] %s\n", r->name, ids[i], s.title,
                   doomo_music_names[s.genre], s.edgy ? "EDGY" : "");
            for (l = 0; l < CONCERT_LINES; l++) {
                uint8_t st[CONCERT_SYL_MAX];
                int n = concert_syllables(s.lines[l], st, CONCERT_SYL_MAX), k;
                printf("  %-28s ", s.lines[l]);
                for (k = 0; k < n; k++) printf("%s%.*s", k ? "-" : "",
                    (int)((k + 1 < n ? st[k + 1] : (int)strlen(s.lines[l])) - st[k]), s.lines[l] + st[k]);
                printf("\n");
            }
        }
    }
}

/* ---- visual review sheets (DOOMO_SHEETS=dir tests/ concert_sheet) ------- */
static uint8_t sheet[960 * 960];

static void sheet_put(int tile, int cols, int tw, int th)
{
    int y, x0 = (tile % cols) * tw, y0 = (tile / cols) * th;
    for (y = 0; y < th; y++) memcpy(&sheet[(y0 + y) * cols * tw + x0], &nat_fb[y * nat_w], (size_t)tw);
}

static void sheet_res(int w, int h)
{
    nat_w = w;
    nat_h = h;
    portrait_cache_reset();
}

TEST(concert_sheet_dreams)
{
    const char *dir = getenv("DOOMO_SHEETS");
    int res, lvl, v;
    char path[256];
    if (!dir) return;
    premade_island();
    for (res = 0; res < 2; res++) {
        int w = res ? 320 : 240, h = res ? 240 : 160, cols = res ? 3 : 4;
        sheet_res(w, h);
        for (lvl = 0; lvl < 3; lvl++) {
            int page;
            for (page = 0; page < 16 / (cols * (res ? 3 : 4)) + (res ? 1 : 0); page++) {
                int per = cols * (res ? 3 : 4), n = 0;
                memset(sheet, 0, sizeof(sheet));
                for (v = page * per; v < 16 && v < (page + 1) * per; v++, n++) {
                    DreamCast c;
                    int creep = lvl * 8, k;
                    dream_cast(lvl == 2 ? 666 : 42, &c);
                    for (k = 0; k < 3; k++) {          /* let the cache settle */
                        portrait_cache_clock((uint32_t)(100 + k), 0);
                        dream_draw_sky(creep, lvl == 2, 100);
                        dream_draw_vignette(v, &c, 100, creep, lvl == 2);
                        dream_draw_frame(creep, lvl == 2, 100);
                    }
                    sheet_put(n, cols, w, h);
                }
                snprintf(path, sizeof(path), "%s/dreams_%dx%d_c%d_p%d.png", dir, w, h, lvl * 8, page);
                nat_png_write(path, sheet, cols * w, (n + cols - 1) / cols * h, 1);
            }
        }
    }
    sheet_res(240, 160);
}

TEST(concert_sheet_stages)
{
    const char *dir = getenv("DOOMO_SHEETS");
    int res, g;
    char path[256];
    static ConcertCast cast;
    if (!dir) return;
    premade_island();
    for (res = 0; res < 2; res++) {
        int w = res ? 320 : 240, h = res ? 240 : 160, cols = res ? 3 : 4, per = res ? 9 : 16, page;
        sheet_res(w, h);
        for (page = 0; page * per < MUSIC_COUNT; page++) {
            int n = 0;
            memset(sheet, 0, sizeof(sheet));
            for (g = page * per; g < MUSIC_COUNT && g < (page + 1) * per; g++, n++) {
                StageView v;
                StageLayout L;
                int k;
                memset(&v, 0, sizeof(v));
                concert_cast_make(&cast, g * 37 + 3, g % 3, 6 + g, g % 4);
                v.cast = &cast;
                v.genre = g;
                v.stage = g % STAGE_COUNT;
                v.lights = 16;
                v.sec = SEC_CHORUS1;
                v.hype = 80;
                v.kick = 4;
                v.t = 200;
                v.singing = 0x7;
                v.confetti = 3;
                for (k = 0; k < CROWD_MAX; k++) v.crowd[k] = (int16_t)(k * 53 + g);
                v.ncrowd = CROWD_MAX;
                concert_layout(&L);
                for (k = 0; k < 3; k++) {
                    portrait_cache_clock((uint32_t)(100 + k), 0);
                    concert_draw_stage(&v, &L);
                    concert_draw_performers(&v, &L);
                    concert_draw_fx(&v, &L);
                    concert_draw_crowd(&v, &L);
                    concert_draw_lyric(&L, "Ate my heart", 5, 4, g >= 12, 200);
                }
                sheet_put(n, cols, w, h);
            }
            snprintf(path, sizeof(path), "%s/stages_%dx%d_p%d.png", dir, w, h, page);
            nat_png_write(path, sheet, cols * w, (n + cols - 1) / cols * h, 1);
        }
    }
    sheet_res(240, 160);
}
