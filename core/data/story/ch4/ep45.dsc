# ch4/ep45.dsc - Chapter 4: Ep 4.5 "Find the Doomogachi" (owner: S3;
# story_final.md sec.3, sec.13): Gatekeeper Grumbleguts at the sealed gate
# (Hell Breach 7, the Underbelly), the Pump room and the 900-page contract,
# the pet-care boss (SCENE_PET under fire), the pet turning its back, and the
# Mayor saying it right. GURGLE lines are plain text (SF sec.3).
@pack ep45 kind=episode chapter=4 minutes=35

string O45_GATE "Follow the beeps to the sealed gate (E)"
string O45_BOTTOM "Go down to the bottom: find @E"

script s45_grumble
  sfx SFX_SCRATCH
  say nar "(The sealed gate. Grumbleguts lounges across//it, admiring himself in a puddle of lava.)"
  [R5]
  say grumble HAPPY "Well, hello, new kid. {..} / You're cute when you're / scared."
  [R13]
  say grumble HAPPY "Padre. Come up and see my / gate sometime."
  [R13]
  say fuzz SHOCK "{d}No."
  [R8]
  say grumble NEUTRAL "Playing hard to get. I / respect it."
  [R5]
  say nar "GURGLE: gurgle?" [GURGLE]
  [R5]
  say grumble NEUTRAL "Not now. We're being / menacing."
  [R5]
  say nar "GURGLE: gurgle." [GURGLE]
  [R8]
  sfx SFX_WHOOSH
  say nar "(He unbuckles his belt with / a long, dramatic sigh.)"
  [R21]
  [HOLD 13]
  say nar "(He sucks the gut in...)"
  shake SHK_POW
  sfx SFX_BOING
  say nar "(...and out. {r})"
  [R5]
  say grumble ANGRY "{big}Nobody passes my gate!{/} {..}Nobody ever tries, honestly."
end

script s45_fed
  sfx SFX_THUD
  say nar "(He sits down heavily. The / gate groans.)"
  [R5]
  say grumble SAD "...Is there dessert?"
  [R8]
  say nar "GURGLE: gurgle." [GURGLE]
  [R13]
  say grumble SAD "There's a THING down there. / It beeps. {..}It ate my lunch. / {d}Nobody eats my lunch."
  [R5]
  say grumble SAD "Kept this gate ten years. / Nobody came up to see it."
  [R13]
  say nox NEUTRAL "Brimstone Bites needs a line cook. You'd have to wear a shirt."
  [R8]
  say grumble SHOCK "...A TIGHT shirt? {r}"
  [R5]
  say nar "GURGLE: gurgle." [GURGLE]
  [R5]
  say grumble HAPPY "He says go on, kid."
  [R13]
  say nar "(He straightens a harness / strap and winks at the / Gate.)"
end

script s45_pump
  sfx SFX_THUD
  say nar "(The Pump room. The old / contract's engine. Twelve / soul bottles in a rack.)"
  sfx SFX_WELD
  say rivet ANGRY "Main valve. Mine now."
  [R5]
  say nar "(Tallyworn, Shackles and / Drudge put their backs into / the wheel. It turns.)"
  [R13]
  say nar "(Vex, late, out of breath, / opens every bottle. By / hand. One at a time.)"
  [MUTE 40]
  [HOLD 26]
  sfx SFX_CHOIR
  say nar "(Twelve wisps rise through / the cracks in the roof. / Toward both towns.)" [HEART]
  [R21]
  say ID_SIGNHERE NEUTRAL "Excuse me. This is in / breach of page 412."
  [R5]
  say nar "(Signhere holds out the / labour contract. Nine / hundred pages.)"
  [R13]
  say nar "(Everyone in the room tears / out one page.)"
  sfx SFX_CONFETTI
  [HOLD 26]
  say ID_SIGNHERE SHOCK "...That's never happened / before. {..}I'm keeping a / page. For my scrapbook."
end

script s45_bottom
  [MUTE 26]
  sfx SFX_BEEP
  say nar "(The bottom of Hellvile. / @E is here.)"
  [HOLD 21]
  say nar "(Horned, huge, a swollen / purple silhouette. The fear / is inside, talking.)" [HEART]
  [R5]
  say nar "???: {d}Hee. Hee. They'll / never forgive you."
  [R13]
  say nar "(It is in pain. Feed it. / Hold it. Play. Dodge what / the fear throws at you.)"
end

script s45_turn
  [MUTE 26]
  say nar "(The fear speaks in the / Mayor's voice. @E looks at / the fear. Looks at you.)"
  [R5]
  say nar "@E: bep?"
  [HOLD 13]
  sfx SFX_SCRATCH
  say nar "(@E turns its back on the / fear. {r})"
  [R13]
  say nar "(The Mayor steps out in front of you. He followed you all the way down.)"
  [R5]
  say mayor SAD "That's mine. That's my fear. / Give it back, little one."
  [R5]
  say mayor SAD "I was scared. Of them. Of / me. Of what I did."
  [R13]
  say mayor SAD "{d}I'm still scared.{..} You / can let go now." [HEART]
  [HOLD 21]
  shake SHK_FWOOMP
  sfx SFX_WHOOSH
  [FLASH W8,4]
  say nar "FWOOMP."
end

script s45_fine
  native CB_C34 C34A_PET 2
  say nar "(@E, pet-sized again. With / horns. Little ones. They / suit it.)"
  [R2]
  say nar "@E: beep."
  [R8]
  say nar "@E: {d}...it's fine."
  [R13]
  music SONG_HELLVILE
  sfx SFX_BOING
  say nar "(This time it is fine. A real / beep! with a hop.)" [HEART]
  native CB_C34 C34A_PACIFIST
  [R21]
  [SCRATCH]
  say nar "(Grumbleguts, in a tight shirt, carries//the forklift's next course up the stairs.)"
  [R5]
  say grumble HAPPY "Light snack."
end
