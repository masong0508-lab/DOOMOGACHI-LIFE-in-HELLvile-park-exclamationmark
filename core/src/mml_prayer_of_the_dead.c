/* mml_prayer_of_the_dead.c - "Prayer of the Dead (exorcist failure)", transcribed from the mp3.
 *
 * D# minor / F# major, 4/4 at 100 bpm, 22 bars of 192 units on every channel (audio_song_check needs
 * the four channel lengths equal). A slow organ / choir drone over the loop F# - D#m - C#(sus4) - B,
 * with low impacts at the chord changes. Timing is rubato in the recording, so notes are snapped to 16ths.
 *   ch0 lead    = the top sustained voice (church organ)
 *   ch1 harmony = chord arps on the bass root (major / minor / sus4 / dim instruments)
 *   ch2 bass    = the lowest sustained note (triangle bass)
 *   ch3 drums   = toms and kicks on the low impacts, crash on the two big swells (about 15 s and 47 s)
 * Played two octaves down: ch0-2 start with k-24 (a transpose, so every interval and the chord / bass spacing stay as
 *   transcribed; the drums are not pitched). The bass bottoms out at F#0 (about 23 Hz), below what a small speaker makes,
 *   so it is felt more than heard on a phone or GBA.
 * Not registered in songs.c: it still needs a SONG_ id, a SONG_EXT / SONG_ROW line and a stubs.c entry.
 * Not run through audio_song_check or listened to on the engine. */
const char doomo_mml_prayer_of_the_dead_0[] =
    "t100 k-24 @11 o5 v11 q8 o4 f+2.^16 r8. | r4 r16 g+2^8 o5 f+16^4 r8. o4 f+4^16 g8. g+16^2^8. r8 f+8 r16 | r8. a8. r16 o5 f+8. o4 f+4.^8 b4.^16 r16 a+8 f+4^4^16 r16 b2^8^16 r16 a8 r16 g+4 r16 f8 f+8. r16 | b4 o5 f+8 r2 r16 o4 g+16^4^16 r16 g8. f+4 r16 a8^16 o5 f+4^16 r16 o4 b4.^16 r8 | r16 f+8. f2^8 f+8^4^16 b4.^16 f+8 b8^4^16 r8 b2^16 | f2^16 r16 a4 r8 | r2 r16 f+4.^16 | r16 a+4 f+4 r4 r16 f+8^4^16 b4.^16 f+4^2^8. a+8. r16 f+16^2. f8 g+8^2^16 f4.^16^1";
const char doomo_mml_prayer_of_the_dead_1[] =
    "k-24 o4 v7 q8 @5 o4 d+4 @22 c+2^16 @4 f+8.^2. @22 d+4^4.^16 c+4. @4 b8.^2. @21 f8. @4 f+16^4 @5 d+2^16 @4 c+8.^2^16 b4.^16^4^16 c+2^8.^8 b2 @22 c+4.c+8 @5 d+4^16 @4 f+2 b16^4. @22 c+2 d+8^4. f+2^8^16 @4 c+2.^8 @5 d+16^2.^8 @22 f+8^4^16 @4 c+2^8.^2^16 b4.^16^4. @22 f+8. @4 b4.^16^2.^8. @22 c+16^1^4^16 @5 d+4. @22 c+4^16^2.^8. @4 b16^2^16 c+4.^16^1";
const char doomo_mml_prayer_of_the_dead_2[] =
    "k-24 @2 o2 v13 q8 o3 d+4 c+2^16 o2 f+8.^2. o3 d+4^4.^16 c+4. o2 b8.^2. f8. f+16^4 o3 d+2^16 c+8.^2^16 b4.^16^4^16 c+2^8.^8 b2 c+4.c+8 d+4^16 o2 f+2 b16^4. o3 c+2 d+8^4. o2 f+2^8^16 o3 c+2.^8 d+16^2.^8 o2 f+8^4^16 o3 c+2^8.^2^16 b4.^16^4. o2 f+8. o3 b4.^16^2.^8. c+16^1^4^16 d+4. c+4^16^2.^8. o2 b16^2^16 o3 c+4.^16^1";
const char doomo_mml_prayer_of_the_dead_3[] =
    "v10 r8 K16 r2 T16 r16 T16 r8 | r4 T16 r4. T16 r4 | r8. T16 r8 T16 r4. T16 r8 | r2. T16 r8. | r4. r16 T16 r8. T16 r4 | r2 r16 T16 r4. | K16 r4 C16 r2 r8 | r16 K16 r2 T16 r4 r16 | T16 r4. T16 r4. r16 T16 | r4 r16 T16 r8 T16 r4 r16 T16 r16 | r4. T16 r4. T16 r8 | r4. T16 r4. r16 T16 r16 | r1 | r4. K16 r2 r16 | r2 r16 T16 r4. | r4. T16 r2 r16 | r2. r16 K16 r8 | r1 | r1 | r2. C16 r8. | r1 | r2. r8. T16";
