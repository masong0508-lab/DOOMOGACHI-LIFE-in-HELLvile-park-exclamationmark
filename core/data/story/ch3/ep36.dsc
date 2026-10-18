# ch3/ep36.dsc - Chapter 3: Ep 3.6 "The Climb" (Hell Breach 2, Lord Vapula),
# Ep 3.7 "The Leftover Breath" (the siege; the pet's sacrifice; the first
# player line) and flashback F2 "Mingle Night" (owner: S3; story_final.md
# sec.12). CUT_F2 is drawn by core/src/cutstage.c from b_f2 below.
@pack ep36 kind=episode chapter=3 minutes=55

string O36_SPIRE "Climb Cloud Nine Spire (E)"
string O37_PARTY "The party moves to your house"
string O37_TRAIL "Follow the trail to the Wall gate (E)"

# the Spire's floor captions (stashed before the climb, SPEC C5; combat's HUD
# shows one per floor cleared: story_ch34.c C34A_SPIRE_CAPS picks four)
string V36_F1 "COME UP, LITTLE SNACK."
string V36_F2 "You smell like Hillvile. Like / frosting over a frown."
string V36_F3P "Ooh. I KNOW your flavour."
string V36_F3N "You didn't even TRY me. Rude."
string V36_F4A "You ASKED. Nobody ever ASKS. / ...thank you."
string V36_F4N "Nobody ever asks. Nobody / ever ASKED."
string V36_F5 "Every sigh. Every it's fine. / That's ME."
string V36_F6 "I am every sigh you swallowed. / And I am SO. HUNGRY."

# ---- Ep 3.6: the climb ----
script s36_base
  sfx SFX_WHOOSH
  [R5]
  say nar "(Cloud Nine Vapor is a tower of churning vapor now.//Cloud stairs spiral into the storm.)"
  [R13]
  say nar "(From far above, a voice / like a thunderhead: / COME UP, LITTLE SNACK.)"
  choice nar "Climb Cloud Nine Spire?" { "Climb! (Hell Breach 2)" -> go ; "Not yet" -> no }
go:
  native CB_C34 C34A_SPIRE_CAPS
  push combat MAPID_CLOUD_NINE_SPIRE
  jlt v0 1 lost
  push cutscene CUT_VAPULA_DOWN
  reward 300 ITEM_GATE_SIGIL
  call s36_jar
  native CB_C34 C34A_DONE
  end
lost:
  say nar "(Lord Vapula's laughter rolls down the cloud stairs.//Catch your breath and climb again.)"
no:
end

script s36_jar
  [MUTE 26]
  [HOLD 13]
  say nar "(He does not die. A small grey cloud, the//size of a cupcake, drifts to the floor.)" [HEART]
  [R13]
  say nar "(On floor 4 you untied Vex / from a chair, with his own / shop ribbon.)"
  [R8]
  say vex SAD "I left the hatch unlocked. / Don't make it weird."
  [R8]
  sfx SFX_TINK
  [R5]
  say nar "(Vex scoops the little cloud into a jar. He puts the lid on. He pokes air holes.)"
  [R21]
  sfx SFX_SAD
  [R5]
  say nar "(The jar sighs.)"
  [R8]
  say vex SAD "He's small now."
  [HOLD 26]
  say vex NEUTRAL "He was always small. He / just had a lot of air."
  [R13]
  music SONG_VICTORY
  sfx SFX_CONFETTI
  [R5]
  say nar "(At the church: confetti, / fireworks, Nana's emergency / cake. Everyone cheers.)"
  [R8]
  say nar "(Too early.)"
end

# ---- Ep 3.7: the party, the siege, the chimney ----
script s37_party
  [NIGHT]
  say mayor HAPPY "Party! At @P's! It's the / only house still standing / with a door!"
  [R8]
  say mayor NEUTRAL "I'll stand at the window. / As a decoy. I'm very / decorative."
  [R8]
  say nar "(Pip and Mr. Flops. Miss / Lulla and two sleepy kids. / Nana with a casserole.)"
  [R8]
  say nar "(Kevin, who nobody invited, / with a bust of himself. / It has three eyes.)"
  [R8]
  say ID_WISP HAPPY "GUESTS! real ones! i'll / fluff the cushions!"
  sfx SFX_BOING
  [R5]
  say nar "(@E bounces off every wall. / Somebody starts taking the / boards down.)"
  [R13]
  [MUTE 30]
  [HOLD 21]
  sfx SFX_BEEP
  [R5]
  say nar "(The sky beeps.)"
  [R21]
  music SONG_INVASION
  shake SHK_ROAR
  sfx SFX_DEMON_ROAR
  [R5]
  say nar "(Every demon left in town / is coming up Hillvile Lane. / Something is driving them.)"
  home DEFEND 2
  [R8]
  say nar "(Wave three. The quiet. / Kevin's third eye watches / the chimney.)"
end

beatscript b37_chimney
  +0 freeze -1 ; mute 14 ; cam CAM_ZOOM 300
  +3 sfx SFX_WHOOSH
  +8 caption "(One tendril of fog comes / down the chimney.)"
  +21 sfx SFX_GLITCH
  +26 label L_PUNCH
L_PUNCH:
  +0 shake SHK_TAP
  +26 nobutton
  +29 end
end

script s37_fog
  runbeat b37_chimney
  [MUTE 30]
  say nar "???: {d}Hee.{..} Hee."
  [R13]
  say nar "???: What if they hate us. / What if they're right to."
  jifn SF_VAPE_SAMPLE nograpes
  [R8]
  say nar "???: Grape. {..}I know you."
nograpes:
  [R13]
  say nar "(It is the Mayor's voice. It goes for you: the newest, emptiest heart in town.)" [HEART]
  [HOLD 21]
  say nar "(Everyone is frozen. / Everyone but @E.)"
  [R8]
  say nar "@E: bep?"
  [R13]
  say nar "(The fog, in @E's voice, / lower:) beep."
  [R8]
  sfx SFX_WHOOSH
  [R5]
  say nar "(@E steps between you and / the fog, and breathes in. / All of it.)"
  [HOLD 5]
  sfx SFX_EAT
  insert 2 4
  native CB_C34 C34A_PET 2
  [R8]
  say nar "(@E looks at you. Two / little horns.)"
  [R8]
  say nar "@E: beep."
  [R13]
  [FLASH W7,4]
  sfx SFX_SHWIP
  [R5]
  say nar "(And then @E runs. Up the / chimney, and gone.)"
  music SONG_INVASION
  shake SHK_ROAR
  say nar "(Everyone thaws. Everyone / panics. The longest wave / of the night.)"
  native CB_C34 C34A_HELD
  jlt v0 1 dawn
  say nar "(Dawn. Four guests, safe. / Not a scratch.)"
  [R8]
  say nana SAD "Held, sweet pea. We held."
dawn:
  native CB_C34 C34A_WALL_GONE
  say nar "(Dawn. A wall is gone. The / Garden's sky shows through / the hole.)"
  [R13]
  say nar "(The egg nest is empty.)" [HEART]
  [R21]
  say nar "(Outside, a trail of tiny horned poops. One every four steps. Toward the Wall.)"
end

# ---- the gate: the first of your three lines ----
script s37_gate
  duck 100 200
  say nar "(The gate is shut. Something small squeezed under it. A scrape in the mud.)"
  [HOLD 21]
  [HOLD 13]
  [MUTE 5]
  say me "@E!" [PILL]
  [R8]
  say nar "(From the other side, very / far away, very small:)"
  sfx SFX_BEEP
  [R5]
  say nar "beep." [HEART]
  [SILENCE]
  [HOLD 26]
  iris 0 30
  [R21]
  push cutscene CUT_F2
  iris 0 0
end

# F2 "Mingle Night" (60 s; CUT_F2): eleven years ago, the full version
beatscript b_f2
  +0 shot 0 ; music SONG_MINGLE ; pal PAL_SEPIA 12 8 ; sfx SFX_SPARKLE
  +13 caption "The Midsummer Mingle. / Eleven years ago."
  +26 waita M1 12 150
M1:
  +0 shot 1 ; caption "Lanterns. Both sides of the / park, dancing. Baby Pip. / Rivet, six, with a torch."
  +8 sfx SFX_HAPPY
  +21 waita M2 12 150
M2:
  +0 shot 2 ; caption "A purple cloud rolls in, / enormous. A string trails / from it."
  +5 sfx SFX_WHOOSH ; duck 60 120
  +26 waita M3 12 150
M3:
  +0 shot 3 ; caption "Lady Smog breathes it in." ; sfx SFX_POSSESS
  +13 caption "She walks into the kids' / tent, saying their fears in / their own voices."
  +26 waita M4 12 150
M4:
  +0 shot 4 ; caption "Young Father Fuzz tries a / hug. It fails. She runs."
  +8 sfx SFX_SAD
  +21 waita M5 12 150
M5:
  +0 shot 5 ; caption "Young Giggs stands between / the two sides, shaking." ; mute 60
  +13 hold M6 13
M6:
  +0 shot 6 ; caption "He looks up at the cloud. / He does not recognise it."
  +21 waita M7 12 150
M7:
  +0 shot 7 ; caption "Nox reaches for him across / the grass. He doesn't take / her hand."
  +26 waita M8 12 150
M8:
  +0 shot 8 ; caption "YOUNG GIGGS: {d}...Close the / park."
  +26 label L_PUNCH
L_PUNCH:
  +0 nobutton ; pal PAL_SEPIA 0 0
  +3 end
end

script s37_mayor_idle
  say mayor SAD "The window was a good / spot. Very decorative. / {.}I didn't help, did I."
end
