# ch1/ep12.dsc - Ep 1.2 "The Key in the Muffin" (owner: S2; story_final.md
# sec.10). Nana, the egg, the first room, the decorate tutorial. Legacy
# beats C1_KEY + C1_HOME. The house: the tutorial push (HSM_TUTORIAL),
# the COSY goal, the welcome basket (the starter kit is granted when you
# move in; the lamp and the nest are Nana's).
@pack ep12 kind=episode chapter=1 minutes=25

# three wrong muffins at 15 frames, the fourth crunches (the rule of three
# plus the topper R13 later)
beatscript b12_muffins
  +0 sfx SFX_EAT
  +14 sfx SFX_EAT
  +28 sfx SFX_TINK
  +41 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_SQUEAK ; flash PAL_WHITE 6 3 ; pal PAL_HELLIFY 16 0
  +3 pal PAL_HELLIFY 0 0
  +8 sfx SFX_TIK
  +21 label L_BUTTON
L_BUTTON:
  +0 sfx SFX_SPARKLE
  +8 end
end

script e12_nana
  say nana HAPPY "You're the new one! You look / hungry. Everyone looks / hungry to me."
  say nana "The key's in a muffin. / Don't ask which. I forgot. / I always forget."
  say nana HAPPY "Every tray has two of / everything. Habit. / Have a nibble, sweet pea."
  [R13]
  choice nar "(A tray of muffins.)" { "Bite one" ; "Bite them ALL" }
  say nar "(Raisin. Raisin. And... / that's a button.)"
  runbeat b12_muffins
  say nar "(CRUNCH. A key!)"
  [R13]
  [R8]
  say nana SAD "{..}And this. An egg. It's / been waiting for someone."
  [MUTE 13]
  [HOLD 13]
  say nar "(She holds on to it a little / longer than she needs to.)" [HEART]
  [R13]
  say nana HAPPY "The last one hatched into / a little demon! Ha!{.} / Kidding."
  [R8]
  say nana NEUTRAL "Mostly."
  [R21]
  give ITEM_HOUSE_KEY
  give ITEM_DOOMOGACHI_EGG
  setf SF_KEY_GIVEN
  setf SF_EGG_GIVEN
  befriend nana 3
  sfx SFX_PICKUP
  say nar "(You got the House Key and / a warm Doomogachi egg.)"
  say nana "Hillvile Lane. The pink / door. Up the stairs, / north-west!"
  [R13]
  beat C1_HOME
end

script e12_nana_again
  say nana HAPPY "Off you go! Keep that egg / warm. Lamps help. My mother / always said lamps help."
end

# entering the cottage the first time: the iris, the jelly door, the
# tutorial, the first rating, the night
beatscript b12_door
  +0 iris_in 0 13 ; sfx SFX_DOOR
  +13 sfx SFX_BOING
  +21 end
end

script e12_cottage
  runbeat b12_door
  setf SF_HOME_SEEN
  say nar "(Your very own cottage. / It smells like fresh paint / and cinnamon.)"
  say nar "(Empty, except for a crate / and a window. Let's make it / yours.)"
  [R13]
  home GRANT furn=cupcake_lamp
  push home 0x4000
  home GOAL HG_COSY
  sfx SFX_THUD
  say nar "{big}COSY: B+{/}"
  [R8]
  [R13]
  say nar "(Imaginary visitors:) / <3"
  [R13]
  say nar "<3 <3"
  [R13]
  say nar "...is that a crate?"
  [R13]
  sfx SFX_SQUEAK
  say nar "(The crate slides one pixel / to the left, in shame.)"
  [R26]
  say nana NEUTRAL "(at the door) You put a lamp / by the egg. That's exactly / where my mother put hers." [HEART]
  [R26]
  [NIGHT]
  music SONG_HAHALAND_NIGHT
  [MUTE 21]
  [HOLD 21]
  say nar "(Night. Through the window, / far off at the Wall, a tiny / purple glow.)"
  [R21]
  say nar "(It pulses.)"
  [R21]
  say nar "(It pulses again.)"
  [MUTE 26]
  [HOLD 26]
  [DAY 2]
  native CB_C12 C12A_EP_DONE BEAT_EP1_2
  beat C1_HATCH
end

script e12_locked
  say nar "(Your new cottage! A pink / door. Locked. Nana has / the key.)"
end
