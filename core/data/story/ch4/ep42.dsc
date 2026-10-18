# ch4/ep42.dsc - Chapter 4: Ep 4.2 "Shift Swap" (owner: S3; story_final.md
# sec.13; replaces the old C4_PRISONERS counter). Five named workers at the
# pump yard; you take each one's bellows for a round of the rhythm (the comic
# timing game, thump skin: "Lullaby for the Wall") and they say who they were
# before the Wall. Then Haze meets Hattie, the foghorn, the jar.
@pack ep42 kind=episode chapter=4 minutes=25

string O42_SWAP "Swap shifts at the pumps (@0/5)"

script s42_yard
  sfx SFX_THUD
  say nar "(The pump yard. Rows of Hellvile folk working bellows, in, out, in, out.)"
  [R5]
  say nar "(Up on a crate, a loyalist / with a foghorn: Hazey. He / does not look sure of much.)"
  [R8]
  say tallyworn NEUTRAL "Swap a shift or two. Give / their arms a rest. They'll / talk. Nobody asks them to."
end

script s42_clinker
  say nar "(You take Clinker's bellows. / In, out. In, out.)"
  push timing pattern=three skin=thump
  say ID_CLINKER NEUTRAL "I write poems on the pumps. / With chalk. Forty, so far."
  [R8]
  say ID_CLINKER HAPPY "This one's about you. It / doesn't rhyme yet."
  befriend ID_CLINKER 2
end

script s42_ploddy
  say nar "(You take Ploddy's bellows. / In, out. In, out.)"
  push timing pattern=three skin=thump
  say ID_PLODDY NEUTRAL "Before the Wall I baked bone bread. Over the river, for both sides."
  [R8]
  say ID_PLODDY SAD "I still smell it in my sleep. / {..}Smells like mornings."
  befriend ID_PLODDY 2
end

script s42_wheezle
  say nar "(You take Wheezle's bellows. / In, out. In, cough.)"
  push timing pattern=three skin=thump
  say ID_WHEEZLE NEUTRAL "I cough little ghosts out. / I name them. One went over / the Wall."
  [R13]
  say ID_WHEEZLE SAD "I called it Sprinkles. I hope somebody over there was nice to it." [HEART]
  befriend ID_WHEEZLE 2
end

script s42_haze
  say nar "(You take Haze's bellows. In, out.//She coughs at the smoke like it owes her money.)"
  push timing pattern=three skin=thump
  say haze ANGRY "I HATE the smoke. Ten years / of it. I water the plants / when nobody's looking."
  [R8]
  say haze NEUTRAL "Somebody over there had my / breath in them. Laughing at / the wrong time. I'm sorry."
  befriend haze 2
end

script s42_thumpkin
  say nar "(You take Thumpkin's bellows. In, out.//Thumpkin thumps the pipe. Three, then one.)"
  push timing pattern=three skin=thump
  say ID_THUMPKIN NEUTRAL "That's for my sister. She's / on the other side. In a / bunny suit, last I heard."
  [R13]
  say ID_THUMPKIN SAD "Three, then one. It means / 'still here'. She thumps / back. Some nights."
  befriend ID_THUMPKIN 2
end

script s42_done
  jif 40 met
  jmp foghorn
met:
  jif SF_HATTIE_LOST foghorn
  say nar "(Hattie walks into the yard. / She stops in front of Haze.)"
  [R5]
  say hattie SHOCK "You. You were in my head. / You have GREAT timing."
  [R5]
  say haze SHOCK "{..}You were LOUD in there."
  [R5]
  say hattie HAPPY "Knock knock."
  [R5]
  say haze NEUTRAL "...Who's there."
  [R5]
  say hattie HAPPY "Fresh air."
  [R8]
  sfx SFX_POP2
  say nar "(Haze snorts. The first laugh in ten years. The pumps stop, one at a time.)" [HEART]
  [R3]
  sfx SFX_THUD
  [R3]
  sfx SFX_THUD
foghorn:
  [R13]
  sfx SFX_KAZOO
  say nar "HAZEY: LORD VAPULA WILL... / um. Is he... coming?"
  [MUTE 26]
  [HOLD 26]
  say nar "(Vex walks past, carrying a / jar at head height. From / inside: ...squeak.)"
  [R8]
  say nar "HAZEY: {d}...Oh."
  [R13]
  say ID_WISP SHOCK "the sky's too red here. / i don't like it. {..}home is / that way. Hell Pits. 13."
end

script s42_worker_idle
  say nar "(They are still pumping. The pumps don't know yet that nobody has to.)"
end
