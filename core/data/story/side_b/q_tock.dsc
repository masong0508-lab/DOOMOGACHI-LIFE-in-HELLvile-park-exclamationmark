# side_b/q_tock.dsc - Chain Q "Tallyworn and Tock" (S163-S165; continues
# quest #38) and Chain V "Housekeeping" (S208-S210: Hauntessa, Wobblewisp,
# Pharaohnie; grafted heart S54, S68, S115) (owner: S5; story_final.md
# sec.14.1). A clock with no hands that counts something, a man who counted
# the Wall for ten years, and three ghosts with opinions about tidying.

# =============================================================================
# S163 "A Clock With No Hands" - Tock (104) - Ch2-3 - fetch, find (S77)
# =============================================================================
@pack quests kind=side
@mission S163 giver=104 side=hill ch=2-3 after=BEAT_EP2_2 at=33,10 verbs=fetch,find chain=Q step=1 later=6m reward=30 offer=s163_offer active=s163_again title="A Clock With No Hands" hint="Find the handless clock in Town Hall. Bring Tock a Milk. Listen at midnight."
@pack sb_tock kind=side minutes=105

script s163_offer
  sfx SFX_TIK
  say 104 SLEEPY "I fix every clock in town. / {.}And I nap through every / chime."
  [R8]
  say 104 NEUTRAL "One clock in Town Hall has / no hands. {..}It ticks anyway. I / want to know why."
  choice 104 "Find the clock?" { "Tick tock." -> yes ; "Later." }
  end
yes:
  quest S163 active
  call s163_again
end

script s163_again
  say 104 NEUTRAL "The Mayor's office. {.}Behind / the drawing, I'd guess. / Clocks like to hide."
  verb find cell=31,7 then=s163_found
end

script s163_found
  sfx SFX_TIK
  say nar "(Behind the drawing: a clock / face with no hands. {.}Tick. / Tick. Tick.)"
  [R13]
  say 104 NEUTRAL "Three thousand, six hundred / and fifty ticks an hour. / {..}Exactly."
  [R8]
  say 104 SLEEPY "Which is wrong for a clock / and... {..}and... zzz."
  [HOLD 21]
  say nar "(Tock is asleep. Mid-sentence. / You poke him.)"
  [R5]
  say 104 SHOCK "...where was I. {.}Ticking. / Yes. Bring me a Milk. I'll / listen till midnight."
  call s163_milk
end

script s163_milk
  native CB_SIDE_HAVE ITEM_MILK
  jlt v0 1 need
  take ITEM_MILK
  [NIGHT]
  [SILENCE]
  say nar "(Midnight. No music. Just the / clock with no hands.)"
  [R13]
  sfx SFX_BELL
  [R8]
  sfx SFX_BELL
  [R8]
  sfx SFX_BELL
  say nar "(Twelve chimes, each one lower / than the last.)"
  [HOLD 26]
  sfx SFX_BELL
  say nar "(A thirteenth.)"
  [HOLD 26]
  sfx SFX_SQUEAK
  say nar "(The cuckoo comes out. {.}It / doesn't go back in.)"
  [R13]
  sfx SFX_KAZOO
  say nar "(The thirteenth chime was the / fountain's giggle.)"
  [HOLD 13]
  shake SHK_TAP
  [MUTE 14]
  sfx SFX_GLITCH
  say nar "(The whole office shifts one / step to the left.)" [PUNCH]
  [HOLD 26]
  native CB_SIDE_MUSIC
  time ADVANCE 360
  say 104 NEUTRAL "Morning. {.}I wrote it down." [HEART]
  [R8]
  say nar "(His pad: '3,651'. And above / it, crossed out, '3,652'.)"
  [HOLD 13]
  say 104 SAD "It's counting DOWN now. {..}I / don't know to what."
  [R13]
  say 104 NEUTRAL "I won't fix it. {.}It's / counting something. Let it."
  reward 30 furn=handless_clock furn=cuckoo_out
  home PLACE_GIFT furn=handless_clock
  befriend 104 3
  quest S163 done
  later 6m s163_later
  end
need:
  say 104 SLEEPY "One Milk. {.}For the long / listen. Warm, if they have it."
end

script s163_later
  sfx SFX_TIK
  say nar "(The handless clock on your / wall ticks. {..}You count. / 3,650 an hour. Exactly.)"
end

# =============================================================================
# S164 "Day 3,653" - Tallyworn (712, Hellvile) - Ch4 - fetch, minigame,
# housing (Clinker's poem rating)
# =============================================================================
@pack quests kind=side
@mission S164 giver=712 side=hell ch=4-5 after=BEAT_EP4_1 at=9,30 verbs=fetch,minigame,housing chain=Q step=2 later=6m reward=50 offer=s164_offer active=s164_again title="Day 3,653" hint="Trade Clinker a poem for her chalk, then give Tallyworn a second wall."
@pack sb_tock kind=side

script s164_offer
  sfx SFX_TIK
  say tallyworn NEUTRAL "My tally wall is full. {.}I / need a second wall."
  [R8]
  say tallyworn SAD "Or a reason to stop. {..}I'm / not ready to stop."
  [HOLD 13]
  say tallyworn NEUTRAL "I need chalk. {.}Clinker has / it. She doesn't share."
  choice tallyworn "Get him chalk?" { "Leave it to me." -> yes ; "Later." }
  end
yes:
  quest S164 active
  call s164_again
end

script s164_again
  say tallyworn NEUTRAL "She uses it for poems. {.}Trade / her one. A good one."
  verb deliver 753 cell=5,24 then=s164_clinker
end

script s164_clinker
  say 753 NEUTRAL "Chalk for a poem. {.}Three / lines. On the board."
  push minigame SMG_B_POEM
  jge v0 3 much
  jge v0 2 good
  say 753 NEUTRAL "Six. {d}Needs death." [PUNCH]
  [R13]
  say 753 NEUTRAL "Try again. {.}Add death."
  push minigame SMG_B_POEM
  jge v0 3 much
  jge v0 2 good
  say 753 NEUTRAL "Five. {.}Still needs death."
  jmp chalk
much:
  say 753 NEUTRAL "Four." [PUNCH]
  [R8]
  say 753 NEUTRAL "Too much death."
  jmp chalk
good:
  say 753 HAPPY "Seven. {.}The death is in the / right place."
chalk:
  [R13]
  sfx SFX_PICKUP
  say 753 NEUTRAL "Take the chalk. {..}I'm keeping / the poem."
  verb deliver tallyworn cell=9,30 then=s164_wall
end

script s164_wall
  say tallyworn NEUTRAL "A new wall. {.}In the pump-yard / shed. You pick where."
  push home 0x4D04
  home DECORATE_FOR 13
  [R13]
  sfx SFX_TIK
  say nar "(He lifts the chalk to make / the first mark.)"
  [HOLD 21]
  say nar "(His hand stops.)"
  [HOLD 21]
  say tallyworn SAD "Maybe tomorrow I'll know what / I'm counting."
  [MUTE 21]
  [HOLD 21]
  say nar "(The mark he does make is a / heart. His hand shook. {..}He / leaves it.)" [HEART]
  [NO BUTTON]
  [HOLD 26]
  reward 50 furn=clinkers_poem
  home PLACE_GIFT furn=clinkers_poem
  befriend tallyworn 4
  befriend 753 2
  quest S164 done
  later 6m s164_later
end

script s164_later
  say nar "(Clinker's poem: three lines / in chalk. {.}The death is in / the right place.)"
end

# =============================================================================
# S165 "Counting Down" - Tock (104) + Tallyworn (712) - Ch5
# weld, deliver, housing
# =============================================================================
@pack quests kind=side
@mission S165 giver=104 side=hill ch=5-5 at=33,10 verbs=weld,deliver chain=Q step=3 later=6m reward=150 offer=s165_offer active=s165_again title="Counting Down" trophy="Day One" hint="Weld hands onto the handless clock, then carry it to Tallyworn's gatepost."
@pack sb_tock kind=side

script s165_offer
  say 104 NEUTRAL "The handless clock. {.}I want / to give it hands. And set it / counting down..."
  [R8]
  say 104 HAPPY "...to the next Mingle. {..}And / Tallyworn wants to be the / one who turns it."
  choice 104 "Give it hands?" { "Let's set it." -> yes ; "Later." }
  end
yes:
  quest S165 active
  call s165_again
end

script s165_again
  say rivet NEUTRAL "Clock hands don't need to be / structural."
  push workshop
  sfx SFX_WELD
  [R8]
  say nar "(They are structural.)"
  [R13]
  say 104 NEUTRAL "Now carry it. {.}Gently. To the / old gatepost, in Hellvile."
  verb deliver tallyworn cell=3,27 fragile then=s165_post   # map=hell
end

script s165_post
  say nar "(The gatepost. The key from / Nana's loaf still hangs on / it.)"
  [R13]
  say tallyworn NEUTRAL "Three hundred and sixty-five / days."
  sfx SFX_TIK
  say nar "(He turns the hands. {.}Click.)"
  [R13]
  sfx SFX_BELL
  say nar "(The clock chimes.)"
  [R8]
  say nar "(Tock dozes on the post. The / crowd whispers, so as not / to wake him.)"
  [DUCK 77 60]
  [R21]
  [MUTE 26]
  say tallyworn SAD "Three thousand, six hundred / and sixty-five days, / counting up." [HEART]
  [HOLD 13]
  say tallyworn NEUTRAL "{..}Day one, counting down."
  [HOLD 13]
  sfx SFX_TIK
  say nar "(One last tally mark on the / gatepost, under the key. / Then he stops.)"
  [HOLD 21]
  say tallyworn SAD "My eyes are just doing a rain."
  [R8]
  say nar "(He pockets the chalk.)"
  reward 150 furn=countdown_clock
  home PLACE_GIFT furn=countdown_clock
  befriend tallyworn 4
  befriend 104 3
  quest S165 done
  later 6m s165_later
end

script s165_later
  sfx SFX_TIK
  say nar "(The countdown clock reads / 364. {.}It's the only clock / Tock never naps through.)"
end

# =============================================================================
# S208 "Notes From the Housekeeper" - Hauntessa (109) - Ch1-2
# housing, find (grafted heart S54)
# =============================================================================
@pack quests kind=side
@mission S208 giver=109 side=hill ch=1-2 after=BEAT_EP1_4 at=6,8 verbs=find,housing chain=V step=1 later=5m reward=40 offer=s208_offer active=s208_again title="Notes From the Housekeeper" hint="Find the housekeeper's three notes round your cottage. Then tidy up."
@pack sb_tock kind=side

script s208_offer
  sfx SFX_TINK
  say nar "(A note on your door, in very / neat handwriting:)"
  [R8]
  say nar "'your crate is a table now. / you're welcome.'"
  [R13]
  say nar "(The handwriting is see-through.)"
  choice nar "Look for more notes?" { "Who's tidying?" -> yes ; "Later." }
  end
yes:
  quest S208 active
  call s208_again
end

script s208_again
  say nar "(There are more. {.}Follow the / tidiness.)"
  verb find cell=8,7 then=s208_n2
end

script s208_n2
  sfx SFX_TINK
  say nar "'swept your step. it was / ALREADY clean. i swept it / anyway.'"
  verb find cell=2,10 then=s208_n3
end

script s208_n3
  sfx SFX_TINK
  say nar "'sorry. sorry about all this. / i'll go.'"
  [R13]
  sfx SFX_POP2
  npc 109 HOP
  say 109 SHOCK "Oh! {.}You can see me! Sorry. / I'm Hauntessa. I'm sorry."
  [R8]
  say 109 SAD "I tidy empty houses. {..}Yours / was empty for a while."
  [HOLD 8]
  say 109 HAPPY "Six things. {.}Nothing in front / of the door. Then I'll grade / it. Sorry."
  push home 0x4101
  home GOAL 1
  [R13]
  say nar "(She rearranges your chair, / apologising.)"
  [R8]
  sfx SFX_BEEP
  say nar "(@E moves it back.)"
  [R13]
  say nar "(She moves it again. {.}Sorry.)"
  [R8]
  sfx SFX_BEEP
  say nar "(@E moves it back.)"
  [R13]
  say nar "(She moves the pet bed. {..}@E / sits in it. She gives up on / the pet bed.)"
  [R13]
  jlt v0 1 messy
  sfx SFX_FANFARE
  shake SHK_SLAM
  say 109 HAPPY "{big}TIDY.{/} {.}Sorry. It's very / tidy."
  jmp heart
messy:
  say 109 NEUTRAL "Tidy-ish. {.}I'll keep an eye / on it. Sorry."
heart:
  [R21]
  [MUTE 21]
  say 109 SAD "I haunt empty houses because / someone should keep them / warm..." [HEART]
  [HOLD 13]
  say 109 SAD "{..}...for whoever's next."
  reward 40 furn=hauntessa_duster
  home PLACE_GIFT furn=hauntessa_duster
  befriend 109 3
  quest S208 done
  later 5m s208_later
end

script s208_later
  sfx SFX_TINK
  say nar "(A note on your chair, in / see-through ink: 'not me. / (i'd have centred it.)')"
end

# =============================================================================
# S209 "Two Inches to the Left" - Hauntessa (109) - Ch2 after Ep 2.3
# housing, find (grafted heart S68)
# =============================================================================
@pack quests kind=side
@mission S209 giver=109 side=hill ch=2-2 after=BEAT_EP2_3 at=6,8 verbs=find,housing chain=V step=2 later=6m reward=40 offer=s209_offer active=s209_again title="Two Inches to the Left" hint="Somebody else is tidying your house, badly. Wait up at night and catch them."
@pack sb_tock kind=side

script s209_offer
  say 109 ANGRY "Someone ELSE is tidying your / house. {..}Badly."
  [R8]
  say 109 NEUTRAL "Your keys. Your lamp. Your / egg basket. {.}All two inches / to the left. Every night."
  choice 109 "Catch the tidier?" { "Stake out tonight." -> yes ; "Later." }
  end
yes:
  quest S209 active
  call s209_again
end

script s209_again
  [NIGHT]
  sfx SFX_CREAK
  say nar "(Night one. You lie awake. / {.}In the morning the keys are / two inches to the left.)"
  time ADVANCE 1440
  [HOLD 13]
  say nar "(Night two. The lamp. Two / inches.)"
  [R13]
  [MUTE 14]
  say nar "(And the chair is facing / your bed. {..}Not two inches. / It's just facing you.)" [PUNCH]
  [HOLD 26]
  say 109 SHOCK "...That one wasn't me."
  [R8]
  say 109 SHOCK "{.}And it's not the two-inch / one either."
  time ADVANCE 1440
  [R13]
  say nar "(Night three. Wait by the / door. Very still.)"
  verb find cell=4,7 then=s209_caught
end

script s209_caught
  sfx SFX_POP2
  cast 972 5 8
  npc 972 EMOTE EMO_BANG
  say 972 SHOCK "AH! {.}You saw nothing. I was / never here. {..}Two inches."
  [R13]
  say 972 NEUTRAL "Wobblewisp. Poltergeist. I / came through with the fog. / {.}For the quiet."
  [R8]
  npc 109 FACE 972
  say 972 HAPPY "Two inches is TASTEFUL."
  [R5]
  say 109 ANGRY "Two inches is a DRAUGHT."
  npc 972 RIPPLE 3
  sfx SFX_TINK
  [R5]
  sfx SFX_TINK
  [R5]
  sfx SFX_TINK
  say nar "(Every cup in the house gets / knocked over, one after / another.)"
  [R21]
  say 109 NEUTRAL "...Truce. {.}You may move the / vapor detector. Only."
  [R8]
  say 972 HAPPY "Two inches."
  [R8]
  say 109 NEUTRAL "...Two inches."
  reward 40 furn=wobblewisp_vase
  home PLACE_GIFT furn=wobblewisp_vase
  befriend 109 2
  befriend 972 3
  quest S209 done
  later 6m s209_later
end

script s209_later
  say nar "(Wobblewisp's vase is two / inches to the left of where / you left it.)"
  [R8]
  say nar "(You leave it there. {.}It looks / better.)"
end

# =============================================================================
# S210 "Cosy, Dark, Eternal" - Hauntessa (109) + Pharaohnie (673) - Ch4
# after the Annex - housing (grafted heart S115)
# =============================================================================
@pack quests kind=side
@mission S210 giver=109 side=hill ch=4-5 after=BEAT_EP4_4 at=6,8 verbs=housing chain=V step=3 later=6m reward=0 offer=s210_offer active=s210_again title="Cosy, Dark, Eternal" hint="Furnish the Annex to the ETERNAL grade: Hellvile pieces and one cursed thing."
@pack sb_tock kind=side

script s210_offer
  say 109 HAPPY "Your Annex! {.}Half Hillvile, / half Hellvile. It needs / rating by both traditions."
  [R8]
  say 673 HAPPY "Pharaohnie. Tomb realtor. / {..}Both traditions is MY / tradition, darling."
  choice 109 "Get the Annex rated?" { "Rate away." -> yes ; "Later." }
  end
yes:
  quest S210 active
  call s210_again
end

script s210_again
  say 673 NEUTRAL "Four Hellvile pieces. A cursed / one. {.}Cosy. Dark. Eternal."
  push home 0x4501
  home GOAL 5
  say 673 NEUTRAL "Cosy. {.}Dark."
  [R8]
  say 673 NEUTRAL "{..}Where's eternal?"
  jge v0 1 eternal
  say 673 SAD "Not yet. {.}Every tomb needs / something cursed, darling."
  end
eternal:
  sfx SFX_GLITCH
  pal PAL_HELLIFY 16
  wait 4
  pal PAL_HELLIFY 0
  [R8]
  say nar "VOICE FROM A SHELF: ...it's / fine."
  [R13]
  sfx SFX_THUD
  shake SHK_SLAM
  say 109 HAPPY "{big}TIDY.{/}"
  [R13]
  sfx SFX_THUD
  shake SHK_SLAM
  say 673 HAPPY "{big}ETERNAL.{/}"
  [R21]
  say 673 HAPPY "I could sell this house to / you, Hauntessa."
  [R8]
  say 109 SHOCK "...I'll make an offer."
  [R8]
  say 673 HAPPY "Accepted."
  [R13]
  say nar "(Neither of them can hold a / pen.)"
  [R21]
  [MUTE 21]
  say nar "(Hauntessa dusts the Annex, / around the height marks on / the door frame. Not over.)" [HEART]
  [HOLD 13]
  say 109 SAD "I've never been inside a house / someone STAYED in."
  reward 0 furn=realtors_sign
  home PLACE_GIFT furn=realtors_sign
  befriend 109 3
  befriend 673 3
  quest S210 done
  later 6m s210_later
end

script s210_later
  say nar "(The realtor's sign in your / garden has a second line now:)"
  [R8]
  say nar "'SOLD (to a ghost) (she's / keeping it warm)'"
end
