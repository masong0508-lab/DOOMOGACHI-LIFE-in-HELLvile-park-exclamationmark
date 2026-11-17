/* test_song_ext.c - ST-ADV (owner: studio): the Advanced song extension, its player and its save tail. */
#include "test.h"
#include "../src/studio_int.h"

static void base(DoomoSong *s)
{
    song_clear(s);
    s->bpm = 120;
    s->key = 0;
    s->scale = SONG_SCALE_MAJOR;
    s->notes[0][0][0] = 60;
    s->notes[1][0][0] = 64;
    s->order_len = 1;
    s->order[0] = 0;
}

static int run_to_end(const DoomoSong *s, const SongExt *x, uint32_t cap)
{
    uint32_t n = 0;
    song_play_ext(s, x, -1);
    while (song_playing() && n < cap) { song_tick(); n++; }
    return (int)n;
}

TEST(songx_init_encode_decode_round_trip)
{
    DoomoSong s;
    SongExt x, y;
    uint8_t buf[SONGX_PAGE_BYTES];
    uint32_t n;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_add_section(&x, SX_CHORUS), 1);
    x.sect[1].key = 2; x.sect[1].tempo = 150; x.sect[1].fade = SX_FADE_OUT; x.sect[1].npat = 2;
    x.sect[1].pat[0] = 5; x.sect[1].pat[1] = 9; x.sect[1].line[0] = 3;
    CHECK(songx_arrange_push(&x, 1));
    x.tracks = 6; x.patterns = 12;
    CHECK(songx_set_cell(&s, &x, 5, 4, 7, 70));
    CHECK(songx_set_cell(&s, &x, 1, 2, 3, 40));                 /* classic cell: lands in the song */
    CHECK_EQ(s.notes[1][2][3], 40);
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(n > 0 && n < 200);
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(y.sections, 2); CHECK_EQ(y.arrange_len, 2); CHECK_EQ(y.sect[1].key, 2);
    CHECK_EQ(y.sect[1].tempo, 150); CHECK_EQ(y.sect[1].pat[1], 9); CHECK_EQ(y.sect[1].line[0], 3);
    CHECK_EQ(songx_cell(&s, &y, 5, 4, 7), 70);
    CHECK_EQ(songx_cell(&s, &y, 5, 4, 8), 0);
    CHECK(!songx_decode(&s, &y, buf, n / 2));                    /* torn */
    buf[0] = 9;
    CHECK(!songx_decode(&s, &y, buf, n));                        /* unknown version */
}

TEST(songx_sanitize_clamps_everything)
{
    DoomoSong s;
    SongExt x;
    base(&s);
    songx_init(&x, &s);
    x.sections = 99; x.arrange_len = 0; x.sect[0].tempo = 3; x.sect[0].key = 100; x.sect[0].npat = 50;
    x.sect[0].pat[0] = 200; x.arrange[3] = 77; x.map[0][0] = 5; x.map[9][9 % SX_TRACKS] = 200; x.pool[0][0] = 250;
    CHECK(songx_sanitize(&x) > 5);
    CHECK_EQ(x.sections, SX_SECTIONS); CHECK_EQ(x.arrange_len, 1); CHECK_EQ(x.sect[0].tempo, 50);
    CHECK_EQ(x.sect[0].key, 12); CHECK_EQ(x.sect[0].npat, SX_SECT_PATS); CHECK_EQ(x.sect[0].pat[0], 0);
    CHECK_EQ(x.map[0][0], 0); CHECK_EQ(x.pool[0][0], 0);
    CHECK_EQ(songx_sanitize(&x), 0);
}

TEST(songx_pool_is_sparse_and_can_fill)
{
    DoomoSong s;
    SongExt x;
    int i;
    base(&s);
    songx_init(&x, &s);
    CHECK(songx_set_cell(&s, &x, 8, 5, 0, 0));                   /* empty: no block */
    CHECK_EQ(songx_blocks_free(&x), SX_POOL);
    for (i = 0; i < SX_POOL; i++) CHECK(songx_set_cell(&s, &x, 4 + i % 28, 4 + i / 28 * 2, 0, 50));
    CHECK_EQ(songx_blocks_free(&x), 0);
    CHECK(!songx_set_cell(&s, &x, 31, 7, 0, 50));               /* full: refused, nothing broken */
    CHECK(!songx_set_cell(&s, &x, 40, 0, 0, 50));
}

TEST(songx_arrangement_walks_sections_and_lasts_the_computed_time)
{
    DoomoSong s;
    SongExt x;
    int sec, pass, n;
    base(&s);
    songx_init(&x, &s);
    x.sect[0].npat = 2; x.sect[0].pat[0] = 0; x.sect[0].pat[1] = 1;
    songx_add_section(&x, SX_CHORUS);
    x.sect[1].pat[0] = 1;
    songx_arrange_push(&x, 1); songx_arrange_push(&x, 0);
    CHECK_EQ(songx_len(&x), 2 + 1 + 2);
    CHECK_EQ(songx_at(&x, 1, &sec, &pass), 1); CHECK_EQ(sec, 0); CHECK_EQ(pass, 1);
    CHECK_EQ(songx_at(&x, 2, &sec, &pass), 1); CHECK_EQ(sec, 1);
    CHECK_EQ(songx_at(&x, 3, &sec, &pass), 0); CHECK_EQ(sec, 0);
    n = run_to_end(&s, &x, 100000);
    CHECK(!song_playing());
    CHECK(n >= (int)songx_ticks(&s, &x) - 2 && n <= (int)songx_ticks(&s, &x) + 2);
    /* a 64-entry, 8-pass-section song is 512 passes: far beyond the classic 16 */
    for (n = 0; n < SX_SECT_PATS; n++) x.sect[0].pat[n] = 0;
    x.sect[0].npat = SX_SECT_PATS;
    x.arrange_len = SX_ARRANGE;
    for (n = 0; n < SX_ARRANGE; n++) x.arrange[n] = 0;
    CHECK_EQ(songx_len(&x), 512);
}

TEST(songx_classic_play_is_unchanged_with_a_null_extension)
{
    DoomoSong s;
    int a[40], i;
    base(&s);
    s.order_len = 2; s.order[1] = 1;
    song_play(&s, -1);
    for (i = 0; i < 40; i++) { a[i] = song_pos_note(0) * 1000 + song_pos_step(); song_tick(); }
    song_stop();
    song_play_ext(&s, 0, -1);
    for (i = 0; i < 40; i++) { CHECK_EQ(a[i], song_pos_note(0) * 1000 + song_pos_step()); song_tick(); }
    song_stop();
}

TEST(songx_section_key_shift_and_tempo_change_what_plays)
{
    DoomoSong s;
    SongExt x;
    int n1, n2;
    base(&s);
    songx_init(&x, &s);
    song_play_ext(&s, &x, -1);
    CHECK_EQ(song_pos_note(0), 60);
    song_stop();
    x.sect[0].key = 2;
    song_play_ext(&s, &x, -1);
    CHECK_EQ(song_pos_note(0), 62);
    song_stop();
    x.sect[0].key = 0;
    n1 = run_to_end(&s, &x, 100000);
    x.sect[0].tempo = 200;
    n2 = run_to_end(&s, &x, 100000);
    CHECK(n2 * 2 >= n1 - 3 && n2 * 2 <= n1 + 3);                 /* twice as fast = half as long */
}

TEST(songx_extra_tracks_fold_onto_a_lane_and_say_so)
{
    DoomoSong s;
    SongExt x;
    char msg[96];
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_folded(&x, msg, sizeof(msg)), 0);
    x.tracks = 8;
    CHECK(songx_set_cell(&s, &x, 0, 4, 0, 55));                   /* track 5 -> the harmony lane */
    song_play_ext(&s, &x, -1);
    CHECK_EQ(song_pos_note(1), 55);
    song_stop();
    CHECK_EQ(songx_folded(&x, msg, sizeof(msg)), 4);
    CHECK_STR(msg, "This song has 8 tracks, this console plays 4: tracks 5..8 are mixed in");
    s.notes[0][1][0] = 50;                                        /* a played note is never replaced */
    song_play_ext(&s, &x, -1);
    CHECK_EQ(song_pos_note(1), 50);
    song_stop();
}

TEST(songx_last_chorus_up_changes_only_the_last_chorus)
{
    DoomoSong s;
    SongExt x;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_last_chorus_up(&x, 2), 0);                     /* no chorus: nothing */
    x.sect[0].name = SX_CHORUS;
    songx_arrange_push(&x, 0);                                    /* the same section twice */
    CHECK_EQ(songx_last_chorus_up(&x, 2), 1);
    CHECK_EQ(x.sections, 2);
    CHECK_EQ(x.sect[0].key, 0); CHECK_EQ(x.sect[x.arrange[1]].key, 2); CHECK(x.arrange[0] != x.arrange[1]);
}

TEST(songx_book_keeps_pages_through_the_save_chunk_and_old_saves_load)
{
    DoomoSong s, k;
    SongExt x, y;
    static uint8_t chunk[8192];
    uint32_t n;
    int slot;
    if (SONGX_BOOK_PAGES == 0) return;
    doomo_save_ext_studio->reset();
    base(&s);
    s.title[0] = 'A';
    slot = song_book_store(&s);
    CHECK(slot >= 0);
    songx_init(&x, &s);
    x.tracks = 5; x.patterns = 8; x.sect[0].key = 3;
    songx_set_cell(&s, &x, 6, 4, 2, 66);
    CHECK(songx_book_put(slot, &doomo_song_book[slot], &x));
    k = doomo_song_book[slot];
    /* classic book: the chunk is the old bytes only; with a page it is longer and round-trips */
    n = doomo_save_ext_studio->write(chunk, sizeof(chunk));
    CHECK_EQ(n, doomo_save_ext_studio->size());
    doomo_save_ext_studio->reset();
    CHECK(!songx_book_has(slot));
    CHECK(doomo_save_ext_studio->read(chunk, n, 1));
    CHECK(songx_book_has(slot));
    CHECK(songx_book_get(slot, &doomo_song_book[slot], &y));
    CHECK_EQ(y.sect[0].key, 3); CHECK_EQ(songx_cell(&doomo_song_book[slot], &y, 6, 4, 2), 66);
    CHECK(memcmp(&k, &doomo_song_book[slot], sizeof(k)) == 0);
    /* a chunk cut inside the tail: the songs stay, the arrangements are dropped */
    doomo_save_ext_studio->reset();
    CHECK(!doomo_save_ext_studio->read(chunk, n - 3, 1) || !songx_book_has(slot));
    /* a classic save over the page drops its arrangement; an old chunk (no tail) loads */
    doomo_save_ext_studio->reset();
    slot = song_book_store(&s);
    CHECK(!songx_book_has(slot));
    n = doomo_save_ext_studio->write(chunk, sizeof(chunk));
    doomo_save_ext_studio->reset();
    CHECK(doomo_save_ext_studio->read(chunk, n, 1));
    CHECK(!songx_book_has(slot));
    doomo_save_ext_studio->reset();
}

/* ---- Mix view playback: Sound (the envelope of a lane's notes) ---------------------------------------------------------- */
#include "../native/native.h"
static uint32_t ext_tone_calls(int env, int ticks)
{
    DoomoSong s;
    SongExt x;
    uint32_t c0;
    int i;
    opt_set(OPT_MUSIC_VOL, 7);                    /* (the default may be silent in a test) */
    base(&s);
    s.vol[0] = 15;
    memset(s.notes, 0, sizeof(s.notes));
    s.notes[0][0][0] = 72;
    memset(&s.notes[0][0][1], SONG_HOLD, SONG_STEPS - 1);          /* one long note, so only the sound changes its level */
    songx_from_order(&x, &s);
    x.trk[0].env = (uint8_t)env;
    c0 = nat_tone_calls;
    song_play_ext(&s, &x, -1);
    for (i = 0; i < ticks; i++) song_tick();
    song_stop();
    return nat_tone_calls - c0;
}

TEST(song_ext_sound_shapes_change_how_a_note_dies_away)
{
    uint32_t soft = ext_tone_calls(0, 100), punchy = ext_tone_calls(1, 100), longer = ext_tone_calls(2, 100), pluck = ext_tone_calls(3, 100);
    CHECK(punchy > soft);          /* a faster fall steps the level more often */
    CHECK(pluck > soft);
    CHECK(longer < soft);          /* a long note rings on: fewer level changes */
}

/* ---- Mix view playback: Swing (a track's off-beat notes come later than the song's own swing) ---------------------------- */
static int ext_first_tone_tick(int swing, int song_swing)
{
    DoomoSong s;
    SongExt x;
    uint32_t c0;
    int i, at = -1;
    opt_set(OPT_MUSIC_VOL, 7);
    base(&s);
    s.vol[0] = 15;
    s.bpm = 120;
    s.swing = (uint8_t)song_swing;
    memset(s.notes, 0, sizeof(s.notes));
    s.notes[0][0][1] = 72;                                        /* one note, on an off-beat step */
    songx_from_order(&x, &s);
    x.trk[0].swing = (uint8_t)swing;
    song_play_ext(&s, &x, -1);
    c0 = nat_tone_calls;
    for (i = 0; i < 40 && at < 0; i++) {
        song_tick();
        if (nat_tone_calls != c0) at = i;
    }
    song_stop();
    return at;
}

TEST(song_ext_track_swing_delays_an_off_beat_note_and_the_song_swing_comes_first)
{
    int straight = ext_first_tone_tick(0, 0), swung = ext_first_tone_tick(8, 0);
    CHECK(straight >= 0);
    CHECK(swung > straight);                       /* the most swing waits a tick or two longer */
    CHECK_EQ(ext_first_tone_tick(2, 0), straight); /* a swing finer than a tick rounds away */
    /* a track swing at or under the song's own adds nothing */
    CHECK_EQ(ext_first_tone_tick(3, 5), ext_first_tone_tick(0, 5));
    /* and a classic song is untouched: the same note, no arrangement, plays when the straight one does */
    {
        DoomoSong s;
        uint32_t c0;
        int i, at = -1;
        base(&s);
        s.vol[0] = 15; s.bpm = 120; s.swing = 0;
        memset(s.notes, 0, sizeof(s.notes));
        s.notes[0][0][1] = 72;
        song_play(&s, -1);
        c0 = nat_tone_calls;
        for (i = 0; i < 40 && at < 0; i++) { song_tick(); if (nat_tone_calls != c0) at = i; }
        song_stop();
        CHECK_EQ(at, straight);
    }
}

/* ---- the velocity bar: a note's loudness is its own layer, saved with the page, and the player multiplies the track's volume ---- */
TEST(song_ext_velocity_is_a_layer_that_saves_and_loads_and_an_unaccented_page_is_the_old_page)
{
    DoomoSong s;
    SongExt x, y;
    uint8_t buf[SONGX_PAGE_BYTES], plain[SONGX_PAGE_BYTES];
    uint32_t n, n0;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_vel(&x, 0, 0, 0), SX_VEL_NORMAL);              /* a fresh layer: every note normal */
    CHECK(!songx_vel_any(&x));
    n0 = songx_encode(&s, &x, plain, sizeof(plain));
    CHECK(n0 > 0);
    CHECK_EQ(plain[5] & SX_FLAG_VEL, 0);                           /* no accents: no tail, the old bytes */
    CHECK(songx_set_vel(&x, 0, 0, 0, SX_VEL_PEAK));
    CHECK(songx_set_vel(&x, 0, 0, 5, SX_VEL_SOFT));
    CHECK(songx_set_vel(&x, 31, 7, 31, SX_VEL_LOUD));
    CHECK(!songx_set_vel(&x, 32, 0, 0, SX_VEL_LOUD));
    CHECK(!songx_set_vel(&x, 0, 0, 0, 4));
    CHECK_EQ(songx_vel(&x, 0, 0, 0), SX_VEL_PEAK);
    CHECK_EQ(songx_vel(&x, 0, 0, 5), SX_VEL_SOFT);
    CHECK_EQ(songx_vel(&x, 0, 0, 6), SX_VEL_NORMAL);               /* its neighbours are untouched */
    CHECK_EQ(songx_vel(&x, 31, 7, 31), SX_VEL_LOUD);
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(n > n0);
    CHECK(buf[5] & SX_FLAG_VEL);
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(songx_vel(&y, 0, 0, 0), SX_VEL_PEAK);
    CHECK_EQ(songx_vel(&y, 0, 0, 5), SX_VEL_SOFT);
    CHECK_EQ(songx_vel(&y, 31, 7, 31), SX_VEL_LOUD);
    CHECK_EQ(songx_vel(&y, 3, 3, 3), SX_VEL_NORMAL);
    CHECK(memcmp(x.vel, y.vel, sizeof(x.vel)) == 0);
    /* a page cut inside the tail keeps the song and drops the accents */
    CHECK(songx_decode(&s, &y, buf, n - 4));
    CHECK(!songx_vel_any(&y));
    CHECK_EQ(y.arrange_len, x.arrange_len);
    /* a plain page decodes to a layer of normals */
    CHECK(songx_decode(&s, &y, plain, n0));
    CHECK(!songx_vel_any(&y));
    /* an accent is not part of the cell byte: the song's cells are what they were */
    CHECK_EQ(songx_cell(&s, &x, 0, 0, 0), 60);
}

TEST(song_ext_pattern_copy_brings_the_accents_along)
{
    DoomoSong s;
    SongExt x;
    int np;
    base(&s);
    songx_init(&x, &s);
    CHECK(songx_set_vel(&x, 0, 0, 0, SX_VEL_LOUD));
    np = songx_pattern_copy(&s, &x, 0);
    CHECK_EQ(np, SONG_PATTERNS);
    CHECK_EQ(songx_vel(&x, np, 0, 0), SX_VEL_LOUD);
    CHECK_EQ(songx_vel(&x, np, 0, 1), SX_VEL_NORMAL);
}

static int ext_note_vol(int level, int ext, int vol)
{
    DoomoSong s;
    SongExt x;
    int i, got;
    opt_set(OPT_MUSIC_VOL, 7);
    base(&s);
    s.vol[0] = (uint8_t)vol;
    memset(s.notes, 0, sizeof(s.notes));
    s.notes[0][0][0] = 72;
    songx_from_order(&x, &s);
    songx_set_vel(&x, 0, 0, 0, level);
    nat_tone_vol[0] = 0;
    if (ext) song_play_ext(&s, &x, -1); else song_play(&s, -1);
    for (i = 0; i < 2; i++) song_tick();
    got = nat_tone_vol[0];                                        /* (song_stop sends the silence) */
    song_stop();
    return got;
}

TEST(song_ext_velocity_scales_the_tracks_volume_and_normal_plays_as_a_classic_song)
{
    int soft = ext_note_vol(SX_VEL_SOFT, 1, 10), norm = ext_note_vol(SX_VEL_NORMAL, 1, 10), loud = ext_note_vol(SX_VEL_LOUD, 1, 10),
        peak = ext_note_vol(SX_VEL_PEAK, 1, 10);
    CHECK(soft > 0);
    CHECK(soft < norm);
    CHECK(norm < loud);
    CHECK(loud < peak);
    CHECK(peak <= 15);
    CHECK_EQ(norm, ext_note_vol(SX_VEL_NORMAL, 0, 10));          /* a normal note is the classic note */
    CHECK_EQ(ext_note_vol(SX_VEL_PEAK, 1, 15), 15);              /* nothing goes over the top */
}

/* ---- Mix view pan: the lane's pan reaches the platform when a note sounds, and the middle comes back when the song stops ---------------- */
TEST(song_ext_mix_pan_is_sent_to_the_platform_while_the_song_plays_and_reset_when_it_stops)
{
    DoomoSong s;
    SongExt x;
    int i;
    opt_set(OPT_MUSIC_VOL, 7);
    base(&s);
    memset(s.notes, 0, sizeof(s.notes));
    s.notes[0][0][0] = 72;                                         /* a lead note on the first step */
    s.notes[0][2][0] = 48;                                         /* a bass note (lane 1, the harmony, gives way to the choir voice) */
    songx_from_order(&x, &s);
    x.trk[0].pan = 0;                                              /* lead left */
    x.trk[2].pan = 2;                                              /* bass right */
    nat_tone_pan[0] = nat_tone_pan[1] = nat_tone_pan[2] = nat_tone_pan[3] = 1;
    song_play_ext(&s, &x, -1);
    for (i = 0; i < 2; i++) song_tick();
    CHECK(nat_tone_vol[2] > 0 && nat_tone_vol[0] > 0);
    CHECK_EQ(nat_tone_pan[0], 0);
    CHECK_EQ(nat_tone_pan[2], 2);
    CHECK_EQ(nat_tone_pan[1], 1);                                  /* an untouched lane stays in the middle */
    song_stop();
    CHECK_EQ(nat_tone_pan[0], 1);                                  /* every other user of the channels hears the middle again */
    CHECK_EQ(nat_tone_pan[2], 1);
    /* a classic song never pans, and sends nothing while it plays */
    nat_tone_pan_calls = 0;
    song_play(&s, -1);
    for (i = 0; i < 2; i++) song_tick();
    song_stop();
    CHECK_EQ(nat_tone_pan_calls, 0);
}

/* ---- one-bar patterns: a pattern may play 16 steps and end its pass; the length, the player and the page agree ---------------------- */
TEST(song_ext_a_one_bar_pattern_ends_its_pass_early_and_is_saved_with_the_page)
{
    DoomoSong s;
    SongExt x, y;
    uint8_t buf[SONGX_PAGE_BYTES], plain[SONGX_PAGE_BYTES];
    uint32_t two, one, n, n0;
    int ticks2, ticks1;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_plen(&x, 0), SONG_STEPS);
    CHECK(!songx_set_plen(&x, 0, 20));                              /* only a bar or two */
    CHECK(!songx_set_plen(&x, 32, 16));
    n0 = songx_encode(&s, &x, plain, sizeof(plain));
    CHECK_EQ(plain[5] & SX_FLAG_SHORT, 0);                          /* no short pattern: the old bytes */
    two = songx_ticks(&s, &x);
    ticks2 = run_to_end(&s, &x, 100000);
    CHECK(songx_set_plen(&x, 0, 16));
    CHECK_EQ(songx_plen(&x, 0), 16);
    CHECK_EQ(songx_plen(&x, 1), SONG_STEPS);                        /* the others are untouched */
    one = songx_ticks(&s, &x);
    CHECK(one < two);
    CHECK(one * 2 + 2 >= two && one * 2 <= two + 2);                /* half the time, give or take a rounding tick */
    ticks1 = run_to_end(&s, &x, 100000);
    CHECK(ticks1 < ticks2);
    CHECK(ticks1 * 2 <= ticks2 + 4);                                /* and the player really ends its pass at step 16 */
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(n == n0 + 4);
    CHECK(buf[5] & SX_FLAG_SHORT);
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(songx_plen(&y, 0), 16);
    CHECK_EQ(songx_plen(&y, 1), SONG_STEPS);
    /* with accents too: both tails read back */
    songx_set_vel(&x, 0, 0, 0, SX_VEL_LOUD);
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(songx_plen(&y, 0), 16);
    CHECK_EQ(songx_vel(&y, 0, 0, 0), SX_VEL_LOUD);
    /* a page cut inside the mask keeps the song and goes back to two bars */
    CHECK(songx_decode(&s, &y, buf, n - 2));
    CHECK_EQ(songx_plen(&y, 0), SONG_STEPS);
    CHECK(songx_decode(&s, &y, plain, n0));
    CHECK_EQ(songx_plen(&y, 0), SONG_STEPS);
}

TEST(song_ext_lyric_lines_9_to_16_read_write_and_stay_out_of_the_classic_song)
{
    DoomoSong s;
    SongExt x;
    char *b;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_lines(&x), SX_LYRIC_LINES);
    CHECK_EQ(songx_lines(NULL), SONG_LYRIC_LINES);
    CHECK(!songx_lyr_any(&x));
    CHECK_EQ(songx_line_text(&s, &x, 12)[0], 0);
    CHECK_EQ(songx_line_text(&s, NULL, 12)[0], 0);                 /* no SongExt: only the classic 8 */
    CHECK_EQ(songx_line_text(&s, &x, 16)[0], 0);                   /* past the last line */
    CHECK_EQ(songx_line_text(&s, &x, -1)[0], 0);
    CHECK(songx_line_buf(&s, NULL, 9) == NULL);
    CHECK(songx_line_buf(&s, &x, 16) == NULL);
    b = songx_line_buf(&s, &x, 10);
    CHECK(b != NULL);
    strcpy(b, "la la la");
    CHECK(strcmp(songx_line_text(&s, &x, 10), "la la la") == 0);
    CHECK(songx_lyr_any(&x));
    CHECK_EQ(s.lyrics[0][0], 0);                                   /* the classic lines did not move */
    CHECK_EQ(s.lyrics[7][0], 0);
    songx_set_line_singers(&s, &x, 13, 0x5u);
    CHECK_EQ(songx_line_singers(&s, &x, 13), 0x5u);
    CHECK_EQ(songx_line_singers(&s, &x, 3), s.line_singers[3]);    /* a classic line reads the song's mask */
}

TEST(song_ext_lyric_lines_9_to_16_are_saved_with_the_page_and_a_page_without_them_is_the_old_page)
{
    DoomoSong s;
    SongExt x, y;
    uint8_t buf[SONGX_PAGE_BYTES], plain[SONGX_PAGE_BYTES];
    uint32_t n, n0;
    base(&s);
    songx_init(&x, &s);
    n0 = songx_encode(&s, &x, plain, sizeof(plain));
    CHECK(n0 > 0);
    CHECK_EQ(plain[5] & SX_FLAG_LYR, 0);                           /* no words past line 8: the old bytes */
    strcpy(songx_line_buf(&s, &x, 9), "nine");
    strcpy(songx_line_buf(&s, &x, 15), "sixteen");
    songx_set_line_singers(&s, &x, 12, 0x3u);
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(n > n0);
    CHECK(n <= n0 + 257);                                          /* the documented worst case */
    CHECK(buf[5] & SX_FLAG_LYR);
    memset(&y, 0, sizeof(y));
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK(strcmp(songx_line_text(&s, &y, 9), "nine") == 0);
    CHECK(strcmp(songx_line_text(&s, &y, 15), "sixteen") == 0);
    CHECK_EQ(songx_line_singers(&s, &y, 12), 0x3u);
    CHECK_EQ(songx_line_text(&s, &y, 10)[0], 0);                   /* a line left empty stays empty */
    CHECK(songx_lyr_any(&y));
    /* with the old page the lines are empty */
    memset(&y, 0xAA, sizeof(y));
    CHECK(songx_decode(&s, &y, plain, n0));
    CHECK(!songx_lyr_any(&y));
    /* a page cut inside the lyric tail keeps the song and drops lines 9..16 only */
    CHECK(songx_decode(&s, &y, buf, n - 3));
    CHECK(!songx_lyr_any(&y));
    CHECK_EQ(y.sections, x.sections);
    CHECK_EQ(y.arrange_len, x.arrange_len);
}

TEST(song_ext_sanitize_cleans_lyric_lines_9_to_16_and_clamps_section_lines_to_16)
{
    DoomoSong s;
    SongExt x;
    base(&s);
    songx_init(&x, &s);
    memset(x.lyr[2], 'x', sizeof(x.lyr[2]));                       /* no NUL at all */
    x.lyr[3][0] = 'o'; x.lyr[3][1] = 1; x.lyr[3][2] = 'k'; x.lyr[3][3] = 0;   /* an unprintable byte */
    x.sect[0].line[0] = 15;
    x.sect[0].line[1] = 99;
    songx_sanitize(&x);
    CHECK_EQ(x.lyr[2][SONG_LYRIC_LEN - 1], 0);                     /* always terminated */
    CHECK(strlen(x.lyr[2]) < SONG_LYRIC_LEN);
    {
        int i;
        for (i = 0; x.lyr[3][i]; i++) CHECK(x.lyr[3][i] >= 32 && x.lyr[3][i] < 127);
    }
    CHECK_EQ(x.sect[0].line[0], 15);                               /* line 16 is a real line now */
    CHECK(x.sect[0].line[1] <= 15 || x.sect[0].line[1] == SX_NO_LINE);
}

TEST(song_ext_the_player_reads_lyric_lines_9_to_16_through_the_extension)
{
    DoomoSong s;
    SongExt x;
    base(&s);
    s.singer[0] = 0; s.singer[1] = 1;                              /* a two-voice choir: the mask only keeps singers that exist */
    songx_init(&x, &s);
    songx_set_line_singers(&s, &x, 9, 0x2u);
    CHECK_EQ(song_line_mask_x(&s, &x, 9), 0x2u);
    CHECK_EQ(song_line_mask_x(&s, &x, 10), 0x1u);                  /* no mask = the first singer */
    CHECK_EQ(song_line_mask_x(&s, NULL, 9), 0);                    /* a classic song has no line 10 */
    CHECK_EQ(song_line_mask_x(&s, &x, 16), 0);
    CHECK_EQ(song_line_mask_x(&s, &x, 2), song_line_mask(&s, 2));
}

TEST(song_ext_continuation_links_follow_the_rules_of_two_block_chains)
{
    DoomoSong s;
    SongExt x;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_cont(&x, 0), -1);
    CHECK_EQ(songx_head_of(&x, 5), -1);
    CHECK(songx_set_cont(&x, 0, 5));
    CHECK_EQ(songx_cont(&x, 0), 5);
    CHECK_EQ(songx_head_of(&x, 5), 0);
    CHECK(!songx_set_cont(&x, 3, 3));                              /* not to itself */
    CHECK(!songx_set_cont(&x, 3, 32));                             /* out of range */
    CHECK(!songx_set_cont(&x, -1, 4));
    CHECK(!songx_set_cont(&x, 5, 6));                              /* 5 already ends pattern 0: no chain of three */
    CHECK(!songx_set_cont(&x, 2, 0));                              /* 0 already has a continuation of its own */
    CHECK(!songx_set_cont(&x, 2, 5));                              /* 5 belongs to pattern 0 */
    CHECK(songx_set_cont(&x, 0, 5));                               /* the same link again is fine */
    CHECK(songx_set_cont(&x, 0, 6));                               /* ... and a link can move */
    CHECK_EQ(songx_head_of(&x, 5), -1);
    CHECK_EQ(songx_head_of(&x, 6), 0);
    CHECK(songx_set_cont(&x, 0, -1));
    CHECK_EQ(songx_cont(&x, 0), -1);
    CHECK_EQ(songx_cont(NULL, 0), -1);
    CHECK(!songx_set_cont(NULL, 0, 1));
}

TEST(song_ext_a_pass_is_one_to_four_bars_and_the_song_lasts_that_long)
{
    DoomoSong s;
    SongExt x;
    uint32_t t2, t3, t4, t1;
    base(&s);
    songx_init(&x, &s);
    CHECK_EQ(songx_pass_steps(&x, 0), SONG_STEPS);
    CHECK_EQ(songx_pass_bars(&x, 0), 2);
    t2 = songx_ticks(&s, &x);
    CHECK(songx_set_plen(&x, 0, 16));
    CHECK_EQ(songx_pass_bars(&x, 0), 1);
    t1 = songx_ticks(&s, &x);
    CHECK(songx_set_plen(&x, 0, 32));
    CHECK(songx_set_cont(&x, 0, 5));
    CHECK(songx_set_plen(&x, 5, 16));
    CHECK_EQ(songx_pass_steps(&x, 0), 48);
    CHECK_EQ(songx_pass_bars(&x, 0), 3);
    t3 = songx_ticks(&s, &x);
    CHECK(songx_set_plen(&x, 5, 32));
    CHECK_EQ(songx_pass_steps(&x, 0), 64);
    CHECK_EQ(songx_pass_bars(&x, 0), 4);
    t4 = songx_ticks(&s, &x);
    CHECK(t1 < t2 && t2 < t3 && t3 < t4);
    CHECK(t4 >= t2 * 2 - 2 && t4 <= t2 * 2 + 2);                   /* four bars = twice the two */
    CHECK(t3 >= t2 * 3 / 2 - 2 && t3 <= t2 * 3 / 2 + 2);
    CHECK_EQ(songx_pass_steps(NULL, 0), SONG_STEPS);
}

TEST(song_ext_the_player_plays_the_second_block_in_the_same_pass_and_keeps_the_lyric_line)
{
    DoomoSong s;
    SongExt x;
    int i, first_blk = -1, line0, ord0;
    int ticks2, ticks4;
    base(&s);
    songx_init(&x, &s);
    strcpy(s.lyrics[0], "la la la la");
    x.sect[0].line[0] = 0;
    ticks2 = run_to_end(&s, &x, 100000);
    CHECK(songx_set_cont(&x, 0, 5));
    CHECK(songx_set_cell(&s, &x, 5, 0, 0, 70));                    /* the second block opens on its own note */
    song_play_ext(&s, &x, -1);
    line0 = song_pos_line();
    ord0 = song_pos_order();
    CHECK_EQ(song_pos_pattern(), 0);
    for (i = 0; i < 4000 && song_playing(); i++) {
        song_tick();
        if (song_pos_pattern() == 5) { first_blk = i; break; }
    }
    CHECK(first_blk > 0);
    CHECK_EQ(song_pos_order(), ord0);                              /* the same pass: the arrangement did not move on */
    CHECK_EQ(song_pos_line(), line0);                              /* ... so the same lyric line goes on */
    CHECK_EQ(song_pos_step(), 0);
    song_tick();                                                   /* the note of block two sounds on its first step */
    song_stop();
    ticks4 = run_to_end(&s, &x, 100000);
    CHECK(ticks4 >= ticks2 * 2 - 3 && ticks4 <= ticks2 * 2 + 3);   /* the song really lasts twice as long */
    CHECK(ticks4 >= (int)songx_ticks(&s, &x) - 3 && ticks4 <= (int)songx_ticks(&s, &x) + 3);
    /* a one-bar second block: three bars */
    CHECK(songx_set_plen(&x, 5, 16));
    {
        int ticks3 = run_to_end(&s, &x, 100000);
        CHECK(ticks3 > ticks2 && ticks3 < ticks4);
        CHECK(ticks3 >= (int)songx_ticks(&s, &x) - 3 && ticks3 <= (int)songx_ticks(&s, &x) + 3);
    }
}

TEST(song_ext_continuation_links_are_saved_with_the_page_and_a_page_without_them_is_the_old_page)
{
    DoomoSong s;
    SongExt x, y;
    uint8_t buf[SONGX_PAGE_BYTES], plain[SONGX_PAGE_BYTES];
    uint32_t n, n0;
    base(&s);
    songx_init(&x, &s);
    n0 = songx_encode(&s, &x, plain, sizeof(plain));
    CHECK_EQ(plain[5] & SX_FLAG_CONT, 0);                          /* no links: the old bytes */
    CHECK(songx_set_cont(&x, 0, 5));
    CHECK(songx_set_cont(&x, 1, 9));
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK_EQ(n, n0 + 1 + 2 * 2);                                   /* a count and two (head, next) pairs */
    CHECK(buf[5] & SX_FLAG_CONT);
    memset(&y, 0, sizeof(y));
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(songx_cont(&y, 0), 5);
    CHECK_EQ(songx_cont(&y, 1), 9);
    CHECK_EQ(songx_cont(&y, 2), -1);
    /* with the other tails too: all of them read back */
    CHECK(songx_set_plen(&x, 5, 16));
    songx_set_vel(&x, 0, 0, 0, SX_VEL_LOUD);
    strcpy(songx_line_buf(&s, &x, 9), "nine");
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(songx_cont(&y, 0), 5);
    CHECK_EQ(songx_plen(&y, 5), 16);
    CHECK_EQ(songx_vel(&y, 0, 0, 0), SX_VEL_LOUD);
    CHECK(strcmp(songx_line_text(&s, &y, 9), "nine") == 0);
    /* a page cut inside the links keeps the song and plays every pattern on its own */
    CHECK(songx_set_plen(&x, 5, 32));
    memset(&x.lyr, 0, sizeof(x.lyr));
    songx_set_vel(&x, 0, 0, 0, SX_VEL_NORMAL);
    n = songx_encode(&s, &x, buf, sizeof(buf));
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK_EQ(songx_cont(&y, 0), 5);
    CHECK(songx_decode(&s, &y, buf, n - 2));
    CHECK_EQ(songx_cont(&y, 0), -1);
    CHECK_EQ(songx_cont(&y, 1), -1);
    CHECK_EQ(y.sections, x.sections);
    CHECK(songx_decode(&s, &y, plain, n0));
    CHECK(!songx_cont_any(&y));
    /* a link to a pattern that does not exist is a damaged page too */
    buf[n - 1] = 99;
    CHECK(songx_decode(&s, &y, buf, n));
    CHECK(!songx_cont_any(&y));
}

TEST(song_ext_sanitize_drops_links_that_break_the_chain_rules)
{
    DoomoSong s;
    SongExt x;
    base(&s);
    songx_init(&x, &s);
    x.cont[0] = 6;                                                 /* 0 -> 5 */
    x.cont[5] = 8;                                                 /* 5 -> 7: a chain of three */
    x.cont[1] = 2;                                                 /* 1 -> 1: itself */
    x.cont[2] = 99;                                                /* past the end */
    x.cont[3] = 6;                                                 /* 3 -> 5: two heads, one block */
    songx_sanitize(&x);
    CHECK_EQ(songx_cont(&x, 0), -1);                               /* 0 -> 5 breaks because 5 has a link of its own ... */
    CHECK_EQ(songx_cont(&x, 1), -1);
    CHECK_EQ(songx_cont(&x, 2), -1);
    CHECK(songx_cont(&x, 3) == 5 || songx_cont(&x, 3) == -1);
    CHECK(songx_cont(&x, 5) == 7 || songx_cont(&x, 5) == -1);
    {
        int p;                                                     /* whatever is left obeys the rules */
        for (p = 0; p < SX_PATTERNS; p++) {
            int c = songx_cont(&x, p);
            if (c < 0) continue;
            CHECK(c != p);
            CHECK_EQ(songx_cont(&x, c), -1);
            CHECK_EQ(songx_head_of(&x, c), p);
        }
    }
}

TEST(song_ext_a_free_pattern_is_one_nobody_uses)
{
    DoomoSong s;
    SongExt x;
    int f;
    base(&s);
    songx_init(&x, &s);
    f = songx_free_pattern(&x);
    CHECK_EQ(f, SONG_PATTERNS);                                    /* the first one past the classic four */
    CHECK(songx_set_cont(&x, 0, f));
    CHECK_EQ(songx_free_pattern(&x), f + 1);                       /* now linked: taken */
    CHECK(songx_set_cell(&s, &x, f + 1, 0, 0, 60));
    CHECK_EQ(songx_free_pattern(&x), f + 2);                       /* now has notes: taken */
    x.sect[0].pat[0] = (uint8_t)(f + 2);
    CHECK_EQ(songx_free_pattern(&x), f + 3);                       /* now in a pass: taken */
    CHECK_EQ(songx_free_pattern(NULL), -1);
}
