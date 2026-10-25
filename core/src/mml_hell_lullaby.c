/* mml_hell_lullaby.c - SONG_HELL_LULLABY, "Hellvile Lullaby" (owner: S5;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14 S214;
 * docs/design/music/SPEC.md sec.15.2: "HELL_LULLABY = BEEP + the redeemed
 * Riff").
 *
 * Ploddy's all-night hum, conducted by Maestro Mew's tail: "Lullaby for the
 * Wall" slowed to the hum Wisp's mum hums (E major, 3/4, on the soft pad an
 * octave down), with the jar cloud raining one pixel on the beat (the BEEP
 * motif, e g+ b . e', then a single drip a bar) and, in the second phrase,
 * the bass walking the redeemed Riff (e e g+ | e e b) as slowly as it will
 * go. A heartbeat kick; the hall sleeps. 16 bars of 144 units on every
 * channel (audio_song_check needs the four channel lengths equal), looping.
 */
const char doomo_mml_hell_lullaby_0[] =
    "t56 @9 o4 v11 l4"
    "e f+ e | b2. | a g+ f+ | g+2 e | e f+ e | >c+2.< | b a f+ | e2. |"
    "g+ a b | >c+2 <b | a g+ f+ | e2 b | e f+ e | b2 a | g+ f+ d+ | e2. |";
const char doomo_mml_hell_lullaby_1[] =
    "@14 o6 v4 [e16 g+16 b16 r16 >e8< r8 r4 | b8 r8 r4 r4 | b8 r8 r4 r4 | b8 r8 r4 r4 |]4";
const char doomo_mml_hell_lullaby_2[] =
    "@2 o3 v8 l4"
    "e2. | e2. | a2. | e2. | e2. | a2. | b2. | e2. |"
    "e e g+ | e e b | e e a | g+2 f+ | e2. | b2. | b2. | e2. |";
const char doomo_mml_hell_lullaby_3[] =
    "v4 [K4 r4 r4 | r4 r4 r4 | K4 r4 r4 | r4 H4 r4 |]4";
