# ch1/int1.dsc - Interlude I "Night Shift" (owner: S2; story_final.md
# sec.10). Playable, 2 minutes: the pet's first full insert. Starts by
# itself at the end of Ep 1.6 (C1_WALL stage 2, story_ch12.c). The pet
# scene feeds it; the insert is 4 frames of hell over the room (the LCD's
# pentagram), the music muted for exactly those 4; the one persistent
# change is two horn nubs (SF_PET_NUBS, STRY.pet_form; the pet's art: S3).
@pack int1 kind=episode chapter=1 minutes=2

script i1_night
  [MUTE 21]
  [HOLD 21]
  say nar "(Later. @E's tummy rumbles. / Loudly. Only a cupcake will / fix this.)"
  give ITEM_CUPCAKE
  push pet
  sfx SFX_EAT
  say nar "(@E eats the cupcake. All / of it. At once.)"
  [R8]
  say nar "(Strain...)"
  [R8]
  sfx SFX_POOP
  say nar "(plop.)"
  [R13]
  mute 4
  pal PAL_HELLIFY 16
  sfx SFX_GLITCH
  hold 4
  pal PAL_HELLIFY 0
  setf SF_PET_NUBS
  native CB_C12 C12A_PET_FORM 1
  [R2]
  sfx SFX_BEEP
  say nar "beep."
  [R8]
  say nar "{d}...it's fine."
  [R13]
  say nar "(@E has two tiny horn nubs. / They were not there a / minute ago.)"
  [MUTE 26]
  [HOLD 26]
  [DAY 4]
  native CB_C12 C12A_EP_DONE BEAT_INT1
  beat C2_VAPOR
end
