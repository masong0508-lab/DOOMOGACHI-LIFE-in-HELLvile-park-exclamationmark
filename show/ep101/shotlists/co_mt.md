# EP101 "Mingle Night": COLD OPEN, MAIN TITLE and the three EYECATCH pairs (seg = co_mt)

| | |
|---|---|
| **Covers** | COLD OPEN 00:00:00:00-00:01:40:00 (3,000 f, 21 shots) · MAIN TITLE 00:01:40:00-00:02:30:00 (1,500 f, 13 shots) · EYECATCH pairs 1-3 at 07:50, 12:15, 16:40 (3 x 300 f, 6 shots) |
| **Frames** | 3,000 + 1,500 + 900 = **5,400 f** in **40 shots** |
| **Sources** | `docs/show/COLD_OPEN_SHOTLIST.md` (converted 1:1, every +N kept), `PILOT_BEATSHEET.md` (timing), `ep101/PILOT_SCRIPT.md` SC01-SC13, SC28-29, SC38-39, SC47-48, the theme's main-title bar map (`show_lib/music/theme_main_title.json`: 150 bpm, 48 f/bar, 30 bars = 1,440 f + 60 f ring) |
| **Files** | `show/ep101/shots/co.shw` (cold open), `shots/co_mt.shw` (main title), `shots/ec1.shw` (included after a1), `ec2.shw`, `ec3.shw` (NOT included: a2 and a3 already stage EC2/EC3 with the same beat ids; kept as the co_mt versions, include exactly one per pair; see DECISIONS), `strings/co_mt.shw` |
| **Canvas** | show px on 320x180 (`doc:` = the shot list's 384x216 units, x5/6). Wall line / bandstand centre = **x 160** (doc 192) |
| **Line timing** | `show_out/table_read/LINE_TIMING.md` does not exist yet: the four Mayor lines keep the shot list's frame-locked slots (hum +0..+120, "...Hee. Hee." +4..+36, "Perfect." +153..+170, "Stay." +38..+50), which match lines.csv's target seconds (4.00 / 1.07 / 0.57 / 0.40 s). Re-fit when the table read lands. |

Columns: **id** · **f** (frames at 30 fps) · **TC in** · **set[:variant]** (stage kind) · **cast** (look, kind/size) · **camera** · **action and acting** · **lines** · **sound** (frame-0 sound first; music cues) · **out** (the cut into the NEXT shot is the next row's `in`).

## TODO (engine features this segment needs; re-check after every fetch)

Status 2026-10-25 (origin 8505a78): no compiler (`show compile`), no player, no `show.exe` on origin yet;
README has no "Staging reference" section yet. Everything below is written to spec §5-§6 and waits.

1. **Compiler** (lane 1): `show compile ep101 --shots co01..co21` - parse, time resolve, beat sums. BLOCKS everything.
2. **Timed prop state** (lane 2): `+N show <prop>[:<state>]` / `hide <prop>` (proposed `SOP_PROP_STATE`): co05 bowls, co07 open hand, co15 blooms/drip/balloon, co20 lamp/tag, mt08 candles, mt11 drip, ec IN cards. Fallback if refused: split the shot at the change with `base=` + one `change`.
3. **Prop motion**: `move <prop> to= over= tw=` on props (spec lists `move A` for actors): co10 roller, mt02 logo drops, mt11 roller. Fallback: make them actors with `kind=prop`.
4. **Flipbooks** (ART + lane 2 `pose`): `young_giggs` from behind (co04), `fluffy_bob21` (co04/06/08), `nana_paw_tuck` (co20), `mayor_roll`, `mayor_tie_balloon`, `mayor_pat`, `mayor_reach_turn`, `mayor_climb_down` (co15/co18), `hand_slip` (co07).
5. **Crowds** with a stopped clock member (`clock=none`, Mascotty, co19), `formation=lanterns` + `gen=randomize` citizens (co19), mirrored lanes (mt05).
6. **OVERWORLD** (lane 3) for mt01/mt04/mt07 (`hahaland_crane`, `hahaland_wall_pan`), **VIEW3D** + HUD gore switch (mt09 `title_corridor`), puff demon `burst=mist` and lens gibs.
7. **Camfx**: `iris_out shape=heart hold=` (co21), `cut=WIPE:brick` (mt05), `pal PAL_HELLIFY` lerp (mt07, ec IN flashes), `flash` at reduced level (mt01).
8. **Text**: `super style=type|pop|deadpan` + child_hand font (M2; game font until then); name plates `style=plate` (mt03, proposed).
9. **Audio** (AUDIO): cues not yet in `series/music.shw`: `hahaland_dawn` (co19-21), `theme_eyecatch_in` (RIFF -> ITS_FINE, every IN card). `mayor` voice takes `lib:mayor/hum_dawn_v1`, `heehee_v1`; `pet` `lib:pet/beep_v1`.
10. **ART**: `gen:logo(part=doomogachi|life|park)` for the mt02 letter drops; `gen:mascotty_tiny(h=8)` (ec IN cards; or a `scale=` on actors); `gen:candle` props on the bandstand rail (mt08); `gen:nana_paw`.

---

## COLD OPEN, "The Last Mingle" (00:00:00:00-00:01:40:00, 3,000 f)

Beats: CO-1 co01 (90) · CO-2 co02-co08 (900) · CO-3 co09 (240) · CO-4 co10-co11 (450) · CO-5 co12-co18 (630) · CO-6 co19-co21 (690).
Scenes: `co_storybook` co01-co11 (+co17 insert; crayon, kids on screen: STD-KIDS, gore=CUTE, no innuendo) · `co_dawn` co12-co18 · `co_plaza` co19-co21.

| id | f | TC in | in | set (kind) | cast · size | camera | action and acting | lines | sound | stage |
|---|---|---|---|---|---|---|---|---|---|---|
| co01 | 90 | 00:00:00:00 | FADE IN (R8) | calendar_page px=2 | - · INSERT | top-down, static | crayon circles MIDSUMMER +0..+24 (stroke reveal); fireworks scribble +30..+60, outside the lines | - | FOL_crayon_scratch +0, +30 (-3 dB); **mx mingle_box start +60** (smp 96,000) | 2D STROKES+GRAIN |
| co02 | 180 | 00:00:03:00 | CUT | park_storybook:long_ago px=1 | - · EWS | circle iris opens on the bandstand +0..+13; drift 1.0 doc px/f, x=0 at +90 (lock) | one park, no Wall; SUPER s_co02 types +30..+132 | walla kids_far on (-28 dB): w_kids_01..06 | FX_SHWIP +0; walla; music box | 2D IMAGE parallax |
| co03 | 120 | 00:00:09:00 | CUT | bridge_storybook px=2 | crowds: 6 sb bunnies (R) + 6 sb imps (L), age=child, chibi walk4 · MS | side-on, static | kids cross both ways, ripple 0-3 f, nobody minds; SUPER s_co03 +13..+63 | walla swells to -20 dB over 30 | FOL_paw_patter +0 | 2D + crowds |
| co04 | 75 | 00:00:13:00 | CUT | sky_storybook:sky | young_giggs (figure, from behind, no face) + fluffy (chibi, crayon) · MCU | low, from behind, static | arm up holding the string; Fluffy bobs (21-f cycle); Fluffy blinks +40 (2 f) | - | FX_PUFF_DRIFT +0 (the tiny puff); music box | 2D + actors |
| co05 | 60 | 00:00:15:15 | CUT | soup_table_storybook:empty px=2 | young_imp hands only (flipbook) · INSERT | top-down, static | bowl 1 down +5, bowl 2 down +20 | - | FX_BOWL_CLINK +5, +20 (frame 0: amb room tone of the music box) | 2D props |
| co06 | 150 | 00:00:17:15 | CUT | sky_storybook:sky | fluffy (chibi) · WS | low angle; tilt up 12 px over the shot (frame cherub -> tilt) | crayon fireworks bloom +15, +52, +90 (each ramps >= 8 f, Harding); Fluffy rises in +35 and looks up; heart eyes POP +95 (4 f, overshoot); SUPER s_co06 +75..+111 | - | FX_FIREWORK_SOFT +15, +52, +90; frame 0: the music box (sfx FX_PLUCK-free) | 2D + fx |
| co07 | 120 | 00:00:22:15 | CUT | sky_storybook:hand | the young cherub's hand (prop) · CU | static | a burst's light flickers on the hand +0; the loop slips off +8; HOLD 13 on the empty hand +8..+21; SUPER s_co07 +25..+93 | - | FX_FIREWORK_SOFT +0; FX_PLUCK +8 | 2D props (co17 source) |
| co08 | 195 | 00:00:26:15 | CUT | river_storybook | fluffy (chibi, shrinking) · WS | pan R 0.5 px/f (start -> end) | Fluffy drifts south over the river, embers fading, smaller; never looks back; SUPER s_co08 +105..+153 | - | music box +0; **mx mingle_box mute 29 at +75** (the Mayor's note, G5), resumes -16 dB at +104; embers fizz (FX_SPARKLE -18 dB) | 2D pan |
| co09 | 240 | 00:00:33:00 | SMASH | park_storybook:later (`base=co02@+90`, one change: the variant) | - · EWS | the co02 frame, locked | the Wall on x=160, pink drained, 2 bowls S / 1 cupcake N; deadpan SUPER s_co09 at +60 | - | FX_THUD +0, shake SHK_RUMBLE (13 f); **mx mingle_box stop +0**; silence to the end | 2D (base=) |
| co10 | 120 | 00:00:41:00 | CUT | banner_storybook:mingle | - (the roller is a prop) · MS | static | banner reads MINGLE; pink roller enters L +15, crosses in 30 f leaving GIGGLE (wipe reveal) | - | **mx mingle_thin start +0** (one voice); FX_ROLLER_LONG +15..+45 | 2D wipe |
| co11 | 330 | 00:00:45:00 | CUT | banner_storybook:giggle | - · MCU | push in 4% over the shot | SUPERs POP on the beat: +10 s_co11a, +100 s_co11b, +160 s_co11c; deadpan +240 s_co11d | - | music box downbeat +0; **mute at +240 for R26**, resumes +266 and holds a note into the cut | 2D |
| co12 | 150 | 00:00:56:00 | MATCH:co11 | bandstand_banner:giggle | mayor (figure, roller, up the ladder) · MCU -> MS | scale 1.10 -> 1.00 +60..+105 ease OUT | GIGGLE wet, a drip (drip pink prop); pull back finds the Mayor humming, wings on idle; stops +120 on the note; eyes flick + 2-f blink +120..+128 | **co12_hum** +0..+120 | hum +0 (frame 0 = the hum, at the music box's pitch); AMB_dawn_birds on | 2D + actor |
| co13 | 44 | 00:01:01:00 | CUT | bandstand_banner:giggle | mayor (bust) · MCU | eye level, static (cam scale 1.6 on him) | smile pops on the second Hee | **co13_heehee** +4..+36 | breath +0 (FX_WHOOSH -24 dB) | 2D + bust |
| co14 | 31 | 00:01:02:14 | CUT | fountain_faces:dawn px=2 | - · INSERT | static | the two stone faces giggle back, 8 f after his line ends | - | FX_FOUNTAIN_GIGGLE +0; AMB_fountain | 2D |
| co15 | 225 | 00:01:03:15 | CUT | bandstand_banner:giggle (frame ms) | mayor (figure, ladder) · MS | static | rule of three: blooms +15/+45/+76/+107 (bloom1/2/2+/3 red + drip); rolls +30/+61/+92 (15 f); +120 balloon POP out, ties over the N (TINK +128), pats twice +140; "Perfect." +153; pleased idle to the end | **co15_perfect** +153..+170 | FX_DRIP_TICK +0; FX_WET_BLIP +15/+45/+76/+107 (rising); FX_ROLLER_1/2/3 +30/+61/+92; FX_POP +120; FX_TINK +128; FX_PAT_PAT +140 | 2D + actor + prop states |
| co16 | 40 | 00:01:11:00 | CUT | bandstand_banner:topper | mayor (figure) · MS 2-shot with the balloon | static (frame ms; co18 is this frame) | glance south +10 (eyes 3 f, head 5 f); FREEZE 13 +27..+40, wings stop | - | FX_WINGS loop +0, cut at +27 | 2D + actor |
| co17 | 4 | 00:01:12:10 | CUT | sky_storybook:empty (`replay=co07@+8 insert`) | the hand · INSERT | the co07 frame | the empty hand, the loop sliding away (4 f, never 1) | - | FX_PLUCK owns all 4 f | 2D replay |
| co18 | 136 | 00:01:12:14 | CUT | bandstand_banner:turned (`base=co16@+39`, one change: balloon faces the Wall) | mayor (figure) · MS 2-shot | static | +2 creak; eyes slide +8 (R8); +21 (R13) reaches up, turns it back; +38 firm pat + "Stay."; +62 climbs down, out of frame by +116, wings resume; hold on the balloon | **co18_stay** +38..+50 | FX_BALLOON_CREAK +2; FX_BALLOON_SQUEAK +21; FX_PAT_PAT +38; FX_LADDER_CLACK +62/+67/+72; FX_WINGS +62 | 2D (base=) |
| co19 | 180 | 00:01:17:00 | CUT | fountain_plaza:dawn | citizens crowd (gen=randomize, lantern stringers, ripple 0-3) + mascotty (figure, clock none) · EWS | high; drift R 0.25 px/f (ews -> ews_end) | lanterns strung; poster s_co19_poster; fountain FG, bandstand + Wall far; Mascotty perfectly still at the far edge | - | FX_LANTERN_FWIP +0; **mx hahaland_dawn start** (low); AMB_dawn_birds, AMB_fountain | 2D + crowd |
| co20 | 240 | 00:01:23:00 | CUT | bakery_window:dawn | nana's paw (flipbook, sheepkin wool) · MS | push in 1.00 -> 1.08 over the shot | two of everything; paw in +30, tucks the towel round the egg +30..+90; lamp down +110; lingers R21; out +131..+150; tag turns +170 (s_co20_tag) | - | FX_OVEN_TIK +0; FX_TOWEL_RUSTLE +30; FX_LAMP_TINK +110; dawn arrangement | 2D + props |
| co21 | 270 | 00:01:31:00 | CUT | egg_basket:dawn px=2 | the egg (prop) · CU | static | horn +15; wobble +45; horn nearer +105; bigger wobble +135; heart iris closes +195..+200 round the egg, hold in silence; CUT ON THE CRACK | - | FX_BASKET_CREAK +0; FX_BUS_HORN_FAR +15; FX_EGG_WOBBLE +45; FX_BUS_HORN_NEAR +105; FX_EGG_WOBBLE_LOW +135; mx duck +135, out by +195; FX_SHWIP +195; silence from +200 | 2D + iris |

Sum: 90 + 900 + 240 + 450 + 630 + 690 = **3,000 f** (co02-08: 180+120+75+60+150+120+195 = 900; co10-11: 450; co12-18: 150+44+31+225+40+4+136 = 630; co19-21: 690).

---

## MAIN TITLE, "Doomtastic Times" (00:01:40:00-00:02:30:00, 1,500 f)

Cut to the master's bar map, not to the beat sheet's provisional TCs (the beat sheet says to
conform; recorded in DECISIONS). Bar B starts at MT frame 48 x (B-1); the CRACK (mt01 +0) is bar 1.1.

| Section (bars) | MT frames | Beat (this cut) | Shots |
|---|---|---|---|
| cold open + intro (1-9) | 0-431 | MT-1, 432 f | mt01, mt02 |
| verse (10-13) | 432-623 | MT-2, 192 f | mt03 |
| wall window (14) + pre-chorus "Knock, knock, knock! Come out!" (15-16.1) | 624-731 | MT-3, 108 f | mt04, mt05 |
| the drop: stop-time after "Come out!" (16.2-16.4) | 732-767 | MT-4, 36 f | mt06 |
| chorus (17-24) | 768-1151 | MT-5, 384 f | mt07-mt10 |
| post-chorus + button to the last chord (25-30.2) | 1152-1415 | MT-6, 264 f | mt11 |
| last chord's ring, mute, beep, title (30.3-ring) | 1416-1499 | MT-7, 84 f | mt12, mt13 |

Scene `mt` (montage; Pip is in mt03 only, so mt03 is never reprised under hellify: FC standards). The puff burst (mt09) has no child in frame or scene.

| id | f | TC in | in | set (kind) | cast · size | camera | action and acting | lines | sound | stage |
|---|---|---|---|---|---|---|---|---|---|---|
| mt01 | 240 | 00:01:40:00 | SMASH (on the crack) | hahaland_crane:day | town NPCs (map's own) · EWS | crane rail plaza -> Town Hall, ease OUT | white flash off the crack, 3 f at level 8 (reduced); pastel HaHaland from above | - | FX_EGG_CRACK +0; **mx theme_main_title start +0** (bar 1.1) | OVERWORLD |
| mt02 | 192 | 00:01:48:00 | CUT | cast_popin:day | - · WS | static | logo letters DROP on the intro hits: DOOMOGACHI @bar:6.1, LIFE @bar:7.1, in HELLvile Park! @bar:8.1, squash 1.22-1.38 wide on landing; all three bounce @bar:9.1 | - | FX_LETTER_DROP on each drop | 2D props |
| mt03 | 192 | 00:01:54:12 | CUT | cast_popin:day | 12 pop-ins (chibi, CUTE plates): sunny+suitcase, pet, mayor, nana, pip (+Mr. Flops), fuzz, bloom, steamy, hattie, grampy, bonejangles (at his ribs), rivet (mask+sparks); mascotty (figure, idle none) far R · WS | static | one POP per beat @bar:10.1 .. 12.4 (4-6 f, TWT_POP_BUBBLE 0.4x -> 1.2x), plate super with each; bar 13 holds on the line-up; Mascotty never pops, never moves | - | FX_NAME_POP on each pop | 2D + 13 actors |
| mt04 | 36 | 00:02:00:24 | CUT | hahaland_wall_pan:day | - · EWS | pan rail E to the Wall (wall window bar, WALL motif) | the camera arrives on the Wall | - | FX_WHOOSH +0 | OVERWORLD |
| mt05 | 72 | 00:02:02:00 | WIPE:brick (down x 160) | wall_split:day | crowds: 5 HaHaland citizens (R) / 5 Hellvile silhouettes (L, mirrored, SILHOUETTE) · WS | static | both sides run at the Wall +4..+12, mirrored to the frame; knock on "Knock, knock, knock!" +12/+18/+24 (= @bar:15.1, 15.1+6, 15.2); "Come out!" on the hold | - | FX_BRICK_WIPE +0; FX_KNOCK_WALL x3 both sides (pan L/R) | 2D + crowds |
| mt06 | 36 | 00:02:04:12 | CUT | sealed_gate:night | - · MS | static; camera STOPS (no drift) | the gate: seven padlocks, KEEP OUT. FOREVER., the grille shut | - | FX_RECORD_SCRATCH +0; tapestop 5 f; near-silence +5..+19 (mx duck -30 dB 14 f) | 2D |
| mt07 | 96 | 00:02:05:18 | CUT | hahaland_crane:day (`replay=mt01@+120`) | (no child) · EWS | the crane, again | on the lift: hellify rots the verse town, PAL_HELLIFY 0 -> 16 over 21 f from +0 | - | chorus bar 17.1 +0 (FX_HELLIFY_FLASH -12 dB) | OVERWORLD replay + pal |
| mt08 | 96 | 00:02:08:24 | CUT | bandstand_cu:north_night px=2 | - (3 candle props on the rail) · CU | static | three candles go out in rhythm: beats 1, 3, 5 (+0, +24, +48) | - | FX_CANDLE_OUT_BEAT +0/+24/+48 | 2D props |
| mt09 | 144 | 00:02:12:00 | CUT | title_corridor:cute -> truth | puff_demon (sprite) · FP | cam3d rail, first person, HUD | a Puff Demon bobs toward the lens; on "side!" (@bar:23.1 = +96) the filter drops: PAL_HELLIFY + gore BRUTAL, it bursts into red mist and gibs that slap the lens and slide off on twos by the next bar (+144); a tooth tinks off the glass | - | FX_GORE_BURST +96; FX_GIB_SPLAT +98; FX_TOOTH_TINK +110; FX_LENS_DRIP +112 | VIEW3D gore=BRUTAL |
| mt10 | 48 | 00:02:16:24 | CUT | sealed_gate:night | (a featureless round rear silhouette, a door state, no look) · MS | static | the last bar: the silhouette presses on the grille, bounces once: BOING +12 (@bar:24.2); padlocks jump +14 | - | FX_BOING_GATE +12; FOL_padlock_1 +14 | 2D |
| mt11 | 264 | 00:02:18:12 | SMASH | logo_card:pose | - · WS | static | the halves meet on the logo (logo drops +0..+10, TW bounce); the pink roller crosses from +15 in 30 f, paints over the red drip (variant paint by +45); on the last chord (@bar:30.1 = +240) the drip bleeds back (the one persistent change) | - | post-chorus +0; FX_ROLLER_LONG +15; FX_WET_BLIP +240 | 2D props |
| mt12 | 36 | 00:02:27:06 | CUT (same frame, `base=mt11@+263`) | logo_card:pose | - · WS | static | MUTE 13; "beep." +13; the last sparkle is purple and sinks +14..+35 | **mt7_pet_01** +13 | mx theme_main_title mute (to the end) +0; PET beep +13 (line); FX_SPARKLE -20 dB +14 | 2D fx |
| mt13 | 48 | 00:02:28:12 | FADE (dissolve 13 f) | title_card:dusk | - · MS | static | the Giggle banner: GIGGLE NIGHT, with MINGLE showing through the wet paint (s_mt7_title); holds 35 f after the dissolve | - | frame 0: room tone only (AMB_night_fair -30 dB in, for 1-01) | 2D |

Sum: 240 + 192 + 192 + 36 + 72 + 36 + 96 + 96 + 144 + 48 + 264 + 36 + 48 = **1,500 f**.

---

## EYECATCH PAIRS (3 x 300 f)

Each pair: OUT 150 f (the act-out sound on frame 0, the HEE_HEE bell, the theme's 2-bar hook in
major, a seamless 5 s loop) + IN 150 f (4-f hellify flash at reduced level, one persistent
change, Mascotty 8 px tall in the deep background, the RIFF resolving on ITS_FINE).
Scenes `ec1`, `ec2`, `ec3` (cards; no child, no gore).

| id | f | TC in | in | set (kind) | cast · size | camera | action and acting | lines | sound | stage |
|---|---|---|---|---|---|---|---|---|---|---|
| ec1o | 150 | 00:07:50:00 | CUT | eyecatch:ec1_out | - · card | static | the logo lantern, the Giggle banner, clock 21:00; lantern sways 2 px on a 75-f cycle (seamless loop) | - | FX_BONE_CLICK +0 (the act-out click); **mx theme_eyecatch_out start +0**; heehee_bell_150 +6 | 2D |
| ec1i | 150 | 00:07:55:00 | CUT | eyecatch:ec1_in | mascotty (tiny prop, 8 px) · card | static | hellify flash 4 f at level 8; the lantern glows purple like a ribcage | - | FX_HELLIFY_FLASH +0; **mx theme_eyecatch_in start +0** | 2D |
| ec2o | 150 | 00:12:15:00 | CUT | eyecatch:ec2_out | - · card | static | two eggshell halves on the string, a tiny "beep." bubble (text only, no sound), clock 21:40 | - | FX_GLASS_CRACK +0; theme_eyecatch_out +0; heehee_bell_150 +6 | 2D |
| ec2i | 150 | 00:12:20:00 | CUT | eyecatch:ec2_in | puff_demon (sprite, tiny, in shell 2) + mascotty tiny · card | static | flash 4 f; the demon sits in the second shell; beeps lower +40 | - | FX_HELLIFY_FLASH +0; FX_DEMON_BEEP +40 cc "[a lower beep]"; theme_eyecatch_in +0 | 2D |
| ec3o | 150 | 00:16:40:00 | CUT | eyecatch:ec3_out | - · card | static | three candles on the rail, two out (smoke curls like question marks), one gold; clock 22:59 | - | FX_FLAME_GUTTER +0; theme_eyecatch_out +0; heehee_bell_150 +6 | 2D |
| ec3i | 150 | 00:16:45:00 | CUT | eyecatch:ec3_in | mascotty tiny · card | static | flash 4 f; the last flame leans toward the Wall (screen right) | - | FX_HELLIFY_FLASH +0; theme_eyecatch_in +0 | 2D |

Sum: 6 x 150 = **900 f**.

## Gag beats checked against the shot list / GAG_BIBLE

Rule of three with constant 15/16-f interval and escalation in every channel (co15); whiplash = insert (co17, 4 f) + same frame with exactly one change (co18 `base=co16@+39`, one `change`); FREEZE 13 (co16); R8/R13/R21/R26 holds kept to the frame; the 8-f fountain lag (co14 +0 = 8 f after co13's line end); Mascotty the Starer (co19, mt03, every IN card); "beep." gag beep 1 of 6 (mt12); one-change "previously on" cards (ec IN).
