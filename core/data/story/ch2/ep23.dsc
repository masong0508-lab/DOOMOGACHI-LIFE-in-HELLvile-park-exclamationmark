# ch2/ep23.dsc - Ep 2.3 "Things Move When You're Not Looking" (owner: S2;
# story_final.md sec.11, housing_final.md sec.10.2). The scariest episode of
# Ch1-2, almost entirely funny: the house becomes HAUNTED. Legacy beat
# C2_WEIRD, stages 3-5: one night per visit home (story_ch12.c). The house
# keeps the haunt (HOME HAUNT_STEP: the chair, the wallpaper eye, the rug);
# Wisp's haunting begins here, invisible, and Ep 4.4 reads it.
@pack ep23 kind=episode chapter=2 minutes=26

# the rule of three, one creak more each time; the third is silent
beatscript b23_chair
  +0 sfx SFX_CREAK
  +14 sfx SFX_CREAK
  +17 sfx SFX_CREAK
  +30 mute 60
  +43 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_TIK ; still -1
  +21 label L_BUTTON
L_BUTTON:
  +0 sfx SFX_BEEP
  +8 sfx SFX_WHOOSH
  +21 thaw -1
  +26 end
end

script e23_night1
  [NIGHT]
  home SET_PHASE HP_HAUNTED
  home HAUNT_STEP 1
  [MUTE 21]
  [HOLD 21]
  say nar "(Night. Your chair is one / tile left of where you put / it. You did not move it.)"
  push home 0x4002
  say nar "You put it back. You look at / the bed. You look back."
  sfx SFX_CREAK
  say nar "The chair is where it was. / {.}Creak. Smug little creak."
  [R13]
  say nar "You put it back AGAIN. Look / away. Look back."
  sfx SFX_CREAK
  [R3]
  sfx SFX_CREAK
  say nar "Now it's facing the wall, / like a kid in time-out. / Creak, creak. Sulking."
  [R13]
  say nar "Third time. You put it back, / you look away, you look back / VERY fast."
  runbeat b23_chair
  say nar "The chair is in your bed. / Tucked in. It looks cosier / than you have ever looked."
  [HOLD 21]
  say nar "From the pet bed: {.}beep."
  [R8]
  say nar "The blanket rises, and / falls. Once. {.}The chair / is breathing. Great."
  [R13]
  say nar "DECORATE MODE: Tiny bed. / Imp-sized. In the corner. / Placed by: ???"
  say nar "(Taped above it: a house / with two windows. The same / hand as the paper boats.)" [HEART]
  [R21]
  say nar "{d}Something in your house / likes your house."
  native CB_C12 C12A_SLEEP
  native CB_C12 C12A_STEP 4
end

script e23_night2
  [NIGHT]
  home HAUNT_STEP 2
  [MUTE 21]
  [HOLD 21]
  say nar "Night two. You light the / lamp. Just to be safe."
  mute 4
  pal PAL_HELLIFY 16
  sfx SFX_GLITCH
  hold 4
  pal PAL_HELLIFY 0
  [R2]
  sfx SFX_TIK
  say nar "Every heart on the wallpaper / was an eye. For one frame. / You saw it. It saw you."
  [R13]
  say nar "One heart is still an eye. / It blinks at you. Politely. / That's worse."
  [R26]
  say nar "It blinks again. Right on / time, like it's on the / clock. Light some lamps!"
  push home 0x4002
  say nar "Three lamps. Candles count / double. The eye stops / blinking. It looks sulky."
  [R8]
  say nar "{d}It is still an eye."
  [R13]
  sfx SFX_BEEP
  say nar "In its sleep, @E's nub / grows a little. {.}beep."
  say nana HAPPY "Nana here! Wallpaper / doesn't blink, sweet pea. / {..}Mostly. Night night!"
  native CB_C12 C12A_SLEEP
  native CB_C12 C12A_STEP 5
end

script e23_night3
  [NIGHT]
  home HAUNT_STEP 3
  home PLACE_GIFT furn=pentagram_rug
  [MUTE 21]
  [HOLD 21]
  say nar "Night three. Two pieces of / furniture have shuffled / toward the cellar hatch."
  say nar "Under your bed: a pentagram / rug. You didn't buy it. It / matches the curtains."
  choice nar "Where do you sleep?" { "In the living room" -> couch ; "In my bed, anyway" -> bed }
couch:
  say nar "You sleep on the sofa. All / night, something in the / bedroom tidies up. Loudly."
  jmp morning
bed:
  say nar "Your bed. Brave. You wake / up with the chair cuddling / you. It's a good cuddler."
  sfx SFX_LOVE
  say nar "BRAVE STICKER GET!"
morning:
  [R21]
  say nar "{d}...It moved the rug too. / It looks better, honestly."
  native CB_C12 C12A_SLEEP
  beat C2_SCREAM
end
