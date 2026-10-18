# ch1/ep16.dsc - Ep 1.6 "The Giggling Fog" (owner: S2; story_final.md
# sec.10). The first real scare. Legacy beat C1_WALL: stage 0 the thumping
# (it starts by itself after the party), 1 the fog at the Wall, 2 home
# (Interlude I starts by itself: ch1/int1.dsc). The fog itself is the
# CUTV_FOG vignette (core/src/cutscene_ch12.c).
@pack ep16 kind=episode chapter=1 minutes=30

# woken by thumping: 3, then 1 (W6); the crickets stop one at a time
beatscript b16_thump
  +0 sfx SFX_THUD ; shake SHK_TAP
  +14 sfx SFX_THUD ; shake SHK_TAP
  +28 sfx SFX_THUD ; shake SHK_TAP
  +41 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_THUD ; shake SHK_STAMP ; nobutton
  +13 duck 100 90
  +21 sfx SFX_TIK
  +26 sfx SFX_TIK
  +34 sfx SFX_TIK
  +47 end
end

script e16_thump
  [MUTE 26]
  [HOLD 26]
  runbeat b16_thump
  say nar "(THUMP. THUMP. THUMP. / {..}THUMP.)"
  [R13]
  say nar "(It's coming from the Wall.)"
  native CB_C12 C12A_STEP 1
end

# CUTV_FOG (the Peek): the fog wall drifts; two red eyes pop (obj 1) with
# a tik, then two more at R8, R5, R3 (obj 2-4: an accelerating three plus
# one); everything goes still and silent for 40; the caption types slowly;
# a 4-frame insert (1: a Hillvile kids' tent eleven years ago, a cloud lady
# with glowing eyes, a crowned silhouette trailing a string: the first
# flash of F2); SMASH to the Mayor's face (shot 1), panting.
beatscript bv_fog
  +0 music SONG_OMINOUS ; shot 0
  +26 sfx SFX_WHOOSH
  +40 label EYE1
EYE1:
  +0 pop 1 POP_EYE ; sfx SFX_TIK
  +8 label EYE2
EYE2:
  +0 pop 2 POP_EYE ; sfx SFX_TIK
  +5 label EYE3
EYE3:
  +0 pop 3 POP_EYE ; sfx SFX_TIK
  +3 label EYE4
EYE4:
  +0 pop 4 POP_EYE ; sfx SFX_TIK ; mute 40 ; freeze -1
  +20 caption "{w}come and PLAY with us"
  +40 label L_PUNCH
L_PUNCH:
  +0 insert 1 4
  +4 shot 1 ; sfx SFX_THUD ; shake SHK_POW ; nobutton
  +30 waita END 12 90
END:
  +0 end
end

# the purple fog at the Wall: Peek or Back away; either way the Mayor
script e16_fog
  choice nar "(Purple fog seeps through / the Wall. It smells like / grape candy.)" { "Peek" -> peek ; "Back away" -> away }
peek:
  setf SF_FOG_SEEN
  push cutscene CUTV_FOG
  jmp mayor
away:
  setf SF_FOG_SEEN
  say nar "(You back away. The fog / takes one step after you.)"
  [R8]
  say nar "{w}...come and PLAY with us..."
  shake SHK_POW
  sfx SFX_THUD
mayor:
  say mayor SHOCK "{big}NOPE.{/} Nope! Home! It's / bedtime! It's always / bedtime!"
  [R13]
  say mayor NEUTRAL "It's just...{..} weather. / Hellvile weather."
  [R21]
  music SONG_MINGLE
  say nar "(He walks you home. Quietly, / he hums a little tune.)"
  [R21]
  [MUTE 13]
  [HOLD 13]
  say nar "(He stops when he notices.)"
  say mayor SAD "{d}Please don't go near there / again." [HEART]
  native CB_C12 C12A_HOME_DOOR
  [MUTE 21]
  [HOLD 21]
  say nar "(Home. @E is at the window, / staring at the Wall.)"
  say nar "(You turn @E around.)"
  [R13]
  say nar "(@E is facing the Wall / again.)"
  duck 140 13
  sfx SFX_BEEP
  say nar "beep."
  [R21]
  sfx SFX_SQUEAK
  say nar "(Under the door: a second / paper boat, and a note in / Pip's handwriting.)"
  say pip "ANOTHER ONE CAME. IT SAYS / HELLO. I DIDN'T OPEN IT. / (I opened it.)"
  [R13]
  say nar "(Inside: a candle, unlit, / and the same house. This / time a light in a window.)" [HEART]
  home GRANT furn=window_candle
  sfx SFX_PICKUP
  say nar "(You put the candle in your / window and light it.)"
  runbeat b16_glow
  [HOLD 26]
  iris off
  native CB_C12 C12A_STEP 2
end

# the iris closes on the window; in the hold, the far glow blinks once
beatscript b16_glow
  +0 iris_out 0 74
  +13 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_TINK ; nobutton
  +21 end
end
