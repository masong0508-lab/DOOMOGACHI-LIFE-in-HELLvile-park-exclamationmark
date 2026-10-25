# side_a/h_mascot.dsc - Chain J "Mascotty and Thumpkin" (S134 S135 S204
# S136) (owner: S4; story_final.md sec.14.1). Three thumps and one: "are
# you there / yes". Mascotty never speaks until the head comes off.

# =============================================================================
# S134 "Ignore That One" - Hugsley (20) + Hootenanny (26) - Ch1-2
# find, photo, talk (absorbs S141)
# =============================================================================
@pack quests kind=side
@mission S134 giver=20 side=hill ch=1-2 after=BEAT_EP1_3 at=30,28 verbs=find,photo chain=J step=1 later=6m reward=30 offer=s134_offer active=s134_again title="Ignore That One" hint="Find Mascotty by the Wall. Then Hootenanny's window, at night."
@pack sa_masc kind=side minutes=60

script s134_offer
  sfx SFX_SQUEAK
  say 20 HAPPY "I get hugged a thousand / times a day! {.}Mascotty gets / hugged zero."
  [R8]
  say 20 SAD "She never takes the head off. / {..}Could you check on her?"
  choice 20 "Check on Mascotty?" { "Sure." -> yes ; "Later." }
  end
yes:
  quest S134 active
  call s134_again
end

script s134_again
  say nar "(Mascotty is always somewhere. / Always facing the Wall.)"
  verb deliver mascotty cell=52,27 then=s134_meet
end

script s134_meet
  say nar "(You wave.)"
  [R13]
  say nar "(Nothing.)"
  [R8]
  say nar "(You hug her. She lets you. / She doesn't move.)"
  [R13]
  say nar "(A photo, then. Through the / mouth-mesh.)"
  verb photo mascotty cell=52,27 then=s134_photo
end

script s134_photo
  sfx SFX_SPARKLE
  say nar "(Through the mesh: two tired / eyes.)"
  [HOLD 13]
  say nar "(You stand still. So does / she.)"
  [HOLD 21]
  npc mascotty TURN
  say nar "(She waves. {..}Slowly.)"
  [R21]
  [MUTE 21]
  sfx SFX_THUD
  [R8]
  sfx SFX_THUD
  [R8]
  sfx SFX_THUD
  [R21]
  sfx SFX_THUD
  say nar "(From the Wall: thump thump / thump. {..}Thump.)"
  [HOLD 13]
  sfx SFX_THUD
  [R8]
  sfx SFX_THUD
  [R8]
  sfx SFX_THUD
  [R21]
  sfx SFX_THUD
  say nar "(Mascotty answers with one / foot. Three, then one.)"
  [HOLD 21]
  say nar "(At night, Hootenanny keeps / watch from the Towers roof.)"
  [NIGHT]
  verb photo 26 cell=40,8 then=s134_window
end

script s134_window
  say 26 NEUTRAL "Ninth floor. {.}A face. Every / night. It never moves."
  [R8]
  say nar "(The photo: Mascotty's head, / in a dark window.)"
  sfx SFX_SQUEAK
  say 26 ANGRY "HOO."
  [R13]
  sfx SFX_SQUEAK
  say 26 ANGRY "HOO."
  [R13]
  sfx SFX_SQUEAK
  say 26 ANGRY "HOO."
  [R5]
  sfx SFX_CREAK
  say nar "(The head tilts. A little.)"
  [R5]
  shake SHK_SLAM
  sfx SFX_THUD
  say nar "(Hootenanny falls off the roof. / He is an owl. He's fine.)"
  [HOLD 13]
  say nar "(Next night the window is / empty. The one beside it / isn't.)"
  verb deliver 20 cell=30,28 then=s134_report
end

script s134_report
  say 20 SHOCK "It WAVED?!"
  [R8]
  npc 20 PANIC
  say nar "(Hugsley tries to copy the / slow wave. He can't go slowly.)"
  [R13]
  say 20 NEUTRAL "{d}I'm a hugger, not a waver."
  reward 30 furn=mascotty_postcard furn=hoedown_fiddle
  befriend 20 3
  befriend 26 2
  befriend mascotty 1
  quest S134 done
  later 6m s134_later
end

script s134_later
  say nar "(Something at your window: a / mascot head. {..}Then just / the window.)"
end

# =============================================================================
# S135 "Three Thumps and One" - Prof. Fizz (69) - Ch2-3
# find, timing, fetch (absorbs S160) - three nights at the Wall
# =============================================================================
@pack quests kind=side
@mission S135 giver=69 side=hill ch=2-3 after=BEAT_EP2_1 at=50,12 verbs=find,timing chain=J step=2 later=6m reward=50 offer=s135_offer active=s135_again title="Three Thumps and One" hint="Three nights at the Wall with Fizz's vapor detector. Press A on every thump."
@pack sa_masc kind=side

script s135_offer
  sfx SFX_BEEP
  say fizz NEUTRAL "My vapor detector beeps at / everything. {.}But it also / RECORDS the Wall's thumps."
  [R8]
  say fizz HAPPY "They have a pattern! {!}Three / nights. You hold it, I / write."
  choice fizz "Hold the detector?" { "Beep beep." -> yes ; "Not tonight." }
  end
yes:
  quest S135 active
  call s135_again
end

script s135_again
  [NIGHT]
  say fizz NEUTRAL "Night one. {.}Walk it to the / Wall and listen."
  verb find cell=53,24 then=s135_n1
end

script s135_n1
  sfx SFX_BEEP
  say nar "(BEEP: a lamp post. BEEP: a / duck. BEEP: a sandwich.)"
  [R5]
  sfx SFX_BEEP
  [R3]
  sfx SFX_BEEP
  [R2]
  sfx SFX_BEEP
  say nar "(Near Cloud Nine the beeps / blur into one long tone.)"
  [R13]
  say fizz SHOCK "That's not a reading. That's / a scream."
  [R8]
  jif SF_VAPE_SAMPLE you
  say nar "(It beeps at the pet. The pet / beeps back.)"
  jmp wall
you:
  say nar "(It beeps at YOU. {..}Fizz / writes that down.)"
wall:
  [HOLD 13]
  say nar "(At Mascotty, by the Wall: / nothing. At the Wall itself: / nothing.)"
  [HOLD 13]
  say fizz SAD "Either she's the only clean / thing in town, or she's not / here."
  [HOLD 13]
  time ADVANCE 1440
  say fizz NEUTRAL "Night two. {.}Press A on every / thump."
  verb find cell=53,25 then=s135_n2
end

script s135_n2
  push timing pattern=three skin=thump
  say fizz HAPPY "Three, one. Three, one, two. / Three, one, two, one!"
  [R8]
  say fizz NEUTRAL "Same time tomorrow. {.}I'll / bring the codebook."
  time ADVANCE 1440
  verb find cell=53,24 then=s135_n3
end

script s135_n3
  sfx SFX_PLUCK
  say fizz NEUTRAL "Bunny thump. {.}Three and one / means 'are you there'."
  [R8]
  say fizz NEUTRAL "And three-one-two-one: 'yes. / Still. Yes.' {..}And the / word for 'sister'."
  [HOLD 13]
  sfx SFX_THUD
  say nar "(Nearby, Mascotty thumps once. / Yes.)"
  [R21]
  [MUTE 21]
  say fizz SAD "Somebody's been asking the / same question for ten years." [HEART]
  [HOLD 13]
  say fizz SAD "{..}And somebody's been / answering."
  reward 50 furn=vapor_detector
  befriend fizz 3
  befriend mascotty 2
  quest S135 done
  later 6m s135_later
end

script s135_later
  sfx SFX_BEEP
  say nar "(The detector on your shelf / beeps at the pet. The pet / beeps back.)"
end

# =============================================================================
# S204 "Cocoa Through the Boards" - Lumina (76) - Ch3 (night) - deliver
# =============================================================================
@pack quests kind=side
@mission S204 giver=76 side=hill ch=3-3 after=BEAT_EP3_1 at=36,42 verbs=deliver chain=J step=3 later=5m reward=30 offer=s204_offer active=s204_again title="Cocoa Through the Boards" hint="At night, carry Lumina's cocoa to the boarded window at the Towers."
@pack sa_masc kind=side

script s204_offer
  sfx SFX_TINK
  say 76 NEUTRAL "I light every lamp still / standing. {.}One window at the / Towers stays dark."
  [R8]
  say 76 SAD "It's boarded up. {..}Someone / lives behind it. Take her / some cocoa?"
  choice 76 "Carry the cocoa?" { "Carefully." -> yes ; "Later." }
  end
yes:
  quest S204 active
  call s204_again
end

script s204_again
  native CB_SIDE_NIGHT
  jlt v0 1 day
  say nar "(A mug of cocoa. Still hot. / The Towers are across the / square.)"
  verb find cell=41,8 then=s204_boards
  end
day:
  say 76 NEUTRAL "Lamps are for the dark. {.}Come / back after nine."
end

script s204_boards
  [NO GAG]
  say nar "(The boards. A gap between two / of them, just wide enough.)"
  [R8]
  sfx SFX_SHWIP
  say nar "(You push the cocoa through.)"
  [MUTE 26]
  [HOLD 26]
  say nar "(A hand takes it.)"
  [R21]
  [R21]
  sfx SFX_TINK
  say nar "(The cup comes back out. / Empty. {..}Washed.)"
  [HOLD 21]
  reward 30 furn=washed_cup
  befriend 76 3
  befriend mascotty 2
  quest S204 done
  later 5m s204_later
end

script s204_later
  say nar "(Morning. On the boards, a / '!' and inside it a smiley, / made of little mesh dots.)"
end

# =============================================================================
# S136 "The Head Comes Off" - Thumpkin (894, Hellvile) - Ch4-5
# escort, talk, housing
# =============================================================================
@pack quests kind=side
@mission S136 giver=894 side=hell ch=4-5 after=BEAT_EP4_2 at=4,19 verbs=escort,housing chain=J step=4 later=7m reward=150 offer=s136_offer active=s136_again title="The Head Comes Off" trophy="Nobody Knows Who" hint="Walk Thumpkin through the gate to his sister at the fountain."
@pack sa_masc kind=side

script s136_offer
  sfx SFX_THUD
  [R8]
  sfx SFX_THUD
  [R8]
  sfx SFX_THUD
  [R21]
  sfx SFX_THUD
  say 894 NEUTRAL "They call me the killer / bunny. {.}I thump until the / neighbours move."
  [R8]
  say 894 SAD "Ten years of thumping, so / my sister knew I was here. / {..}I want to walk her home."
  [HOLD 13]
  say 894 SAD "What if she keeps the head / on?"
  choice 894 "Walk him to her?" { "Let's go." -> yes ; "Not yet." }
  end
yes:
  quest S136 active
  call s136_again
end

script s136_again
  say 894 NEUTRAL "Through the gate. {.}To the / fountain. {..}Don't walk fast."
  verb escort 894 cell=30,20 then=s136_fountain   # map=hill
end

script s136_fountain
  jlt v1 1 lost
  shake SHK_TAP
  sfx SFX_THUD
  say nar "(He thumps. The fountain / giggles. On time.)"
  [R8]
  shake SHK_TAP
  sfx SFX_THUD
  [R5]
  shake SHK_TAP
  sfx SFX_THUD
  say nar "(He thumps faster to make it / laugh more.)"
  [R13]
  say mayor HAPPY "He's good."
  [R13]
  npc mascotty STILL
  say nar "(Mascotty, by the fountain. / Still.)"
  [MUTE 26]
  [HOLD 26]
  sfx SFX_CREAK
  say nar "(The head comes off.)"
  [HOLD 13]
  sfx SFX_THUD
  say nar "(He thumps once.)"
  [R13]
  sfx SFX_THUD
  say nar "(She thumps back.)"
  [HOLD 21]
  say mascotty NEUTRAL "{d}I didn't know what you'd / look like after ten years."
  [R13]
  say 894 NEUTRAL "Taller."
  [R13]
  say mascotty NEUTRAL "...No." [HEART]
  [NO BUTTON]
  [HOLD 26]
  say 894 NEUTRAL "She needs a room. {.}One / window. Facing away from / where the Wall was."
  push home 0x4004
  home DECORATE_FOR 7
  reward 150 furn=mascot_head
  befriend 894 4
  befriend mascotty 4
  quest S136 done
  later 7m s136_later
  end
lost:
  say 894 SAD "You went too fast. {..}I / thumped at a lamp post and / you were gone."
end

script s136_later
  say 20 HAPPY "One thousand and ONE hugs / today! {.}The extra one was / Mascotty."
end
