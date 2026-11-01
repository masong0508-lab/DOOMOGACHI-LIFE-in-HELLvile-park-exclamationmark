# ch4/ep41.dsc - Chapter 4 "Hellvile Park": Ep 4.1 "The Gate" (owner: S3;
# story_final.md sec.13). Rivet's lock, Grumbleguts' toll, the record-scratch
# crossing, Tallyworn's chalk. The template "Ten years I" is allowed here once
# (SF sec.7 rule 11: Tallyworn, Ep 4.1, its first and model use).
@pack ep41 kind=episode chapter=4 minutes=25

# Lady Smog / FLUFFY (exorcism.c row 4): the story's possession for Chapter 4
@armed smog row=4 from=C4_GATE to=C4_HELP unless=SF_C4_LADY_SMOG

string O41_GATE "Meet everyone at the Wall gate (E)"
string O41_TALLY "Talk to Tallyworn at the Pump (W)"

script s41_dawn
  [DAY 0]
  music SONG_HAHALAND_DAY
  duck 255 1
  say nar "Morning. A real one. The / sky is pink and has no idea / what it did last night."
  [R13]
  say nar "The DAYS SINCE LAST FIRE / sign at the Workshop says / 0. Someone drew a flower."
  [R8]
  say mayor NEUTRAL "@P! Big day! We're / going over. All of us. / Hats on. To the gate!"
  [R5]
  say mayor SAD "{..}I'll be at the back. / Shaking. Gobbling a bit. / Hee. Hee."
end

script s41_gate
  say rivet NEUTRAL "I made the lock. I was ten."
  [R8]
  say rivet HAPPY "It's a REALLY good lock."
  [R26]
  say rivet NEUTRAL "Hold this. Pointy end / AWAY from your face."
  sfx SFX_WELD
  say nar "You cut your own bead / through Rivet's lock. WELD / RANK: D. Ugly. Yours."
  push timing pattern=rimshot skin=thump
  sfx SFX_CREAK
  [R13]
  say nar "The gate swings open. / Behind it: a belly."
  [SCRATCH]
  say nar "The belly has a gatekeeper / attached: Grumbleguts, / reading a little black book."
  [R13]
  [SCRATCH]
  say nar "He turns round. Slowly. / Rear first. It takes a / while. There's a lot of it."
  [R5]
  say grumble HAPPY "Well HELLO, Hillvile. Come up and see my gate sometime. This is my gate."
  [R5]
  say grumble NEUTRAL "Nobody passes without / giving me their number."
toll:
  choice nar "What do you give him?" { "A fake number" -> fake ; "A cupcake" -> cake ; "Is that a riding crop?" -> crop }
fake:
  say grumble HAPPY "Five, five, five... that's / the BAKERY. {..}I love the / bakery."
  jmp cake
crop:
  say grumble NEUTRAL "It's for pointing, sugar. / At exits. At snacks. {.}At / more snacks."
  jmp toll
cake:
  say nar "GURGLE: gurgle. {r}" [GURGLE]
  [R8]
  say grumble HAPPY "...The belly says yes. The / belly outranks me."
  [R8]
  say grumble HAPPY "And who is THIS stuffed / little-"
  [R5]
  say fuzz SHOCK "{d}No."
  [R5]
  say grumble HAPPY "Playing hard to get. I / respect it."
end

beatscript b41_cross
  +0 tapestop 5 ; mute 14 ; cam CAM_BRAKE 8 ; sfx SFX_SCRATCH
  +13 music SONG_HELLVILE
  +21 caption "HELLVILE. Grim AND cosy. / COSY RATING: A+.//Bone lamps, soup steam, laundry lines of black lace."
  +26 label L_PUNCH
L_PUNCH:
  +0 caption "Kids play hopscotch on / lava. In oven mitts. On / a school night." ; sfx SFX_SPLASH
  +13 label L_BUTTON ; sfx SFX_HAPPY
  +21 end
end

script s41_cross
  native CB_C34 C34A_TRAVEL 1
  runbeat b41_cross
end

script s41_tally
  say nar "Tallyworn, at the Pump, / chalk in hand, looks at the / open gate like it's a bill."
  npc tallyworn STILL
  [MUTE 26]
  [HOLD 26]
  say nar "He puts the chalk down."
  [R13]
  sfx SFX_TIK
  say nar "He picks it back up and / writes 3,661 anyway."
  [R5]
  say tallyworn NEUTRAL "{d}Habit."
  [R21]
  npc tallyworn THAW
  say tallyworn NEUTRAL "Tomorrow I'll write something else. I don't know what yet."
end

script s41_tally_name
  say tallyworn SAD "Ten years I pumped because / nobody said stop. {..}Nobody / said my name, either." [HEART]
  [R13]
  say tallyworn SAD "Some days back... someone / shouted my name over the / Wall."
  [R5]
  say tallyworn SAD "First time in ten years / anyone said it. {..}I sat down / and had a little cry."
  choice nar "Tell him it was you?" { "That was me." -> me ; "(say nothing)" -> quiet }
me:
  setf SF_TALLY_THAT_WAS_ME
  befriend tallyworn 3
  [R8]
  say tallyworn LOVE "...Oh.{..} Thank you for / saying it right." [HEART]
  jmp beep
quiet:
  befriend tallyworn 2
  [R8]
  say tallyworn NEUTRAL "Whoever it was. I hope / they're having a nice day."
beep:
  [R21]
  sfx SFX_BEEP
  say nar "From far off, under the / town, to the north: / beep."
  [R8]
  say tallyworn NEUTRAL "That's from the Underbelly. / Under the pumps. Nobody / goes down there.//Except the night shift. / {.}It has great dental."
end
