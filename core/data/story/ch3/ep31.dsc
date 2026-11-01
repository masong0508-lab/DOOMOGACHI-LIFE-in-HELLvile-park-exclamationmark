# ch3/ep31.dsc - Chapter 3 "Hell Invades": Ep 3.1 "The Sky Ripped" and Ep 3.2
# "Board Up the Windows" (owner: S3; story_final.md sec.12, SPEC sec.7 S3, R11).
# One pack for the legacy beat C3_ERUPTION: the scripts, the HUD objectives
# (O31_*) and the beat scripts, so a running script and the HUD read one pack
# (the PS1 episode slot holds one pack at a time). story_ch34.c picks them.
@pack ep31 kind=episode chapter=3 minutes=50

# the authored possessions of Chapter 3 (exorcism.c rows 2 / 3); story.c re-arms
# them after a load. 40 / 41 = SF_C3_EXO0 + 0 / 1 (Hattie, Grampy decided).
@armed hattie row=2 from=C3_ERUPTION to=C3_NESTS unless=40       # HAZE
@armed grampy row=3 from=C3_ERUPTION to=C3_NESTS unless=41       # SOOTKIN

# ---- the HUD (story_ch34.c) ----
string O31_WAKE "Get up and get outside!"
string O31_RUN "Run to the church! (S)"
string O31_CHURCH "Get inside the church (S)"
string O31_RIVET "Talk to Rivet at the church door"
string O32_BOARD "Go home and board up the windows"
string O32_NIGHT "Hold your front door till dawn!"

# ---- Ep 3.1: you wake on your floor ----
script s31_wake
  [SILENCE]
  [HOLD 13]
  sfx SFX_THUD
  [R5]
  say nar "You wake up on the floor. / The front door is buzzing / like a wasp in a jar."
  [R8]
  shake SHK_RUMBLE
  music SONG_INVASION
  say nar "(Outside, the sky is red. / Not sunset red. / Wrong red.)" [HEART]
  [R13]
  sfx SFX_DEMON_ROAR
  [R5]
  say nar "Somewhere in town, a demon / screams. Somewhere else, a / clown honks back. Rudely."
  [R8]
  say nar "Everyone in town is running / to the church. Good plan. / Copy it. RUN."
end

# Squeaker the clown versus a demon pack: the honk wins
script s31_squeaker
  sfx SFX_DEMON_ROAR
  [R5]
  say ID_SQUEAKER SHOCK "Stay back! I have done / FOUR HUNDRED birthday / parties!"
  [R5]
  sfx SFX_KAZOO
  [R5]
  say nar "HONK."
  [R8]
  sfx SFX_WHOOSH
  [R5]
  say nar "The whole demon pack flees / the honk. Single file. Very / well behaved."
  [R13]
  say ID_SQUEAKER HAPPY "Hell's got nothing on a / room full of six-year-olds / on cake."
end

# Hattie and Grampy in the road: possessions three and four, staged
beatscript b31_knock
  +0 npc hattie STILL ; npc grampy STILL ; cam CAM_PAN_TO 4362 ; mute 21
  +3 sfx SFX_TIK
  +8 npc hattie TWITCH ; sfx SFX_GLITCH
  +13 headturn hattie 128 8 ; headturn grampy 128 8
  +21 sfx SFX_CREAK
  +26 label L_PUNCH
L_PUNCH:
  +0 say hattie "{d}Knock knock."
  +26 nobutton ; cam CAM_FOLLOW
  +29 end
end

script s31_knock
  runbeat b31_knock
  [R8]
  say nar "You do not ask who's there. / For once, you skip the bit. / You RUN."
  native CB_C34 C34A_STEP 2
end

# ---- the church shelter ----
script s31_church
  sfx SFX_DOOR
  music SONG_CHURCH
  duck 120 400
  say nar "The church is a pastel / island in the red. Inside: / a shadow-puppet bunny show."
  [R8]
  say lulla HAPPY "Okay, little ones! Who / knows the magic words? / Everybody say it!"
  [R8]
  say nar "KIDS: IT'S FINE!"
  [R8]
  say fuzz SHOCK "@P! You made it! Praise / be. Also: cardio."
  [R8]
  say nar "Father Fuzz is out of / breath. His hymnal has a / squashed cloud spider on it."
  [MUTE 18]
  [HOLD 18]
  say nar "(He does not look at the / hymnal. He puts it down / very, very carefully.)" [HEART]
  [R13]
  say mayor SHOCK "The sky RIPPED. Like a / cupcake wrapper. A big red / cupcake."
  [R8]
  say mayor SAD "Everyone's here. Nearly / everyone. {..}Steamy's duck is / here."
  [R8]
  say fuzz NEUTRAL "Four nests, one tower, and / our friends are possessed. / Bad tenants, all of them."
  [R8]
  say mayor NEUTRAL "And it's fine! {..}{d}It's fine.{punch}"
  [R13]
  say nar "Somebody has painted it on / the barricade in pink: / IT'S FINE! (with a smiley)"
end

script s31_window
  [SCRATCH]
  say nar "Through the window: a huge / red belly walks past. / Backwards. Eating a bagel."
  [R5]
  [SCRATCH]
  say nar "It drops the bagel. It bends / to get it. The whole window / is now just... rear."
  [R8]
  say grumble HAPPY "Evening, sugar. Are those / muffins? For the big guy?"
  [R8]
  say nana ANGRY "They're for the CHILDREN."
  [R8]
  say grumble HAPPY "I'm somebody's child."
  [R8]
  say nar "KIDS: BEEP! BEEP! BEEP!"
  [R8]
  say grumble ANGRY "Fine! Keep your muffins. / {d}I'll eat a demon. They're / mostly crust."
  sfx SFX_BOING
  [R5]
  say nar "He bounces off into the red / night, belly first, chewing."
end

script s31_rivet
  sfx SFX_WELD
  [R5]
  say rivet HAPPY "@P! Quit gawking and grab / a plate. They're heavy. / That's the point."
  give ITEM_HELLSTEEL 3
  say rivet NEUTRAL "Three welded plates. They / snap onto your windows. / Check your Shelter tab."
  [R8]
  say rivet HAPPY "And THIS. A toaster turret. / Fires every twelve frames. / Wind it every twenty shots."
  [R8]
  say rivet NEUTRAL "The door's HP is however / much junk you pile on it. / Tap A on it to fix it."
  [R8]
  jif SF_WEAPON_BRUTAL brutal
  say rivet HAPPY "Your bubble wand? Bring it. / Demons HATE bubbles."
  jmp done
brutal:
  say rivet SHOCK "Did you name it? / {..}You named it."
done:
  home SET_PHASE HP_BARRICADED
  setf SF_HOUSE_PLATED
  setf SF_HOUSE_TURRET
  [R8]
  say rivet NEUTRAL "Go home and board up. I'm / right behind you with the / torch. The WELDING torch."
end

# ---- Ep 3.2: board up the windows; the first wave; Wisp ----
script s32_board
  [NIGHT]
  sfx SFX_WELD
  [R5]
  say rivet NEUTRAL "Windows are done. Now the / door. Brace it with heavy / furniture."
  [R8]
  say rivet HAPPY "Pick a dresser. Not the / cute one. {..}Fine. The / cute one."
  [R8]
  sfx SFX_DEMON_ROAR
  shake SHK_ROAR
  say nar "Three imps at the door. Two / at the windows. One tries / the chimney. Ambitious."
  home DEFEND 1
  [R5]
  sfx SFX_THUD
  shake SHK_SLAM
  say nar "Mid-wave, the dresser slides / across the room BY ITSELF / and blocks a broken window."
  [MUTE 21]
  [R8]
  say nar "{w}not in MY room."
  [R13]
  sfx SFX_CHOIR
  [R5]
  say nar "In the corner, a small imp / kid flickers. You can see / right through them."
  [R8]
  say ID_WISP SHOCK "you can SEE me??"
  [R8]
  say ID_WISP SAD "sorry about the chair. and / the dresser. and the rug. i / was making it like home."
  [R8]
  say ID_WISP SAD "my home's over the Wall. i / cried there so long i... / fell out of myself." [HEART]
  [MUTE 8]
  [HOLD 21]
  say nar "NEW ROOMMATE: Wisp (ghost). / Rent: none. From now on, / Wisp follows you at home."
  [R8]
  say rivet NEUTRAL "...Is the dresser haunted? / Don't answer that. I don't / want to know."
end

# idle lines this episode (story_ch34.c: talking to the cast at the church)
script s31_mayor_idle
  say mayor SAD "Hee. Hee. I'm fine! I've / had three cups of tea and / {.}one small gobble. Fine!"
end

script s31_nana_idle
  say nana NEUTRAL "I saved the sourdough / starter. And the kids. / {.}In that order, sweet pea."
end

script s31_pip_idle
  say pip SHOCK "Mr. Flops isn't scared. / {..}I'm holding him extra / tight anyway."
end

script s32_rivet_idle
  say rivet NEUTRAL "Go home, @P. The boards / won't nail themselves. / {.}Actually they might."
end
