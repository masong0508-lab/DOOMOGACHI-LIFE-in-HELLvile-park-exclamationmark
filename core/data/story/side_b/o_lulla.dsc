# side_b/o_lulla.dsc - Chain O "Miss Lulla's daycare and the Hellvile
# kids" (S157-S159) and Chain P "Prof. Fizz, Dr. Quark and Dr. Frankly"
# (S142 S161 S162) (owner: S5; story_final.md sec.14.1). Monsters are real;
# some of them are lonely; they also nap. And three scientists measure the
# wrongness until one of them builds it a flower stall.

# =============================================================================
# S157 "Naptime Monsters" - Miss Lulla (36) - Ch1-2 - talk, housing, pet
# =============================================================================
@pack quests kind=side
@mission S157 giver=36 side=hill ch=1-2 after=BEAT_EP1_4 at=30,42 verbs=housing,pet chain=O step=1 later=5m reward=20 offer=s157_offer active=s157_again title="Naptime Monsters" hint="Check under the daycare cots for Miss Lulla. Monsters aren't real."
@pack sb_kids kind=side minutes=110

script s157_offer
  say lulla NEUTRAL "The kids won't nap. {.}They / say there's a monster under / the cots."
  [R8]
  say lulla HAPPY "Monsters aren't real, of / course. {..}So you check. Then / I don't have to."
  choice lulla "Check under the cots?" { "Monster patrol!" -> yes ; "Later." }
  end
yes:
  quest S157 active
  call s157_again
end

script s157_again
  say lulla NEUTRAL "First, the cots. {.}None of / them should face the Wall / window."
  choice nar "Turn the cots:" { "Toward the toy box." -> toys ; "Toward the window." -> wall }
wall:
  sfx SFX_CREAK
  say nar "(Six small faces turn, as / one, to look at the window. / {..}You turn the cots back.)"
toys:
  [R8]
  say lulla NEUTRAL "Now under each one, please."
  sfx SFX_TIK
  say nar "(Cot one: a sock.)"
  [R13]
  sfx SFX_TIK
  say nar "(Cot two: a crayon.)"
  [R13]
  sfx SFX_TIK
  [R13]
  say nar "(Cot three: a drawing. A / crowned cloud with a speech / bubble: 'we'.)"
  [HOLD 13]
  [MUTE 14]
  say nar "(The paper is wet. {..}Nobody / has been near it.)" [PUNCH]
  [HOLD 21]
  say lulla NEUTRAL "...Juice."
  [R13]
  say lulla HAPPY "Your little @E looks sleepy. / {.}Pop it under the third cot. / Monsters hate a snorer."
  push pet
  sfx SFX_BEEP
  say nar "(@E naps under cot three. / One by one, the kids nap too.)"
  [R21]
  say lulla HAPPY "I can get any baby to nap / in under a minute. {.}Watch."
  [R8]
  say lulla HAPPY "Shh. {.}Shh. {..}Shhh..."
  time ADVANCE 30
  [HOLD 21]
  sfx SFX_TINK
  say nar "(You wake up thirty minutes / later. A blanket. {.}A gold / star on your forehead.)"
  [R13]
  say lulla HAPPY "Works on everyone."
  reward 20 furn=gold_star
  home PLACE_GIFT furn=gold_star
  befriend lulla 3
  quest S157 done
  later 5m s157_later
end

script s157_later
  sfx SFX_BEEP
  say nar "(@E has learned to nap on / command. You say 'nap'. @E / naps. {..}You say 'stop'.)"
  [R8]
  say nar "(@E does not stop.)"
end

# =============================================================================
# S158 "The Same Friend" - Miss Lulla (36) - Ch2-3 - find, deliver, dream
# =============================================================================
@pack quests kind=side
@mission S158 giver=36 side=hill ch=2-3 after=BEAT_EP2_2 at=30,42 verbs=deliver,dream chain=O step=2 later=6m reward=40 offer=s158_offer active=s158_again title="The Same Friend" hint="Ask four kids about the crowned cloud they all drew. Then peek at a dream."
@pack sb_kids kind=side

string S158_D1 "a cloud with a crown"
string S158_D2 "it listens. it listens to everyone"
string S158_D3 "nobody ever listens to ME"
string S158_D4 "(you wake up. your pillow is damp.)"

script s158_offer
  say lulla SAD "Every kid in the daycare / drew the same friend. / {.}Purple. With a crown."
  [R8]
  say lulla NEUTRAL "Who's been telling them / stories? {..}It isn't Story / Owl. I asked."
  choice lulla "Ask the kids?" { "Interview time." -> yes ; "Later." }
  end
yes:
  quest S158 active
  call s158_again
end

script s158_again
  say lulla NEUTRAL "Pip, Twinkly, Wobbles. / {.}And Homework, who isn't a / kid and drew it anyway."
  verb deliver pip cell=10,47 then=s158_pip
end

script s158_pip
  say pip NEUTRAL "It's a cloud. {.}It comes in / dreams. It listens."
  [R8]
  say pip SAD "Mr. Flops doesn't like it. / {..}He says it listens TOO / much."
  verb deliver 72 cell=8,46 then=s158_twinkly
end

script s158_twinkly
  say 72 HAPPY "The crown cloud! {.}It asked / me what I was scared of. / Nobody ever asks."
  verb deliver 127 cell=14,46 then=s158_wobbles
end

script s158_wobbles
  say 127 HAPPY "Cloud. {.}Big cloud. {..}Sad."
  [R8]
  say nar "(Wobbles is two. That was / his whole statement.)"
  verb deliver 40 cell=21,45 then=s158_homework
end

script s158_homework
  say 40 NEUTRAL "I'm not a kid. {.}I drew a / normal cloud. With a hat."
  [R8]
  say lulla NEUTRAL "{d}The hat has points."
  [R8]
  say 40 NEUTRAL "It's a fashion cloud."
  [R21]
  say lulla SAD "Every one of them dreamed / it. {.}Peek at a dream with / me? Yours, tonight."
  [NIGHT]
  stash 0 S158_D1
  stash 1 S158_D2
  stash 2 S158_D3
  stash 3 S158_D4
  push dream 0x4001
  [MUTE 26]
  [HOLD 21]
  say lulla SAD "Monsters are real." [HEART]
  [HOLD 13]
  say lulla SAD "{..}Some of them are lonely."
  [HOLD 21]
  say nar "(Next morning she pins every / drawing up in the church. / A wall of crowned clouds.)"
  [HOLD 13]
  say nar "(The Mayor sees it, and sits / down.)"
  [NO BUTTON]
  [HOLD 26]
  reward 40 furn=crowned_cloud_drawing
  home PLACE_GIFT furn=crowned_cloud_drawing
  befriend lulla 3
  befriend pip 1
  quest S158 done
  later 6m s158_later
end

script s158_later
  say nar "(The cloud drawing on your / wall has a new crayon line. / A tear. {..}Or rain.)"
end

# =============================================================================
# S159 "Field Trip" - Miss Lulla (36) + the Hellvile kids - Ch4-5
# escort, minigame (the nap-off) - absorbs heart S125
# =============================================================================
@pack quests kind=side
@mission S159 giver=36 side=hill ch=4-5 after=BEAT_EP4_2 at=30,42 verbs=escort,minigame chain=O step=3 later=6m reward=100 offer=s159_offer active=s159_again title="Field Trip" trophy="Under a Minute" hint="Walk Miss Lulla's bus to the gate, then win the nap-off in Nox's chapel."
@pack sb_kids kind=side

script s159_offer
  say lulla HAPPY "The first joint nap time! / {!}Hillvile kids to Hellvile. / One pink bus."
  [R8]
  say lulla NEUTRAL "I have never lost a child. / {..}I would like to keep it / that way."
  choice lulla "Ride the bus?" { "All aboard!" -> yes ; "Later." }
  end
yes:
  quest S159 active
  call s159_again
end

script s159_again
  say lulla NEUTRAL "Walk with me to the gate. / {.}Slowly. We count heads at / every stop."
  verb escort lulla cell=53,30 then=s159_count
end

script s159_count
  jlt v1 1 lost
  say lulla NEUTRAL "Head count."
  sfx SFX_TIK
  say nar "Twelve."
  [R13]
  sfx SFX_TIK
  say nar "Eleven."
  [R8]
  say nar "(Pokefork is behind the bus. / Pokefork is always behind / the bus.)"
  [R13]
  sfx SFX_TIK
  [R13]
  say nar "Thirteen."
  [HOLD 13]
  sfx SFX_BEEP
  say nar "(The thirteenth is @E. {.}In a / tiny bonnet.)" [PUNCH]
  [R21]
  teleport MAPID_HELLVILE 7 14
  say nar "(Nox's chapel. Nap mats in / rows. Twenty-four kids, both / sides. Nobody is sleepy.)"
  [R13]
  say 530 ANGRY "We can't sleep. {.}There's no / monster under the mat."
  [R8]
  say 540 NEUTRAL "Hellvile mats always have a / monster. It's how you know / it's a mat."
  [HOLD 13]
  jif SF_GRUMBLE_DEAD snarl
  say grumble NEUTRAL "...Fine. {.}Fine! I'll do it."
  [R8]
  say nar "(Grumbleguts lies down under / three mats at once. He is / asleep first.)"
  [R13]
  sfx SFX_THUD
  say nar "(The belly snores in a / different key from the rest / of him.)"
  [R21]
  jmp napoff
snarl:
  say 855 NEUTRAL "...Fine. {.}Fine! I'll do it."
  [R8]
  say nar "(Big Snarl lies down under / three mats at once. He is / asleep first.)"
  [R21]
napoff:
  say lulla NEUTRAL "Now the nap-off. {.}Dozy Doe / against Batty Betty. Breathe / on the beat."
  say 34 SLEEPY "Two hundred citizens trained. / {..}In. And out."
  say 920 HAPPY "I nap upside down, dear. / {.}It's yoga."
  push minigame SMG_B_NAPOFF
  jif SF_GRUMBLE_DEAD bouncer
  npc grumble EMOTE EMO_ZZZ
  sfx SFX_PLUCK
  say nar "(Twenty-four kids. One / gatekeeper. Asleep in under / a minute.)"
  jmp asleep
bouncer:
  sfx SFX_PLUCK
  say nar "(Twenty-four kids. One / bouncer. Asleep in under a / minute.)"
asleep:
  [HOLD 21]
  [MUTE 26]
  say lulla HAPPY "Monsters are real." [HEART]
  [HOLD 13]
  say lulla HAPPY "{..}They also nap."
  reward 100 furn=nap_mat
  home PLACE_GIFT furn=nap_mat
  befriend lulla 3
  befriend 530 2
  befriend 540 2
  befriend 34 2
  befriend 920 2
  quest S159 done
  later 6m s159_later
  end
lost:
  say lulla SAD "We lost you at the second / stop. {.}Everyone else is here. / Shall we try again?"
end

script s159_later
  jif SF_GRUMBLE_DEAD wall
  say grumble NEUTRAL "...Is there a snack after nap / time?"
  [R13]
  say lulla HAPPY "There is. {.}I planned for it."
  [R8]
  say nar "(She hands him a plate the / size of a door. He blushes.)"
  end
wall:
  say nar "(At home, Lulla writes it on / the daycare wall: MONSTERS / ALSO NAP.)"
end

# =============================================================================
# S142 "The Fountain Is Late" - Prof. Fizz (69) - Ch1 - timing, photo
# =============================================================================
@pack quests kind=side
@mission S142 giver=69 side=hill ch=1-2 after=BEAT_EP1_3 at=50,12 verbs=timing,photo chain=P step=1 later=5m reward=20 offer=s142_offer active=s142_again title="The Fountain Is Late" hint="Press A when the fountain giggles, then photograph Fizz by it."
@pack sb_kids kind=side

script s142_offer
  sfx SFX_BEEP
  say fizz NEUTRAL "I've timed the fountain's / giggle for a week. {.}It is / late."
  [R8]
  say fizz NEUTRAL "Two hundred and sixty-seven / milliseconds. Exactly. {..}Every / time."
  [HOLD 8]
  say fizz ANGRY "Fountains aren't LATE. / Fountains are plumbing."
  choice fizz "Help time it?" { "Ready, set..." -> yes ; "Later." }
  end
yes:
  quest S142 active
  call s142_again
end

script s142_again
  say fizz NEUTRAL "Press A when it giggles. {.}Ten / times. I'll plot it."
  push timing pattern=three skin=laugh
  sfx SFX_BEEP
  say nar "(The histogram: every press / lands in one bar. {.}All of / them late. By the same.)"
  [R13]
  say fizz NEUTRAL "Now a photo. {.}Me, by the / fountain, for scale."
  verb photo fizz cell=34,26 then=s142_photo
end

script s142_photo
  sfx SFX_SPARKLE
  say nar "(The photo: the fountain's / two stone faces. {..}They are / not facing each other.)"
  [HOLD 13]
  say fizz SHOCK "Someone TURNED one."
  [R8]
  sfx SFX_BEEP
  say nar "(His stopwatch beeps at @E.)"
  [R5]
  say fizz NEUTRAL "Everything is 267 milliseconds / late today."
  [R8]
  say fizz NEUTRAL "...Except that."
  [HOLD 21]
  [MUTE 21]
  say fizz NEUTRAL "A two-faced fountain only / giggles if someone laughs on / the other side." [HEART]
  [HOLD 13]
  say fizz SAD "{..}So who's laughing?"
  [R13]
  say nar "(He writes it down. {.}He / underlines it three times.)"
  reward 20 furn=fizz_histogram
  home PLACE_GIFT furn=fizz_histogram
  befriend fizz 3
  quest S142 done
  later 5m s142_later
end

script s142_later
  sfx SFX_TINK
  say nar "(Fizz's histogram on your / wall: one very tall bar. / {.}267 ms, underlined 3x.)"
end

# =============================================================================
# S161 "Tiny Things" - Dr. Quark (24) - Ch3 - find, combat
# =============================================================================
@pack quests kind=side
@mission S161 giver=24 side=hill ch=3-3 after=BEAT_EP3_2 at=47,13 verbs=find,combat chain=P step=2 later=6m reward=60 offer=s161_offer active=s161_again title="Tiny Things" hint="Scrape vapor samples off three cleared nests for Dr. Quark, then catch a sigh."
@pack sb_kids kind=side

string S161_C1 "Catch one as it dissipates. Quark wants the sigh."
string S161_C2 "They're made of very small sighs."

script s161_offer
  say 24 NEUTRAL "The demons are made of / something. {.}Very small sighs, / I think."
  [R8]
  say 24 NEUTRAL "Samples from three nests, / please. {..}And one caught in / the act of dissipating."
  choice 24 "Collect samples?" { "Science!" -> yes ; "Later." }
  end
yes:
  quest S161 active
  call s161_again
end

script s161_again
  say 24 NEUTRAL "The nests by the Fun Fair, / the beach and the graveyard. / {.}Scrape gently."
  verb find cell=8,43 then=s161_s1
end

script s161_s1
  sfx SFX_PICKUP
  say nar "(A purple smear in a jar. / It fogs the glass from the / inside.)"
  verb find cell=13,51 then=s161_s2
end

script s161_s2
  sfx SFX_PICKUP
  say nar "(Sample two. {.}It's warm.)"
  verb find cell=10,38 then=s161_s3
end

script s161_s3
  sfx SFX_PICKUP
  say nar "(Sample three. {.}It hums, very / faintly, a lullaby.)"
  [R13]
  say 24 NEUTRAL "Now a fresh one. {.}A demon, / right as it goes."
  push combat MAPID_HAHALAND_INVADED captions=S161_C1,S161_C2
  jlt v0 1 retreat
  sfx SFX_SHWIP
  say nar "(You catch the last wisp of / one in the jar as it goes.)"
  verb deliver 24 cell=47,13 then=s161_scope
  end
retreat:
  say 24 NEUTRAL "Retreat is data too. {.}Go / again when you're ready."
end

script s161_scope
  say 24 NEUTRAL "Under the microscope, then."
  [R13]
  say nar "(Magnified, the sigh is two / tiny words in a tiny font: / 'it's fine.')"
  [R8]
  say 24 SHOCK "{d}Oh no. It's ours." [PUNCH]
  [HOLD 21]
  [MUTE 26]
  say 24 SAD "Each one is somebody's. {..}I / can't tell whose." [HEART]
  [HOLD 13]
  say 24 SAD "I've studied tiny things for / forty years. Big things are / frightening."
  [R13]
  say 24 NEUTRAL "This is the first / frightening tiny thing. {.}I'll / keep studying it."
  reward 60 furn=microscope
  home PLACE_GIFT furn=microscope
  befriend 24 3
  quest S161 done
  later 6m s161_later
end

script s161_later
  sfx SFX_BEEP
  say nar "(@E has found your / microscope. @E is looking / into it. {..}The wrong end.)"
end

# =============================================================================
# S162 "They All Want to Be Florists" - Dr. Frankly (556) + Prof. Fizz
# Ch4-5 - creator, housing
# =============================================================================
@pack quests kind=side
@mission S162 giver=556 side=hell ch=4-5 after=BEAT_EP4_2 at=45,18 verbs=creator,housing chain=P step=3 later=6m reward=100 offer=s162_offer active=s162_again title="They All Want to Be Florists" hint="Give Dr. Frankly a florist's look in the Creator, then build the monster a stall."
@pack sb_kids kind=side

script s162_offer
  say 556 HAPPY "I build monsters. {!}They come / ALIVE!"
  sfx SFX_THUD
  flash PAL_WHITE 8 3
  say nar "(Igorina pulls the big lever. / Thunder.)"
  [R8]
  say 556 SAD "And every one of them wants / to be a florist."
  [HOLD 13]
  say fizz NEUTRAL "A calibration opportunity."
  choice 556 "Help the monsters?" { "Flowers for all." -> yes ; "Later." }
  end
yes:
  quest S162 active
  call s162_again
end

script s162_again
  say 556 NEUTRAL "They're shy. {.}Do me first, / so they can see it's safe. / Apron. A flower."
  push creator 556
  sfx SFX_SPARKLE
  say nar "(Dr. Frankly, in an apron, / a daisy behind his bolt.)"
  [R8]
  say 556 HAPPY "Look at me! {.}I'm ALIVE!"
  sfx SFX_THUD
  flash PAL_WHITE 8 3
  [R5]
  sfx SFX_BEEP
  say nar "(Igorina pulls the lever. / Thunder. Fizz's detector / beeps at the thunder.)"
  [R13]
  say nar "(The monster copies the / look. It holds the daisy / like a baby bird.)"
  [R8]
  say 556 NEUTRAL "It needs a stall. Two by / two. {.}Flowers on top, a stool / behind."
  push home 0x4C04
  home DECORATE_FOR 12
  jif SF_GRUMBLE_DEAD nox
  say nar "(First customer: Grumbleguts. / He buys a dozen roses.)"
  [R8]
  say grumble LOVE "They're for the Gate. {.}Don't / tell her. I want it to be a / surprise."
  [R21]
  jmp bed
nox:
  say nar "(First customer: Mother Nox. / One rose. {..}For a very large / headstone.)"
  [R21]
bed:
  say nar "(The monster's first real / bouquet is for Sootkin's / shared flower bed.)"
  [R8]
  say nar "(It arranges them badly. / Stems up.)"
  [HOLD 13]
  [MUTE 21]
  say sootkin NEUTRAL "It's perfect." [HEART]
  [HOLD 21]
  say nar "(It is not. {..}It is perfect.)"
  reward 100 unlock=709
  befriend 556 3
  befriend fizz 2
  befriend sootkin 2
  quest S162 done
  later 6m s162_later
end

script s162_later
  say nar "(At the flower stall a sign, / in stitched letters: FLOWERS / FROM A MONSTER. NO REFUNDS.)"
  [R8]
  say nar "(Everyone gets a refund.)"
end
