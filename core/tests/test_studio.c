/* test_studio.c - the Music Studio and the shared song player (owner:
 * studio).
 *
 * Player: steps land on the exact ticks at several tempos (with and
 * without swing), ties / rests / the order list / looping, singing (a
 * syllable per lead onset, choir masks, the rotating lead voice), playing
 * from a position, pausing and restoring the background music, bounded
 * cost. Helpers: syllable splitting, scale snapping, sanitising.
 * Persistence: the 'SONG' save chunk round-trips through save_serialize /
 * save_deserialize (in memory and streamed), and an old save without the
 * chunk loads with an empty song book. Content: every template loads and
 * fits its words, the choir pieces fill a premade island's choir, Inspire
 * me is deterministic. The editor is driven by scripted input: write a
 * song (title, notes, a tie, lyrics), undo, save it in the book, reload.
 */
#include "test.h"
#include "../src/studio_int.h"

static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}

static void press(uint32_t b)
{
    frames(1, b);
    frames(1, 0);
}

static void type_text(const char *s)
{
    while (*s) ui_text_entry_feed(*s++);
}

static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

/* a simple song: lead C4 on every beat */
static void beat_song(DoomoSong *s, int bpm, int swing, int patterns)
{
    int p, i;
    song_clear(s);
    s->bpm = (uint8_t)bpm;
    s->swing = (uint8_t)swing;
    s->order_len = (uint8_t)patterns;
    for (p = 0; p < patterns; p++) s->order[p] = (uint8_t)(p % SONG_PATTERNS);
    for (p = 0; p < SONG_PATTERNS; p++)
        for (i = 0; i < SONG_STEPS; i += 4) {
            s->notes[p][0][i] = 60;
            s->notes[p][3][i] = SONG_DRUM_KICK;
        }
}

static uint32_t expect_tick(int k, int bpm, int swing)
{
    uint32_t u = 0;
    int j;
    for (j = 0; j < k; j++) u += (j & 1) ? 7200u - 450u * (uint32_t)swing : 7200u + 450u * (uint32_t)swing;
    return (u + 16u * (uint32_t)bpm - 1u) / (16u * (uint32_t)bpm);
}

/* ====================================================================== */
/* player                                                                   */
TEST(studio_player_steps_land_on_exact_ticks)
{
    static const int bpms[] = { 60, 75, 90, 120, 133, 150, 180, 220 };
    static DoomoSong s;
    int b, sw;
    test_reset();
    for (b = 0; b < (int)(sizeof(bpms) / sizeof(bpms[0])); b++)
        for (sw = 0; sw <= 8; sw += 4) {
            int last = 0, k;
            uint32_t t = 0, seen[64];
            beat_song(&s, bpms[b], sw, 2);
            song_play(&s, -1);
            seen[0] = 0;
            CHECK_EQ(song_pos_step(), 0);
            CHECK_EQ(song_pos_order(), 0);
            while (song_playing() && t < 4000) {
                int g;
                song_tick();
                t++;
                if (!song_playing()) break;
                g = song_pos_order() * SONG_STEPS + song_pos_step();
                CHECK_MSG(g == last || g == last + 1, "bpm %d: jumped from step %d to %d", bpms[b], last, g);
                if (g != last) { seen[g] = t; last = g; }
            }
            CHECK_EQ(last, 63);
            for (k = 1; k < 64; k++)
                CHECK_MSG(seen[k] == expect_tick(k, bpms[b], sw),
                          "bpm %d swing %d: step %d at tick %u, expected %u", bpms[b], sw, k,
                          seen[k], expect_tick(k, bpms[b], sw));
            /* the song ends exactly when its last step does */
            CHECK_EQ(t, expect_tick(64, bpms[b], sw));
            CHECK_EQ(song_length_ticks(&s), expect_tick(64, bpms[b], sw));
        }
}

TEST(studio_player_ties_rests_and_singing)
{
    static DoomoSong s;
    int t;
    test_reset();
    song_clear(&s);
    s.bpm = 120;
    s.singer[0] = DOOMO_PLAYER_ID;
    memcpy(s.lyrics[0], "la la", 6);
    s.notes[0][0][0] = 60;
    s.notes[0][0][1] = SONG_HOLD;
    s.notes[0][0][2] = SONG_HOLD;
    s.notes[0][0][3] = SONG_REST;
    s.notes[0][0][4] = 62;
    s.notes[0][0][8] = 64;               /* a third onset: no syllable left */
    song_play(&s, -1);
    CHECK_EQ(song_pos_line(), 0);
    CHECK_EQ(song_pos_syllable(), 0);
    CHECK_EQ_U(song_pos_singers(), 1u);
    CHECK_EQ(song_pos_lead(), 0);
    for (t = 0; t < 40; t++) {
        int st;
        song_tick();
        st = song_pos_step();
        if (st <= 2) {
            CHECK_MSG(song_pos_singers() == 1u, "tied note stopped singing at step %d", st);
            CHECK_EQ(song_pos_syllable(), 0);
        } else if (st == 3) {
            CHECK_MSG(song_pos_singers() == 0u, "a rest kept singing");
        } else if (st >= 4 && st < 8) {
            CHECK_EQ(song_pos_syllable(), 1);
            CHECK_EQ_U(song_pos_singers(), st == 4 ? 1u : 0u);
        } else if (st >= 8 && st < 10) {
            CHECK_EQ_U(song_pos_singers(), 0u);       /* played, not sung */
        }
    }
    song_stop();
    CHECK(!song_playing());
    CHECK_EQ(song_pos_line(), -1);
}

TEST(studio_player_order_and_loop)
{
    static DoomoSong s;
    int t, seen_wrap = 0, prev = 0, max_ord = 0;
    test_reset();
    beat_song(&s, 200, 0, 3);
    s.order[0] = 2; s.order[1] = 0; s.order[2] = 1;
    song_play(&s, -1);
    for (t = 0; t < 2000 && song_playing(); t++) {
        song_tick();
        if (song_pos_order() > max_ord) max_ord = song_pos_order();
    }
    CHECK(!song_playing());
    CHECK_EQ(max_ord, 2);
    CHECK_EQ((uint32_t)t, song_length_ticks(&s));
    /* looping: back to the first pass and still playing */
    s.flags |= SONGF_LOOP;
    song_play(&s, -1);
    for (t = 0; t < 2 * (int)song_length_ticks(&s); t++) {
        song_tick();
        if (prev == 2 && song_pos_order() == 0) seen_wrap = 1;
        prev = song_pos_order();
    }
    CHECK(seen_wrap);
    CHECK(song_playing());
    song_stop();
    /* play from a position */
    song_play_at(&s, -1, 1, 8);
    CHECK_EQ(song_pos_order(), 1);
    CHECK_EQ(song_pos_step(), 8);
    song_stop();
    /* the editor's pattern loop stays on its pattern */
    song_play_pattern(&s, 2, 30);
    for (t = 0; t < 30; t++) song_tick();
    CHECK(song_playing());
    CHECK_EQ(song_pos_order(), 0);
    song_stop();
}

TEST(studio_player_choir_rotates_and_restores_music)
{
    static DoomoSong s;
    int i, t, leads[8], nl = 0, differs = 0;
    test_reset();
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    song_clear(&s);
    s.bpm = 150;
    for (i = 0; i < 4; i++) s.singer[i] = (uint16_t)(10 + i);
    s.line_singers[0] = 0xF;                  /* the whole (small) choir */
    s.line_singers[1] = 0x2;                  /* a solo for singer 1      */
    memcpy(s.lyrics[0], "sing a long long song", 22);
    memcpy(s.lyrics[1], "solo time", 10);
    s.order_len = 2;
    s.order[1] = 0;
    for (i = 0; i < SONG_STEPS; i += 2) s.notes[0][0][i] = (uint8_t)(60 + (i & 7));
    audio_music(SONG_TITLE);
    {
        int had_music = audio_music_current() == SONG_TITLE;
        song_play(&s, -1);
        if (had_music) CHECK_EQ(audio_music_current(), SONG_NONE);
        for (t = 0; t < 300 && song_playing(); t++) {
            uint32_t m = song_pos_singers();
            song_tick();
            if (song_pos_line() == 0 && m) {
                CHECK_EQ_U(m, 0xFu);
                if (nl < 8 && (nl == 0 || leads[nl - 1] != song_pos_lead())) leads[nl++] = song_pos_lead();
            }
            if (song_pos_line() == 1 && song_pos_singers()) {
                CHECK_EQ_U(song_pos_singers(), 0x2u);
                CHECK_EQ(song_pos_lead(), 1);
            }
        }
        for (i = 1; i < nl; i++) differs |= leads[i] != leads[0];
        CHECK_MSG(nl >= 3 && differs, "the lead voice did not rotate (%d leads)", nl);
        song_stop();
        if (had_music) CHECK_EQ(audio_music_current(), SONG_TITLE);
    }
    audio_music(SONG_NONE);
}

TEST(studio_player_cost_is_bounded)
{
    static DoomoSong s;
    uint32_t calls0 = nat_tone_calls;
    int t;
    test_reset();
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    studio_template_load(&s, 7);              /* Park United: 25 singers */
    CHECK(song_singer_count(&s) == SONG_SINGERS);
    song_play(&s, -1);
    for (t = 0; t < 3000 && song_playing(); t++) song_tick();
    CHECK(!song_playing());
    /* at most the 4 channels (+ voice blips) per tick, cached */
    CHECK_MSG(nat_tone_calls - calls0 < (uint32_t)t * 6u, "%u tone calls in %d ticks",
              nat_tone_calls - calls0, t);
}

/* ====================================================================== */
/* helpers                                                                  */
static int syl(const char *s) { return song_syllables(s, NULL, 0); }

TEST(studio_syllable_split)
{
    uint8_t st[SONG_LYRIC_LEN];
    CHECK_EQ(syl(""), 0);
    CHECK_EQ(syl("   ...!"), 0);
    CHECK_EQ(syl("Hello world"), 3);
    CHECK_EQ(song_syllables("Hello world", st, SONG_LYRIC_LEN), 3);
    CHECK_EQ(st[0], 0); CHECK_EQ(st[1], 3); CHECK_EQ(st[2], 6);
    CHECK_EQ(syl("I love cupcakes"), 4);
    CHECK_EQ(syl("bubble"), 2);
    CHECK_EQ(syl("little"), 2);
    CHECK_EQ(syl("the"), 1);
    CHECK_EQ(syl("beautiful"), 3);
    CHECK_EQ(syl("candy"), 2);
    CHECK_EQ(syl("fire"), 1);
    CHECK_EQ(syl("loved"), 1);
    CHECK_EQ(syl("wanted"), 2);
    CHECK_EQ(syl("kisses"), 2);
    CHECK_EQ(syl("cup-cake"), 2);
    CHECK_EQ(syl("don't"), 1);
    CHECK_EQ(syl("hmm brr"), 2);
    CHECK_EQ(syl("123"), 3);
    CHECK_EQ(syl("Hellvile"), 2);
    CHECK_EQ(syl("everyone"), 3);
    CHECK_EQ(syl("yes"), 1);
    CHECK_EQ(syl("la la la"), 3);
    CHECK_EQ(syl("Hallelujah, hugs!"), 5);
    /* the split points of "monster" and "table" */
    CHECK_EQ(song_syllables("monster", st, SONG_LYRIC_LEN), 2);
    CHECK_EQ(st[1], 3);
    CHECK_EQ(song_syllables("table", st, SONG_LYRIC_LEN), 2);
    CHECK_EQ(st[1], 2);
    /* a short buffer still counts everything */
    CHECK_EQ(song_syllables("one two three four", st, 2), 4);
}

TEST(studio_scale_snap_and_shift)
{
    DoomoSong s;
    song_clear(&s);
    /* C major */
    CHECK_EQ(song_scale_snap(&s, 60), 60);
    CHECK_EQ(song_scale_snap(&s, 61), 60);       /* C# -> C (ties go down) */
    CHECK_EQ(song_scale_snap(&s, 66), 65);       /* F# -> F */
    CHECK_EQ(song_scale_shift(&s, 60, 1), 62);
    CHECK_EQ(song_scale_shift(&s, 64, 1), 65);
    CHECK_EQ(song_scale_shift(&s, 60, -1), 59);
    CHECK_EQ(song_scale_shift(&s, 60, 7), 72);
    /* G major has F# */
    s.key = 7;
    CHECK_EQ(song_scale_snap(&s, 66), 66);
    CHECK_EQ(song_scale_snap(&s, 65), 64);
    /* A minor */
    s.key = 9; s.scale = SONG_SCALE_MINOR;
    CHECK_EQ(song_scale_snap(&s, 61), 60);
    CHECK_EQ(song_scale_snap(&s, 68), 67);
    /* C pentatonic skips F and B */
    s.key = 0; s.scale = SONG_SCALE_PENTATONIC;
    CHECK_EQ(song_scale_snap(&s, 65), 64);
    CHECK_EQ(song_scale_snap(&s, 71), 72);
    CHECK_EQ(song_scale_shift(&s, 64, 1), 67);
    /* chromatic snaps nothing */
    s.scale = SONG_SCALE_CHROMATIC;
    CHECK_EQ(song_scale_snap(&s, 61), 61);
    CHECK_EQ(song_scale_shift(&s, 61, 1), 62);
    /* names and frequencies */
    {
        char b[6];
        song_note_name(60, b); CHECK_STR(b, "C4");
        song_note_name(69, b); CHECK_STR(b, "A4");
        song_note_name(73, b); CHECK_STR(b, "C#5");
        CHECK_EQ(song_note_hz(69), 440);
        CHECK_EQ(song_note_hz(57), 220);
    }
}

TEST(studio_sanitize_and_sections)
{
    DoomoSong s;
    song_clear(&s);
    s.bpm = 3; s.key = 40; s.scale = 99; s.swing = 30; s.order_len = 0;
    s.wave[0] = 77; s.vol[2] = 99; s.notes[1][3][5] = 200; s.notes[2][0][1] = 180;
    s.singer[3] = 5000; s.choir_layout = 9;
    s.lyrics[2][SONG_LYRIC_LEN - 1] = 'x';
    s.title[SONG_TITLE_LEN - 1] = 'y';
    CHECK(song_sanitize(&s) >= 10);
    CHECK_EQ(s.bpm, 60); CHECK(s.key < 12); CHECK(s.scale < SONG_SCALE_COUNT);
    CHECK_EQ(s.swing, 8); CHECK_EQ(s.order_len, 1); CHECK(s.wave[0] < SONG_WAVES);
    CHECK_EQ(s.vol[2], 15); CHECK_EQ(s.notes[1][3][5], 0); CHECK_EQ(s.notes[2][0][1], 0);
    CHECK_EQ(s.singer[3], SONG_NO_SINGER); CHECK(s.choir_layout < CHOIR_LAYOUT_COUNT);
    CHECK_EQ(s.lyrics[2][SONG_LYRIC_LEN - 1], 0); CHECK_EQ(s.title[SONG_TITLE_LEN - 1], 0);
    CHECK_EQ(song_sanitize(&s), 0);
    /* sections: automatic from the voice, editable, back to automatic */
    test_reset();
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    s.singer[0] = 0;
    RES_SET_VOICE(&doomo_town.res[0], VOICE_PITCH, 30);
    CHECK_EQ(song_singer_section(&s, 0), SECTION_SOPRANO);
    RES_SET_VOICE(&doomo_town.res[0], VOICE_PITCH, 2);
    CHECK_EQ(song_singer_section(&s, 0), SECTION_BASS);
    song_set_section(&s, 0, SECTION_ALTO);
    CHECK_EQ(song_singer_section(&s, 0), SECTION_ALTO);
    song_set_section(&s, 0, -1);
    CHECK_EQ(song_singer_section(&s, 0), SECTION_BASS);
    song_set_section(&s, 24, SECTION_TENOR);
    CHECK_EQ(s.section[12] & 15, SECTION_TENOR + 1);
}

/* ====================================================================== */
/* persistence                                                              */
static uint8_t image[DOOMO_SAVE_SLOT1_MAX], raw[256 * 1024], old_img[DOOMO_SAVE_SLOT1_MAX];

static uint32_t g32(const uint8_t *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void p32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* Rebuild `img` as an old save written before the studio existed: unpack
 * the LZ body, cut the raw stream where the SONG chunk starts (dropping it
 * and any chunk after it), repack as literal runs, fix the header. */
static uint32_t make_old_save(const uint8_t *img, uint32_t n, uint8_t *out)
{
    uint32_t pos = DOOMO_SAVE_HEADER_SIZE, o = 0, cut = 0, i, p;
    while (pos < n) {
        uint8_t c = img[pos++];
        if (c < 0x80) { uint32_t k = c + 1u; memcpy(raw + o, img + pos, k); pos += k; o += k; }
        else {
            uint32_t len = (c & 0x7Fu) + 3u, d = img[pos] | ((uint32_t)img[pos + 1] << 8);
            pos += 2;
            for (i = 0; i < len; i++, o++) raw[o] = raw[o - d];
        }
    }
    if (o != g32(img + 16)) return 0;
    for (p = o >= 10 ? o - 10 : 0; p > 0; p--)
        if (raw[p] == 'S' && raw[p + 1] == 'O' && raw[p + 2] == 'N' && raw[p + 3] == 'G' &&
            raw[p + 4] == 1 && raw[p + 5] == 0 && p + 10 + g32(raw + p + 6) <= o) { cut = p; break; }
    if (!cut) return 0;
    memcpy(out, img, DOOMO_SAVE_HEADER_SIZE);
    pos = DOOMO_SAVE_HEADER_SIZE;
    for (i = 0; i < cut;) {
        uint32_t k = cut - i < 128 ? cut - i : 128;
        out[pos++] = (uint8_t)(k - 1);
        memcpy(out + pos, raw + i, k);
        pos += k; i += k;
    }
    p32(out + 8, pos);
    p32(out + 16, cut);
    p32(out + 12, save_crc32(out + 16, pos - 16));
    return pos;
}

TEST(studio_song_book_round_trips_through_saves)
{
    static DoomoSong keep[SONG_BOOK_SIZE];
    StudioStats st;
    uint32_t n, sz0;
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    /* an untouched book is tiny */
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    sz0 = n;
    studio_template_load(&doomo_song_book[0], 7);
    studio_template_load(&doomo_song_book[3], 8);
    doomo_song_book[3].flags |= SONGF_FAVORITE;
    memset(doomo_song_book[3].lyrics[5], 0, SONG_LYRIC_LEN);
    song_set_section(&doomo_song_book[3], 4, SECTION_BASS);
    studio_inspire(&doomo_song_book[5], 42, -1, 1234);
    studio_stats.saved = 9; studio_stats.performed = 4; studio_stats.best_choir = 25;
    studio_stats.genres = 0x1234; studio_stats.encores = 2; studio_stats.lines = 77;
    memcpy(keep, doomo_song_book, sizeof(keep));
    st = studio_stats;
    n = save_serialize(image, sizeof(image));
    CHECK(n > sz0);
    printf("      song book: 3 songs add %u bytes to the save (compressed)\n", n - sz0);
    CHECK(doomo_save_ext_studio->size() <= DOOMO_SAVE_EXT_MAX);
    /* a new town wipes the book; loading brings it back exactly */
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_EMPTY);
    CHECK(!(doomo_song_book[0].flags & SONGF_VALID));
    CHECK_EQ(studio_stats.saved, 0);
    CHECK(save_deserialize(image, n));
    CHECK(memcmp(keep, doomo_song_book, sizeof(keep)) == 0);
    CHECK(memcmp(&st, &studio_stats, sizeof(st)) == 0);
    /* streamed through a small window too */
    {
        int w;
        static const int windows[] = { 0, 64, 1000 };
        for (w = 0; w < 3; w++) {
            nat_save_stream = windows[w];
            CHECK(save_game(1));
            memset(doomo_song_book, 0x77, sizeof(doomo_song_book));
            CHECK(load_game(1));
            CHECK(memcmp(keep, doomo_song_book, sizeof(keep)) == 0);
        }
        nat_save_stream = 0;
    }
    /* an old save (no SONG chunk) loads, with an empty book */
    {
        uint32_t m = make_old_save(image, n, old_img);
        CHECK(m > 0 && m != n);
        CHECK(save_deserialize(old_img, m));
        CHECK(!(doomo_song_book[0].flags & SONGF_VALID));
        CHECK(!(doomo_song_book[3].flags & SONGF_VALID));
        CHECK_EQ(studio_stats.saved, 0);
        CHECK_EQ(doomo_town.population, TOWN_POP_PREMADE);
    }
    /* a damaged chunk is reset, the town still loads */
    {
        static uint8_t chunk[8192];
        uint32_t k = doomo_save_ext_studio->write(chunk, sizeof(chunk));
        memcpy(doomo_song_book, keep, sizeof(keep));
        k = doomo_save_ext_studio->write(chunk, sizeof(chunk));
        CHECK(doomo_save_ext_studio->read(chunk, k, 1));
        CHECK(memcmp(keep, doomo_song_book, sizeof(keep)) == 0);
        CHECK(!doomo_save_ext_studio->read(chunk, k / 2, 1));
        CHECK(!(doomo_song_book[0].flags & SONGF_VALID));
        CHECK(!doomo_save_ext_studio->read(chunk, k, 0));
    }
}

TEST(studio_full_book_fits_the_chunk_budget)
{
    int i, p, t, k;
    DoomoRng r;
    test_reset();
    rng_seed(&r, 5);
    for (i = 0; i < SONG_BOOK_SIZE; i++) {
        DoomoSong *s = &doomo_song_book[i];
        song_clear(s);
        s->order_len = SONG_ORDER_MAX;
        for (p = 0; p < SONG_PATTERNS; p++)
            for (t = 0; t < SONG_TRACKS; t++)
                for (k = 0; k < SONG_STEPS; k++)
                    s->notes[p][t][k] = (uint8_t)(t == 3 ? 1 + rng_range(&r, 8) : 2 + rng_range(&r, 120));
        for (k = 0; k < SONG_LYRIC_LINES; k++) memset(s->lyrics[k], 'a' + k, SONG_LYRIC_LEN - 1);
        for (k = 0; k < SONG_SINGERS; k++) s->singer[k] = (uint16_t)k;
        for (k = 0; k < SONG_LYRIC_LINES; k++) s->line_singers[k] = 0x1FFFFFFu;
        memset(s->section, 0x21, sizeof(s->section));
        s->section[12] = 0x01;
    }
    CHECK(doomo_save_ext_studio->size() <= DOOMO_SAVE_EXT_MAX);
    printf("      worst-case book chunk: %u bytes (staging area %u)\n",
           doomo_save_ext_studio->size(), DOOMO_SAVE_EXT_MAX);
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
}

/* ====================================================================== */
/* content                                                                  */
TEST(studio_templates_load_and_fit)
{
    static DoomoSong s;
    int t, i, choirs = 0;
    test_reset();
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_PREMADE);
    CHECK(STUDIO_TEMPLATES >= 8);
    for (t = 0; t < STUDIO_TEMPLATES; t++) {
        const char *nm = studio_template_name(t);
        CHECK(nm[0] && strlen(nm) < SONG_TITLE_LEN);
        CHECK(strlen(studio_template_blurb(t)) > 4);
        studio_template_load(&s, t);
        CHECK_EQ(song_sanitize(&s), 0);
        CHECK_STR(s.title, nm);
        CHECK(s.order_len >= 5);
        for (i = 0; i < SONG_LYRIC_LINES; i++) {
            int pos = studio_line_pos(&s, i), k;
            for (k = 0; s.lyrics[i][k]; k++) CHECK(s.lyrics[i][k] >= 0x20 && s.lyrics[i][k] < 0x7F);
            if (pos < 0 || !s.lyrics[i][0]) continue;
            CHECK_MSG(song_syllables(s.lyrics[i], NULL, 0) == song_lead_onsets(&s, s.order[pos]),
                      "%s line %d \"%s\": %d syllables, %d notes", nm, i, s.lyrics[i],
                      song_syllables(s.lyrics[i], NULL, 0), song_lead_onsets(&s, s.order[pos]));
        }
        if (song_singer_count(&s) >= 12) choirs++;
    }
    CHECK(choirs >= 2);
    /* the hymn: Father Fuzz leads the church choir; the anthem: both sides */
    studio_template_load(&s, 8);
    CHECK(song_singer_id(&s, 0) == ROSTER_ID_FATHER_FUZZ);
    studio_template_load(&s, 7);
    {
        int hill = 0, hell = 0;
        for (i = 0; i < SONG_SINGERS; i++) {
            int id = song_singer_id(&s, i);
            if (id >= 0 && id < DOOMO_DISTRICT_SIZE) hill++;
            if (id >= DOOMO_DISTRICT_SIZE && id < DOOMO_MAX_RESIDENTS) hell++;
        }
        CHECK(hill >= 10 && hell >= 10);
        CHECK(s.line_singers[1] != s.line_singers[2]);       /* call and response */
    }
    /* an empty island: the templates still play, sung by you */
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_EMPTY);
    studio_template_load(&s, 8);
    CHECK_EQ(song_singer_count(&s), 1);
    CHECK_EQ(song_singer_id(&s, 0), DOOMO_PLAYER_ID);
}

TEST(studio_inspire_is_deterministic_and_about_the_muse)
{
    static DoomoSong a, b;
    const DoomoResident *r;
    int i, mentions = 0;
    test_reset();
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_PREMADE);
    studio_inspire(&a, 42, -1, 777);
    studio_inspire(&b, 42, -1, 777);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    r = town_res(42);
    CHECK_EQ(a.genre, RES_LIKE_MUSIC(r) % MUSIC_COUNT);
    CHECK(song_singer_id(&a, 0) == 42);
    CHECK(song_singer_count(&a) >= 1 && song_singer_count(&a) <= 6);
    for (i = 0; i < SONG_LYRIC_LINES; i++) {
        char low[SONG_LYRIC_LEN];
        int k;
        for (k = 0; k < SONG_LYRIC_LEN; k++) low[k] = (char)((a.lyrics[i][k] >= 'A' && a.lyrics[i][k] <= 'Z') ? a.lyrics[i][k] + 32 : a.lyrics[i][k]);
        if (strstr(a.lyrics[i], r->name)) mentions++;
        {
            char food[24];
            const char *f = doomo_food_names[RES_LIKE_FOOD(r)];
            for (k = 0; f[k] && k < 23; k++) food[k] = (char)((f[k] >= 'A' && f[k] <= 'Z') ? f[k] + 32 : f[k]);
            food[k] = 0;
            if (strstr(low, food)) mentions++;
        }
        if (strstr(a.lyrics[i], doomo_catchphrases[RES_CATCHPHRASE(r)])) mentions++;
    }
    CHECK_MSG(mentions >= 1, "the lyrics never mention the muse");
    studio_inspire(&b, 42, -1, 778);
    CHECK(memcmp(&a.notes, &b.notes, sizeof(a.notes)) != 0);
    /* a Hellvile muse: hellish flag */
    studio_inspire(&b, 600, -1, 5);
    CHECK(b.flags & SONGF_HELLISH);
}

TEST(studio_choir_fills_and_parts)
{
    static DoomoSong s;
    int i, hill = 0, hell = 0, n, who;
    char buf[48];
    test_reset();
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_PREMADE);
    song_clear(&s);
    n = studio_choir_fill(&s, FILL_BOTH, -1, 9);
    CHECK_EQ(n, SONG_SINGERS);
    for (i = 0; i < SONG_SINGERS; i++) {
        int id = song_singer_id(&s, i);
        CHECK(id >= 0);
        if (id < DOOMO_DISTRICT_SIZE) hill++; else hell++;
    }
    CHECK(hill >= 12 && hell >= 12);
    CHECK_EQ(studio_choir_fill(&s, FILL_HELLVILE, -1, 2), SONG_SINGERS);
    for (i = 0; i < SONG_SINGERS; i++) CHECK(song_singer_id(&s, i) >= DOOMO_DISTRICT_SIZE);
    n = studio_choir_fill(&s, FILL_FRIENDS, 0, 1);
    CHECK(n >= 2 && song_singer_id(&s, 0) == 0);
    /* parts */
    studio_choir_fill(&s, FILL_RANDOM, -1, 4);
    s.order_len = 4;
    for (i = 0; i < 4; i++) memcpy(s.lyrics[i], "la la", 6);
    studio_set_part(&s, 0, PART_CHOIR, 0);
    CHECK_EQ(studio_part_of(&s, 0, &who), PART_CHOIR);
    CHECK_EQ_U(song_line_mask(&s, 0), (1u << SONG_SINGERS) - 1u);
    studio_set_part(&s, 1, PART_SOLO, 3);
    CHECK_EQ(studio_part_of(&s, 1, &who), PART_SOLO);
    CHECK_EQ(who, 3);
    studio_set_part(&s, 2, PART_HALF, 1);
    CHECK_EQ(studio_part_of(&s, 2, &who), PART_HALF);
    CHECK_EQ(who, 1);
    studio_part_label(&s, 2, buf, sizeof(buf));
    CHECK(strstr(buf, "2nd half") != NULL);
    studio_set_part(&s, 3, PART_DUET, 0);
    CHECK_EQ(studio_part_of(&s, 3, &who), PART_DUET);
    studio_part_label(&s, 3, buf, sizeof(buf));
    CHECK(strstr(buf, "Duet") != NULL);
    {
        int sec = song_singer_section(&s, 0);
        studio_set_part(&s, 0, PART_SECTION, sec);
        CHECK_EQ(studio_part_of(&s, 0, &who), PART_SECTION);
        CHECK_EQ(who, sec);
    }
    CHECK_EQ(studio_choir_fill(&s, FILL_CLEAR, -1, 0), 0);
}

/* The old studio_part_of_raw, verbatim: part_mask() asked for every candidate in turn (about 100 calls, each a
 * valid_list() and, for sections, a voice lookup per singer; ~9 ms on the GBA, the 50.8 ms tick of the sung
 * PERFORM window). The new one builds the candidates from one pass; every answer must be the same. */
static int ref_valid_list(const DoomoSong *s, uint8_t *slots)
{
    int i, n = 0;
    for (i = 0; i < SONG_SINGERS; i++) if (song_singer_id(s, i) >= 0) slots[n++] = (uint8_t)i;
    return n;
}

static uint32_t ref_part_mask(const DoomoSong *s, int part, int who)
{
    uint8_t v[SONG_SINGERS];
    int n = ref_valid_list(s, v), i;
    uint32_t m = 0;
    if (!n) return 0;
    switch (part) {
    case PART_SOLO: return 1u << v[((who % n) + n) % n];
    case PART_DUET:
        if (n < 2) return 1u << v[0];
        who = ((who % n) + n) % n;
        return (1u << v[who]) | (1u << v[(who + 1) % n]);
    case PART_SECTION:
        for (i = 0; i < n; i++) if (song_singer_section(s, v[i]) == who) m |= 1u << v[i];
        return m;
    case PART_HALF:
        for (i = 0; i < n; i++) if ((i < (n + 1) / 2) == !who) m |= 1u << v[i];
        return m ? m : 1u << v[0];
    default:
        for (i = 0; i < n; i++) m |= 1u << v[i];
        return m;
    }
}

static int ref_part_of(const DoomoSong *s, int line, int *who)
{
    uint8_t v[SONG_SINGERS];
    uint32_t m = song_line_mask(s, line);
    int n = ref_valid_list(s, v), k;
    if (who) *who = 0;
    if (n <= 1 || !m) return PART_SOLO;
    if (m == ref_part_mask(s, PART_CHOIR, 0)) return PART_CHOIR;
    for (k = 0; k < n; k++) if (m == ref_part_mask(s, PART_SOLO, k)) { if (who) *who = k; return PART_SOLO; }
    for (k = 0; k < n; k++) if (m == ref_part_mask(s, PART_DUET, k)) { if (who) *who = k; return PART_DUET; }
    for (k = 0; k < SECTION_COUNT; k++)
        if (m == ref_part_mask(s, PART_SECTION, k)) { if (who) *who = k; return PART_SECTION; }
    for (k = 0; k < 2; k++) if (m == ref_part_mask(s, PART_HALF, k)) { if (who) *who = k; return PART_HALF; }
    if (who) *who = -1;
    return PART_CHOIR;
}

TEST(studio_part_of_equals_the_candidate_walk)
{
    static DoomoSong s;
    uint32_t r = 12345u;
    int cfg, seen[PART_COUNT + 1] = { 0 }, line, i, k, checked = 0;
    test_reset();
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_PREMADE);
    for (cfg = 0; cfg < 300; cfg++) {
        int keep = 1 + cfg % SONG_SINGERS;
        song_clear(&s);
        studio_choir_fill(&s, FILL_RANDOM, -1, (uint32_t)cfg + 1);
        for (i = 0; i < SONG_SINGERS; i++) {                 /* thin the choir out: 1..25 singers, any slots */
            r = r * 1664525u + 1013904223u;
            if ((int)((r >> 8) % SONG_SINGERS) >= keep) s.singer[i] = SONG_NO_SINGER;
            r = r * 1664525u + 1013904223u;
            song_set_section(&s, i, (int)((r >> 8) % (SECTION_COUNT + 1)) - 1);   /* -1 = auto (the voice) */
        }
        for (line = 0; line < SONG_LYRIC_LINES; line++) {
            uint32_t cand[3 * SONG_SINGERS + SECTION_COUNT + 4];
            int nc = 0;
            for (k = 0; k < SONG_SINGERS; k++) {                              /* every mask the walk can name */
                cand[nc++] = ref_part_mask(&s, PART_SOLO, k);
                cand[nc++] = ref_part_mask(&s, PART_DUET, k);
            }
            for (k = 0; k < SECTION_COUNT; k++) cand[nc++] = ref_part_mask(&s, PART_SECTION, k);
            cand[nc++] = ref_part_mask(&s, PART_HALF, 0);
            cand[nc++] = ref_part_mask(&s, PART_HALF, 1);
            cand[nc++] = ref_part_mask(&s, PART_CHOIR, 0);
            for (k = 0; k < nc + 8; k++) {                                    /* ... and arbitrary ones */
                int who_a = -7, who_b = -9, pa, pb;
                if (k < nc) s.line_singers[line] = cand[k];
                else { r = r * 1664525u + 1013904223u; s.line_singers[line] = r >> (r & 7); }
                studio_part_memo_reset();
                pa = ref_part_of(&s, line, &who_a);
                pb = studio_part_of(&s, line, &who_b);
                CHECK_EQ(pb, pa);
                CHECK_EQ(who_b, who_a);
                seen[pa]++;
                checked++;
            }
            if (line > 2) break;                                              /* 4 lines a config is plenty */
        }
    }
    CHECK(checked > 20000);
    for (i = 0; i < PART_COUNT; i++) CHECK(seen[i] > 20);   /* every part kind was really exercised */
    studio_part_memo_reset();
}

TEST(studio_autofit_and_word_filter)
{
    static DoomoSong s;
    char t[SONG_LYRIC_LEN];
    test_reset();
    song_clear(&s);
    s.notes[0][0][0] = 60; s.notes[0][0][4] = 62; s.notes[0][0][8] = 64; s.notes[0][0][12] = 65;
    s.notes[0][0][13] = SONG_HOLD; s.notes[0][0][14] = SONG_HOLD; s.notes[0][0][15] = SONG_HOLD;
    memcpy(s.lyrics[0], "hi", 3);
    CHECK(studio_autofit(&s, 0));
    CHECK_EQ(song_syllables(s.lyrics[0], NULL, 0), 4);
    CHECK_STR(s.lyrics[0], "hi la la la");
    memcpy(s.lyrics[0], "one two three four five six", 28);
    CHECK(studio_autofit(&s, 0));
    CHECK_EQ(song_lead_onsets(&s, 0), 6);
    CHECK(!studio_autofit(&s, 0));
    memcpy(t, "oh DAMN this crap", 18);
    CHECK_EQ(studio_clean_text(t, sizeof(t)), 2);
    CHECK_STR(t, "oh darn this crumbs");
    memcpy(t, "class glass assess", 19);
    CHECK_EQ(studio_clean_text(t, sizeof(t)), 0);
}

TEST(studio_unlocks_and_activity_registered)
{
    const DoomoUnlock *u = doomo_unlocks_studio;
    const DoomoActivity *acts[16];
    int n, i, found = 0;
    test_reset();
    for (n = 0; u[n].name; n++) {
        CHECK(u[n].id >= STUDIO_UL_FIRST && u[n].id < STUDIO_UL_FIRST + 16);
        CHECK(unlock_by_id(u[n].id) == &u[n]);
        CHECK(u[n].how && u[n].how[0]);
        CHECK(unlock_find(u[n].cat, u[n].list, u[n].index) == &u[n]);
    }
    CHECK_EQ(n, UL_STUDIO_LAST - STUDIO_UL_FIRST);
    CHECK(!studio_wave_ok(WAVE_SAW));
    studio_stats.performed = 1;
    studio_check_unlocks();
    CHECK(studio_wave_ok(WAVE_SAW));
    CHECK(!studio_template_ok(12));
    studio_stats.performed = 3;
    studio_check_unlocks();
    CHECK(studio_template_ok(12));
    n = activities_at(ACTLOC_CONCERT_HALL, acts, 16);
    for (i = 0; i < n; i++) if (acts[i]->scene == &SCENE_STUDIO) found = 1;
    CHECK(found);
    n = activities_at(ACTLOC_TOWN_HALL, acts, 16);
    for (i = 0, found = 0; i < n; i++) if (acts[i]->scene == &SCENE_STUDIO) found = 1;
    CHECK(found);
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = 0;
    n = activities_at(ACTLOC_CONCERT_HALL, acts, 16);
    for (i = 0, found = 0; i < n; i++) if (acts[i]->scene == &SCENE_STUDIO) found = 1;
    CHECK(!found);
    doomo_town.story.chapter = 1;
    n = activities_at(ACTLOC_CONCERT_HALL, acts, 16);
    for (i = 0, found = 0; i < n; i++) if (acts[i]->scene == &SCENE_STUDIO) found = 1;
    CHECK(found);
    CHECK(scene_by_name("STUDIO") == &SCENE_STUDIO);
}

/* ====================================================================== */
/* the editor, driven by buttons                                            */
static void chord_tab(void)
{
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_R);
    frames(1, 0);
}

TEST(studio_editor_writes_saves_and_reloads_a_song)
{
    uint32_t n;
    int i;
    test_reset();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_STUDIO, -2);
    CHECK(scene_top() == &SCENE_STUDIO);
    CHECK_EQ(studio.tab, STAB_FILE);
    /* SONG: the title */
    press(BTN_R);
    CHECK_EQ(studio.tab, STAB_SONG);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    for (i = 0; i < 12; i++) ui_text_entry_feed('\b');
    type_text("Test Tune\n");
    frames(2, 0);
    CHECK(scene_top() == &SCENE_STUDIO);
    CHECK_STR(studio.song.title, "Test Tune");
    /* tempo up by 5 */
    press(BTN_DOWN); press(BTN_DOWN);
    for (i = 0; i < 5; i++) press(BTN_RIGHT);
    CHECK_EQ(studio.song.bpm, 125);
    /* NOTES: place, raise a step while holding A, tie, erase */
    press(BTN_R);
    CHECK_EQ(studio.tab, STAB_NOTES);
    press(BTN_A);                                /* C5 (the last note), cursor moves on */
    CHECK_EQ(studio.song.notes[0][0][0], 72);
    CHECK_EQ(studio.stp, 1);
    frames(1, BTN_A);
    frames(1, BTN_A | BTN_UP);
    frames(1, BTN_A);
    frames(1, 0);
    CHECK_EQ(studio.song.notes[0][0][1], 74);    /* one scale step up: D5 */
    press(BTN_B);                                /* step 2: a tie */
    CHECK_EQ(studio.song.notes[0][0][2], SONG_HOLD);
    press(BTN_RIGHT); press(BTN_RIGHT); press(BTN_RIGHT);    /* drums */
    press(BTN_A);
    CHECK_EQ(studio.song.notes[0][3][2], SONG_DRUM_KICK);
    /* SELECT + B: undo the kick */
    frames(1, BTN_SELECT);
    frames(1, BTN_SELECT | BTN_B);
    frames(1, 0);
    CHECK_EQ(studio.song.notes[0][3][2], 0);
    /* L / R pick the pattern in NOTES */
    press(BTN_R);
    CHECK_EQ(studio.pat, 1);
    press(BTN_L);
    CHECK_EQ(studio.pat, 0);
    /* LYRICS (NOTES -> ORDER -> LYRICS with SELECT + R) */
    chord_tab();
    CHECK_EQ(studio.tab, STAB_ORDER);
    chord_tab();
    CHECK_EQ(studio.tab, STAB_LYRICS);
    press(BTN_A);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    type_text("la la\n");
    frames(2, 0);
    CHECK_STR(studio.song.lyrics[0], "la la");
    /* the line has 2 notes (C5 tied, D5): it fits */
    CHECK_EQ(song_syllables(studio.song.lyrics[0], NULL, 0), song_lead_onsets(&studio.song, 0));
    /* undo and redo by hand */
    CHECK(ed_undo());
    CHECK_STR(studio.song.lyrics[0], "");
    press(BTN_A);
    type_text("la la\n");
    frames(2, 0);
    CHECK(ed_undo_steps() >= 5);
    /* FILE: save on page 1 */
    press(BTN_R); press(BTN_R); press(BTN_R);
    CHECK_EQ(studio.tab, STAB_FILE);
    CHECK_EQ(studio.fsel, 0);
    press(BTN_A);
    CHECK(st_popup_open());
    press(BTN_A);
    CHECK(!st_popup_open());
    CHECK(doomo_song_book[0].flags & SONGF_VALID);
    CHECK_STR(doomo_song_book[0].title, "Test Tune");
    CHECK_EQ(studio_stats.saved, 1);
    CHECK(!studio.dirty);
    /* the game is saved, the town is wiped, the save is loaded */
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    CHECK(!(doomo_song_book[0].flags & SONGF_VALID));
    CHECK(save_deserialize(image, n));
    CHECK_STR(doomo_song_book[0].title, "Test Tune");
    CHECK_STR(doomo_song_book[0].lyrics[0], "la la");
    CHECK_EQ(doomo_song_book[0].bpm, 125);
    CHECK_EQ(doomo_song_book[0].notes[0][0][2], SONG_HOLD);
    /* reopen it from the book in a fresh studio */
    memset(&studio.song, 0, sizeof(studio.song));
    while (scene_depth() > 1) scene_pop();
    scene_push(&SCENE_STUDIO, 0);
    CHECK_STR(studio.song.title, "Test Tune");
    CHECK_EQ(studio.slot, 0);
    /* leaving (nothing changed) pops back */
    press(BTN_B);
    CHECK(scene_top() == &base_scene);
    test_reset();
}

TEST(studio_editor_templates_inspire_and_stage)
{
    int i;
    test_reset();
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    scene_set(&base_scene, 0);
    scene_push(&SCENE_STUDIO, -2);
    /* FILE: a template */
    studio.fsel = SONG_BOOK_SIZE + 1;
    press(BTN_A);
    CHECK_EQ(studio.fmode, 1);
    for (i = 0; i < 8; i++) press(BTN_DOWN);     /* Hymn of Cuddles */
    press(BTN_A);
    CHECK_STR(studio.song.title, "Hymn of Cuddles");
    CHECK_EQ(studio.tab, STAB_SONG);
    CHECK(ed_undo());                            /* even a template is undoable */
    CHECK_STR(studio.song.title, "New Song");
    /* Inspire me: pick a muse */
    st_set_tab(STAB_FILE);
    studio.fsel = SONG_BOOK_SIZE + 2;
    press(BTN_A);
    CHECK(st_picker_open());
    press(BTN_DOWN);                             /* resident 0 after "You" */
    press(BTN_A);
    CHECK(!st_picker_open());
    CHECK(song_singer_id(&studio.song, 0) == 0);
    CHECK_EQ(studio_stats.inspired, 1);
    /* the show: raise the curtain, sing, take a bow */
    {
        uint8_t happy0 = doomo_town.res[0].stat[STAT_HAPPY];
        doomo_town.res[0].stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(happy0, 50);
        happy0 = doomo_town.res[0].stat[STAT_HAPPY];
        st_set_tab(STAB_STAGE);
        press(BTN_A);
        CHECK(stage_fullscreen());
        for (i = 0; i < 4000 && studio.show == 1; i++) frames(1, 0);
        CHECK_EQ(studio.show, 2);
        CHECK(studio.sung_lines >= 3);
        CHECK(doomo_town.res[0].stat[STAT_HAPPY] > happy0);
        CHECK_EQ(studio_stats.performed, 1);
        CHECK(unlock_has(UL_STUDIO_WAVE_SAW));
        frames(25, 0);
        press(BTN_A);
        CHECK(!stage_fullscreen());
    }
    /* CHOIR: quick fill both districts, a full choir */
    st_set_tab(STAB_CHOIR);
    frames(1, BTN_SELECT);
    frames(1, 0);
    CHECK(st_popup_open());
    press(BTN_DOWN); press(BTN_DOWN); press(BTN_DOWN);
    press(BTN_A);
    CHECK_EQ(song_singer_count(&studio.song), SONG_SINGERS);
    test_reset();
}

/* MS-PF6: the voice-tuning memo returns exactly what the search finds. `ref_tune` is the search as it was before
 * the memo (copied; it must not be changed to follow the real one). Every pitch, every note, the vowels the
 * singers use, the spread range; once cold (a miss), once warm (a hit). */
static uint32_t ref_hz_diff(int a, int b) { return (uint32_t)(a > b ? a - b : b - a); }

static void ref_tune(const uint8_t *v, int midi, char vowel, int spread, uint8_t *out)
{
    uint8_t t[VOICE_COUNT];
    int nat = 45 + v[VOICE_PITCH] * 36 / 31, target = midi, p, a, bp = 0, ba = 0, hz;
    uint32_t best = 0xFFFFFFFFu, d;
    while (target < nat - 6) target += 12;
    while (target > nat + 6) target -= 12;
    target = DOOMO_CLAMP(target, 45, 81);
    hz = song_note_hz(target);
    memcpy(t, v, VOICE_COUNT);
    t[VOICE_INTONATION] = 0;
    t[VOICE_ACCENT] = 0;
    for (p = 0; p < 32; p++) {
        t[VOICE_PITCH] = (uint8_t)p;
        d = ref_hz_diff(audio_voice_freq(t, vowel), hz);
        if (d < best) { best = d; bp = p; }
    }
    best = 0xFFFFFFFFu;
    for (p = DOOMO_MAX(bp - 1, 0), a = bp; p <= DOOMO_MIN(bp + 1, 31); p++) {
        int k;
        t[VOICE_PITCH] = (uint8_t)p;
        for (k = 0; k < 32; k++) {
            t[VOICE_ACCENT] = (uint8_t)k;
            d = ref_hz_diff(audio_voice_freq(t, vowel), hz);
            if (d < best) { best = d; a = p; ba = k; }
        }
    }
    memcpy(out, v, VOICE_COUNT);
    out[VOICE_PITCH] = (uint8_t)a;
    out[VOICE_ACCENT] = (uint8_t)DOOMO_CLAMP(ba + spread, 0, 31);
    out[VOICE_INTONATION] = 0;
}

TEST(studio_voice_tuning_memo_is_bit_identical)
{
    static const char vowels[] = "aeiouyhm";
    uint32_t hits, miss, pass;
    int pitch, midi, vi, spread, n = 0;
    song_tune_cache_reset();
    for (pass = 0; pass < 2; pass++)
        for (pitch = 0; pitch < 32; pitch += 3)
            for (midi = 36; midi <= 96; midi += 5)
                for (vi = 0; vi < 8; vi++)
                    for (spread = -2; spread <= 2; spread++) {
                        uint8_t v[VOICE_COUNT], want[VOICE_COUNT], got[VOICE_COUNT];
                        int k;
                        for (k = 0; k < VOICE_COUNT; k++) v[k] = (uint8_t)((pitch * (k + 1) + k * 7) & 31);
                        v[VOICE_PITCH] = (uint8_t)pitch;
                        ref_tune(v, midi, vowels[vi], spread, want);
                        song_tune_voice(v, midi, vowels[vi], spread, got);
                        CHECK_MSG(memcmp(want, got, VOICE_COUNT) == 0,
                                  "pass %u pitch %d midi %d vowel %c spread %d", (unsigned)pass, pitch, midi, vowels[vi], spread);
                        n++;
                    }
    song_tune_cache_stats(&hits, &miss);
    CHECK(n > 1000);
    CHECK(hits > miss);                 /* the memo is really used */
    CHECK(miss <= (uint32_t)n / 2);     /* and the second pass is all hits (the table holds the working set or recycles) */
}

/* ---- option C: the choir as hardware sprites (docs/STUDIO_30FPS_PLAN.md section 11) -------------------------------
 * A test platform that has sprites: plat_stage_obj records each one (its pixels baked with the patches through the
 * shared plat_stage_bake) and, after the frame, paints them over nat_fb in call order, which is what a platform
 * that shows sprites above the whole bitmap does. The stage is drawn twice every tick, sprites off and on; the two
 * frames must be identical. A wrong planner (a check switched off with studio_stage_obj_fault) must not be. */
#define CURTAIN_T 30   /* studio_stage.c; the sprites start 20 ticks after it */
extern int studio_stage_obj_fault;
static int ts_on, ts_n, ts_chibis;
/* one entry per plat_stage_obj / plat_stage_mini call, in call order: a 16x24 chibi or an 8x8 mini */
static struct { int x, y, w, h; uint8_t pix[16 * 24]; } ts_sp[SONG_SINGERS * 3 + 4];

int plat_stage_obj_begin(void)
{
    if (!ts_on) return 0;
    ts_n = 0; ts_chibis = 0;
    return 1;
}

int plat_stage_obj(int slot, int x, int y, const DoomoImage *img, int variant, const DoomoPatch *patch, int npatch)
{
    (void)variant;
    if (!ts_on || !img || img->w != 16 || img->h != 24 || !img->uid || slot < 0 || slot >= SONG_SINGERS ||
        ts_chibis >= SONG_SINGERS || ts_n >= (int)(sizeof(ts_sp) / sizeof(ts_sp[0])))
        return 0;
    ts_sp[ts_n].x = x; ts_sp[ts_n].y = y; ts_sp[ts_n].w = 16; ts_sp[ts_n].h = 24;
    plat_stage_bake(ts_sp[ts_n].pix, img, patch, npatch);
    ts_n++; ts_chibis++;
    return 1;
}

int plat_stage_mini(int kind, int x, int y, const uint8_t *pix)
{
    if (!ts_on || !pix || kind < 0 || kind >= STAGE_MINI_KINDS || ts_n >= (int)(sizeof(ts_sp) / sizeof(ts_sp[0])))
        return 0;
    ts_sp[ts_n].x = x; ts_sp[ts_n].y = y; ts_sp[ts_n].w = 8; ts_sp[ts_n].h = 8;
    memcpy(ts_sp[ts_n].pix, pix, 64);
    ts_n++;
    return 1;
}

static void ts_paint_sprites(void)
{
    int i, xx, yy;
    for (i = 0; i < ts_n; i++)
        for (yy = 0; yy < ts_sp[i].h; yy++)
            for (xx = 0; xx < ts_sp[i].w; xx++) {
                int px = ts_sp[i].x + xx, py = ts_sp[i].y + yy;
                uint8_t c = ts_sp[i].pix[yy * ts_sp[i].w + xx];
                if (c && (unsigned)px < (unsigned)nat_w && (unsigned)py < (unsigned)nat_h) nat_fb[py * nat_w + px] = c;
            }
}

static uint8_t ts_a[NAT_MAX_W * NAT_MAX_H];

/* one run of a full 25 singer show at the given size and layout; returns the number of ticks that differed */
static int ts_run(int w, int h, int layout, int ticks, long *sprites, long *singers, int *shown)
{
    int i, t, bad = 0, resid[SONG_SINGERS], n = 0;
    nat_w = w; nat_h = h;
    test_reset();
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    scene_set(&base_scene, 0);
    scene_push(&SCENE_STUDIO, -2);
    beat_song(SS, 120, 0, 4);
    for (i = 0; i < DOOMO_MAX_RESIDENTS && n < SONG_SINGERS; i++) if (st_alive(i)) resid[n++] = i;
    for (i = 0; i < SONG_SINGERS; i++) SS->singer[i] = (uint16_t)(i < n ? resid[i] : SONG_NO_SINGER);
    for (i = 0; i < SONG_LYRIC_LINES; i++) {
        strcpy(SS->lyrics[i], "la la la la");
        SS->line_singers[i] = i % 3 == 0 ? 0x1FFFFFFu : i % 3 == 1 ? 0x0AAAAAAu : 0x1555555u;
    }
    SS->choir_layout = (uint8_t)layout;
    st_set_tab(STAB_STAGE);
    press(BTN_A);
    *sprites = *singers = 0; *shown = 0;
    for (t = 0; t < ticks && studio.show == 1; t++) {
        int d;
        frames(1, 0);
        if (studio.show != 1) break;
        if (t % 5 == 0) studio.applause = (uint8_t)((t * 7) % 101);   /* hearts and glow sticks come and go */
        memset(nat_fb, 0, sizeof(nat_fb)); ts_on = 0; ts_n = 0;
        stage_draw_full();
        memcpy(ts_a, nat_fb, (size_t)(nat_w * nat_h));
        memset(nat_fb, 0, sizeof(nat_fb)); ts_on = 1; ts_n = 0;
        stage_draw_full();
        ts_paint_sprites();
        ts_on = 0;
        d = memcmp(ts_a, nat_fb, (size_t)(nat_w * nat_h)) != 0;
        if (d && bad == 0)
            for (i = 0; i < nat_w * nat_h; i++)
                if (ts_a[i] != nat_fb[i]) { fprintf(stderr, "  first diff t=%d at %d,%d (%d vs %d), %d sprites\n", t, i % nat_w, i / nat_w, ts_a[i], nat_fb[i], ts_n);
                    break; }
        bad += d;
        if (ts_n || studio.show_t >= CURTAIN_T + 20) { *sprites += ts_chibis; *singers += song_singer_count(SS); (*shown)++; }
    }
    scene_pop();
    test_reset();
    nat_w = 240; nat_h = 160;
    return bad;
}

TEST(studio_stage_sprites_equal_the_bitmap)
{
    long sp, sg;
    int layout, shown, bad, tot = 0;
    for (layout = 0; layout < CHOIR_LAYOUT_COUNT; layout++) {
        bad = ts_run(240, 160, layout, 700, &sp, &sg, &shown);
        CHECK_MSG(bad == 0, "240x160 layout %d: %d ticks differ", layout, bad);
        CHECK(shown > 100);
        printf("    stage sprites 240x160 layout %d: %.1f of %.1f singers per tick (%d ticks)\n", layout,
               shown ? (double)sp / shown : 0.0, shown ? (double)sg / shown : 0.0, shown);
        tot += (int)sp;
    }
    bad = ts_run(320, 240, CHOIR_RISERS, 700, &sp, &sg, &shown);
    CHECK_MSG(bad == 0, "320x240: %d ticks differ", bad);
    CHECK(tot > 0);                                         /* the planner really does choose sprites */
}

TEST(studio_stage_sprites_wrong_planner_is_caught)
{
    static const int faults[] = { 1 | 64, 2, 8, 32 | 64 };
    long sp, sg;
    int f, shown;
    for (f = 0; f < 4; f++) {
        int bad;
        studio_stage_obj_fault = faults[f];
        bad = ts_run(240, 160, CHOIR_RISERS, 700, &sp, &sg, &shown);
        studio_stage_obj_fault = 0;
        CHECK_MSG(bad > 0, "fault %d went unnoticed", faults[f]);
    }
}
