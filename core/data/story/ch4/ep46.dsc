# ch4/ep46.dsc - Chapter 4: flashback F4 "Cupcakes and Bone Soup" and Ep 4.6
# "The Summit" (the climax; owner: S3; story_final.md sec.13). CUT_F4,
# CUT_SUMMIT and CUT_SAYIT are drawn by core/src/cutstage.c and
# core/src/story_summit.c. "Say It": every friend you pass says one unsaid
# thing (P46_* the pool, C46_* the cast's own lines; story_summit.c picks per
# friend with story_rng and remembers what was said in STRY.sayit_said), and
# then you say yours: the third of the player's three lines. CUT_WALL_FALLS
# is never undercut (SF sec.7 rule 7).
@pack ep46 kind=episode chapter=4 minutes=35

string OF4_FETCH "Fetch the Mayor from the Town Hall (HaHaland)"
string O46_SUMMIT "Take the Mayor to Mother Nox's table"

# ---- the Say It pool (P46_00 .. P46_47: one unsaid thing each) ----
string P46_00 "I'm scared of the dark."
string P46_01 "I miss my brother."
string P46_02 "I never liked grape."
string P46_03 "I'm tired all the time."
string P46_04 "I broke the vase. In 1999."
string P46_05 "I don't know how to swim."
string P46_06 "I cry at weddings. Every / wedding."
string P46_07 "I wanted to be a painter."
string P46_08 "I still sleep with the light / on."
string P46_09 "I'm not as happy as I look."
string P46_10 "I miss my mum's cooking."
string P46_11 "I talk to my plants. They / talk back. Quietly."
string P46_12 "I never said sorry to my / best friend."
string P46_13 "I'm scared of being boring."
string P46_14 "I don't like parties. I come / anyway."
string P46_15 "I'm lonely on Sundays."
string P46_16 "I'm afraid of getting old."
string P46_17 "I pretend to read the news."
string P46_18 "I wanted to go over there. / My whole life."
string P46_19 "I failed my driving test six / times."
string P46_20 "I hate my laugh."
string P46_21 "I'm scared of the Wall. Even / now it's coming down."
string P46_22 "I lied about my age."
string P46_23 "I don't know what I want."
string P46_24 "I'm jealous of my sister."
string P46_25 "I worry nobody will come to / my funeral."
string P46_26 "I eat the last cookie. Every / time. It's me."
string P46_27 "I'm sorry I left."
string P46_28 "I never told her I loved her."
string P46_29 "I'm afraid of thunder."
string P46_30 "I can't sing. I sing anyway."
string P46_31 "I don't like my job."
string P46_32 "I miss the old park."
string P46_33 "I think about the other side / every day."
string P46_34 "I'm scared I'm not a good / parent."
string P46_35 "I have no idea what I'm / doing. Ever."
string P46_36 "I hid in the bathroom at my / own birthday."
string P46_37 "I never forgave myself."
string P46_38 "I want to start again."
string P46_39 "I'm scared of the fountain."
string P46_40 "I was a little bit possessed / once. It was nice. Sorry."
string P46_41 "I don't actually like / cupcakes."
string P46_42 "I write letters I never send."
string P46_43 "I'm afraid I'll be forgotten."
string P46_44 "I keep my dad's hat. It / still smells like him."
string P46_45 "I'm happy. Really happy. / Today. It scares me."
string P46_46 "I kept a soot sprite in a / jar. I let it go today."
string P46_47 "I thought I was the only / one."

# ---- the cast's own unsaid things (story_summit.c's table) ----
string C46_NANA "I'm older than I tell you. / I'm tired. I bake anyway."
string C46_PIP "I cried. A lot. That's okay, / right? It's okay."
string C46_FUZZ "I don't always know if it / works. I hug anyway."
string C46_RIVET "I missed home. Every / single day."
string C46_BLOOM "I'm from here."
string C46_STEAMY "I get lonely in the steam."
string C46_HATTIE "I'm funny so nobody looks / too close."
string C46_GRAMPY "I'm scared of forgetting / you. All of you."
string C46_LULLA "Monsters are real. They / also nap."
string C46_VEX "The cloud was the only one / who listened. I was wrong."
string C46_NOX "I set his bowl every year."
string C46_TALLY "I don't know what to do / with my hands."
string C46_SOOTKIN "I wanted something to grow / where nothing grows."
string C46_SMOG "I was afraid of air."
string C46_ANVIL "I thought she hated me."
string C46_WISP "i was scared i'd never wake / up."
string C46_MASCOTTY "..."
string C46_HAZE "I like the smell of rain."
string C46_WHEEZLE "I name every ghost I cough. / I miss them."
string C46_THUMPKIN "Three, then one. Still here."
string C46_GRUMBLE "Nobody ever came up to / see my gate."
string C46_RATTLE "I'm not a bother."
string C46_SQUEAKER "I'm scared of clowns."
string C46_SPRINKLE "The glitter isn't fine. / {..}It's mostly fine."
string C46_SPROCKET "I read every letter."
string C46_CLINKER "My poems are about her."
string C46_SIGNHERE "I hate paperwork."
string C46_HAZEY "I never wanted the foghorn."
string C46_SHUTTER "I'm never in the photos."
string C46_CHARLOTTE "The cookies were burnt on / purpose. So she'd write."

# ---- F4: fetch the Mayor ----
script sf4_fetch
  say mayor SHOCK "Meet Nox? On the OTHER side? Me? Properly? In the daylight?"
  [R13]
  say mayor SAD "...You walked through demons for us. I can walk through a gate."
  [R5]
  say mayor HAPPY "I'll bring cupcakes. Nana / made them. Everyone likes / cupcakes."
  native CB_C34 C34A_TRAVEL 1
  [R8]
  say nar "(His hands shake on the gate handle. Rivet's bead is still warm.)"
  [R5]
  say grumble HAPPY "Mayor. Love the hat."
  [R5]
  say mayor NEUTRAL "It's my head."
  [R8]
  say grumble HAPPY "{d}I know."
  [R13]
  push cutscene CUT_F4
end

beatscript b_f4
  +0 shot 0 ; music SONG_MINGLE ; pal PAL_SEPIA 12 8 ; sfx SFX_SPARKLE
  +13 caption "Two kids on a blanket at the / Mingle, forty years ago."
  +26 waita K1 12 150
K1:
  +0 shot 1 ; caption "LITTLE NOX: Soup's better." ; sfx SFX_EAT
  +21 waita K2 12 120
K2:
  +0 shot 2 ; caption "LITTLE GIGGS: ...Cupcakes / are better." ; sfx SFX_EAT
  +13 label L_PUNCH
L_PUNCH:
  +0 shot 3 ; caption "BOTH: ...Both are better." ; sfx SFX_HAPPY
  +13 label L_BUTTON ; sfx SFX_SPARKLE
  +26 waita K4 12 150
K4:
  +0 pal PAL_SEPIA 0 8 ; sfx SFX_SHWIP
  +8 end
end

# ---- Ep 4.6: the summit (CUT_SUMMIT) ----
beatscript b_summit
  +0 shot 0 ; mute 300 ; sfx SFX_DOOR
  +13 caption "(Two tables pushed together. Two bowls. A plate of cupcakes.)"
  +26 waita S1 12 150
S1:
  +0 shot 1 ; caption "MAYOR: Nox. {..}I brought / cupcakes."
  +21 waita S2 12 150
S2:
  +0 shot 2 ; caption "NOX: {d}...I brought bone / soup." ; still -1
  +21 hold S3 21
S3:
  +0 shot 3 ; caption "MAYOR: ...I LOVE soup." ; sfx SFX_LOVE
  +13 waita S4 12 150
S4:
  +0 shot 4 ; caption "NOX: You're late." ; sfx SFX_TIK
  +21 hold S5 21
S5:
  +0 shot 5 ; caption "MAYOR: I closed it."
  +21 waita S6 12 150
S6:
  +0 shot 6 ; caption "NOX: I let you." ; nobutton
  +26 waita S7 12 150
S7:
  +0 shot 7 ; caption "(Vex sets the jar on the / table. A grey cloud, the / size of a cupcake.)" ; sfx SFX_TINK
  +26 hold S8 26
S8:
  +0 shot 8 ; pal PAL_CRAYON 10 0 ; caption "MAYOR: ...Hi, Fluffy."
  +2 pal PAL_CRAYON 0 0
  +28 hold S9 26
S9:
  +0 shot 9 ; caption "(Two drops of rain, inside / the jar.)" ; sfx SFX_SPLASH
  +8 sfx SFX_SPLASH
  +26 hold S10 26
S10:
  +0 shot 10 ; caption "VAPULA: (drip.)" ; sfx SFX_SPLASH
  +8 caption "VAPULA: (drip.) (drip.)" ; sfx SFX_SPLASH
  +21 label L_PUNCH
L_PUNCH:
  +0 shot 11 ; caption "MAYOR: Hee. Hee." ; music SONG_HELLVILE ; sfx SFX_HAPPY
  +13 label L_BUTTON ; sfx SFX_LOVE
  +26 waita S12 12 150
S12:
  +0 shot 12 ; caption "MAYOR: I was scared. I'm / still scared.{..} Hold my / hand?"
  +26 hold S13 26
S13:
  +0 shot 13 ; caption "(She takes it.)" ; nobutton
  +26 waita S14 12 150
S14:
  +0 shot 14 ; caption "NOX: Then I believe we have / nothing left to build a / Wall for."
  +26 waita S15 12 150
S15:
  +3 end
end

# SF_SMOG_LOST: no rain; the hold is 39 and he says it again, smaller
beatscript b_summit_dry
  +0 shot 0 ; mute 300 ; sfx SFX_DOOR
  +13 caption "(Two tables pushed together. Two bowls. A plate of cupcakes.)"
  +26 waita S1 12 150
S1:
  +0 shot 1 ; caption "MAYOR: Nox. {..}I brought / cupcakes."
  +21 waita S2 12 150
S2:
  +0 shot 2 ; caption "NOX: {d}...I brought bone / soup." ; still -1
  +21 hold S3 21
S3:
  +0 shot 3 ; caption "MAYOR: ...I LOVE soup." ; sfx SFX_LOVE
  +13 waita S4 12 150
S4:
  +0 shot 4 ; caption "NOX: You're late." ; sfx SFX_TIK
  +21 hold S5 21
S5:
  +0 shot 5 ; caption "MAYOR: I closed it."
  +21 waita S6 12 150
S6:
  +0 shot 6 ; caption "NOX: I let you." ; nobutton
  +26 waita S7 12 150
S7:
  +0 shot 7 ; caption "(Vex sets the jar on the / table. A grey cloud, the / size of a cupcake.)" ; sfx SFX_TINK
  +26 hold S8 26
S8:
  +0 shot 8 ; pal PAL_CRAYON 10 0 ; caption "MAYOR: ...Hi, Fluffy."
  +2 pal PAL_CRAYON 0 0
  +28 hold S9 39
S9:
  +0 shot 9 ; caption "(The jar stays dry.)"
  +26 hold S10 26
S10:
  +0 shot 10 ; caption "MAYOR: ...hi, Fluffy." ; sfx SFX_SAD
  +21 label L_PUNCH
L_PUNCH:
  +0 shot 11 ; caption "MAYOR: Hee. Hee." ; music SONG_HELLVILE ; sfx SFX_HAPPY
  +13 label L_BUTTON ; sfx SFX_LOVE
  +26 waita S12 12 150
S12:
  +0 shot 12 ; caption "MAYOR: I was scared. I'm / still scared.{..} Hold my / hand?"
  +26 hold S13 26
S13:
  +0 shot 13 ; caption "(She takes it.)" ; nobutton
  +26 waita S14 12 150
S14:
  +0 shot 14 ; caption "NOX: Then I believe we have / nothing left to build a / Wall for."
  +26 waita S15 12 150
S15:
  +3 end
end

# ---- Say It (CUT_SAYIT): the walk along the Wall ----
beatscript b_sayit
  +0 shot 0 ; music SONG_HELLVILE ; sfx SFX_WHOOSH
  +13 caption "(Everyone walks to the Wall. / Both sides.)"
  +26 waita W1 12 120
W1:
  +0 shot 1 ; caption "(Rivet and Anvil re-plumb the fountain. Two-way. A new hammer mark.)" ; sfx SFX_WELD
  +26 waita W2 12 120
W2:
  +0 shot 2 ; sfx SFX_BUBBLE
  +26 waita W3 12 90
W3:
  +0 shot 3 ; sfx SFX_BUBBLE
  +26 waita W4 12 90
W4:
  +0 shot 4 ; sfx SFX_BUBBLE
  +26 waita W5 12 90
W5:
  +0 shot 5 ; sfx SFX_BUBBLE
  +26 waita W6 12 90
W6:
  +0 shot 6 ; sfx SFX_BUBBLE
  +26 waita W7 12 90
W7:
  +0 shot 7 ; sfx SFX_BUBBLE
  +26 waita W8 12 90
W8:
  +0 shot 8 ; sfx SFX_BUBBLE
  +26 waita W9 12 90
W9:
  +0 shot 9 ; sfx SFX_BUBBLE
  +26 waita W10 12 90
W10:
  +0 shot 10 ; caption "(The giggle bubbles stack / against the Wall. It's your / turn.)" ; mute 200
  +26 waita W11 12 150
W11:
  +0 nobutton
  +3 end
end

script s46_summit
  push cutscene CUT_SUMMIT
  [R13]
  say rivet NEUTRAL "Mum. The fountain. Two-way, / like you made it."
  [R5]
  say anvil HAPPY "You're my hands. I'll call / the rhythm. Ready?"
  push timing pattern=three skin=bell
  sfx SFX_BUBBLE
  push cutscene CUT_SAYIT
  player_line me "Your turn. What did you / never say?" { "I was lonely." ; "I was scared too." ; "I like it here." ; "I'm home." }
  native CB_C34 C34A_SAYIT
  [HOLD 26]
  push cutscene CUT_WALL_FALLS
end

script s46_mascotty
  [MUTE 26]
  say nar "(Mascotty walks into the / rubble. She takes off her / head. For the first time.)"
  [HOLD 26]
  say nar "(A small, tired bunnykin.)" [HEART]
  [R21]
  sfx SFX_THUD
  say nar "(Thumpkin thumps the ground / once.)"
  [R21]
  sfx SFX_THUD
  say nar "(She thumps back. Once.)"
  [MUTE 26]
  [HOLD 26]
  say nar "(They hug.)" [HEART]
  [HOLD 26]
  setf SF_MASCOTTY_HEAD_OFF
  setf SF_FOUNTAIN_TWOWAY
  music SONG_FESTIVAL
end
