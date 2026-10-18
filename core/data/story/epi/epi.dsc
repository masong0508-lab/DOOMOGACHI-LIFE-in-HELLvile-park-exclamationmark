# epi/epi.dsc - the epilogue "Happily Ever After (Mostly)" (owner: S3;
# story_final.md sec.13a): E1 the post-credits button (CUT_E1, the trailer's
# s15: the poop grows horns), E2 "The Suitcase" (the third line's caption on
# the festival polaroid), morning 17's "Days Since", and post-game greetings.
@pack epi kind=episode chapter=5 minutes=30

string OE1_FREE "Free play! Explore and help out"
string OE2_SUITCASE "Unpack your suitcase at home"

# the polaroid's caption: your Say It line (STRY.branch sayit_line 0..3)
string E2_SAY0 "I was lonely."
string E2_SAY1 "I was scared too."
string E2_SAY2 "I like it here."
string E2_SAY3 "I'm home."

beatscript b_e1
  +0 shot 0 ; music SONG_PET ; sfx SFX_DOOR
  +13 caption "(Home. Later.)"
  +26 waita P1 12 120
P1:
  +0 shot 1 ; sfx SFX_SQUEAK
  +8 sfx SFX_TIK
  +21 label L_PUNCH
L_PUNCH:
  +0 shot 2 ; sfx SFX_POOP
  +8 shot 3 ; sfx SFX_POP2
  +21 label L_BUTTON ; caption "@E: beep."
  +26 sfx SFX_BEEP
  +34 iris_out 0 74
  +55 waita P4 12 120
P4:
  +0 shot 4 ; caption "THE END... of the beginning!" ; sfx SFX_FANFARE
  +26 waita P5 12 150
P5:
  +0 shot 5 ; caption "{d}(it's fine.)"
  +26 waita P6 12 150
P6:
  +3 end
end

script se1_after
  sfx SFX_FANFARE
  say nar "(Free play is open: the / united park is yours. Both / sides. Every day.)"
  [R5]
  say nar "(Keep helping folks, hunting secrets, making friends on both sides of the river.)"
  [R13]
  say nar "(Oh. And your suitcase is / still packed. It's been / packed since the bus.)"
end

script se2_suitcase
  sfx SFX_DOOR
  [SILENCE]
  say nar "(You open the suitcase. / At last.)"
  [HOLD 26]
  say nar "(Inside: a single photo frame. / Empty.)" [HEART]
  [R21]
  say nar "(Shutterbug's festival / polaroid fits it perfectly. / You write on the back:)"
  native CB_C34 C34A_E2
  [HOLD 26]
  reward 0 furn=gallery_frame
  say nar "(Put it anywhere. It's / your place.)"
  home SET_PHASE HP_DREAM
  [R26]
  music SONG_HAHALAND_DAY
  say nar "(Town Hall has a letter for you: the Park Plot is yours. Build whatever you like.)"
end

# ---- morning 17: Days Since ----
script se_tally
  say tallyworn HAPPY "One. {..}Day one, done. / Tomorrow I'll write two."
  [R8]
  say tallyworn NEUTRAL "I could get used to this. / Counting up."
end

script se_rivet
  say rivet HAPPY "Look at the sign! One day / since the last fire. Mum / made a cake. It's on fire."
  [R8]
  say rivet NEUTRAL "{d}...It's a decorative fire."
end

script se_wisp_lie
  say ID_WISP NEUTRAL "I know she didn't say it / every night. {..}Thanks / anyway." [HEART]
end

script se_wisp_hum
  say ID_WISP HAPPY "She still hums it. On / purpose now." [HEART]
end

script se_wisp_chair
  say ID_WISP HAPPY "I sat in it. It's a good / chair." [HEART]
end

script se_grumble
  say grumble HAPPY "New kid! Come up and see my / gate sometime. {..}It's a / trolley now. Long story."
end

script se_mayor
  say mayor HAPPY "Hee. Hee. {.}That one was / real. I've been practising / the real ones."
end

script se_vex
  say vex NEUTRAL "The jar rained on my / sunflowers this morning. / Only mine. Very precise."
end
