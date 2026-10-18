# ch2/ep24.dsc - Ep 2.4 "Steamy Isn't Home" (owner: S2; story_final.md
# sec.11). Legacy beats C2_SCREAM + C2_FUZZ. Steamy (TALLYWORN, row 0) and
# Sprinklepop (WHEEZLE, row 1) are possessed when C2_SCREAM begins
# (story_ch12.c); the practice is a scripted rule of three on Mr. Huggins
# the 2nd, then the real practice rite.
@pack ep24 kind=episode chapter=2 minutes=25

# the third duck: a record scratch, everybody freezes but Steamy, her head
# turns 180 degrees over 6 frames, the music goes
beatscript b24_freeze
  +0 tapestop 5 ; shake SHK_BRAKE ; flash PAL_WHITE 16 1
  +2 npc mayor FREEZE
  +3 npc sprinklepop FREEZE
  +5 twitch 2 1
  +8 headturn steamy 128 6
  +13 mute 60
  +26 npc mayor RIPPLE 8
  +34 end
end

# the scream is a rubber-duck squeak, enormous; everyone flees; the mascot
# does not
beatscript b24_scream
  +0 sfx SFX_SQUEAK ; shake SHK_ROAR
  +2 label L_PUNCH
L_PUNCH:
  +0 npc mayor FLEE 1 ; npc sprinklepop FLEE 2
  +5 npc mayor THAW
  +13 label L_BUTTON
L_BUTTON:
  +0 npc mascotty STILL ; sfx SFX_TIK
  +21 end
end

script e24_scream
  say steamy HAPPY "Ducks! Free ducks! One for / you, one for you, and one / for--"
  sfx SFX_SQUEAK
  [R8]
  runbeat b24_freeze
  [MUTE 26]
  [HOLD 26]
  say steamy POSSESSED "{d}Steamy isn't home right / now."
  [R13]
  say steamy POSSESSED "We moved in."
  [R8]
  say steamy POSSESSED "Leave a message after the / scream."
  [R21]
  runbeat b24_scream
  say mayor SHOCK "(under a bench) Get Father / Fuzz! Get Father Fuzz! I'll / ...get behind you!"
  resume_on_beat
  say sprinklepop POSSESSED "I'm fine! {.}We're fine!"
  sfx SFX_SPARKLE
  say nar "(Sprinklepop sneezes. The / glitter is purple.)"
  [R13]
  beat C2_FUZZ
end

script e24_steamy_wait
  say steamy POSSESSED "We count. We count every / day. Three thousand, six / hundred and..."
end

script e24_fuzz
  say fuzz HAPPY "Bless you, child. I felt it / too. A darkness has moved / into our Steamy."
  say fuzz "I am Father Fuzz. Stuffed / with love and a little bit / of buckwheat."
  say fuzz HAPPY "Nothing is unhuggable, @P. / Nothing. Some things just / need holy water first."
  say fuzz "Let's practise. Meet Mr. / Huggins the 2nd. (The 1st / has a little grave.)"
  sfx SFX_CHOIR
  say fuzz "Part one: the prayer. In / rhythm. Paws together..."
  sfx SFX_THUD
  shake SHK_SLAM
  say nar "(An arm pops off.)"
  [R8]
  [R13]
  sfx SFX_HOLY
  say fuzz NEUTRAL "Part two: holy water. / Gently!"
  sfx SFX_THUD
  shake SHK_SLAM
  say nar "(The other arm.)"
  [R8]
  [R13]
  say fuzz "Part three: the name. Say / what it IS, not what it's / wearing..."
  sfx SFX_TINK
  [R3]
  sfx SFX_TINK
  [R3]
  sfx SFX_TINK
  shake SHK_STAMP3
  flash PAL_UI_RED 8 5
  say nar "(The head rolls off, bounces / three times, and stops. / Facing the Wall.)"
  [MUTE 21]
  [R13]
  say fuzz SAD "{d}Rest in pieces, Mr. Huggins / the 2nd."
  [R8]
  say fuzz HAPPY "Again. Properly, this time. / A demon is a frightened / thing in a loud coat."
  say fuzz "You take the coat off."
  push exo_practice 0
  setf SF_FUZZ_PRACTICE
  befriend fuzz 3
  say fuzz HAPPY "Splendid! You have the paws / of a healer."
  [R13]
  say fuzz "Now, the real thing. Meet me / by poor Steamy in the / plaza. Hurry!"
  beat C2_EXORCISE
end
