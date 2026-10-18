# ch1/ep11.dsc - Ep 1.1 "Population: 500" (owner: S2; story_final.md
# sec.10). First laugh, first wrongness, the refrain, the grave.
# Legacy beats C1_ARRIVE + C1_MAYOR. The arrival cutscene is CUT_ARRIVAL;
# the Mayor waits at the fountain (story_ch12.c); the joke grave (W16) is
# the graveyard door, in ch1/common.dsc.
@pack ep11 kind=episode chapter=1 minutes=20

# the welcome on the plaza: the Mayor flaps over, the crowd ripples, the
# mascot does not (W4)
beatscript b11_welcome
  +0 npc mayor HOP ; sfx SFX_BOING
  +5 npc mayor RIPPLE 6
  +8 npc mascotty STILL
  +13 npc mayor EMOTE EMO_HEART
  +21 end
end

# "What's on the other side?": everybody looks at you
beatscript b11_everyone
  +0 npc mayor FREEZE ; tapestop 5
  +3 npc mayor RIPPLE 9
  +8 label L_PUNCH
L_PUNCH:
  +0 nobutton
  +21 npc mayor THAW
  +26 end
end

script e11_mayor
  runbeat b11_welcome
  say mayor HAPPY "Welcome, welcome! Mayor / Gigglesworth. Friends call / me Giggs."
  say mayor HAPPY "Enemies don't exist!"
  [R13]
  say mayor SHOCK "Ooh, a new face! We get so / few. We had a whole fence / of them once."
  [R5]
  say mayor SHOCK "{..}Not a fence OF faces. / Faces AT a fence. / Hee. Hee."
  say mayor HAPPY "Try Brimstone Bites! Named / for a joke. It's ironic! / Hee. Hee."
  [R21]
  cam CREEP 10
  say mayor HAPPY "There's the bakery! The / fountain! The flowers! / The lovely, lovely sky!"
  [R21]
  sfx SFX_WHOOSH
  cam PAN_TO cell=33,25
  say mayor SHOCK "And this is Hellvile Park. / Two sides. This one, / and..."
  [HOLD 13]
  say mayor NEUTRAL "...the weather side."
  [MUTE 13]
  [HOLD 13]
  say mayor NEUTRAL "{d}We don't talk about the / other side.{punch}"
  [HOLD 13]
  sfx SFX_BUBBLE
  [R8]
  cam FOLLOW
  jifn SF_NGPLUS menu
  say nar "(He will say it twice more.)"
menu:
  choice nar "(What do you say?)" { "Nice to meet you!" -> nice ; "What's on the other side?" -> other ; "Why is the mascot staring?" -> mascot }
nice:
  say mayor LOVE "Manners! We'll keep you."
  [R13]
  jmp key
other:
  setf SF_ASKED_OTHER_SIDE
  runbeat b11_everyone
  [HOLD 21]
  resume_on_beat
  say mayor HAPPY "Hee. Hee."
  jmp key
mascot:
  say mayor NEUTRAL "Ignore that one."
  [R21]
  native CB_C12 C12A_MASCOTTY
  say nar "(The mascot waves. Very, / very slowly.)"
key:
  say mayor HAPPY "Your key is with Nana! / Follow the smell of / nutmeg!"
  [R21]
  befriend mayor 3
  beat C1_KEY
end

# the Mayor, after the welcome
script e11_mayor_again
  say mayor HAPPY "Nana's is the one with the / striped awning! You can't / miss it. People try."
end

# a bus stop sign by the plaza (C1_ARRIVE / C1_MAYOR)
script e11_sign
  sfx SFX_TINK
  say nar "(HaHaland! Population: 500 / lovely citizens.)"
  [R8]
  say nar "(Someone has added, in / pencil: and @P.)"
  [R13]
end
