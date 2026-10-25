# side_a/i_solo.dsc - the batch-A stand-alones S143-S150 (owner: S4;
# story_final.md sec.14.2). One errand each: the hook, the task, the comic
# beat on the ladder, the heart (or scare), the reward and its Later.

# =============================================================================
# S143 "Population: 1000" - Mayor Giggs (0) - Ch5 - fetch, minigame
# =============================================================================
@pack quests kind=side
@mission S143 giver=0 side=hill ch=5-5 at=29,8 verbs=fetch,minigame later=7m reward=100 offer=s143_offer active=s143_again title="Population: 1000" trophy="Census" hint="Bring the Mayor pink from Rainbow Ewe and red from Stencilla, over the gate."
@pack sa_solo1 kind=side minutes=60

script s143_offer
  say mayor NEUTRAL "The town sign still says / 500. {.}Under it, scratched / out, it says 1000."
  [R8]
  say mayor HAPPY "I want to repaint it myself! / {..}I've never held a roller."
  choice mayor "Fetch him the paint?" { "Two colours, coming up." -> yes ; "Later." }
  end
yes:
  quest S143 active
  call s143_again
end

script s143_again
  say mayor NEUTRAL "Rainbow Ewe has the pink. / Stencilla, over the gate, / has the red."
  verb deliver 28 cell=33,18 then=s143_ewe
end

script s143_ewe
  sfx SFX_PICKUP
  say 28 HAPPY "The pinkest pink! {.}Don't let / him lick it."
  verb deliver 565 cell=11,24 then=s143_red   # map=hell
end

script s143_red
  sfx SFX_PICKUP
  say 565 NEUTRAL "Hellvile red. It doesn't come / off. {..}That's the point."
  verb deliver mayor cell=29,8 then=s143_paint
end

script s143_paint
  say mayor NEUTRAL "Here goes."
  sfx SFX_SPLAT
  say nar "(He paints the 1. Then he / stops.)"
  [MUTE 21]
  [HOLD 21]
  say mayor SAD "I painted over this myself. / Ten years ago. {..}With this / roller." [HEART]
  [HOLD 13]
  say mayor NEUTRAL "{..}Help me finish it?"
  push minigame SMG_A_ROLLER
  sfx SFX_FANFARE
  say mayor HAPPY "POPULATION: 1000 lovely / citizens. {.}And @P."
  [R8]
  sfx SFX_TINK
  say nar "(The last 0 falls off. Tink.)"
  [R13]
  say rivet NEUTRAL "Don't make it weird."
  sfx SFX_WELD
  say nar "(Rivet glues it back on.)"
  [R8]
  sfx SFX_TINK
  say nar "(It falls off again.)"
  [R13]
  say tallyworn NEUTRAL "{d}999. I can live with 999."
  reward 100 furn=town_sign_mini
  befriend mayor 4
  befriend 28 2
  befriend 565 2
  quest S143 done
  later 7m s143_later
end

script s143_later
  say nar "(The sign at the edge of / town reads POPULATION: 1000. / {.}The last 0 is taped on.)"
end

# =============================================================================
# S144 "Fishing for Compliments" - Capt. Whisk (48) - Ch1-5 - fishing, talk
# (absorbs heart S65 as its Ch2 variant)
# =============================================================================
@pack quests kind=side
@mission S144 giver=48 side=hill ch=1-5 after=BEAT_EP1_3 at=40,50 verbs=fishing later=6m reward=0 offer=s144_offer active=s144_again title="Fishing for Compliments" hint="Fish off the end of the pier until the Captain's tagged fish bites."
@pack sa_solo1 kind=side

script s144_offer
  sfx SFX_SPLASH
  say 48 HAPPY "I catch 'em, I name 'em, I / let 'em go. {.}Captain's / rules."
  [R8]
  say 48 SAD "One keeps coming back. {..}I've / run out of names."
  choice 48 "Fish with the Captain?" { "Cast off!" -> yes ; "Later." }
  end
yes:
  quest S144 active
  call s144_again
end

script s144_again
  say 48 NEUTRAL "End of the pier. {.}Watch the / float. Pull when it dips."
  verb find cell=38,57 then=s144_cast
end

script s144_cast
  native CB_SIDE_CHAPTER
  jlt v0 2 gerald
  jge v0 3 gerald
  push minigame SMG_A_BOBBER
  say nar "(A fish. It tastes of grape. / {.}Don't ask how you know.)"
  [R13]
  say nar "(Another. On the line, it / turns to face the Wall.)"
  [R13]
  [R8]
  say nar "(The third is purple. {..}It / hums a lullaby.)"
  [HOLD 13]
  say 48 SAD "...Let that one go, matey."
  [HOLD 13]
gerald:
  push minigame SMG_A_BOBBER
  jlt v0 1 lost
  sfx SFX_SPLASH
  say nar "(The tagged one. Of course.)"
  [R8]
  say 48 NEUTRAL "Gerald. Gerald II. Gerry. G. / Fish. {.}Forty names, all / crossed out."
  [R13]
  say 48 NEUTRAL "The last one says 'Please / Stop'. {..}He wrote that one / himself."
  choice nar "Name the fish:" { "Gerald Forever" ; "Steve" ; "Mr. Fish" ; "Please Stop II" }
  [R13]
  sfx SFX_SPLASH
  say nar "(You let it go.)"
  [R21]
  [MUTE 13]
  say 48 SAD "You let something go forty / times, and it comes back / forty times." [HEART]
  [HOLD 13]
  say 48 HAPPY "{..}That's not fishing. That's / a friend."
  reward 0 furn=fish_trophy
  befriend 48 4
  quest S144 done
  later 6m s144_later
  end
lost:
  say 48 NEUTRAL "He got away. {.}He'll be back. / He always is."
end

script s144_later
  sfx SFX_SPLASH
  say nar "(A fish waits by the pier. / The tag has a new name on / it: @P.)"
end

# =============================================================================
# S145 "Bedtime for Everyone" - Story Owl (16) - Ch1-2 - talk, dream, housing
# =============================================================================
@pack quests kind=side
@mission S145 giver=16 side=hill ch=1-2 after=BEAT_EP1_3 at=40,36 verbs=housing,pet later=6m reward=20 offer=s145_offer active=s145_again title="Bedtime for Everyone" hint="Put your pet's bed by the window for Story Owl's bedtime story."
@pack sa_solo1 kind=side

script s145_offer
  say 16 NEUTRAL "I read to the whole street / at once. {.}Tonight: 'The Very / Hungry Demon'."
  [R8]
  say 16 HAPPY "I test every book on a pet / first. {..}Yours will do / nicely."
  choice 16 "Lend her @E?" { "Story time!" -> yes ; "Later." }
  end
yes:
  quest S145 active
  call s145_again
end

script s145_again
  say 16 NEUTRAL "First, the pet's bed. By the / window, so it can hear me."
  push home -1
  [NIGHT]
  sfx SFX_PLUCK
  say 16 NEUTRAL "'The very hungry demon ate / one apple, {.}two pears, {.}three / plums...'"
  [R13]
  say 16 NEUTRAL "'...four Hillvile.'"
  [R8]
  say 16 HAPPY "It's a classic."
  push pet
  say nar "(@E is asleep by page six.)"
  pal PAL_CRAYON 10
  sfx SFX_SAD
  say nar "(In the dream, the demon is / small. {..}It is crying.)"
  [HOLD 13]
  say nar "(It only eats its own tail.)"
  [HOLD 21]
  pal PAL_CRAYON 0
  [MUTE 21]
  say 16 SAD "Oh. {..}That's not how it ends."
  [R13]
  say 16 NEUTRAL "'And the very hungry demon / was given one cupcake, and / it was enough.'" [HEART]
  [HOLD 13]
  say 16 HAPPY "First edit in sixty years."
  reward 20 furn=very_hungry_demon
  befriend 16 3
  quest S145 done
  later 6m s145_later
end

script s145_later
  say nar "(The storybook on your shelf / has a new last page: a / cupcake, in crayon.)"
end

# =============================================================================
# S146 "Whack-a-Nothing" - Squeaker (64) - Ch2 - minigame, find
# (absorbs heart S59's "STAFF" beat)
# =============================================================================
@pack quests kind=side
@mission S146 giver=64 side=hill ch=2-2 at=6,42 verbs=minigame,find later=5m reward=30 offer=s146_offer active=s146_again title="Whack-a-Nothing" hint="Play the Whack-an-Imp, on and off. Then find where its plug goes."
@pack sa_solo1 kind=side

script s146_offer
  sfx SFX_SQUEAK
  say 64 NEUTRAL "The Whack-an-Imp pops up imps / when it's switched OFF."
  [R8]
  say 64 SHOCK "{!}That's not a feature. {.}Give / it a go?"
  choice 64 "Play Whack-an-Imp?" { "Bonk time." -> yes ; "Later." }
  end
yes:
  quest S146 active
  call s146_again
end

script s146_again
  say 64 NEUTRAL "House rule: do NOT bonk Father / Fuzz."
  push minigame SMG_A_WHACK
  jlt v0 2 fine
  shake SHK_POW
  say fuzz ANGRY "tsk."
  [R8]
  say nar "(He was not in the machine. / He was standing next to it.)"
fine:
  [R13]
  say fuzz NEUTRAL "The top prize is Mr. Huggins? / {.}No. He's not a prize."
  [R8]
  say fuzz ANGRY "He's STAFF."
  [R13]
  sfx SFX_TINK
  say 64 NEUTRAL "Now it's off. {.}Play it again."
  push minigame SMG_A_WHACK_OFF
  say nar "(The imps still came. Slower. / {..}Looking at you.)"
  [MUTE 14]
  [HOLD 14]
  sfx SFX_BEEP
  say nar "(The last one said 'beep.' / In @E's voice.)"
  [HOLD 21]
  say 64 SHOCK "Where does the plug GO?"
  verb find cell=53,41 then=s146_plug
end

script s146_plug
  say nar "(The cable runs under the / ground. Toward the Wall.)"
  [R8]
  sfx SFX_TINK
  say nar "(Squeaker pulls the plug.)"
  [R13]
  sfx SFX_POP2
  say nar "(Back at the Fun Fair, an imp / pops up anyway.)"
  [R13]
  say 64 NEUTRAL "{d}It's a very good machine."
  reward 30 furn=mini_whack_an_imp
  befriend 64 3
  quest S146 done
  later 5m s146_later
end

script s146_later
  sfx SFX_POP2
  say nar "(The little Whack-an-Imp at / home pops up an imp. {.}It is / unplugged. @E bonks it.)"
end

# =============================================================================
# S147 "Say Cheese (Again)" - Shutterbug (46) - Ch2-3 - photo, find
# =============================================================================
@pack quests kind=side
@mission S147 giver=46 side=hill ch=2-3 at=37,24 verbs=photo,find later=6m reward=50 offer=s147_offer active=s147_again title="Say Cheese (Again)" hint="Shoot Shutterbug's 'before' set of the town: your house first, the Wall last."
@pack sa_solo2 kind=side minutes=60

script s147_offer
  sfx SFX_SHWIP
  say 46 NEUTRAL "My photos show things that / weren't there. {.}So: a / 'before' set of the town."
  [R8]
  say 46 NEUTRAL "In case. {..}Of whatever."
  choice 46 "Shoot the 'before' set?" { "Say cheese." -> yes ; "Later." }
  end
yes:
  quest S147 active
  call s147_again
end

script s147_again
  say 46 NEUTRAL "Eight landmarks. {.}Start with / your own house."
  verb find cell=5,8 then=s147_house
end

script s147_house
  sfx SFX_SHWIP
  say nar "(Click. It develops: your / house. Mascotty is in the / window.)"
  [R8]
  say nar "(You were inside.)"
  [R13]
  say 46 NEUTRAL "{d}Great composition."
  verb find cell=30,20 then=s147_more
end

script s147_more
  sfx SFX_SHWIP
  say nar "(The fountain. The cafe, the / shop, the church, the pier. / Click, click, click, click.)"
  [R8]
  say nar "(Each has one wrong thing: a / spare shadow, a crown in a / cloud, a chair on a roof.)"
  [R13]
  say 46 NEUTRAL "Last one: the Wall. {.}With me / in it, for scale."
  verb photo 46 cell=53,20 then=s147_wall
end

script s147_wall
  sfx SFX_SHWIP
  say nar "(The Wall's photo develops / slowly.)"
  [MUTE 21]
  [HOLD 21]
  say nar "(A hand, pressed flat on it. / {..}From the other side.)"
  [HOLD 21]
  say 46 SAD "I'll pin them up as a board. / {.}Somebody should remember / what it looked like."
  reward 50 furn=before_board
  befriend 46 3
  quest S147 done
  later 6m s147_later
end

script s147_later
  say nar "(Shutterbug's board hangs in / the church now. The only / record of 'before'.)"
end

# =============================================================================
# S148 "Lullaby for the Wall" - Maestro Mew (3) - Ch2-3 - concert, studio, fetch
# =============================================================================
@pack quests kind=side
@mission S148 giver=3 side=hill ch=2-3 at=27,36 verbs=find,concert later=6m reward=40 offer=s148_offer active=s148_again title="Lullaby for the Wall" hint="Fetch the church bell, write four bars with Mew, play them with Coo."
@pack sa_solo2 kind=side

script s148_offer
  sfx SFX_BELL
  say 3 NEUTRAL "The bells un-ring. {.}Always on / the same note. A low E."
  [R8]
  say 3 HAPPY "I want a piece that rings / them the right way round. / {..}Write it with me?"
  choice 3 "Write a lullaby?" { "Let's compose." -> yes ; "Later." }
  end
yes:
  quest S148 active
  call s148_again
end

script s148_again
  say 3 NEUTRAL "Bring me the church's hand / bell. {.}It knows the note."
  verb find cell=9,29 then=s148_bell
end

script s148_bell
  sfx SFX_BELL
  say 3 NEUTRAL "Four bars. Bell notes only. / {.}How does it start?"
  choice 3 "The first bar:" { "E, F sharp, E." -> kind ; "E, F, E." -> fog ; "Just E." -> fog }
kind:
  say 3 HAPPY "Yes! {!}The fog's tune, turned / kind."
  jmp rest
fog:
  say 3 NEUTRAL "That's the fog's tune. {.}Sharpen / it. E, F sharp, E."
rest:
  [R8]
  say 3 NEUTRAL "Then B. A, G sharp, F sharp. / {..}And home on E."
  push concert 426
  say nar "(Cantor Coo plays it at the / speed of disco.)"
  [R8]
  say nar "(Mew conducts with his tail / at half speed. The orchestra / follows the tail.)"
  [R13]
  say cantor NEUTRAL "Rude."
  [R13]
  native CB_SIDE_ONCE SONG_WALL_LULLABY
  sfx SFX_BELL
  say nar "(The bells ring. {.}Forwards.)"
  [R8]
  sfx SFX_THUD
  say nar "(The Wall thumps. {.}On the / beat.)"
  [HOLD 13]
  say 3 HAPPY "Keep that. {.}Keep the thump / in."
  [MUTE 13]
  say 3 NEUTRAL "Someone over there has / rhythm." [HEART]
  [HOLD 13]
  say 3 HAPPY "{..}I'd like to meet the / percussion section."
  native CB_SIDE_MUSIC
  reward 40 furn=conductors_baton
  befriend 3 3
  befriend cantor 2
  quest S148 done
  later 6m s148_later
end

script s148_later
  sfx SFX_THUD
  say nar "(At night, over the Wall, / someone thumps along to the / lullaby. On the beat.)"
end

# =============================================================================
# S149 "Make Me a Face" - Phantasma (524, Hellvile) - Ch4 - creator, talk
# =============================================================================
@pack quests kind=side
@mission S149 giver=524 side=hell ch=4-5 after=BEAT_EP4_2 at=10,11 verbs=creator later=6m reward=60 offer=s149_offer active=s149_again title="Make Me a Face" hint="Make Phantasma a cute Hillvile look for the festival."
@pack sa_solo2 kind=side

script s149_offer
  say 524 NEUTRAL "Boo. {.}I'm a ghost. {..}I'm / pretending to be a ghost."
  [R8]
  say 524 SAD "I want a disguise for the / festival. A cute Hillvile / look. {.}Make me one?"
  choice 524 "Make Phantasma a face?" { "Open the creator." -> yes ; "Later." }
  end
yes:
  quest S149 active
  call s149_again
end

script s149_again
  push creator 524
  say nar "(The hat falls through them.)"
  [R13]
  say nar "(The bow falls through them.)"
  [R13]
  [R13]
  say 524 NEUTRAL "...A smaller hat."
  [R13]
  say nar "(They float to the mirror. / {..}And through it.)"
  [R8]
  sfx SFX_SHWIP
  say nar "(A polaroid, then. They hold / it very close.)"
  [HOLD 13]
  [MUTE 13]
  say 524 SAD "I pretend to be a ghost so / people aren't scared when / they find out." [HEART]
  [HOLD 13]
  say 524 HAPPY "{..}This face isn't scary."
  choice nar "Say:" { "Neither was the old one." }
  [R13]
  sfx SFX_SPLASH
  say nar "(A happy ghost rains upward.)"
  reward 60 furn=phantasma_portrait
  befriend 524 4
  quest S149 done
  later 6m s149_later
end

script s149_later
  say nar "(Phantasma hosts the / festival's 'fake seance'. It / is a real one. It's fine.)"
end

# =============================================================================
# S150 "Hellvile Hopscotch" - Tantrum (530, Hellvile, kid) - Ch4-5
# minigame, pet
# =============================================================================
@pack quests kind=side
@mission S150 giver=530 side=hell ch=4-5 after=BEAT_EP4_2 at=20,41 verbs=minigame,pet later=6m reward=30 offer=s150_offer active=s150_again title="Hellvile Hopscotch" hint="Play Tantrum at lava hopscotch. Xe wants to lose properly."
@pack sa_solo2 kind=side

script s150_offer
  say 530 ANGRY "Hellvile kids play hopscotch / on lava. {.}In oven mitts."
  [R8]
  say 530 ANGRY "I want a Hillvile kid to lose / to. {.}PROPERLY."
  choice 530 "Play lava hopscotch?" { "You're on." -> yes ; "Later." }
  end
yes:
  quest S150 active
  call s150_again
end

script s150_again
  say nar "(Ten lava squares. @E goes / first.)"
  sfx SFX_SPLAT
  [R8]
  say nar "(Sizzle. {..}@E is fine.)"
  push minigame SMG_A_HOPSCOTCH
  jlt v0 1 lost
  say 530 SHOCK "You WON?!"
  jmp stamp
lost:
  say 530 HAPPY "HA! You lost! {.}PROPERLY!"
stamp:
  [R8]
  shake SHK_TAP
  sfx SFX_THUD
  [R5]
  shake SHK_TAP
  sfx SFX_THUD
  say 530 ANGRY "{d}hmph."
  [R13]
  say nar "(Then xe offers you a mitt.)"
  [R8]
  sfx SFX_SQUEAK
  say nar "(Pokefork pokes you with a / foam pitchfork. Giggles.)"
  [R13]
  [MUTE 13]
  say 530 SAD "Hillvile kids don't come / here." [HEART]
  choice nar "Say:" { "They do now." }
  [R13]
  sfx SFX_HAPPY
  say nar "(Pip arrives. Mr. Flops wears / two tiny oven mitts.)"
  reward 30 furn=oven_mitts
  befriend 530 3
  befriend 540 2
  befriend pip 1
  quest S150 done
  later 6m s150_later
end

script s150_later
  say nar "(Hellvile's kids chalk a new / hopscotch. Half the squares / are lava, half lemonade.)"
end
