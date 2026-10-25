# side_b/m_bloom.dsc - Chain M "Dr. Bloom and Graveley" (S151-S153) and
# Chain N "Hattie Haha and the Hellvile comedy scene" (S154-S156) (owner:
# S5; story_final.md sec.14.1). A ghoul in pastel scrubs who was on the
# wrong side when the Wall went up; a comedian whose act was not talking
# about things, and a clown who was never allowed to be funny on purpose.

# =============================================================================
# S151 "Cold Hands" - Dr. Bloom (111) - Ch1-2 - fetch, deliver, talk
# =============================================================================
@pack quests kind=side
@mission S151 giver=111 side=hill ch=1-2 after=BEAT_EP1_3 at=17,23 verbs=fetch,deliver chain=M step=1 later=5m reward=30 offer=s151_offer active=s151_again title="Cold Hands" hint="Fetch a hot water bottle from Lather's for Dr. Bloom. Don't run."
@pack sb_bloom kind=side minutes=110

script s151_offer
  sfx SFX_SPLASH
  say bloom NEUTRAL "Oh! {.}Sorry. I keep my hands / in warm water between / patients."
  [R8]
  say bloom HAPPY "So nobody notices. {..}Notices / that they're... chilly."
  [HOLD 8]
  say bloom SAD "Steamy has started asking / questions. {.}Could you fetch / me a hot water bottle?"
  choice bloom "Fetch the bottle?" { "Back in a jiffy." -> yes ; "Not right now." }
  end
yes:
  quest S151 active
  call s151_again
end

script s151_again
  say bloom NEUTRAL "Lather sells them. {.}By the / soap shop."
  verb deliver 41 cell=21,26 then=s151_lather
end

script s151_lather
  sfx SFX_SPLASH
  say 41 HAPPY "A hot water bottle? {.}Filled / to the brim. Don't run, it / sloshes."
  [R8]
  say 41 NEUTRAL "Who's it for? {..}...Dr. Bloom? / Huh. Third one this month."
  verb deliver bloom cell=17,23 fragile then=s151_bloom
end

script s151_bloom
  jlt v1 2 spill
  say bloom HAPPY "Perfect! Not a drop spilled. / {.}You're steadier than I am."
  jmp warm
spill:
  say bloom NEUTRAL "It's a little damp. {.}So am I, / usually. It's fine."
warm:
  [R13]
  sfx SFX_SPLASH
  say nar "(She warms her hands on it, / then puts a palm on / Zoomzoom's forehead.)"
  [R5]
  npc 8 FLEE 1
  sfx SFX_WHOOSH
  say nar "(Zoomzoom leaves the clinic / in under four seconds.)"
  [R13]
  say bloom NEUTRAL "That's not the hands. {.}That's / him."
  [HOLD 13]
  say bloom NEUTRAL "...You're going to ask. {.}Everyone / does, eventually."
  choice nar "Ask about her hands?" { "Why are they cold?" -> ask ; "You don't have to say." -> kind }
ask:
  say bloom NEUTRAL "It's a condition."
  [R8]
  say bloom NEUTRAL "...It's a family thing."
  [R13]
  say bloom SAD "...I'm a ghoul."
  jmp heart
kind:
  say bloom SAD "...Thank you. {..}It's a ghoul / thing. I'm a ghoul."
heart:
  [HOLD 21]
  [MUTE 26]
  say bloom SAD "Everyone here is lovely about / everything." [HEART]
  [HOLD 13]
  say bloom SAD "{..}I never found out if / they'd be lovely about / this."
  [NO BUTTON]
  [HOLD 21]
  reward 30 furn=hot_water_bottle
  home PLACE_GIFT furn=hot_water_bottle
  befriend bloom 3
  quest S151 done
  later 5m s151_later
end

script s151_later
  say nar "(At home, @E has found the / hot water bottle. {.}@E is / asleep on it. Belly up.)"
end

# =============================================================================
# S152 "Night Clinic" - Dr. Bloom (111) - Ch3 - escort, combat, deliver
# =============================================================================
@pack quests kind=side
@mission S152 giver=111 side=hill ch=3-3 after=BEAT_EP3_1 at=17,23 verbs=escort,combat,deliver chain=M step=2 later=6m reward=60 offer=s152_offer active=s152_again title="Night Clinic" hint="Walk Dr. Bloom through the plaza at night: three house calls, then demons."
@pack sb_bloom kind=side

string S152_C1 "Bloom holds the lantern. Her hands don't shake."
string S152_C2 "Clear the plaza. The patients are watching."

script s152_offer
  say bloom NEUTRAL "Six people by the church / need medicine. {.}I'll go / myself."
  [R8]
  say bloom HAPPY "I'd like company. {..}For the / cold."
  choice bloom "Walk Bloom's rounds?" { "Lead the way." -> yes ; "Not yet." }
  end
yes:
  quest S152 active
  call s152_again
end

script s152_again
  [NIGHT]
  sfx SFX_CREAK
  say nar "(Red night. Bloom lights a / lantern. The plaza, three / patients, no music.)"
  [MUTE 26]
  verb escort bloom cell=25,22 then=s152_oma
end

script s152_oma
  jlt v1 1 lost
  say bloom NEUTRAL "First: Oma Fluff. {.}She's lost / her glasses again."
  verb deliver 27 cell=24,24 then=s152_snug
  end
lost:
  say bloom SAD "...I lost you in the smoke. / {.}Come find me at the church."
end

script s152_snug
  say 27 HAPPY "Oh, they were on my head! / {..}They're always on my head."
  [R8]
  say bloom NEUTRAL "Second: Snugglesby. {.}He / needs a hug."
  verb deliver 60 cell=36,23 then=s152_lag
end

script s152_lag
  say 60 SAD "Bad night. {.}Hug?"
  sfx SFX_LOVE
  say nar "(He gets one. {..}He gives one / back, longer.)"
  [R8]
  say bloom NEUTRAL "Third: Lagspike. {.}She's / streaming the invasion. To / her grandma."
  verb deliver 14 cell=42,26 then=s152_fight
end

script s152_fight
  say 14 HAPPY "Hi chat! {.}This is the doctor! / She's very pale! Say hi!"
  [R8]
  say nar "CHAT (grandma): are those / demons behind you sweetie"
  [R8]
  say 14 NEUTRAL "yes grandma"
  [R8]
  say nar "CHAT (grandma): ok be safe xx"
  [R13]
  sfx SFX_DEMON_ROAR
  shake SHK_SLAM
  say bloom NEUTRAL "Behind me, then. {.}I'll hold / the light."
  push combat MAPID_HAHALAND_INVADED captions=S152_C1,S152_C2
  jlt v0 1 retreat
  say nar "(The plaza is quiet. Bloom / never once lowered the / lantern.)"
  [R13]
  verb escort bloom cell=9,28 then=s152_mayor
  end
retreat:
  say bloom NEUTRAL "Back to the church. {.}We'll / go round again."
end

script s152_mayor
  say nar "(At the church door, the / Mayor sees her clearly in / the lantern light.)"
  [HOLD 13]
  say nar "(A ghoul. In pastel scrubs.)"
  [HOLD 13]
  say mayor SHOCK "Dr. Bloom. {.}You're..."
  [R13]
  say bloom NEUTRAL "{d}Cold-handed. Yes."
  [HOLD 21]
  [MUTE 26]
  say mayor SAD "{..}...You've been here the / whole time." [HEART]
  [R13]
  say bloom SAD "Ten years."
  [HOLD 21]
  say mayor SAD "...Thank you."
  reward 60 ITEM_MEDICINE 3 furn=blooms_lantern
  befriend bloom 4
  befriend mayor 1
  quest S152 done
  later 6m s152_later
end

script s152_later
  say nar "(The church kids call her / 'Dr. Cold Hands' now.)"
  [R8]
  say nar "(They mean it kindly. {.}She / can tell.)"
end

# =============================================================================
# S153 "A Clinic With Two Doors" - Graveley (730, Hellvile) - Ch4-5
# housing, fetch, weld, talk
# =============================================================================
@pack quests kind=side
@mission S153 giver=730 side=hell ch=4-5 after=BEAT_EP4_1 at=3,33 verbs=fetch,weld,housing chain=M step=3 later=6m reward=120 offer=s153_offer active=s153_again title="A Clinic With Two Doors" trophy="Cold Hands, Warm Heart" hint="Bring Graveley 4 Scrap and 2 Medicine. Rivet welds a door; you furnish."
@pack sb_bloom kind=side

script s153_offer
  say 730 NEUTRAL "I have an empty crypt. On / the old Wall line. {.}Half on / each side."
  [R8]
  say 730 NEUTRAL "My sibling wants a clinic. / {..}With a door on each side."
  [HOLD 8]
  say 730 NEUTRAL "Four Scrap for the door. Two / Medicine for the shelf. {.}Then / furniture. I don't do that."
  choice 730 "Build the clinic?" { "Two doors it is." -> yes ; "Later." }
  end
yes:
  quest S153 active
  befriend 730 1
end

script s153_again
  native CB_SIDE_HAVE ITEM_SCRAP
  jlt v0 4 need
  native CB_SIDE_HAVE ITEM_MEDICINE
  jlt v0 2 need
  take ITEM_SCRAP 4
  take ITEM_MEDICINE 2
  say rivet NEUTRAL "A second door. {.}Hinges on / the Hillvile side, so it / squeaks when you come home."
  push workshop
  sfx SFX_WELD
  say nar "(Rivet welds it. It squeaks. / {..}She leaves the squeak in.)"
  [R13]
  say 730 NEUTRAL "A bed each side. {.}One lamp in / the middle. That's all a / clinic is, really."
  push home 0x4A04
  home DECORATE_FOR 10
  jlt v0 8 rough
  say 730 NEUTRAL "...Yes. That."
  jmp open
rough:
  say 730 NEUTRAL "It'll do. {.}Clinics are mostly / chairs and worry."
open:
  [R13]
  sfx SFX_DOOR
  say nar "(Bloom arrives through the / Hillvile door. Graveley / through the Hellvile one.)"
  [R13]
  say nar "(They shake hands. Two pairs / of very cold hands.)"
  [HOLD 13]
  say bloom SHOCK "Ooh."
  say 730 NEUTRAL "Ooh."
  [R8]
  say bloom SAD "Sorry."
  say 730 NEUTRAL "Sorry."
  [R21]
  call s153_patients
  end
need:
  say 730 NEUTRAL "Four Scrap. Two Medicine. {.}I / counted twice. Ghouls are / thorough."
end

script s153_patients
  jif SF_GRUMBLE_DEAD snarl
  say nar "(First patient: Grumbleguts. / Indigestion. {..}He signs in / with a flourish.)"
  [R8]
  say grumble LOVE "Doc. Two docs. {.}Darlin', I've / never been this sick in my / LIFE." [PUNCH]
  [R8]
  say bloom NEUTRAL "Antacid. Twice a day. {.}With / food. You'll be fine."
  [R13]
  jmp second
snarl:
  say nar "(First patient: Big Snarl. / A splinter. {..}He is very / brave about it.)"
  [R8]
  say bloom NEUTRAL "There. {.}All done. You can / let go of my hand now." [PUNCH]
  [R13]
second:
  say nar "(Second patient: @E. {.}Horn-ache. / A tiny ice pack.)"
  sfx SFX_BEEP
  [R8]
  say nar "(Third patient: Father Fuzz. / He needs nothing. {..}He sits / down anyway.)"
  [HOLD 21]
  [MUTE 26]
  say 730 NEUTRAL "You were on the wrong side when / it closed." [HEART]
  [HOLD 13]
  say bloom SAD "There wasn't a wrong side. / {..}There was just a wall."
  [HOLD 21]
  say nar "(They sit on the step, one / foot each side of the old / line.)"
  [NO BUTTON]
  [HOLD 26]
  reward 120 furn=blooms_scrubs
  home PLACE_GIFT furn=blooms_scrubs
  befriend bloom 4
  befriend 730 4
  quest S153 done
  later 6m s153_later
end

script s153_later
  say nar "(The clinic's sign-in sheet, / pinned by the door:)"
  [R8]
  say nar "(Scared of the doctor: / nobody. {.}Scared of the / forms: Grumbleguts.)"
end

# =============================================================================
# S154 "Open Mic" - Hattie Haha (71) - Ch1-2 - timing, talk
# =============================================================================
@pack quests kind=side
@mission S154 giver=71 side=hill ch=1-2 after=BEAT_EP1_4 at=21,18 verbs=timing chain=N step=1 later=5m reward=20 offer=s154_offer active=s154_again title="Open Mic" hint="Be Hattie's warm-up act at the cafe. Press A on the punchline."
@pack sb_bloom kind=side

script s154_offer
  sfx SFX_KAZOO
  say hattie HAPPY "Open mic tonight! {!}I need a / warm-up act. You're an act!"
  [R8]
  choice hattie "Warm up the room?" { "I don't have an act." -> noact ; "Break a leg (mine)." -> go ; "Not tonight." }
  end
noact:
  say hattie HAPPY "Timing is the act. {.}Setup, / beat, punchline. You press, / they laugh."
go:
  quest S154 active
  call s154_again
end

script s154_again
  say hattie NEUTRAL "Three jokes. {.}A on the punch. / Miss it and you get a / cricket."
  push timing pattern=rimshot skin=laugh
  jlt v0 1 flop
  sfx SFX_KAZOO
  say nar "(A snort-laugh ripples round / the cafe. Somebody spills a / milkshake.)"
  jmp third
flop:
  sfx SFX_TIK
  say nar "(A cricket. {.}A second cricket / joins it, for support.)"
third:
  [R13]
  say nar "(Your third punchline is just: / 'Hee. Hee.')"
  [R8]
  npc mayor HOP
  sfx SFX_KAZOO
  say nar "(The Mayor, front row, laughs / loudest of all.)"
  [R5]
  npc mayor STILL
  [MUTE 13]
  [HOLD 13]
  say mayor NEUTRAL "...Good one." [PUNCH]
  npc mayor THAW
  [R21]
  say hattie HAPPY "Tough room! {.}The back row / didn't laugh at all."
  [R8]
  say nar "(Mascotty, back row. {..}Still / facing the Wall.)"
  [HOLD 13]
  [MUTE 21]
  say hattie SAD "Everybody here laughs at / everything. {.}Do you know how / hard that makes it..." [HEART]
  [HOLD 8]
  say hattie SAD "...to tell if you're funny?"
  [R8]
  say hattie HAPPY "That one was a joke. / {..}Mostly."
  reward 20 furn=mic_stand
  home PLACE_GIFT furn=mic_stand
  befriend hattie 3
  quest S154 done
  later 5m s154_later
end

script s154_later
  say nar "(A chalkboard by the cafe: / OPEN MIC EVERY NIGHT. / 'Timing is the act.')"
end

# =============================================================================
# S155 "No Punchlines" - Hattie Haha (71) (saved) or her ghost - Ch3-4
# after Ep 3.5 - timing, find
# =============================================================================
@pack quests kind=side
@mission S155 giver=71 side=hill ch=3-4 after=BEAT_EP3_5 at=21,18 verbs=timing,find chain=N step=2 later=6m reward=60 offer=s155_offer active=s155_again title="No Punchlines" hint="Three timing sessions with Hattie, then find her old notebook at the cafe."
@pack sb_bloom kind=side

script s155_offer
  jif SF_HATTIE_LOST ghost
  say hattie SAD "I can't finish a joke. {.}I / laugh a beat early. Every / time."
  jmp ask
ghost:
  say nar "(Hattie's ghost, faint as / chalk, is trying to tell a / joke to a lamp post.)"
  [R8]
  say hattie SAD "I laugh before the punchline / now. {..}Being dead does that / to your timing."
ask:
  [R8]
  say hattie NEUTRAL "Relearn it with me? {.}From the / setup."
  choice hattie "Coach Hattie?" { "From the setup." -> yes ; "Later." }
  end
yes:
  quest S155 active
  call s155_again
end

script s155_again
  say hattie NEUTRAL "Session one. {.}Knock knock."
  choice nar "You say:" { "Who's there?" -> who }
who:
  [R2]
  say hattie HAPPY "HA. {..}No. Wait." [PUNCH]
  [R13]
  say hattie NEUTRAL "My laugh is the metronome. / {.}It drifts. Don't count. / Listen."
  push timing pattern=prayer skin=laugh drift
  time ADVANCE 1440
  say hattie NEUTRAL "Session two. {.}Better. The / laugh came after the joke. / Mostly."
  [R13]
  say hattie SAD "My old notebook. {.}In the / cafe, under the counter. The / jokes I never told."
  verb find cell=26,16 then=s155_book
end

script s155_book
  sfx SFX_PICKUP
  say nar "(A notebook, smoke-stained. / Every joke in it is about / the Wall.)"
  [R13]
  say hattie SAD "That's why I never told / them."
  time ADVANCE 1440
  say hattie NEUTRAL "Session three. {.}One joke. On / time."
  push timing pattern=prayer skin=laugh
  [MUTE 21]
  [HOLD 13]
  say hattie NEUTRAL "Why did the Hillviler cross / the park?" [HEART]
  [HOLD 21]
  say hattie NEUTRAL "{..}She didn't. That's the / joke."
  [R13]
  say hattie SAD "For ten years, that's been / the joke."
  [HOLD 26]
  say nar "(Nobody laughs.)"
  [HOLD 13]
  sfx SFX_KAZOO
  say haze HAPPY "...pfft."
  [R8]
  say nar "(Haze, who crossed the park / with you, snorts into her / sleeve.)"
  [R8]
  say hattie HAPPY "Finally. {.}A room."
  [R13]
  sfx SFX_KAZOO
  say nar "(The cafe owner laughs so / hard the jukebox skips. / {..}To the right song.)"
  reward 60 furn=hatties_notebook
  home PLACE_GIFT furn=hatties_notebook
  befriend hattie 4
  befriend haze 2
  quest S155 done
  later 6m s155_later
end

script s155_later
  say nar "(Hattie's notebook, on your / shelf, has twelve readable / jokes. {.}You read one.)"
  [R8]
  say nar "(It's about the Wall. {..}It's / funny now.)"
end

# =============================================================================
# S156 "The Hellvile Set" - Honkers (501, Hellvile) + Hattie (71) - Ch4-5
# timing, talk
# =============================================================================
@pack quests kind=side
@mission S156 giver=501 side=hell ch=4-5 after=BEAT_EP4_2 at=16,19 verbs=timing chain=N step=3 later=7m reward=150 offer=s156_offer active=s156_again title="The Hellvile Set" trophy="Tough Room" hint="Rehearse Honkers and Hattie's double act, then play the comedy tent."
@pack sb_bloom kind=side

script s156_offer
  sfx SFX_KAZOO
  say 501 SAD "I honk at funerals. {.}I'm never / invited back."
  [R8]
  say 501 NEUTRAL "I've never been allowed to / be funny on purpose. {..}I / want a double act. Hattie."
  [HOLD 8]
  say hattie NEUTRAL "Can he do timing?"
  [R13]
  sfx SFX_KAZOO
  say 501 HAPPY "HONK."
  [R8]
  say hattie NEUTRAL "...He cannot do timing."
  choice hattie "Rehearse them?" { "One, two, honk." -> yes ; "Later." }
  end
yes:
  quest S156 active
  call s156_again
end

script s156_again
  say hattie NEUTRAL "Rehearsal. {.}He honks a beat / early. Every time. Keep up / with ME."
  push timing pattern=rimshot skin=laugh drift
  [R13]
  say hattie HAPPY "I've got it. {!}I'll write his / honks INTO the act. He's not / early. He's the metronome."
  [R13]
  sfx SFX_FANFARE
  say nar "(The comedy tent. Hillvile / left, Hellvile right. Laugh / tracks on both sides.)"
  [R8]
  say nar "(Front row: Mr. Solemn. {.}He / has not smiled in two / hundred years.)"
  push timing pattern=rimshot skin=laugh
  sfx SFX_KAZOO
  say nar "(Honk.)"
  [R13]
  sfx SFX_KAZOO
  say nar "(Honk.)"
  [R13]
  [MUTE 21]
  [HOLD 21]
  say nar "(...nothing.)" [PUNCH]
  [HOLD 13]
  sfx SFX_KAZOO
  say nar "(The silence is the punchline. / The tent falls apart.)"
  [R8]
  say nar "(Mr. Solemn's mouth moves one / pixel.)"
  [R21]
  [MUTE 21]
  say hattie SAD "It wasn't that you weren't / funny over there." [HEART]
  [HOLD 8]
  say hattie SAD "{..}It was the law."
  [R13]
  say 501 NEUTRAL "I honked anyway."
  [HOLD 13]
  say hattie HAPPY "I know. {.}I heard you. / Through the Wall."
  reward 150 furn=honkers_nose
  home PLACE_GIFT furn=honkers_nose
  befriend 501 4
  befriend hattie 3
  befriend 555 1
  quest S156 done
  later 7m s156_later
end

string S156_NEWS "Mr. Solemn seen smiling at the comedy tent. Denies it."

script s156_later
  stash 0 S156_NEWS
  native CB_SIDE_NEWS
  sfx SFX_TIK
  say nar "(HELLVILE HERALD: Mr. Solemn / seen smiling at the comedy / tent. {..}Denies it.)"
end
