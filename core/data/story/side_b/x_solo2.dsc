# side_b/x_solo2.dsc - batch-B stand-alones S187 S191 S192 S195 S211 S212
# (owner: S5; story_final.md sec.14.2): a janitor who wrings out the
# unsaid, a pirate who needs a happy shanty, a caller who has only ever
# said "number nine", a thousand fireflies over the Mingle (the game's last
# sincere beat), and two Chapter 3 grafts - the thirteenth bell and the
# hug audit.

# =============================================================================
# S187 "The Sin Bin" - Mopsworth (547, Hellvile janitor) - Ch5
# fetch, weld, find
# =============================================================================
@pack quests kind=side
@mission S187 giver=547 side=hell ch=5-5 at=26,41 verbs=fetch,weld,find later=6m reward=60 offer=s187_offer active=s187_again title="The Sin Bin" hint="Bring Mopsworth 3 Scrap for a bigger bin. Find the sin bubbles in Hellvile."
# S187 is its own pack: Mopsworth's "Ten years I..." (SF sec.7.11).
@pack s187 kind=side minutes=30

script s187_offer
  sfx SFX_SPLASH
  say 547 NEUTRAL "I mop up sins. {.}Wring them / out. Keep them in the bin."
  [R8]
  say 547 SAD "Since the fountain went / two-way, Hillvile ones come / through the pipes."
  [HOLD 8]
  say 547 NEUTRAL "Unsent apologies, mostly. {..}I / need a bigger bin. Three / Scrap."
  choice 547 "Help Mopsworth?" { "Mop it up." -> yes ; "Later." }
  end
yes:
  quest S187 active
end

script s187_again
  native CB_SIDE_HAVE ITEM_SCRAP
  jlt v0 3 need
  take ITEM_SCRAP 3
  push workshop
  sfx SFX_WELD
  say nar "(Rivet welds a bin. It's / enormous. {.}It still might not / be enough.)"
  [R13]
  say 547 NEUTRAL "Now the bubbles. {.}They float / up from the drains. Pop one / and you'll hear it."
  verb find cell=12,14 then=s187_b1
  end
need:
  say 547 NEUTRAL "Three Scrap. {.}Sins are heavy. / The bin has to be strong."
end

script s187_b1
  sfx SFX_BUBBLE
  say nar "(A bubble: 'I ate Nana's / muffin and blamed the pet.')"
  verb find cell=30,19 then=s187_b2
end

script s187_b2
  sfx SFX_BUBBLE
  say nar "(A bubble: 'I never liked / grape.')"
  verb find cell=52,33 then=s187_b3
end

script s187_b3
  sfx SFX_BUBBLE
  say nar "(A bubble: 'I'm scared of / the dark.' {..}Very small. Very / round.)"
  [R13]
  sfx SFX_BUBBLE
  say nar "(And the biggest one: the / Mayor's. The size of a / bathtub.)"
  [R8]
  say nar "(It just says 'hee'.)"
  [R13]
  sfx SFX_SPLASH
  say nar "(Mopsworth wrings it.)"
  [R13]
  sfx SFX_SPLASH
  say nar "(He wrings it again.)"
  [R13]
  sfx SFX_SPLASH
  [R13]
  say nar "(Again.)"
  [R8]
  say 547 NEUTRAL "That's a lot of hee." [PUNCH]
  [R21]
  say nar "(The owners come for their / bubbles. Each sits in Wisp's / thinking chair and says it.)"
  [R13]
  say nar "(Out loud. {.}By evening the / bin is empty.)"
  [MUTE 21]
  [HOLD 21]
  say 547 SAD "Ten years I mopped up things / nobody said, because someone / had to hold them." [HEART]
  [HOLD 13]
  say 547 HAPPY "{..}Turns out you just have to / say them."
  [R13]
  say nar "(He is unemployed. {.}He is / thrilled.)"
  reward 60 furn=sin_bin_planter
  home PLACE_GIFT furn=sin_bin_planter
  befriend 547 4
  quest S187 done
  later 6m s187_later
end

script s187_later
  say nar "(Mopsworth tends the park. / The bin is a planter: one / flower per honest thing.)"
  [R13]
  say nar "(The Mayor says 'hee'. {..}Nothing / comes out of the pipes.)"
end

# =============================================================================
# S191 "Sulfur Shanties" - Barnaby (768, Hellvile) + Capt. Whisk (48) - Ch5
# fishing, studio, concert
# =============================================================================
@pack quests kind=side
@mission S191 giver=768 side=hell ch=5-5 at=31,42 verbs=fishing,concert later=7m reward=80 offer=s191_offer active=s191_again title="Sulfur Shanties" hint="The Lake of Fire's fish weep when Barnaby sings. Write xyr a happy shanty."
@pack sb_solo2 kind=side minutes=110

script s191_offer
  say 768 SAD "When I sing, the Lake of Fire's / fish weep. {.}The lake rises."
  [R8]
  say 48 NEUTRAL "I name fish and let them go. / {..}I'd like them not crying."
  [HOLD 8]
  say 768 NEUTRAL "A fishing concert. {.}Where / nobody cries. The fish have / opinions."
  choice 768 "Write a shanty?" { "Yo ho." -> yes ; "Later." }
  end
yes:
  quest S191 active
  call s191_again
end

script s191_again
  say 48 HAPPY "First, catch a few. {.}Lava / fish are shy."
  push minigame SMG_A_BOBBER
  [R13]
  say 768 NEUTRAL "Now, a shanty."
  sfx SFX_PLUCK
  say nar "(Barnaby sings. {.}The fish / weep. The lake rises.)"
  [R8]
  say 48 NEUTRAL "Try a happy one."
  [R8]
  say 768 HAPPY "This one's about a boat that / sinks, but everyone's fine!"
  [R8]
  sfx SFX_SPLASH
  say nar "(The fish weep.)"
  [R13]
  say 48 NEUTRAL "Try one where the boat floats."
  [R8]
  say 768 SAD "...Where's the song in that." [PUNCH]
  [R21]
  choice nar "The new shanty:" { "The boat floats." -> floats ; "Everyone's dry." -> floats ; "Nobody drowns." -> floats }
floats:
  [R8]
  say 768 NEUTRAL "...Huh. {.}It's catchy."
  push concert 768
  native CB_SIDE_ONCE SONG_BOAT_FLOATS
  say nar "(The concert on the pier. {.}The / fish don't weep.)"
  [R13]
  say nar "(One of them is Gerald. {..}It / laughs. Fish can laugh now.)"
  [R21]
  [MUTE 21]
  say nar "(Barnaby keeps a tally on / the pier post: fish who / cried, per song.)" [HEART]
  [HOLD 13]
  say nar "(The new song's column is / empty. {.}Xe carves a smile in / it.)"
  native CB_SIDE_MUSIC
  reward 80 furn=weeping_fish_tank
  home PLACE_GIFT furn=weeping_fish_tank
  befriend 768 3
  befriend 48 2
  quest S191 done
  later 7m s191_later
end

string S191_NEWS "Lake of Fire water level normal for the first time in a decade."

script s191_later
  stash 0 S191_NEWS
  native CB_SIDE_NEWS
  sfx SFX_TIK
  say nar "(HELLVILE HERALD: Lake of / Fire water level normal for / the first time in a decade.)"
end

# =============================================================================
# S192 "Number Nine" - Number Nine (514, Hellvile) - Ch5 - deliver, escort,
# timing
# =============================================================================
@pack quests kind=side
@mission S192 giver=514 side=hell ch=5-5 at=23,18 verbs=deliver,escort,timing later=6m reward=50 offer=s192_offer active=s192_again title="Number Nine" hint="Number Nine has called 'number nine' for eighty years. Find ticket nine."
@pack sb_solo2 kind=side

script s192_offer
  shake SHK_TAP
  say 514 NEUTRAL "{big}NUMBER NINE.{/}"
  [R8]
  say nar "(A voice like a foghorn. {.}A / body like a thimble.)"
  [R8]
  say 514 NEUTRAL "Eighty years. {.}Nobody has / ever been number nine. {..}The / ticket is in Hillvile."
  choice 514 "Find ticket nine?" { "I'll find it." -> yes ; "Later." }
  end
yes:
  quest S192 active
  call s192_again
end

script s192_again
  say nar "(Keysie keeps everyone's spare / everything.)"
  verb deliver 21 cell=39,10 then=s192_keysie   # map=hill
end

script s192_keysie
  say 21 NEUTRAL "Ticket nine? {.}Not in my / spares. {..}Try the Mayor's / drawer. He keeps strings."
  verb deliver mayor cell=29,8 then=s192_mayor   # map=hill
end

script s192_mayor
  say mayor NEUTRAL "My drawer? {.}String, string, / string... {..}Nana's recipe tin! / From the last Mingle."
  verb deliver nana cell=25,17 then=s192_tin   # map=hill
end

script s192_tin
  sfx SFX_PICKUP
  say nar "(In Nana's recipe tin, under / the cupcakes: a ticket. / 'Charlotte's turn: #9'.)"
  [R8]
  say nana SAD "Charlotte's. {..}From before the / Wall."
  [R13]
  say nana NEUTRAL "Bring her to the counter. {.}I'll / bake the bread."
  verb escort 566 cell=23,19 then=s192_counter
end

script s192_counter
  jlt v1 1 lost
  shake SHK_TAP
  say 514 NEUTRAL "{big}NUMBER NINE.{/}"
  [HOLD 26]
  say 566 NEUTRAL "{d}...Here." [PUNCH]
  [R21]
  push timing pattern=knock skin=bell
  [MUTE 21]
  say nar "(Charlotte gets her bread. / Eleven years late. {.}Nana / baked it. It's warm.)" [HEART]
  [HOLD 21]
  say 514 NEUTRAL "...Number ten."
  [HOLD 13]
  say nar "(Nobody has a ten. {..}They / call it anyway.)"
  native CB_SIDE_ONCE SONG_NUMBER_TEN
  reward 50 furn=ticket_nine
  home PLACE_GIFT furn=ticket_nine
  befriend 514 3
  befriend 566 2
  quest S192 done
  later 6m s192_later
  end
lost:
  say nana NEUTRAL "She wandered off to look at / the bread. {.}Fetch her again?"
end

script s192_later
  say 528 NEUTRAL "Ticket nine. {.}Late. Eleven / years."
  [R8]
  sfx SFX_THUD
  say nar "(Mandible stamps it: ON TIME.)"
end

# =============================================================================
# S195 "Glowbug's Constellation" - Glowbug (12) - Ch5 (the Mingle)
# pet, minigame, photo
# =============================================================================
@pack quests kind=side
@mission S195 giver=12 side=park ch=5-5 at=24,28 verbs=pet,minigame,photo later=10m reward=200 offer=s195_offer active=s195_again title="Glowbug's Constellation" trophy="Constellation" hint="Herd a thousand fireflies with @E's glow, into a shape over the Mingle."
@pack sb_solo2 kind=side

script s195_offer
  say 12 HAPPY "The first Mingle in eleven / years! {.}A thousand fireflies, / both sides' names."
  [R8]
  say 12 NEUTRAL "Your @E glows a little now, / when it's happy. {..}Fireflies / follow a glow."
  choice 12 "Herd the fireflies?" { "Glow, @E!" -> yes ; "Later." }
  end
yes:
  quest S195 active
  call s195_again
end

script s195_again
  [NIGHT]
  say 12 NEUTRAL "Lead them into the shape. {.}Go / slowly. They're nervous."
  push minigame SMG_B_FIREFLY
  sfx SFX_SPARKLE
  say nar "(Over the park, the fireflies / spell: HAHALAND.)"
  [R13]
  say nar "(Then, drifting: HAHALANO.)"
  [R8]
  say 12 NEUTRAL "{d}Close enough." [PUNCH]
  [R8]
  sfx SFX_TINK
  say nar "(A single firefly fixes the D.)"
  [R21]
  say 12 HAPPY "A photo! {.}Me and the sky."
  verb photo 12 cell=24,28 then=s195_photo
end

script s195_photo
  [NO GAG]
  sfx SFX_SPARKLE
  say nar "(Midnight. The constellation / over Hellvile Park.)"
  npc mayor RIPPLE 9
  [HOLD 21]
  [MUTE 26]
  say nar "(A thousand citizens look up at / once.)" [HEART]
  [HOLD 21]
  say nar "(The Mayor and Nox, holding / hands. {.}Tallyworn, not / counting.)"
  [HOLD 13]
  say nar "(Mascotty, no head. {.}Grumbleguts, / eating, looking up anyway.)"
  [HOLD 13]
  say nar "(@E, horned, glowing.)"
  [NO BUTTON]
  [HOLD 26]
  reward 200 furn=firefly_jar unlock=700
  home PLACE_GIFT furn=firefly_jar
  befriend 12 4
  quest S195 done
  later 10m s195_later
end

script s195_later
  [NO GAG]
  say nar "(The firefly jar glows. {..}It / spells nothing. It doesn't / need to.)"
end

# =============================================================================
# S211 "Thirteen Bells" - Cantor Coo (426) - Ch3 (night, after #24)
# find, timing (grafted heart S87; the whole quest is the scare: no gag)
# =============================================================================
@pack quests kind=side
@mission S211 giver=426 side=hill ch=3-3 after=BEAT_EP3_2 at=10,28 verbs=find,timing later=6m reward=50 offer=s211_offer active=s211_again title="Thirteen Bells" hint="Bellwether's saved, but the tower rings backwards at midnight. Go and listen."
@pack sb_solo2 kind=side

script s211_offer
  [NO GAG]
  native CB_SIDE_QUEST 24
  jlt v0 4 early
  say cantor SAD "Bellwether is saved. {.}But the / tower still rings backwards. / At midnight."
  [R8]
  say cantor NEUTRAL "Will you climb it with me? {..}I / don't want to go alone."
  choice cantor "Climb the bell tower?" { "Tonight." -> yes ; "Not tonight." }
  end
early:
  say cantor SAD "The bells ring wrong. / {.}Something is wrong with / Bellwether, too."
  end
yes:
  quest S211 active
  call s211_again
end

script s211_again
  [NO GAG]
  native CB_SIDE_NIGHT
  jlt v0 1 day
  say cantor NEUTRAL "The tower stair. {.}By the / church wall."
  verb find cell=11,22 then=s211_tower
  end
day:
  say cantor NEUTRAL "Midnight. {.}Come back at / midnight."
end

script s211_tower
  [NO GAG]
  [SILENCE]
  say nar "(The bell chamber. 00:00. / No music.)"
  [HOLD 13]
  sfx SFX_BELL
  [R13]
  sfx SFX_BELL
  [R13]
  sfx SFX_BELL
  say nar "(Twelve bells. Backwards. / {.}Every one unrings.)"
  [HOLD 26]
  sfx SFX_BELL
  say nar "(A thirteenth.)"
  [HOLD 26]
  sfx SFX_CREAK
  say nar "(The bell rope moves. By / itself.)"
  [HOLD 26]
  say nar "(A wisp on the rope. Grey. / Confused. {..}It doesn't know / which way is out.)"
  [HOLD 13]
  say cantor SAD "...Hum. {.}The lullaby. The / first line."
  push timing pattern=prayer skin=bell
  [HOLD 21]
  say nar "(The wisp lets go of the rope. / {..}It goes south.)"
  [HOLD 26]
  native CB_SIDE_MUSIC
  say cantor SAD "Not every soul needed a rite." [HEART]
  [HOLD 13]
  say cantor SAD "{..}Some only needed the tune."
  [R13]
  say nar "(In the hymnal's margin Coo / writes: 'no rite'.)"
  native CB_SIDE_ONCE SONG_THIRTEEN_BELLS
  reward 50 furn=small_bell
  home PLACE_GIFT furn=small_bell
  befriend cantor 3
  quest S211 done
  later 6m s211_later
end

script s211_later
  [NO GAG]
  sfx SFX_BELL
  say nar "(At midnight your small bell / rings. {.}Once. Forwards.)"
end

# =============================================================================
# S212 "The Hug Audit" - Twisty (43) - Ch3 (after #30) - combat
# (grafted heart S93)
# =============================================================================
@pack quests kind=side
@mission S212 giver=43 side=hill ch=3-3 after=BEAT_EP3_2 at=23,40 verbs=combat later=5m reward=50 offer=s212_offer active=s212_again title="The Hug Audit" hint="Hugsley is hugging again. A bit too well. Watch him hug a demon."
@pack sb_solo2 kind=side

string S212_C1 "Hugsley hugs. Things pop."
string S212_C2 "Stay behind the mascot."

script s212_offer
  native CB_SIDE_QUEST 30
  jlt v0 4 early
  say 43 NEUTRAL "Hugsley's saved. {.}He's hugging / again."
  [R8]
  say 43 SHOCK "{..}A bit too well. Come and / see."
  choice 43 "Audit the hugs?" { "Show me." -> yes ; "Later." }
  end
early:
  say 43 SAD "Hugsley's not himself. {.}Father / Fuzz might know what to do."
  end
yes:
  quest S212 active
  call s212_again
end

script s212_again
  say nar "(Hugsley spots a cloud spider. / He hugs it.)"
  [R5]
  sfx SFX_POP2
  shake SHK_POW
  say nar "(It pops into grape goo.)"
  [R8]
  say 20 NEUTRAL "...I meant that one."
  [R13]
  push combat MAPID_HAHALAND_INVADED captions=S212_C1,S212_C2
  sfx SFX_POP2
  say nar "(Hug.)"
  [R13]
  sfx SFX_POP2
  say nar "(Hug. {.}Bigger pop.)"
  [R13]
  sfx SFX_CONFETTI
  [R13]
  say nar "(Hug. {..}Confetti.)"
  [R13]
  say 43 NEUTRAL "He's our best weapon. {.}Don't / tell him."
  [R13]
  sfx SFX_BOING
  shake SHK_TAP
  say nar "(Hugsley hugs you. {.}The whole / screen squashes.)"
  [R21]
  [MUTE 21]
  say 20 SAD "A thousand hugs a day. Every / day since the bus. {..}I counted." [HEART]
  [HOLD 13]
  say 20 SAD "Except eleven years of none, / from the south."
  reward 50 furn=hug_counter
  home PLACE_GIFT furn=hug_counter
  befriend 20 3
  befriend 43 2
  quest S212 done
  later 5m s212_later
end

script s212_later
  sfx SFX_TIK
  say nar "(The hug counter in your / house ticks up one. {.}Someone / held @E.)"
end
