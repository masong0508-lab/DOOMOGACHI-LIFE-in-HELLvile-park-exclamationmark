# ch1/ep13.dsc - Ep 1.3 "Hatch Day" (owner: S2; story_final.md sec.10).
# The pet, the "beep." runner, Pip, and the first paper boat. Legacy beats
# C1_HATCH, C1_PIP, C1_BUNNY, C1_BUNNY_RETURN.
@pack ep13 kind=episode chapter=1 minutes=20

# the egg wobbles at a period of 6, then 3, and hatches on the downbeat
beatscript b13_wobble
  +0 sfx SFX_EGG_CRACK
  +6 sfx SFX_TIK
  +12 sfx SFX_EGG_CRACK
  +18 sfx SFX_TIK
  +21 sfx SFX_EGG_CRACK
  +24 sfx SFX_TIK
  +26 label L_PUNCH
L_PUNCH:
  +0 flash PAL_WHITE 10 5 ; sfx SFX_POP2
  +13 label L_BUTTON
L_BUTTON:
  +0 sfx SFX_SPARKLE
  +8 end
end

script e13_hatch
  say nar "(Morning. The egg is / wobbling.)"
  runbeat b13_wobble
  push pet
  jlt v0 1 notyet
  take ITEM_DOOMOGACHI_EGG
  say nar "(@E blinks at you with two / enormous, glossy eyes.)"
  say nar "(@E hops to the lamp and / sits in the light.)"
  [MUTE 21]
  [HOLD 21]
  duck 140 6
  sfx SFX_BEEP
  say nar "beep."
  [R8]
  say nar "{d}...it's fine."
  pal PAL_HELLIFY 16
  wait 1
  pal PAL_HELLIFY 0
  [R13]
  say nar "(@E will follow you around. / START: check on it. Feed / it. Love it.)"
  say nar "(Somewhere outside, a little / kid is crying.)"
  [R21]
  native CB_C12 C12A_EP_DONE BEAT_EP1_3
  beat C1_PIP
  end
notyet:
  say nar "(The egg is still warm. / Step inside again when / you're ready.)"
end

script e13_pip
  npc pip EMOTE EMO_SWEAT
  [R3]
  npc pip EMOTE EMO_SWEAT
  [R3]
  npc pip EMOTE EMO_SWEAT
  say pip SAD "I lost Mr. Flops. He's a / bunny. I'm a bunny. / We match."
  say pip SAD "My eyes are just doing a / rain. It's fine."
  say pip "A big wave went WHOOSH and / took him. The sea goes all / the way to the Wall."
  say pip "Mr. Flops loves sand. / Maybe the beach?"
  [R13]
  befriend pip 2
  beat C1_BUNNY
end

script e13_pip_wait
  say pip SAD "Did you find him? He has one / floppy ear and one / not-floppy ear."
end

script e13_flops
  sfx SFX_SPLASH
  say nar "(Something fluffy in the / surf, bobbing up and down / with every wave.)"
  say nar "(Mr. Flops. Face down. / Staring at the Wall.)"
  [R13]
  say nar "(You turn him over. The / stitched smile.)"
  sfx SFX_FANFARE
  say nar "{big}FOUND!{/}"
  [R13]
  give ITEM_LOST_BUNNY
  setf SF_BUNNY_FOUND
  [R3]
  duck 140 13
  sfx SFX_BEEP
  say nar "(@E beeps at the sea.)"
  [R13]
  say nar "(A second shape in the surf: / a little paper boat.)"
  [MUTE 18]
  [HOLD 18]
  say nar "(Inside, a crayon drawing: a / house, a candle in the / window, two little imps.)" [HEART]
  say nar "(Along the bottom, in / backwards letters: / ERAHT UOY ERA)"
  [HOLD 13]
  say nar "(@E sits on the drawing / and will not move.)"
  beat C1_BUNNY_RETURN
end

script e13_pip_return
  say pip HAPPY "MR. FLOPS! You're damp! / Were you scared?"
  [R8]
  say pip NEUTRAL "{d}...He says he saw the / Wall breathe."
  say nar "(You show Pip the boat.)"
  say pip SHOCK "That's not my drawing. / That's somebody's HOUSE."
  [MUTE 13]
  [HOLD 13]
  say pip HAPPY "I'll keep it safe. Like / Mr. Flops." [HEART]
  take ITEM_LOST_BUNNY
  reward 30 ITEM_SKULL_TRINKET
  befriend pip 4
  say pip HAPPY "Here. My best sticker. It's / a skull. Skulls are cute."
  [R13]
  say pip "Make friends with EVERYONE! / Steamy at the pond, Hattie, / Grampy in his garden!"
  beat C1_FRIENDS
end
