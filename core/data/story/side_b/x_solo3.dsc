# side_b/x_solo3.dsc - batch-B stand-alones S213-S217, the post-game grafts
# (owner: S5; story_final.md sec.14.2; grafted hearts S51+S122, S128, S119,
# S131, S117): one eye and three eyes, the longest lullaby, two brave kids
# in one frame, free hugs on both sides, and the first contract nobody
# regrets.

# =============================================================================
# S213 "One Eye, Three Eyes" - Oddsball (522, Hellvile) for Kevin (the bust
# in your house) - Ch5 - deliver, talk (grafted hearts S51 + S122)
# =============================================================================
@pack quests kind=side
@mission S213 giver=522 side=hell ch=5-5 at=17,18 verbs=deliver later=6m reward=40 offer=s213_offer active=s213_again title="One Eye, Three Eyes" hint="Find Kevin a hat that fits all three eyes. Then carry him over to meet Oddsball."
@pack sb_solo3 kind=side minutes=85

script s213_offer
  say 522 NEUTRAL "There's a fella in your / house. {.}Three eyes. A bust. / Says 'it's fine' a lot."
  [R8]
  say 522 HAPPY "I've got one eye. {..}I'd like / to meet him. Odds are we get / on."
  choice 522 "Introduce them?" { "I'll bring Kevin." -> yes ; "Later." }
  end
yes:
  quest S213 active
  call s213_again
end

script s213_again
  say nar "KEVIN: A hat. {.}For the trip. / Every hat clips the third / eye."
  [R8]
  sfx SFX_POP2
  say nar "(A hat. {.}It pops off.)"
  [R5]
  sfx SFX_POP2
  say nar "(A bow. {.}It pops off.)"
  [R5]
  sfx SFX_POP2
  [R5]
  say nar "(A smaller hat. {.}It pops off.)"
  [R13]
  choice nar "Last try:" { "A hat with a hole." -> hole }
hole:
  sfx SFX_TINK
  say nar "(A hat with a hole in it. / Kevin looks in the mirror.)"
  [HOLD 13]
  say nar "(Only the third eye blinks.)"
  [R13]
  say nar "KEVIN: ...it's fine."
  [R8]
  say nar "(He means it, this time.)"
  verb deliver 522 cell=17,18 fragile then=s213_meet
end

script s213_meet
  say nar "(The market. You set Kevin / down on a crate facing / Oddsball.)"
  [MUTE 26]
  [HOLD 26]
  say nar "(They stare.)"
  [HOLD 13]
  say nar "KEVIN: Yes, three."
  [R8]
  say 522 NEUTRAL "Yes, one."
  [R8]
  sfx SFX_TINK
  say nar "(They blink at the same time. / {.}That's the friendship.)"
  [R13]
  say 522 HAPPY "Odds on which eye blinks / first? {.}I'll take the left."
  [R8]
  sfx SFX_TINK
  say nar "(The third eye blinks first. / Oddsball loses.)"
  [R13]
  say nar "KEVIN: Nobody ever asks about / the third one."
  [R21]
  say nar "KEVIN: {d}It can see Tuesday." [PUNCH]
  [R21]
  [MUTE 21]
  say 522 HAPPY "Four eyes between us. {.}Finally / the right number for cards." [HEART]
  reward 40 furn=kevins_hat_stand furn=eye_chart
  home PLACE_GIFT furn=kevins_hat_stand
  befriend 522 3
  quest S213 done
  later 6m s213_later
end

script s213_later
  sfx SFX_TINK
  say nar "(Kevin blinks his third eye / with the eye chart. {.}Every / 83 frames, exactly.)"
end

# =============================================================================
# S214 "The Longest Lullaby" - Ploddy (701, Hellvile) + Maestro Mew (3)
# Ch5 - concert (grafted heart S128)
# =============================================================================
@pack quests kind=side
@mission S214 giver=701 side=hell ch=5-5 at=8,21 verbs=concert later=6m reward=0 offer=s214_offer active=s214_again title="The Longest Lullaby" hint="Ploddy hums lullabies to clouds all night. Maestro Mew wants it conducted."
@pack sb_solo3 kind=side

script s214_offer
  say 701 SLEEPY "I haul clouds. {.}At night I / hum to them. It helps them / drift."
  [R8]
  say 3 HAPPY "And I want it CONDUCTED. {!}In / the Concert Hall. Tonight."
  choice 701 "Play the lullaby?" { "Shh. Tonight." -> yes ; "Later." }
  end
yes:
  quest S214 active
  call s214_again
end

script s214_again
  [NO GAG]
  [NIGHT]
  say nar "(The Concert Hall, at night. / Mew lifts his tail.)"
  push concert 701
  native CB_SIDE_ONCE SONG_HELL_LULLABY
  say nar "(It's the lullaby for the / Wall, slowed to a hum. {.}The / one Wisp's mum hums.)"
  [HOLD 13]
  say nar "(The orchestra follows Mew's / tail. Off the edge of the / stage.)"
  [HOLD 13]
  sfx SFX_SPLASH
  say nar "(In the front row, the jar / cloud on its cushion rains / one pixel. On the beat.)" [HEART]
  [HOLD 21]
  say nar "(The hall sleeps.)"
  [NO BUTTON]
  [HOLD 26]
  native CB_SIDE_MUSIC
  reward 0 furn=cloud_cushion
  home PLACE_GIFT furn=cloud_cushion
  befriend 701 3
  befriend 3 2
  quest S214 done
  later 6m s214_later
end

script s214_later
  [NO GAG]
  sfx SFX_SPLASH
  say nar "(The jar on your shelf rains / one pixel. {.}The jukebox is / playing the hum.)"
end

# =============================================================================
# S215 "Two Brave Kids" - Shutterbug (46) + Pip (42) + Wisp (563) - Ch5
# photo (grafted heart S119)
# =============================================================================
@pack quests kind=side
@mission S215 giver=46 side=hill ch=5-5 at=37,24 verbs=photo later=6m reward=0 offer=s215_offer active=s215_again title="Two Brave Kids" hint="Shutterbug wants Pip, Wisp and both plushies in one frame."
@pack sb_solo3 kind=side

script s215_offer
  say 46 HAPPY "Two kids. Two plushies. {.}One / frame. {..}The best picture / I'll ever take."
  choice 46 "Set up the shot?" { "Say cheese!" -> yes ; "Later." }
  end
yes:
  quest S215 active
  call s215_again
end

script s215_again
  [NO GAG]
  say 46 NEUTRAL "The bandstand. {.}You take it. / My hands shake."
  verb photo pip cell=37,26 then=s215_photo
end

script s215_photo
  [NO GAG]
  sfx SFX_SPARKLE
  say nar "(Pip and Wisp, side by side. / Mr. Flops. Flops the 1st, / sticker and all.)"
  [HOLD 13]
  say nar "(At the back: a faint shape, / mid-step. {.}The flash caught / Wisp's mum.)"
  [HOLD 13]
  say ID_WISP NEUTRAL "is mum in it?"
  [HOLD 13]
  say 46 SAD "She's in it. {..}Trust me." [HEART]
  [HOLD 21]
  say nar "(In the photo, Mr. Flops / blinks. {.}He has button eyes.)"
  [NO BUTTON]
  [HOLD 26]
  reward 0 furn=two_brave_kids
  home PLACE_GIFT furn=two_brave_kids
  befriend 46 3
  befriend pip 2
  befriend 563 2
  quest S215 done
  later 6m s215_later
end

script s215_later
  [NO GAG]
  say nar "(Wisp's mum has the same / photo on her wall. {..}In hers, / she is in focus.)"
end

# =============================================================================
# S216 "Free Hugs (Both Sides)" - Hugsley (20) + Big Snarl (855) - Ch5
# fetch, minigame (grafted heart S131)
# =============================================================================
@pack quests kind=side
@mission S216 giver=20 side=hill ch=5-5 at=30,28 verbs=fetch,minigame later=6m reward=50 offer=s216_offer active=s216_again title="Free Hugs (Both Sides)" hint="Hugsley wants to hug all of Hellvile. Bring Big Snarl a Plushie (his rule)."
@pack sb_solo3 kind=side

script s216_offer
  say 20 HAPPY "I want to hug everyone in / Hellvile! {!}EVERYONE."
  [R8]
  say 20 SAD "Big Snarl is the bouncer. {..}He / has a rule. A squeaky toy at / the door."
  choice 20 "Help Hugsley?" { "Free hugs!" -> yes ; "Later." }
  end
yes:
  quest S216 active
end

script s216_again
  native CB_SIDE_HAVE ITEM_PLUSHIE
  jlt v0 1 need
  take ITEM_PLUSHIE
  say nar "(A squeaky Plushie for Big / Snarl. He squeezes it.)"
  sfx SFX_SQUEAK
  [R8]
  say nar "(Hugsley hugs Big Snarl.)"
  sfx SFX_SQUEAK
  [HOLD 8]
  say 855 NEUTRAL "...Again."
  [R13]
  npc 855 RIPPLE 4
  say nar "(A hug line forms. {.}Down the / street. Round the corner.)"
  push minigame SMG_B_HUGS
  jif SF_GRUMBLE_DEAD gate
  [R13]
  say grumble LOVE "Hey, sugar. {.}My turn?"
  [R8]
  sfx SFX_LOVE
  say nar "(Hugsley hugs him.)"
  [R8]
  say nar "GURGLE: grrrbl." [GURGLE]
  [R8]
  say nar "(A tone nobody has heard / before.)"
  [R13]
  sfx SFX_CREAK
  say nar "(Hugsley hugs the Gate. {.}It / creaks.)"
  [R5]
  say grumble SHOCK "Hey."
  [R21]
  jmp thousand
gate:
  [R13]
  sfx SFX_CREAK
  say nar "(Hugsley hugs the Gate. {.}It / creaks. {..}Nobody says 'Hey.')"
  [HOLD 21]
thousand:
  [MUTE 21]
  say 20 HAPPY "One thousand hugs today. {.}For / the first time in eleven / years." [HEART]
  [HOLD 13]
  say 20 HAPPY "{..}With both sides."
  reward 50
  befriend 20 3
  befriend 855 3
  quest S216 done
  later 6m s216_later
  end
need:
  say 20 NEUTRAL "A squeaky toy. {.}A Plushie / squeaks! The shop has them."
end

script s216_later
  sfx SFX_SQUEAK
  say nar "(Big Snarl keeps the squeaky / toy. {.}He squeaks it once at / everyone who crosses.)"
  [R8]
  say nar "(It means welcome.)"
end

# =============================================================================
# S217 "Signhere's Receipt" - Signhere (567, Hellvile contract devil) - Ch5
# deliver, talk (grafted heart S117)
# =============================================================================
@pack quests kind=side
@mission S217 giver=567 side=hell ch=5-5 at=38,13 verbs=deliver later=6m reward=50 offer=s217_offer active=s217_again title="Signhere's Receipt" hint="Everyone who tore a page in Ep 4.5 must sign Signhere's acknowledgment."
@pack sb_solo3 kind=side

script s217_offer
  say 567 NEUTRAL "Everyone who tore a page of / my contract must sign this. / {.}An acknowledgment."
  [R8]
  say 567 NEUTRAL "That they tore it. {..}It's nine / hundred pages."
  [HOLD 8]
  say 567 NEUTRAL "Plus the appendix."
  choice 567 "Collect signatures?" { "Got a pen?" -> yes ; "Later." }
  end
yes:
  quest S217 active
  call s217_again
end

script s217_again
  say 567 NEUTRAL "Each signs one letter of / their name. {.}It's binding. / Probably."
  verb deliver mayor cell=29,8 then=s217_mayor   # map=hill
end

script s217_mayor
  sfx SFX_TIK
  say nar "(The Mayor signs a G. {.}A big / loopy G.)"
  verb deliver nox cell=7,13 then=s217_nox   # map=hell
end

script s217_nox
  sfx SFX_TIK
  say nar "(Nox signs an N. {.}It's very / neat.)"
  verb deliver rivet cell=53,10 then=s217_rivet   # map=hill
end

script s217_rivet
  sfx SFX_WELD
  say nar "(Rivet welds an R onto the / page. {.}It's structural.)"
  jif SF_GRUMBLE_DEAD gone
  verb deliver grumble cell=38,27 then=s217_grumble   # map=hell
  end
gone:
  say nar "(The last line reads / GRUMBLEGUTS. {..}@E signs for / him. A tiny horn.)"
  verb deliver 567 cell=38,13 then=s217_file
end

script s217_grumble
  say grumble LOVE "Where do I sign, handsome? / {.}Don't answer. I know where."
  [R8]
  sfx SFX_THUD
  say nar "(He sits on the page. {r} / A rear-print.)"
  [R13]
  sfx SFX_BEEP
  say nar "(@E signs with a beep. A / tiny horn-shaped mark.)"
  verb deliver 567 cell=38,13 then=s217_file
end

script s217_file
  say 567 NEUTRAL "...All signed."
  [R13]
  say 567 SHOCK "This is in breach of / nothing. {..}I don't know what / to do with that."
  [R21]
  [MUTE 21]
  say nar "(His first contract that nobody / regrets.)" [HEART]
  [HOLD 13]
  say nar "(He keeps the pen.)"
  reward 50 furn=contract_confetti_jar
  home PLACE_GIFT furn=contract_confetti_jar
  befriend 567 3
  quest S217 done
  later 6m s217_later
end

script s217_later
  say nar "(A single page of the old / contract is nailed to the / gatehouse door.)"
  [R8]
  jif SF_GRUMBLE_DEAD sealed
  say nar "(Across it, in gurgly writing: / VOID.)"
  end
sealed:
  say nar "(The door is sealed. {..}Someone / has left a takeaway box on / the step.)"
end
