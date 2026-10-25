# side_b/r_wisp.dsc - Chain R "Wisp, the haunting" (S166 S167 S205 S168
# S206: your house, then Hellvile; grafted hearts S99 and S133) (owner: S5;
# story_final.md sec.14.1). An imp kid who cried at the Wall until they
# exhaled themselves out, haunting your house into the shape of their room;
# then a body, a door frame, a party, and a candle allowed to go out.
# Wisp writes and talks in lower case, before and after.

# =============================================================================
# S166 "Placed By: ???" - Hauntessa (109) notices it - Ch2 after Ep 2.3
# housing, find (absorbs Creakster's uncued creak)
# =============================================================================
@pack quests kind=side
@mission S166 giver=109 side=hill ch=2-2 after=BEAT_EP2_3 at=6,8 verbs=housing,find chain=R step=1 later=5m reward=0 offer=s166_offer active=s166_again title="Placed By: ???" hint="A tiny bed nobody placed is in your corner. Watch it for three nights."
@pack sb_wisp kind=side minutes=115

string S166_Q "Leave a note by the tiny bed:"
string S166_A "who are you"

script s166_offer
  native CB_SIDE_STATE 208
  jge v0 3 known
  say 109 SHOCK "Oh! {.}Hello. Sorry. I'm the / housekeeper. A ghost. Sorry."
  [R8]
known:
  say 109 NEUTRAL "There's a bed in your corner. / {.}A tiny one. Imp-sized."
  [R8]
  say 109 NEUTRAL "I didn't put it there. {..}I'd / have centred it."
  [HOLD 13]
  say 109 SAD "The catalogue says 'placed / by: ???'. {.}You can't sell it. / You can try."
  choice 109 "Look into it?" { "Three nights." -> yes ; "Later." }
  end
yes:
  quest S166 active
  call s166_again
end

script s166_again
  [NIGHT]
  push home 0x4002
  say nar "(Night one. You carry the tiny / bed out to the porch.)"
  time ADVANCE 600
  [R13]
  sfx SFX_CREAK
  say nar "(By morning it's back / inside. {.}One step closer to / your bed.)"
  [HOLD 13]
  say 109 NEUTRAL "Night two. {.}Put a lamp next / to it. A test. Ghosts hate / being seen."
  [NIGHT]
  push home -1
  [MUTE 14]
  say nar "(In decorate mode, for three / frames, the cursor moves by / itself. It places the lamp.)" [PUNCH]
  [HOLD 13]
  sfx SFX_CREAK
  say nar "(Then, with nobody on it, a / floorboard creaks. {.}Once. On / no beat at all.)"
  [NO BUTTON]
  [HOLD 26]
  time ADVANCE 600
  say nar "(Morning. The lamp is on. / {.}It's turned toward the tiny / bed. A night-light.)"
  [R21]
  say 109 NEUTRAL "Night three. {.}Leave it a / note."
  verb find cell=4,7 then=s166_note
end

script s166_note
  [NIGHT]
  sfx SFX_TINK
  say nar "(The tiny bed has a tiny / sign now. // Made from your shopping / list. 'DO NOT SELL'.)"
  [R8]
  say nar "(The handwriting is terrible. / {..}The spelling is worse.)"
  [R8]
  sfx SFX_BEEP
  say nar "beep."
  [R13]
  stash 0 S166_Q
  stash 1 S166_A
  push minigame STORY_MG_TEXT_ENTRY
  time ADVANCE 600
  [R13]
  sfx SFX_SHWIP
  say nar "(Morning. On your note, in / crayon: a house with two / windows.)"
  [HOLD 21]
  say nar "(It's not your house. {..}It / has a door on the wrong / side.)"
  reward 0 furn=wisp_drawing_1
  home PLACE_GIFT furn=wisp_drawing_1
  befriend 109 1
  quest S166 done
  later 5m s166_later
end

script s166_later
  sfx SFX_CREAK
  say nar "(Wisp's drawing is on your / fridge. {.}You didn't put it / there either.)"
end

# =============================================================================
# S167 "Night Light" - Wisp (563), visible from Ep 3.2 - Ch3
# housing, fetch, weld
# =============================================================================
@pack quests kind=side
@mission S167 giver=563 side=hill ch=3-3 after=BEAT_EP3_2 at=3,8 verbs=fetch,weld,housing chain=R step=2 later=6m reward=40 offer=s167_offer active=s167_again title="Night Light" hint="Get lamp oil from Lumina for Wisp's window box. Paint it a sky."
@pack sb_wisp kind=side

script s167_offer
  say ID_WISP SAD "it's not the demons. {.}it's / the sky. the red sky."
  [R8]
  say ID_WISP SAD "can i have one window that's / always daytime? {..}just / one."
  choice ID_WISP "Make Wisp a window?" { "One daytime window." -> yes ; "Later." }
  end
yes:
  quest S167 active
  call s167_again
end

script s167_again
  say ID_WISP NEUTRAL "lumina has lamp oil. {.}for / the sun part."
  verb deliver 76 cell=36,42 then=s167_oil
end

script s167_oil
  sfx SFX_PICKUP
  say 76 NEUTRAL "Lamp oil? {.}For a window? / {..}For Wisp. Take two."
  [R8]
  say rivet NEUTRAL "A window box. {.}Bring it. I / weld, you paint."
  push workshop
  sfx SFX_WELD
  choice nar "Paint the sky:" { "Blue." -> blue ; "Pink." -> pink ; "Gold." -> gold ; "Mint." -> mint }
blue:
  say nar "(Blue, like a Tuesday.)"
  jmp cloud
pink:
  say nar "(Pink, like the bus.)"
  jmp cloud
gold:
  say nar "(Gold, like four o'clock.)"
  jmp cloud
mint:
  say nar "(Mint, like a nice sky that / doesn't exist.)"
cloud:
  [R8]
  say nar "(You add one cloud.)"
  [R13]
  say ID_WISP NEUTRAL "...no crown."
  [R8]
  choice nar "You:" { "No crown." -> nocrown }
nocrown:
  [R13]
  say nar "(Wisp adds a tiny sun with a / face. {.}It's the Mayor's face.)"
  [R8]
  say ID_WISP NEUTRAL "he looks worried."
  [R8]
  say nar "(It does.)"
  [R13]
  push home -1
  say nar "(You hang it over a boarded / window. // Wisp sits under it all / night, through the waves.)"
  [HOLD 13]
  say nar "(By morning Wisp is slightly / more solid. {.}You can almost / see their freckles.)"
  [R21]
  [MUTE 21]
  say ID_WISP SAD "at home i cried at the Wall / till i fell out." [HEART]
  [HOLD 13]
  say ID_WISP SAD "{..}i didn't know it was MY sky / i was scared of."
  [HOLD 21]
  say ID_WISP HAPPY "this one's better. {.}yours / has a sun."
  reward 40 furn=daytime_window
  home PLACE_GIFT furn=daytime_window
  befriend 563 3
  quest S167 done
  later 6m s167_later
end

script s167_later
  say nar "(The daytime window is / always day. // {.}At three in the morning you / can hear birds in it.)"
end

# =============================================================================
# S205 "Taller" - Wisp (563), in a body - Ch4 after Ep 4.4 - housing, talk
# (grafted heart S99)
# =============================================================================
@pack quests kind=side
@mission S205 giver=563 side=hill ch=4-5 after=BEAT_EP4_4 at=3,8 verbs=housing chain=R step=3 later=5m reward=0 offer=s205_offer active=s205_again title="Taller" hint="Measure Wisp against your door frame. Pip wants a mark too."
@pack sb_wisp kind=side

script s205_offer
  say ID_WISP NEUTRAL "can you measure me? {.}against / YOUR door frame."
  [R8]
  say ID_WISP SAD "ours has mum's marks from / before. {..}i don't want to go / past them yet."
  choice ID_WISP "Measure Wisp?" { "Stand up straight." -> yes ; "Later." }
  end
yes:
  quest S205 active
  call s205_again
end

script s205_again
  push home -1
  say nar "(The door frame becomes a / height board.)"
  [R8]
  say nar "(Wisp stands against it. {.}Very / straight.)"
  [HOLD 13]
  sfx SFX_TIK
  say nar "(A mark.)"
  [R13]
  say pip HAPPY "Me too! {!}Me too! I'm taller! / I'm WAY taller!"
  [R8]
  say nar "(Pip is on tiptoe. {.}Pip is / caught. {r})"
  [R8]
  sfx SFX_TIK
  say nar "(A mark for Pip. {..}Exactly / where Pip's heels are.)"
  [R13]
  sfx SFX_BEEP
  say nar "(@E wants one. {.}@E's mark is / one pixel tall.)"
  [R21]
  say nar "(@E looks at it for a long / time.)"
  [R8]
  sfx SFX_BEEP
  say nar "beep."
  [R13]
  say nar "(Satisfied.)"
  [R21]
  [MUTE 26]
  sfx SFX_DOOR
  say nar "(Wisp's mum is at the door. / She adds her own mark. {.}Then, / lower, one more.)"
  [HOLD 13]
  say nar "(Where Wisp's head was, two / years ago.)"
  [HOLD 21]
  say nar "WISP'S MUM: You grew. {.}In our house. / {..}While you were out." [HEART]
  [NO BUTTON]
  [HOLD 26]
  reward 0 furn=height_marks
  home PLACE_GIFT furn=height_marks
  befriend 563 3
  befriend pip 2
  quest S205 done
  later 5m s205_later
end

script s205_later
  sfx SFX_TIK
  say nar "(A month later you measure / again. // Wisp's line is higher. @E's / is still one pixel.)"
end

# =============================================================================
# S168 "The Housewarming" - Wisp (563), in a body - Ch4-5 after Ep 4.4
# housing, cooking, talk
# =============================================================================
@pack quests kind=side
@mission S168 giver=563 side=hell ch=4-5 after=BEAT_EP4_4 at=20,10 verbs=housing,cooking chain=R step=4 later=7m reward=150 offer=s168_offer active=s168_again title="The Housewarming" trophy="Placed By: Wisp" hint="Decorate your Hell Pits flat, cook two dishes, and let Wisp throw you a party."
@pack sb_wisp kind=side

script s168_offer
  say ID_WISP HAPPY "i want to throw YOU a / housewarming. {.}at your Hell / Pits flat."
  [R8]
  say ID_WISP NEUTRAL "you did one for me. {..}you / didn't even know."
  [HOLD 8]
  say ID_WISP NEUTRAL "everyone who knocked, on the / dark night. {.}they're all / coming."
  choice ID_WISP "Have a housewarming?" { "Party at mine!" -> yes ; "Later." }
  end
yes:
  quest S168 active
  call s168_again
end

script s168_again
  say ID_WISP NEUTRAL "make it nice. {.}hellvile / things count double."
  push home -3
  say ID_WISP NEUTRAL "food. one dish from each side. / {.}bone broth. angel cake."
  push cooking 0x500
  push cooking 0x1200
  sfx SFX_DOOR
  say nar "(The guests arrive. Everyone / who knocked on the dark / night. // Each finds the thing they / gave.)"
  [R13]
  say nar "(Wisp's parents carry in a / chair. {.}The thinking chair.)"
  [R8]
  say nar "WISP'S DAD: Sit in it, face / the wall, and say one honest / thing. // {..}It's tradition. We just made / it up."
  [R21]
  sfx SFX_DOOR
  say nar "(Mascotty, in the doorway.)"
  npc mascotty STILL
  [HOLD 21]
  say nar "(This time she comes in.)"
  [R5]
  sfx SFX_FANFARE
  say nar "(Everyone cheers.)"
  [R13]
  say nar "(She sits in the thinking / chair. Facing the wall. {.}Out / of habit.)"
  [HOLD 13]
  say ID_WISP HAPPY "that's how you use it."
  npc mascotty THAW
  [R21]
  [MUTE 21]
  say ID_WISP SAD "i was in your house when it / was empty." [HEART]
  [HOLD 13]
  say ID_WISP HAPPY "{..}it's not empty now."
  reward 150 furn=thinking_chair
  home PLACE_GIFT furn=thinking_chair
  befriend 563 4
  befriend mascotty 2
  quest S168 done
  later 7m s168_later
end

string S168_NEWS "Local child reportedly 'un-floated'. Parents thrilled."

script s168_later
  stash 0 S168_NEWS
  native CB_SIDE_NEWS
  sfx SFX_TIK
  say nar "(HELLVILE HERALD: local / child reportedly / 'un-floated'. // {..}Parents thrilled.)"
end

# =============================================================================
# S206 "A Candle Goes Out" - Wisp (563) - Ch5 after S168 - talk, housing
# (grafted heart S133; the chain's true finale: no comic beat, no button)
# =============================================================================
@pack quests kind=side
@mission S206 giver=563 side=hell ch=5-5 at=20,10 verbs=housing chain=R step=5 later=8m reward=0 offer=s206_offer active=s206_again title="A Candle Goes Out" trophy="Somebody Left the Light On" hint="Wisp's candle has burned since Chapter 1. Help it go out."
@pack sb_wisp kind=side

script s206_offer
  [NO GAG]
  say nar "WISP'S MUM: The candle in / Wisp's window. // {.}It's burned since the night / of the second paper boat."
  [R8]
  say nar "WISP'S MUM: They're home. {..}It / can rest. Will you help them / let it go out?"
  choice nar "Help with the candle?" { "I'll be there." -> yes ; "Not yet." }
  end
yes:
  quest S206 active
  call s206_again
end

script s206_again
  [NO GAG]
  native CB_SIDE_MUSIC
  say nar "(Apartment 13. Dusk. The / candle in the window.)"
  [HOLD 21]
  say ID_WISP SAD "if i blow it out... {..}is it / giving up?"
  native CB_SIDE_BRANCH 0
  jge v0 2 lied
  choice ID_WISP "Is it giving up?" { "No." -> rest ; "It's letting it rest." -> rest ; "(Say nothing.)" -> rest }
  end
lied:
  jge v0 3 truth
  choice ID_WISP "Is it giving up?" { "No." -> rest ; "It's letting it rest." -> rest ; "(Say nothing.)" -> rest ; "I lied, before." -> sorry }
  end
truth:
  choice ID_WISP "Is it giving up?" { "No." -> rest ; "It's letting it rest." -> rest ; "(Say nothing.)" -> rest }
  end
sorry:
  say nar "(I lied, before. {.}Your mum / hummed. That was the true / one.)"
  [HOLD 21]
  say ID_WISP SAD "...i know. {..}i heard her too."
rest:
  [MUTE 64]
  [HOLD 26]
  sfx SFX_SHWIP
  say nar "(Wisp blows.)"
  [HOLD 26]
  say nar "(Wisp's mum puts a hand on / their shoulder.)"
  [HOLD 13]
  say nar "WISP'S MUM: There. {.}Now I can SEE out."
  [HOLD 21]
  sfx SFX_DOOR
  say pip NEUTRAL "I brought a lantern. {.}For the / room. Not the window."
  [HOLD 21]
  sfx SFX_BEEP
  say nar "(@E beeps once. Very small.)"
  [NO BUTTON]
  [HOLD 26]
  native CB_SIDE_MUSIC
  reward 0 furn=window_candle_out
  home PLACE_GIFT furn=window_candle_out
  befriend 563 4
  quest S206 done
  later 8m s206_later
end

script s206_later
  [NO GAG]
  say nar "(Your own window candle has / gone out too. {..}Nobody blew / it.)"
end
