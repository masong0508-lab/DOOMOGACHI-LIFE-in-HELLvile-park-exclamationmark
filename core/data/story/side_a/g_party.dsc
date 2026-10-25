# side_a/g_party.dsc - Chain I "Sprinklepop's lost soul and Wheezle"
# (S130-S133) (owner: S4; story_final.md sec.14.1). The soul is a party
# that wandered off; Wheezle's little ghosts are his swallowed laughs.

# =============================================================================
# S130 "Party Supplies" - Sprinklepop (81) - Ch1-2 (before Ep 2.5)
# fetch, minigame
# =============================================================================
@pack quests kind=side
@mission S130 giver=81 side=hill ch=1-2 after=BEAT_EP1_3 at=21,42 verbs=fetch,minigame chain=I step=1 later=6m reward=20 offer=s130_offer active=s130_again title="Party Supplies" hint="Win Sprinklepop twenty balloons at Twisty's Fun Fair stall."
@pack sa_sprk kind=side minutes=60

script s130_offer
  sfx SFX_SPARKLE
  say sprinklepop HAPPY "I need twenty balloons! {.}For / a party I haven't been / invited to yet."
  [R8]
  say sprinklepop HAPPY "YET. {!}Twisty gives them out / if you pop the bad ones."
  choice sprinklepop "Win her twenty balloons?" { "Party time!" -> yes ; "Later." }
  end
yes:
  quest S130 active
  befriend sprinklepop 1
  call s130_again
end

script s130_again
  say 43 HAPPY "Pop the grey ones, keep the / pink ones. {.}Twenty pink and / they're yours!"
  push minigame SMG_A_BALLOONS
  jlt v0 1 lost
  sfx SFX_POP2
  say 43 HAPPY "Twenty! {.}Plus a bonus. I / made one of YOU."
  [R8]
  say nar "(The balloon-you has the / wrong number of ears.)"
  [R13]
  say 43 NEUTRAL "It's an interpretation."
  verb deliver sprinklepop cell=21,42 then=s130_party
  end
lost:
  say 43 SAD "You popped a pink one. {..}It's / fine. They're only feelings. / Have another go."
end

script s130_party
  say sprinklepop HAPPY "TWENTY! {!}Now I'm ready for / whoever invites me."
  [R8]
  sfx SFX_SPARKLE
  say sprinklepop SHOCK "Ah-{.}CHOO!"
  [R13]
  sfx SFX_SPARKLE
  say sprinklepop SHOCK "Ah-{.}CHOO!"
  [R13]
  [R8]
  sfx SFX_GLITCH
  say sprinklepop POSSESSED "Ah-{..}choo."
  [HOLD 13]
  say nar "(Purple glitter.)"
  [R8]
  say sprinklepop HAPPY "Doctors say it's fine!"
  [R21]
  [MUTE 21]
  sfx SFX_CREAK
  say nar "(Behind her, twenty balloons / turn on their strings to / face the Wall.)"
  [HOLD 21]
  say nar "(She doesn't notice.)"
  reward 20 furn=balloon_you
  befriend sprinklepop 3
  befriend 43 2
  quest S130 done
  later 6m s130_later
end

script s130_later
  say nar "(A queue outside Cloud Nine / Vapor. Sprinklepop is first. / She's brought a balloon.)"
end

# =============================================================================
# S131 "Lost and Found" - Dr. Bloom (111) - Ch2-3 (after Ep 2.5)
# find, photo, pet
# =============================================================================
@pack quests kind=side
@mission S131 giver=111 side=hill ch=2-3 after=BEAT_EP2_5 at=17,23 verbs=find,photo,pet chain=I step=2 later=6m reward=50 offer=s131_offer active=s131_again title="Lost and Found" hint="Follow the glitter: her stand, the Fun Fair, the pond, then the Wall."
@pack sa_sprk kind=side

script s131_offer
  say bloom SAD "Sprinklepop's soul wandered / off. {..}With its map upside / down."
  [R8]
  say bloom NEUTRAL "Souls go exactly as far as / they're scared to. {.}Follow / the glitter."
  choice bloom "Follow the trail?" { "On it." -> yes ; "Not now." }
  end
yes:
  quest S131 active
  call s131_again
end

script s131_again
  say nar "(Purple glitter, in a line. / It starts at her lollipop / stand.)"
  verb find cell=22,43 then=s131_stand
end

script s131_stand
  sfx SFX_SPARKLE
  say nar "(A tiny see-through balloon / is tied to the stand. {.}It / bobs.)"
  [R8]
  say nar "(Somebody saw 'a bouncy / see-through bunny reading a / map wrong'. Heading west.)"
  verb find cell=7,42 then=s131_fair
end

script s131_fair
  sfx SFX_SPARKLE
  say nar "(At the Fun Fair: a / see-through cake. Seven / see-through candles.)"
  [R13]
  say 43 NEUTRAL "{d}She ordered that."
  [R8]
  say nar "(The trail goes on toward / the pond.)"
  verb find cell=24,43 then=s131_pond
end

script s131_pond
  sfx SFX_SPARKLE
  say nar "(Little balloons on the / water. The glitter turns / east. Toward the Wall.)"
  verb photo sprinklepop cell=54,29 then=s131_photo
end

script s131_photo
  jlt v1 1 miss
  sfx SFX_SPARKLE
  say nar "(The polaroid: the soul at / the Wall, waving. {.}Its map is / the right way up.)"
  [R13]
  say nar "(It looks at Hillvile. It / looks at Hellvile.)"
  [HOLD 21]
  say nar "(It chooses Hellvile.)"
  [R13]
  push pet
  sfx SFX_BEEP
  say nar "(The pet sniffs the crack. / 'beep.' {..}It went through.)"
  [R21]
  [MUTE 21]
  say bloom NEUTRAL "She went toward the party she / wasn't invited to." [HEART]
  [HOLD 13]
  say bloom HAPPY "{..}That's not lost. That's / brave."
  reward 50 furn=translucent_balloon
  befriend bloom 3
  quest S131 done
  later 6m s131_later
  end
miss:
  say nar "(Just the Wall. {.}The soul / bounced off to the side. / Try again.)"
  verb photo sprinklepop cell=54,29 then=s131_photo
end

script s131_later
  say nar "(Somewhere in Hellvile, a / man with a cough names a / little ghost 'Sprinkles'.)"
end

# =============================================================================
# S132 "Wheezle's Little Ghosts" - Wheezle (707, Hellvile) - Ch4
# talk, find, timing
# =============================================================================
@pack quests kind=side
@mission S132 giver=707 side=hell ch=4-5 after=BEAT_EP4_2 at=53,17 verbs=timing chain=I step=3 later=6m reward=80 offer=s132_offer active=s132_again title="Wheezle's Little Ghosts" hint="Meet Wheezle's ghosts and catch Sprinkles's sneezes in a jar."
@pack sa_sprk kind=side

script s132_offer
  sfx SFX_BUBBLE
  say 707 HAPPY "I cough out little ghosts. {.}I / name them."
  [R8]
  say 707 HAPPY "One's bigger. Pink. Throws / parties. {..}I call her / Sprinkles. She's mine."
  choice 707 "Meet the ghosts?" { "Introduce me." -> yes ; "Later." }
  end
yes:
  quest S132 active
  call s132_again
end

script s132_again
  sfx SFX_BUBBLE
  say nar "(A small ghost bows.) {.}'Tuesday.'"
  [R13]
  say nar "(Another.) {.}'Wednesday.'"
  [R13]
  say nar "(Another.) {.}'Thursday.'"
  [R13]
  say nar "{d}(Another.) 'Also Thursday.'"
  [R8]
  say 707 NEUTRAL "I had a long week."
  [R13]
  say nar "(Sprinkles sneezes glitter / on Sunday. {.}Catch every / sneeze in the jar!)"
  push timing pattern=three skin=laugh
  sfx SFX_POP2
  say nar "(A jar full of sneezes. It / glows pink.)"
  [R13]
  say nar "(Sprinkles waves a map. / It's the right way up.)"
  choice nar "Tell Wheezle, gently:" { "She's somebody's friend." ; "She's visiting." ; "She's a party guest." }
  [HOLD 13]
  say 707 SAD "...She's not mine."
  [HOLD 13]
  say 707 NEUTRAL "{..}But she can visit?"
  [R21]
  [MUTE 21]
  say 707 SAD "Laughing was against the law / here. So I swallowed them." [HEART]
  [HOLD 13]
  say 707 SAD "They came out as ghosts. {..}She / taught them to throw parties."
  reward 80 furn=jar_of_sneezes
  befriend 707 4
  quest S132 done
  later 6m s132_later
end

script s132_later
  sfx SFX_CONFETTI
  say nar "(Little ghosts are having a / party in your Hell Pits / flat. You didn't throw it.)"
end

# =============================================================================
# S133 "Bubbles Bounces Back" - Dr. Bloom (111) + Sprinklepop's soul (81)
# Ch4-5 - escort, exorcism (reverse)
# =============================================================================
@pack quests kind=side
@mission S133 giver=111 side=hill ch=4-5 after=BEAT_EP4_2 at=17,23 verbs=escort,exorcism chain=I step=4 later=7m reward=150 offer=s133_offer active=s133_again title="Bubbles Bounces Back" trophy="Unfailed" hint="Meet the soul at the Wall gate; walk her to the clinic."
@pack sa_sprk kind=side

string S133_ASK "Her name. From memory:"
string S133_ANS "SPRINKLEPOP"

script s133_offer
  say bloom NEUTRAL "A soul going home is an / exorcism run backwards."
  [R8]
  say fuzz NEUTRAL "I've never done one. {.}The / scroll says 'probably'."
  choice bloom "Bring the soul home?" { "Let's un-fail it." -> yes ; "Not yet." }
  end
yes:
  quest S133 active
  call s133_again
end

script s133_again
  say nar "(The soul is waiting at the / Wall gate. {.}It bounces.)"
  verb find cell=54,30 then=s133_gate
end

script s133_gate
  sfx SFX_BOING
  say nar "(The soul bounces ahead. / Keep it in sight, or it / starts a party.)"
  verb escort sprinklepop cell=17,23 then=s133_clinic
end

script s133_clinic
  jlt v1 1 lost
  say fuzz NEUTRAL "Metronome in reverse. {.}The / bell will un-ring. {..}That / used to be scary."
  possess sprinklepop 10
  push exorcism 81
  jlt v0 1 failed
  shake SHK_STAMP
  sfx SFX_KAZOO
  say nar "(The stamp: EXORCISM / UNFAILED x1. A kazoo, played / backwards: 'wahhh wah wah.')"
  [R13]
  sfx SFX_SPARKLE
  say nar "(She wakes. Pink glitter.)"
  [R8]
  say sprinklepop NEUTRAL "{..}What's my name again?"
  stash 0 S133_ASK
  stash 1 S133_ANS
  push minigame STORY_MG_TEXT_ENTRY
  jlt v0 1 forgot
  say sprinklepop HAPPY "THAT'S IT! {!}You remembered!"
  jmp dream
forgot:
  say sprinklepop HAPPY "Close! It's Sprinklepop. {.}I / forgive you. I was asleep."
dream:
  [R13]
  say sprinklepop HAPPY "I had the best dream. / Everyone was invited."
  [R13]
  [MUTE 13]
  say sprinklepop SAD "...Was I?" [HEART]
  [HOLD 13]
  shake SHK_POW
  say nar "{big}YES.{/}"
  [R8]
  say nar "(The whole clinic said it.)"
  reward 150 furn=heart_balloon
  befriend sprinklepop 4
  befriend bloom 3
  befriend fuzz 2
  quest S133 done
  later 7m s133_later
  end
failed:
  say fuzz SAD "The bell rang the right way. / {..}That's the wrong way. Again."
  end
lost:
  say bloom SAD "She bounced off to a party. / {.}Ask me again; she'll be back / at the gate."
end

script s133_later
  sfx SFX_BOING
  say nar "(A heart balloon bobs past / your window. Someone under / it is bouncing.)"
end
