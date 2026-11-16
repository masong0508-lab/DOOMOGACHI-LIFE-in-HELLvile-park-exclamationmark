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
