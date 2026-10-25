# side_a/f_fuzz.dsc - Chain E "Father Fuzz and the Mr. Huggins succession"
# (S117 S118 S201 S119 S120) and Chain F "Lilypad Lou and the two-way mail"
# (S121-S123) (owner: S4; story_final.md sec.14.1). Sutura is xe. Nobody is
# practice.

# =============================================================================
# S117 "Buckwheat Run" - Father Fuzz (77) - Ch2 after Ep 2.4 - fetch, talk
# =============================================================================
@pack quests kind=side
@mission S117 giver=77 side=hill ch=2-2 after=BEAT_EP2_4 at=7,28 verbs=fetch,timing chain=E step=1 later=5m reward=30 offer=s117_offer active=s117_again title="Buckwheat Run" hint="Farmer Bram only talks to his carrots. Get Fuzz two sacks of buckwheat."
@pack sa_fuzz kind=side minutes=95

script s117_offer
  say fuzz NEUTRAL "Mr. Huggins the 3rd needs / stuffing. {.}Love he has."
  [R8]
  say fuzz SAD "Buckwheat he hasn't. {..}Farmer / Bram has it. He only talks / to his carrots."
  choice fuzz "Fetch the buckwheat?" { "I'll talk carrot." -> yes ; "Later." }
  end
yes:
  quest S117 active
  call s117_again
end

script s117_again
  say nar "(Farmer Bram is in the field / by the flower beds, talking / to a carrot.)"
  verb deliver 120 cell=43,48 then=s117_bram
end

script s117_bram
  say 120 NEUTRAL "..."
  choice nar "Compliment the carrot:" { "What a lovely orange." ; "Strong roots." ; "Nice leaves." }
  sfx SFX_LOVE
  say nar "(The carrot blushes.)"
  [R8]
  say 120 HAPPY "Two sacks. {.}For the carrot's / sake."
  say nar "(Back at the church, Fuzz / threads a needle.)"
  push timing pattern=three skin=heart
  sfx SFX_POP2
  say nar "(Mr. Huggins the 3rd sits up.)"
  [R8]
  say nar "(Fuzz hugs him.)"
  [R5]
  shake SHK_SLAM
  sfx SFX_THUD
  say nar "(His head falls off.)"
  [R5]
  sfx SFX_TIK
  [R5]
  sfx SFX_TIK
  [R5]
  sfx SFX_TIK
  say nar "(It rolls three times and / stops facing the Wall. As / the 2nd's did.)"
  [HOLD 13]
  say fuzz NEUTRAL "They all do that."
  [R13]
  say fuzz NEUTRAL "I've stopped asking."
  [R21]
  [MUTE 21]
  say fuzz SAD "I keep every one of them in / a drawer. {..}Nobody is / practice." [HEART]
  [HOLD 13]
  say fuzz SAD "The 1st, the 2nd... {..}I know / all their names."
  reward 30 furn=huggins_marker
  befriend fuzz 3
  quest S117 done
  later 5m s117_later
end

script s117_later
  say nar "(The church has a drawer that / is never quite closed.)"
end

# =============================================================================
# S118 "Holy Water, Hold the Water" - Father Fuzz (77) + Sr. Lumen (354)
# Ch3 (before the beach nest) - fetch, exorcism (practice), timing
# =============================================================================
@pack quests kind=side
@mission S118 giver=77 side=hill ch=3-3 after=BEAT_EP3_1 at=7,28 verbs=find,exorcism,timing chain=E step=2 later=6m reward=50 offer=s118_offer active=s118_again title="Holy Water, Hold the Water" hint="Fetch water from the bath house; Fuzz needs someone to keep the beat."
@pack sa_fuzz kind=side

script s118_offer
  say 354 NEUTRAL "The holy water ran out / mid-invasion."
  [R8]
  say 354 NEUTRAL "The scrolls say any water / will do. {.}If blessed by / someone who means it."
  [HOLD 13]
  say fuzz SAD "{..}I'm not sure I mean it / anymore."
  choice fuzz "Help him mean it?" { "I'll fetch water." -> yes ; "Not yet." }
  end
yes:
  quest S118 active
  call s118_again
end

script s118_again
  say nar "(The bath house. Its tub is / the only clean water left.)"
  verb find cell=24,44 then=s118_water
end

script s118_water
  sfx SFX_SPLASH
  jif SF_STEAMY_LOST ghost
  say steamy NEUTRAL "Take all you need. {.}Clean is a / kind of brave."
  jmp bless
ghost:
  say nar "(The tub fills itself. The / water comes out warm.)"
bless:
  [R13]
  say nar "(Back in the church. The / metronome is muted. You keep / the beat.)"
  push timing pattern=prayer skin=bell
  sfx SFX_HOLY
  say nar "(The blessed water giggles.)"
  [R8]
  say fuzz NEUTRAL "That's fine. {.}Is that fine?"
  [R8]
  say 354 NEUTRAL "{d}The scroll says 'probably'."
  [R13]
  say nar "(The test: Mr. Huggins the / 4th.)"
  say fuzz SAD "Eleven years ago I let go too / early." [HEART]
  [HOLD 13]
  say fuzz SAD "{..}Hold the beat for me."
  possess -1 8
  push exo_practice
  sfx SFX_SQUEAK
  say nar "(Mr. Huggins the 4th / survives two parts. On the / third he loses an ear.)"
  [R13]
  say fuzz HAPPY "{..}That's a win."
  reward 50 ITEM_HOLY_WATER 3
  befriend fuzz 3
  befriend 354 2
  quest S118 done
  later 6m s118_later
end

script s118_later
  say nar "(Mr. Huggins the 4th sits on / the altar. One ear. {.}Very / proud.)"
end

# =============================================================================
# S201 "Buckwheat" - Father Fuzz (77) - Ch3 (after the beach nest) - fetch, deliver
# =============================================================================
@pack quests kind=side
@mission S201 giver=77 side=hill ch=3-3 after=BEAT_EP3_3 at=7,28 verbs=fetch,deliver chain=E step=3 later=5m reward=40 offer=s201_offer active=s201_again title="Buckwheat" hint="Get two sacks of buckwheat from Nana for Fuzz's 'recipe'."
@pack sa_fuzz kind=side

script s201_offer
  sfx SFX_SQUEAK
  say nar "(Fuzz is leaking. A little / trail of stuffing follows / him down the aisle.)"
  [R8]
  say fuzz NEUTRAL "Two buckwheat. {.}For a / recipe."
  [HOLD 13]
  say fuzz NEUTRAL "{..}A recipe."
  choice fuzz "Get the buckwheat?" { "For the recipe." -> yes ; "Later." }
  end
yes:
  quest S201 active
  call s201_again
end

script s201_again
  say nar "(Nana has two of everything.)"
  verb deliver nana cell=25,17 then=s201_nana
end

script s201_nana
  say nana HAPPY "Two? {.}Take three."
  [R8]
  say nana NEUTRAL "Tell him it's gluten-free. / {..}He worries."
  verb deliver fuzz cell=8,28 then=s201_fuzz
end

script s201_fuzz
  say nar "(He disappears behind the / altar.)"
  [R21]
  sfx SFX_POP2
  say nar "(He comes out rounder.)"
  [R8]
  say fuzz HAPPY "...Recipe went well."
  [R8]
  say nar "(The kids hug him. A pin / pings off.)"
  sfx SFX_TINK
  [R21]
  [MUTE 13]
  say fuzz SAD "Don't tell the bishop. {.}Or / Nox. Or anyone." [HEART]
  [HOLD 13]
  say fuzz SAD "{..}That faith needed a / top-up."
  reward 40 furn=buckwheat_sack
  befriend fuzz 3
  quest S201 done
  later 5m s201_later
end

script s201_later
  sfx SFX_BELL
  say nar "(Eight o'clock. Fuzz rings / the bedtime bell. {.}Every / night now.)"
end

# =============================================================================
# S119 "The Plush Surgeon" - Sutura (542, Hellvile night nurse) - Ch4
# deliver, fetch, housing
# =============================================================================
@pack quests kind=side
@mission S119 giver=542 side=hell ch=4-5 after=BEAT_EP4_2 at=24,36 verbs=deliver,timing chain=E step=4 later=6m reward=100 offer=s119_offer active=s119_again title="The Plush Surgeon" hint="Bring Sutura Father Fuzz's drawer of Mr. Huggins heads."
@pack sa_fuzz kind=side

script s119_offer
  say 542 NEUTRAL "I stitch up crying kids' / plushies. Free. At night."
  [R8]
  say 542 ANGRY "I hear of a priest who / destroys one a week. {..}I'd / like a word."
  choice 542 "Bring xem the drawer?" { "He means well." -> yes ; "Later." }
  end
yes:
  quest S119 active
  call s119_again
end

script s119_again
  say nar "(Father Fuzz keeps a drawer. / Ask him for it.)"
  verb deliver fuzz cell=7,29 then=s119_drawer   # map=hill
end

script s119_drawer
  sfx SFX_DOOR
  say fuzz SAD "Be gentle with them."
  [R8]
  say nar "(The drawer is heavy. It / rattles. {..}The heads mutter.)"
  verb deliver 542 fragile then=s119_mend
end

script s119_mend
  [NIGHT]
  say nar "(Sutura works all night.)"
  push timing pattern=three skin=heart
  [DAY 0]
  say nar "(In the morning: Mr. Huggins / the 1st through the 8th. In / a row. Repaired.)"
  [HOLD 13]
  say nar "(Fuzz cannot speak.)"
  [HOLD 26]
  sfx SFX_CREAK
  say nar "(All eight heads turn to face / the Wall. At once.)"
  [R8]
  say nar "(The Wall is gone. They turn / back. Confused.)"
  [R8]
  say 542 NEUTRAL "Muscle memory."
  [R21]
  [MUTE 21]
  say 542 SAD "Nobody's practice. {..}That's / why I don't charge." [HEART]
  [HOLD 21]
  say nar "(Fuzz has said the same / thing for ten years. To an / empty church.)"
  reward 100 furn=huggins_choir
  befriend 542 4
  befriend fuzz 3
  quest S119 done
  later 6m s119_later
end

script s119_later
  sfx SFX_CHOIR
  say nar "(At night the Huggins Choir / in your house hums the / church hymn. Slightly flat.)"
end

# =============================================================================
# S120 "Mr. Huggins the 9th" - Father Fuzz (77) - Ch5 - exorcism, talk
# =============================================================================
@pack quests kind=side
@mission S120 giver=77 side=hill ch=5-5 at=7,28 verbs=exorcism chain=E step=5 later=6m reward=150 offer=s120_offer active=s120_again title="Mr. Huggins the 9th" trophy="Nobody Is Practice" hint="One more practice rite, in public. With Sutura's stitches."
@pack sa_fuzz kind=side

script s120_offer
  say fuzz NEUTRAL "One more practice. {.}In / public. With Sutura's / stitching."
  [HOLD 13]
  say fuzz SAD "If this one survives, I'll / believe it."
  choice fuzz "Gather a crowd?" { "Everyone's coming." -> yes ; "Later." }
  end
yes:
  quest S120 active
  call s120_again
end

script s120_again
  say nar "(Five festival-goers. Each / has a reason not to come. / Each comes.)"
  npc fuzz RIPPLE 6
  possess -1 9
  push exo_practice
  say nar "(The crowd flinches at every / part.)"
  [MUTE 21]
  [HOLD 21]
  sfx SFX_KAZOO
  say nar "(Honkers honks.)"
  [R3]
  shake SHK_POW
  say nar "{big}EVERYBODY SCREAMS.{/}"
  [R13]
  say nar "(Mr. Huggins the 9th is fine.)"
  [R8]
  say nar "(He hugs Fuzz.)"
  [HOLD 26]
  say fuzz HAPPY "Oh." [HEART]
  [R13]
  say fuzz HAPPY "{..}Oh, that's what it feels / like."
  [R8]
  say smog HAPPY "Told you it was lovely."
  reward 150 furn=huggins_9th
  befriend fuzz 4
  quest S120 done
  later 6m s120_later
end

script s120_later
  say nar "(Mr. Huggins the 1st's grave / has a second line: CAME / BACK. HUGGED A PRIEST.)"
end

# =============================================================================
# S121 "Return to Sender" - Lilypad Lou (97) - Ch1-2 - find, deliver
# =============================================================================
@pack quests kind=side
@mission S121 giver=97 side=hill ch=1-2 after=BEAT_EP1_3 at=20,20 verbs=find,deliver chain=F step=1 later=6m reward=30 offer=s121_offer active=s121_again title="Return to Sender" hint="Bring Lou the junk mail 'from The Other Side' in your mailbox."
@pack sa_fuzz kind=side

script s121_offer
  sfx SFX_SPLASH
  say 97 ANGRY "Your mailbox keeps getting / junk mail. {.}From 'The Other / Side'."
  [R8]
  say 97 NEUTRAL "I want it back. {..}Undeliverable / mail is a personal failure."
  choice 97 "Fetch the junk mail?" { "Ribbit." -> yes ; "Later." }
  end
yes:
  quest S121 active
  call s121_again
end

script s121_again
  say nar "(Your mailbox. By the pink / door.)"
  verb find cell=5,8 then=s121_mail
end

script s121_mail
  sfx SFX_PICKUP
  say nar "(Five letters. 'Cloud Nine / Vapor: opening soon!' A tax / notice from Taxfang.)"
  say nar "(A chain letter. It says 'we' / a lot.)"
  verb deliver 97 then=s121_wall
end

script s121_wall
  say nar "(Lou tries to return them to / the Wall. {.}It has no slot.)"
  [R8]
  say nar "(She posts them through a / crack.)"
  [HOLD 21]
  sfx SFX_THUD
  say nar "(Something posts one back.)"
  [R13]
  say nar "(It's addressed to THE SNACK, / PINK DOOR COTTAGE.)"
  [HOLD 13]
  say 97 NEUTRAL "Well. {.}It's a valid address."
  [MUTE 14]
  [HOLD 14]
  say nar "(Inside: a single tally mark.)"
  reward 30 furn=cursed_coupon
  befriend 97 3
  quest S121 done
  later 6m s121_later
end

script s121_later
  say nar "(Your mailbox has a second / tally mark in it. {..}Nobody / delivered it.)"
end

# =============================================================================
# S122 "Dead Letter Office" - Lilypad Lou (97) - Ch3 - find, combat, deliver
# =============================================================================
@pack quests kind=side
@mission S122 giver=97 side=hill ch=3-3 after=BEAT_EP3_1 at=20,20 verbs=combat,deliver chain=F step=2 later=6m reward=60 offer=s122_offer active=s122_again title="Dead Letter Office" hint="The mail sack went down the chute to the Sugar Rush Sewers. Get it."
@pack sa_fuzz kind=side

string S122_C1 "The mail sack is down here somewhere."
string S122_C2 "A demon is using it as a pillow."

script s122_offer
  say 97 SHOCK "The post office is a nest! / {!}And under it, a sack I / didn't know existed."
  [R8]
  say 97 SAD "Letters from Hillvile to / Hellvile. {..}Ten years old. / Never sent."
  choice 97 "Rescue the mail sack?" { "The mail must go!" -> yes ; "Not yet." }
  end
yes:
  quest S122 active
  call s122_again
end

script s122_again
  say nar "(The mail chute drops straight / into the Sugar Rush Sewers.)"
  push combat MAPID_SUGAR_RUSH_SEWERS captions=S122_C1,S122_C2
  jlt v0 1 lost
  say nar "(The demon on the sack does / not want to fight. It offers / a letter as a bribe.)"
  [R8]
  say nar "(Lou accepts the letter. {.}And / the fight.)"
  verb deliver 97 then=s122_read
  end
lost:
  say 97 SAD "The mail will wait. {..}It's / very good at waiting."
end

script s122_read
  say 97 NEUTRAL "Nana to Charlotte. Giggs to / Nox: ninety of those. Fuzz / to Smog."
  [R8]
  say 97 NEUTRAL "Grampy to 'the mouse who / whistles'."
  [HOLD 13]
  say 97 SAD "{..}And one in my handwriting. / To a frog called Warty."
  [MUTE 21]
  [HOLD 21]
  say 97 SAD "...I was eleven." [HEART]
  [R13]
  say 97 NEUTRAL "I'm not reading that one out."
  [R8]
  say 97 HAPPY "I'll deliver them. All of / them. {.}When the Wall opens."
  reward 60 furn=mail_sack
  befriend 97 3
  quest S122 done
  later 6m s122_later
end

script s122_later
  say nar "(The mail sack in your house / smells of ten years. {.}And / faintly of lilies.)"
end

# =============================================================================
# S123 "First Post" - Mandible (528, Hellvile) + Lilypad Lou (97) - Ch4
# deliver, talk, escort
# =============================================================================
@pack quests kind=side
@mission S123 giver=528 side=hell ch=4-5 after=BEAT_EP4_2 at=22,14 verbs=escort,deliver chain=F step=3 later=7m reward=100 offer=s123_offer active=s123_again title="First Post" trophy="Mail Carrier" hint="Walk Lou and her sack to Mother Nox's chapel. Mandible keeps time."
@pack sa_fuzz kind=side

script s123_offer
  sfx SFX_TIK
  say 528 ANGRY "Letters. From Hillvile. / {.}Ten years late."
  [R8]
  sfx SFX_TIK
  say 528 NEUTRAL "I clack at latecomers. {..}I / will be clacking a lot."
  choice 528 "Help Lou deliver?" { "Clack away." -> yes ; "Later." }
  end
yes:
  quest S123 active
  call s123_again
end

script s123_again
  say 97 HAPPY "The first stop is Mother Nox. / {.}Stay with me!"
  verb escort 97 cell=8,14 then=s123_rounds
end

script s123_rounds
  jlt v1 1 lost
  sfx SFX_TIK
  say nar "(Six deliveries. Mandible / clacks at every one.)"
  sfx SFX_TIK
  [R5]
  sfx SFX_TIK
  [R3]
  sfx SFX_TIK
  [R2]
  sfx SFX_TIK
  [R2]
  sfx SFX_TINK
  say 97 HAPPY "{big}ON TIME.{/}"
  [HOLD 13]
  say 528 NEUTRAL "{d}...Acceptable."
  [R13]
  say nar "(Nox reads one of Giggs's / ninety. She puts the other / eighty-nine in a drawer.)"
  [R8]
  say nox NEUTRAL "For later."
  [R21]
  [MUTE 26]
  say nox SAD "'Dear Nox. I'm sorry. I'm / sorry. I'm'" [HEART]
  [HOLD 21]
  say nar "(The page is wet.)"
  [HOLD 13]
  say nox SAD "He never finished one. {..}That's / why they never came."
  [HOLD 26]
  reward 100 furn=two_way_mailbox
  befriend 97 3
  befriend 528 3
  quest S123 done
  later 7m s123_later
  end
lost:
  say 528 ANGRY "LATE. {.}Again. {..}Ask Lou to / try again."
end

script s123_later
  say nar "(A letter from Warty. A song / lyric and a pickle.)"
end
