# ch2/ep25.dsc - Ep 2.5 "The Power of Cuddles" and "The Bottle" (owner: S2;
# story_final.md sec.11). The first true exorcism, the first true name, the
# midpoint before the midpoint. Legacy beat C2_EXORCISE: stage 0
# Sprinklepop's rite (scripted to fail: the CUTV_RITE_FAIL vignette, her
# soul is lost, chain I finds it), 1 Steamy for real (the odds shown;
# saved or lost), then CUT_BOTTLE (both branches, no button). The two
# cutscenes are drawn by core/src/cutscene_ch12.c.
@pack ep25 kind=episode chapter=2 minutes=26

# CUTV_RITE_FAIL: THE POWER OF CUDDLES COMPELS YOU (the trailer's s11).
# FAIL x1 at 92, x2 at +14, x3 at +14 with the stamp and the red flash,
# three kazoo notes stepping down; topper R13: the candles go out one a
# frame, SOUL LOST. Shot 0 the plaza, 1 the fails, 2 the soul with its map.
beatscript bv_rite
  +0 music SONG_EXORCISM ; shot 0 ; caption "\"THE POWER OF CUDDLES / COMPELS YOU!\""
  +26 sfx SFX_HOLY
  +34 caption "{d}ow."
  +92 label F1
F1:
  +0 shot 1 ; sfx SFX_KAZOO ; shake SHK_STAMP ; caption "FAIL x1 (spray bottle)"
  +14 label F2
F2:
  +0 sfx SFX_KAZOO ; shake SHK_STAMP ; caption "FAIL x2 (a real hug)"
  +14 label F3
F3:
  +0 sfx SFX_KAZOO ; shake SHK_STAMP3 ; flash PAL_UI_RED 10 4 ; caption "FAIL x3 (the whole bucket)"
  +13 label L_PUNCH
L_PUNCH:
  +0 mute 40 ; sfx SFX_TIK
  +5 sfx SFX_TIK
  +8 sfx SFX_THUD ; shake SHK_SLAM ; caption "SOUL LOST."
  +21 label L_BUTTON
L_BUTTON:
  +0 shot 2 ; sfx SFX_POP2 ; caption "Her soul pops out. It holds / a map upside down, waves, / and wanders off."
  +26 waita END 12 140
END:
  +0 end
end

# CUT_BOTTLE (30 s, both branches): Fuzz holds the empty vape bottle up to
# the light. Inside, tiny: a chalk stub, a tally mark. No button.
beatscript bv_bottle
  +0 music SONG_SAD ; shot 0 ; caption "(Father Fuzz holds the empty / vape bottle up to the / light.)"
  +26 waita LOOK 12 150
LOOK:
  +0 shot 1 ; caption "(Inside, tiny: a stub of / chalk. A tally mark.)"
  +21 hold SAY 21
SAY:
  +0 mute 41 ; caption "\"@P.{..} That was a / PERSON.\""
  +41 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_BEEP ; nobutton
  +21 iris_out 0 5
  +34 waita END 12 90
END:
  +0 end
end

script e25_rite
  say fuzz NEUTRAL "Sprinklepop first. Her demon / is smaller. A warm-up."
  say fuzz HAPPY "Hold the holy water like / you mean it!"
  push cutscene CUTV_RITE_FAIL
  native CB_C12 C12A_SOUL_LOST
  native CB_C12 C12A_STEP 1
  say nar "(Sprinklepop's body falls / asleep, smiling. They carry / her to the clinic.)"
  [MUTE 21]
  [HOLD 21]
  say fuzz SAD "We'll find her. Souls don't / go far.{..} {d}They go exactly / as far as they're scared to." [HEART]
  [R26]
  call e25_steamy
end

script e25_steamy
  native CB_C12 C12A_ODDS
  choice fuzz "Steamy now. The odds look / like @1%. Hold the water / like you mean it." { "Begin the exorcism!" -> go ; "Not yet..." -> wait }
wait:
  say fuzz "Rest. Pray. Come back to me / when you're ready."
  end
go:
  push exorcism 2
  jge v0 1 saved
  jlt v0 0 lost
  say fuzz SAD "The demon clings on! Rest, / then try again. Don't let / go of her."
  end
saved:
  setf SF_STEAMY_SAVED
  befriend steamy 4
  say steamy SICK "...Why do I taste grape?"
  [R8]
  [R13]
  say steamy SAD "There was someone in my / head. Counting. He was so / tired." [HEART]
  say fuzz NEUTRAL "The name you shouted, @P... / {..}That's a real person. / Over there."
  [R21]
  jmp bottle
lost:
  setf SF_STEAMY_LOST
  say steamy SAD "(a ghost, holding a duck) / Oh.{..} Oh, that's... cold."
  [R13]
  say fuzz SAD "When you're ready, we can lay / her to rest."
  [R21]
bottle:
  push cutscene CUT_BOTTLE
  setf SF_BOTTLE_SEEN
  [R26]
  beat C2_RIVET
end
