# side_b/k_vex.dsc - Chain K "Vex and Haze, Cloud Nine to Fresh Air"
# (S137-S140) (owner: S5; story_final.md sec.14.1; docs/design/story/SPEC.md
# sec.7 S5). The vape counter, the closet, the exit interview and the
# bubble machine: a fox who only wanted to be listened to, and the clerk
# who watered the plants in the smoke because somebody had to.
#
# Every mission: the hook, one task (a verb, a fetch or a scene push), the
# comic beat on the timing ladder, the heart (or the scare), the reward and
# its Later. Talking to the giver while a mission is under way runs its
# `active` script, which hands the task out again (a verb is not saved, so
# a reload just asks for it once more). The @mission headers live in the
# quest log's index pack (quests49/index.dsc explains why).

# =============================================================================
# S137 "Employee of the Month" - Haze (709), at the Cloud Nine counter - Ch2
# photo, find
# =============================================================================
@pack quests kind=side
@mission S137 giver=709 side=hill ch=2-2 after=BEAT_EP2_1 at=55,36 verbs=photo,find chain=K step=1 later=5m reward=30 offer=s137_offer active=s137_again title="Employee of the Month" hint="Photograph the Employee of the Month frame. Water Haze's 3 plants."
@pack sb_vex kind=side minutes=80

script s137_offer
  sfx SFX_TIK
  say haze NEUTRAL "The frame behind the till. / 'Employee of the Month.' {.}It / never changes."
  [R8]
  say haze HAPPY "Vex says it's a motivational / poster. {..}I want a photo. / For evidence."
  [HOLD 8]
  say haze NEUTRAL "Of what, I'll know when I see / it."
  choice haze "Take the photo?" { "Say cheese." -> yes ; "Not now." }
  end
yes:
  quest S137 active
  call s137_again
end

script s137_again
  say haze NEUTRAL "I'll hold it up by the door. / {.}Step back till it fits the / frame. Then A."
  verb photo haze cell=55,37 then=s137_photo
end

script s137_photo
  [MUTE 8]
  sfx SFX_SPARKLE
  say nar "(The flash. {.}For one frame / the portrait's eyes slide / toward the lens.)"
  [HOLD 13]
  say nar "(The polaroid: the eyes are / looking straight at you.)"
  [R8]
  jif SF_VEX_GONE novex
  npc vex STILL
  sfx SFX_TIK
  say vex HAPPY "Hey, hey. Customers only by / the frame." [PUNCH]
  [HOLD 21]
  say vex HAPPY "He's very motivational."
  [R8]
  say vex SAD "{d}Please don't tell him I / said 'he'."
  npc vex THAW
  jmp plants
novex:
  say haze NEUTRAL "Vex always stood right there / and smiled at it."
  [R8]
  say haze SAD "He's gone. {..}The frame isn't."
plants:
  [R13]
  say haze NEUTRAL "One more thing. {.}I water / three plants out back. / Secretly. Vex hates green."
  say haze NEUTRAL "They need a drink, and I need / to stay at the till."
  verb find cell=50,33 then=s137_p1
end

script s137_p1
  sfx SFX_SPLASH
  say nar "(A fern in a teacup, hidden / behind the bins. It perks up / at once.)"
  verb find cell=50,36 then=s137_p2
end

script s137_p2
  sfx SFX_SPLASH
  say nar "(A cactus wearing a little / bow. Someone tied it on.)"
  verb find cell=53,39 then=s137_p3
end

script s137_p3
  sfx SFX_SPLASH
  say nar "(A tomato plant. {.}One of the / tomatoes has an eye.)"
  [R13]
  sfx SFX_TINK
  say nar "(It blinks.)"
  [R8]
  say nar "(You water it anyway.)"
  verb deliver 46 cell=37,24 then=s137_develop
end

script s137_develop
  say 46 HAPPY "A Cloud Nine polaroid? {.}I'll / develop it properly. Give it / a minute."
  [R21]
  sfx SFX_SHWIP
  say 46 NEUTRAL "Huh."
  [MUTE 14]
  [HOLD 21]
  say nar "(In the proper print, the / frame is empty.)" [PUNCH]
  [HOLD 26]
  say 46 NEUTRAL "...Light leak. {..}Probably."
  verb deliver haze cell=55,37 then=s137_turnin
end

script s137_turnin
  say haze SAD "Empty." [HEART]
  [HOLD 13]
  say haze NEUTRAL "Then whoever's in that frame / isn't on the wall. {..}They're / in the shop."
  [R13]
  say haze NEUTRAL "Here. {.}A pen that vapes plain / water. For when the grape / runs out."
  reward 30 furn=haze_frame
  home PLACE_GIFT furn=haze_frame
  befriend haze 3
  quest S137 done
  later 5m s137_later
end

script s137_later
  say nar "(A new frame is taped to / your mailbox. 'Employee of / the Month.' Nobody sent it.)"
  [R8]
  say nar "(It's facing your door.)"
end

# =============================================================================
# S138 "Closet Space" - Vex (666) - Ch3 after Ep 3.1 - housing, fetch, talk
# =============================================================================
@pack quests kind=side
@mission S138 giver=666 side=hill ch=3-3 after=BEAT_EP3_1 at=54,37 verbs=housing,fetch chain=K step=2 later=6m reward=40 offer=s138_offer active=s138_again title="Closet Space" hint="Vex is hiding in your closet. Bring him Pixie Sugar (his 'cotton candy')."
@pack sb_vex kind=side

script s138_offer
  npc vex EMOTE EMO_SWEAT
  say vex SHOCK "Psst. {.}Don't look at me. / Everybody here looks at me."
  [R8]
  say vex SAD "Since the sky ripped, I've / been in your closet. {..}It's / a very nice closet."
  [HOLD 13]
  say vex NEUTRAL "I would like a cushion. {.}And / cotton candy. I'm a fox. We / stress-eat."
  choice vex "Let Vex stay?" { "The closet's yours." -> yes ; "Not now." }
  end
yes:
  quest S138 active
  befriend vex 1
  call s138_again
end

script s138_again
  native CB_SIDE_HAVE ITEM_PIXIE_SUGAR
  jlt v0 1 need
  take ITEM_PIXIE_SUGAR
  say nar "(Pixie Sugar, spun round a / stick. Close enough to / cotton candy, in a war.)"
  home PLACE_GIFT furn=vex_cushion
  push home -1
  sfx SFX_CREAK
  say nar "(Your closet, back home. {.}The / catalogue lists a cushion: / 'placed by: Vex. Please.')"
  [R13]
  call s138_waves
  end
need:
  say vex NEUTRAL "Cotton candy. {.}Pixie Sugar / from the Shop is close. / Fluffy. Pink. Like me."
end

script s138_waves
  [NIGHT]
  sfx SFX_GLITCH
  shake SHK_TAP
  say nar "(Wave one. A window breaks / somewhere down the street.)"
  [R8]
  say vex NEUTRAL "{.}...not a customer."
  [R13]
  say vex SAD "Cloud Nine was my first / shop. {..}I thought customers / were friends."
  [HOLD 13]
  sfx SFX_GLITCH
  shake SHK_TAP
  say nar "(Wave two. Something big / bumps the fence.)"
  [R8]
  say vex SHOCK "...not a customer."
  [R13]
  say vex SAD "Vapula... the big cloud. {..}He / told me what to sell. {.}I said / yes to everything."
  [HOLD 13]
  sfx SFX_THUD
  shake SHK_SLAM
  say nar "(Wave three. The whole house / jumps.)"
  [R8]
  say vex SHOCK "...not a customer."
  [HOLD 21]
  [MUTE 26]
  say vex SAD "Nobody over there laughs. / It's the law. {.}He said if I / opened a shop here..." [HEART]
  [HOLD 13]
  say vex SAD "...he'd listen. {..}He did. / He's the only one who ever / did."
  [HOLD 26]
  say nar "(Quiet. Then, from the closet, / very small:)"
  [R13]
  say vex NEUTRAL "...I'll do a survey."
  reward 40 furn=vex_cushion
  befriend vex 3
  quest S138 done
  later 6m s138_later
end

script s138_later
  sfx SFX_CREAK
  say nar "(Your closet door is open a / crack. A survey form is / pinned inside.)"
  [R8]
  say nar "(Question one: 'Was I a good / shop?' {..}Nobody filled it in.)"
end

# =============================================================================
# S139 "Exit Interview" - Vex (666) + Haze (709) - Ch4 - fetch, housing
# =============================================================================
@pack quests kind=side
@mission S139 giver=666 side=hill ch=4-5 after=BEAT_EP4_1 at=54,37 verbs=fetch,housing chain=K step=3 later=6m reward=100 offer=s139_offer active=s139_again title="Exit Interview" hint="Bring 3 Soap to air out Cloud Nine, then redecorate it as the Fresh Air Lounge."
# S139 is its own pack: Haze's "Ten years I..." is one of the four the game
# allows (SF sec.7.11), and textpack.py allows it only in packs named s139.
@pack s139 kind=side minutes=25

script s139_offer
  say vex NEUTRAL "Cloud Nine is closing. {.}Haze / is taking over. I need a / reference letter."
  [R8]
  say haze NEUTRAL "And I need the smoke out of / the curtains. {..}Ten years of / grape. It's IN the wood."
  [HOLD 8]
  say haze HAPPY "Three Soap. Then we / redecorate. Rule one: if / it's purple, it goes."
  choice haze "Air out Cloud Nine?" { "Open the windows!" -> yes ; "Later." }
  end
yes:
  quest S139 active
  say vex SAD "Can the neon stay? {..}It's my / name. In neon."
  [R8]
  say haze NEUTRAL "It's purple."
  [R13]
  say vex SAD "...It's purple."
end

script s139_again
  native CB_SIDE_HAVE ITEM_SOAP
  jlt v0 3 need
  take ITEM_SOAP 3
  sfx SFX_SPLASH
  say nar "(Soap, a mop, every window / open. The grape comes out / of the curtains in sobs.)"
  [R13]
  say haze NEUTRAL "If it's purple, it goes."
  [R8]
  say nar "(The purple cushions go. The / purple lamps go. The purple / sign goes.)"
  [R13]
  sfx SFX_BEEP
  say nar "(@E is, right now, purple.)"
  [HOLD 13]
  say haze NEUTRAL "..."
  [R8]
  say nar "(@E goes outside.)"
  [R5]
  sfx SFX_BEEP
  say nar "beep."
  [R21]
  say haze NEUTRAL "Now: the Fresh Air Lounge. / {.}Plants. Open windows. / Something nice on the wall."
  push home 0x4004
  call s139_letter
  end
need:
  say haze NEUTRAL "Three Soap. {.}Lather sells it, / and he won't ask why it / smells like grape."
end

script s139_letter
  sfx SFX_TINK
  say nar "(Haze hangs a new frame: / 'Employee of the Month: / VEX'.)"
  [R13]
  say vex SHOCK "...me?"
  [R8]
  say haze NEUTRAL "Don't make it weird. {..}It's / my one act of mercy this / year."
  [HOLD 13]
  say vex NEUTRAL "The reference letter. {.}You / write it. I sign anything. / That's sort of my problem."
  choice nar "The letter says Vex is:" { "A good listener." -> listen ; "Very punctual." -> punct ; "Purple." -> purple }
listen:
  say vex SAD "...Nobody's ever written that / about me." [HEART]
  jmp heart
punct:
  say vex HAPPY "I AM punctual. {.}I opened at / seven every day of the / apocalypse."
  jmp heart
purple:
  say haze NEUTRAL "It goes."
  [R8]
  say vex SAD "Fair."
heart:
  [R13]
  sfx SFX_SHWIP
  say vex NEUTRAL "Plain-water vapes, Haze said. / {.}Let's see."
  [R13]
  say vex SICK "They're terrible."
  [R8]
  sfx SFX_SHWIP
  say nar "(He has another one.)"
  [HOLD 13]
  [MUTE 21]
  say haze SAD "Ten years I watered plants / in that smoke because / somebody had to." [HEART]
  [HOLD 13]
  say haze NEUTRAL "{..}I think I was practising / for this."
  [NO BUTTON]
  setf SF_VEX_REFORMED
  reward 100 furn=fresh_air_neon furn=employee_vex
  home PLACE_GIFT furn=fresh_air_neon
  befriend vex 4
  befriend haze 4
  quest S139 done
  later 6m s139_later
end

script s139_later
  say nar "(A poster on the Unity / Stage: VEX! LIVE! Plain / water vapes. {..}Three come.)"
  [R8]
  say nar "(They clap. He looks like he / might cry. He vapes plain / water instead.)"
end

# =============================================================================
# S140 "The Bubble Machine" - Vex (666) - Ch5 - weld, minigame, concert
# =============================================================================
@pack quests kind=side
@mission S140 giver=666 side=hill ch=5-5 at=54,37 verbs=weld,minigame,concert chain=K step=4 later=8m reward=100 offer=s140_offer active=s140_again title="The Bubble Machine" trophy="Terrible, Actually" hint="Weld Vex's rig into a bubble machine, test it at the Fun Fair, then run it on stage."
@pack sb_vex kind=side

script s140_offer
  say vex NEUTRAL "I want to be useful at the / festival. {.}I have a vape rig / and no vapor."
  [R8]
  say rivet NEUTRAL "Soap. {.}A rig's just a / bubble machine that made bad / life choices."
  [HOLD 8]
  say vex SHOCK "If it's purple I'm leaving."
  choice vex "Build a bubble machine?" { "Bubbles for everyone!" -> yes ; "Later." }
  end
yes:
  quest S140 active
  call s140_again
end

script s140_again
  say rivet NEUTRAL "Workshop. {.}Bring the rig."
  push workshop
  sfx SFX_WELD
  say nar "(Rivet welds the rig to a / soap tank and a fan. Vex / holds the torch. Badly.)"
  [R13]
  sfx SFX_BUBBLE
  say nar "(The first bubble floats up.)"
  [R5]
  say nar "(It's grape.)" [PUNCH]
  [MUTE 13]
  [HOLD 13]
  say vex SHOCK "Residue."
  [R8]
  sfx SFX_POP2
  say nar "(It pops. Everyone is fine. / {..}Everyone checks anyway.)"
  [R21]
  say rivet NEUTRAL "Test run. Fun Fair. {.}Your pet / pops, you count."
  push minigame SMG_B_BUBBLES
  jlt v0 1 again
  say vex HAPPY "It WORKS. {!}Nobody coughed! / Nobody bought anything!"
  [R13]
  say vex NEUTRAL "Now the real thing. {.}The / Unity Stage. With music."
  push minigame SMG_B_BUBBLES_STAGE
  sfx SFX_FANFARE
  say nar "(A thousand citizens, both / sides of the park, in a / storm of bubbles. On beat.)"
  [HOLD 21]
  [MUTE 26]
  say vex SAD "They're laughing. {..}Both / sides." [HEART]
  [HOLD 13]
  say haze NEUTRAL "Yeah. {.}That's what it sounds / like."
  reward 100 furn=bubble_machine
  home PLACE_GIFT furn=bubble_machine
  befriend vex 4
  befriend haze 2
  quest S140 done
  later 8m s140_later
  end
again:
  say vex SAD "@E popped them all... {..}or / none? I couldn't look. / Again?"
end

string S140_NEWS "Vex: Fresh Air Lounge Employee of the Month, 3rd month running."

script s140_later
  stash 0 S140_NEWS
  native CB_SIDE_NEWS
  sfx SFX_TIK
  say nar "(HELLVILE HERALD: Vex, Fresh / Air Lounge employee of the / month, 3rd month running.)"
  [R8]
  say nar "(The photo: Vex, a bubble on / his nose, trying not to look / pleased.)"
end
