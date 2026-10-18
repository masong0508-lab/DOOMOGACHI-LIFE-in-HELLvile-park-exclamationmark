# ch3/ep34.dsc - Chapter 3: Ep 3.4 "Under the Fountain" (the midpoint twist),
# flashback F3 "The Plumber's Order" and Ep 3.5 "Hattie's Last Laugh" /
# "Grampy's Garden", the two authored exorcisms (owner: S3; story_final.md
# sec.12). CUT_F3 is drawn by core/src/cutstage.c from b_f3 below.
@pack ep34 kind=episode chapter=3 minutes=40

string O34_FOUNTAIN "The fountain is shaking (plaza)"
string O35_RITES "Free Hattie and Grampy (@0/2)"

# ---- Ep 3.4: the fountain cracks ----
script s34_fountain
  sfx SFX_CREAK
  [R13]
  shake SHK_CRACK
  [FLASH W10,5]
  sfx SFX_EXPLODE
  [R5]
  say nar "(The Giggling Fountain cracks / down the middle.)"
  [R8]
  say nar "(Underneath: pipes. Purple / sludge pulses through them, / toward the Wall.)"
  [R8]
  say nar "(A note on the rim, in Dr. / Quark's hand: TWO-WAY? / CHECK THE VALVES.)"
  [R8]
  say nar "(Every pipe is stamped with / the same little hammer.)"
  npc rivet STILL
  [MUTE 26]
  [HOLD 21]
  say rivet SHOCK "{d}...That's Mum's mark." [HEART]
  [R13]
  npc rivet THAW
  say rivet ANGRY "Who re-plumbed this? This was a GIFT. It was a TWO-WAY fountain!"
  [R13]
  npc rivet RIPPLE 6
  [R3]
  say nar "(Everyone turns, slowly, / to the Mayor.)"
  [R21]
  push cutscene CUT_F3
end

script s34_confess
  duck 120 300
  say mayor SAD "Everyone was so sad after / the Mingle. So scared."
  [R8]
  say mayor SAD "I just wanted the sad to / go... away."
  [HOLD 13]
  say mayor NEUTRAL "We don't talk about the / other side.{..} That was the / whole point."
  [R13]
  [MUTE 40]
  say mayor SAD "{d}I never asked where it / went.{punch}" [HEART]
  [HOLD 26]
  sfx SFX_BELL
  [R5]
  say fuzz SAD "{..}It went to them. / And it grew."
  [R21]
  music SONG_INVASION
  say bloom SAD "I'm from over there."
  [R8]
  say bloom SAD "I've been here ten years. / I was too scared to say." [HEART]
  [R21]
  say nar "(Nana takes her hand. Nobody says anything. It's the right thing to say.)"
end

# F3 "The Plumber's Order" (40 s, sepia; CUT_F3): ten years ago
beatscript b_f3
  +0 pal PAL_SEPIA 16 8 ; shot 0 ; music SONG_SAD ; sfx SFX_WHOOSH
  +2 caption "Ten years ago."
  +13 waita F1 12 120
F1:
  +0 shot 1 ; caption "A young Mayor in a too-big / sash signs a form."
  +5 sfx SFX_TIK
  +13 waita F2 12 150
F2:
  +0 shot 2 ; caption "RE: FOUNTAIN. Keep giggles / IN. Pump glums OUT."
  +8 sfx SFX_THUD
  +21 waita F3 12 150
F3:
  +0 shot 3 ; caption "THE PLUMBER: Out where?" ; mute 26
  +13 sfx SFX_TIK
  +26 label L_PUNCH
L_PUNCH:
  +0 shot 4 ; caption "YOUNG GIGGS: {d}Not here."
  +8 sfx SFX_SCRATCH
  +13 label L_BUTTON
  +26 waita F5 12 120
F5:
  +0 shot 5 ; caption "(The signature is the same / loopy hand as the speech / in the Town Hall drawer.)"
  +13 sfx SFX_CREAK
  +26 waita F6 12 150
F6:
  +0 pal PAL_SEPIA 0 8 ; sfx SFX_SHWIP
  +8 end
end

# ---- Ep 3.5: Hattie Haha / HAZE (the prayer is the timing game) ----
script s35_hattie
  sfx SFX_GLITCH
  [R5]
  say nar "(In the burning cafe, Hattie does stand-up for nobody. She laughs a beat late.)"
  [R8]
  say hattie POSSESSED "What's brown and sticky?"
  [R13]
  [R13]
  say hattie POSSESSED "...A stick."
  [R8]
  say hattie POSSESSED "{..}Hee. Hee."
  [R8]
  say nar "(A clue in the smoke: WE / HATE THE SMOKE. WE WATER / PLANTS WHEN NOBODY LOOKS.)"
  choice nar "Free Hattie from the / smoke?" { "Begin the rite!" -> go ; "Not yet..." -> no }
go:
  push exorcism ID_HATTIE
  jlt v0 0 lost
  jlt v0 1 holds
  setf 40
  befriend hattie 3
  say hattie SICK "I was in there the whole / time. Laughing on the / wrong beat."
  [R8]
  say hattie SAD "There was someone else in / there too. She hated every / second." [HEART]
  [R8]
  say hattie HAPPY "{..}She had great timing, / though."
  end
lost:
  setf 40
  [MUTE 26]
  say nar "(Hattie's ghost tells one / joke to an empty room. / Nobody laughs.)"
  [HOLD 21]
  sfx SFX_TIK
  [R5]
  say nar "(Mascotty, in the doorway, / claps. Once.)"
  end
holds:
  say nar "(The demon holds on. Try / again when you're ready.)"
no:
end

# ---- Ep 3.5: Grampy Bean / SOOTKIN (the name is the tomato memory) ----
script s35_grampy
  sfx SFX_SQUEAK
  [R5]
  say nar "(Grampy's garden. Every / tomato has an eye. Every / eye is looking at you.)"
  [R8]
  say grampy POSSESSED "Sonny? {.}Toaster? / Do I know you, toaster?"
  [R8]
  say nar "(Hand him tomatoes, one / memory each. The seeds came / over the fence...)"
  choice nar "Help Grampy remember?" { "Begin the rite!" -> go ; "Not yet..." -> no }
go:
  push exorcism ID_GRAMPY
  jlt v0 0 lost
  jlt v0 1 holds
  setf 41
  befriend grampy 3
  say grampy HAPPY "Sunny! You're Sunny! I / remember everything! {..} / Where are my glasses?"
  [R13]
  say grampy SAD "{d}...The mouse who threw / the seeds. I never said / thank you." [HEART]
  end
lost:
  setf 41
  [MUTE 21]
  say nar "(The tomatoes stay purple. / Grampy sits on his bench / and does not get up.)"
  [HOLD 21]
  end
holds:
  say nar "(The demon holds on. Try / again when you're ready.)"
no:
end

script s35_done
  [R13]
  say fuzz NEUTRAL "The tower by the Wall, @P. / Cloud Nine Spire. Lord / Vapula waits at the top."
  [R8]
  say fuzz SAD "Go with love. {..}I'll ring / the bell when you're up / there. So you can hear it."
end
