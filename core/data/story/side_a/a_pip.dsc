# side_a/a_pip.dsc - Chain A "Pip and Mr. Flops" (S100-S102) and Chain G
# "Grampy Bean and Sootkin" (S124-S126) (owner: S4; story_final.md
# sec.14.1; docs/design/story/SPEC.md sec.7 S4).
#
# Every mission: a hook, one task (a verb, a fetch or a scene push), the
# comic beat on the timing ladder, the heart (or scare) beat, the reward
# and its Later. Talking to the giver while a mission is under way runs its
# `active` script, which hands the task out again (a verb is not saved, so
# a reload just asks for it once more). The @mission headers live in the
# quest log's index pack (quests49/index.dsc explains why).

# =============================================================================
# S100 "A Bed for Mr. Flops" - Pip (42) - Ch1 after Ep 1.3 - fetch, housing
# =============================================================================
@pack quests kind=side
@mission S100 giver=42 side=hill ch=1-2 after=BEAT_EP1_3 at=10,47 verbs=fetch,housing chain=A step=1 later=5m reward=30 offer=s100_offer active=s100_again title="A Bed for Mr. Flops" hint="Bring Pip a Plushie: Nana will sew Mr. Flops a bed for your house."
@pack sa_pip kind=side minutes=95

script s100_offer
  npc pip EMOTE EMO_SWEAT
  say pip SAD "Mr. Flops has nightmares. / {..}About the beach."
  [R8]
  say pip NEUTRAL "He needs a bed. In YOUR / house. {.}Because yours has a / door that locks."
  [HOLD 8]
  say pip HAPPY "Nana can sew one if we bring / her stuffing. {.}A Plushie / from the shop has loads."
  choice pip "Help Mr. Flops sleep?" { "Of course!" -> yes ; "Not right now." }
  end
yes:
  quest S100 active
  befriend pip 1
  say pip HAPPY "Mr. Flops says thank you. / {..}He says it very quietly."
end

script s100_again
  native CB_SIDE_HAVE ITEM_PLUSHIE
  jlt v0 1 none
  take ITEM_PLUSHIE
  say pip HAPPY "STUFFING! {!}I'll run it to / Nana. {.}Don't move!"
  npc pip HOP
  [R13]
  sfx SFX_WHOOSH
  say nar "(Pip is back before you can / blink, holding a shoebox / with a tea-towel blanket.)"
  [R8]
  say pip HAPPY "Nana sewed it! {.}It has a / pillow! The pillow has a / smaller pillow!"
  reward 30 furn=mr_flops_bed
  home PLACE_GIFT furn=mr_flops_bed
  push home -1
  home DECORATE_FOR 4
  say pip NEUTRAL "Let me check it."
  sfx SFX_TIK
  [R13]
  say pip NEUTRAL "Too close to the door."
  [R13]
  sfx SFX_TIK
  say pip NEUTRAL "Too close to the window."
  [R13]
  sfx SFX_TIK
  [R13]
  say pip NEUTRAL "{d}...Too close to the floor."
  [HOLD 13]
  sfx SFX_TINK
  say nar "(The box slides one pixel / to the left.)"
  [R8]
  say pip HAPPY "Perfect."
  [R21]
  [MUTE 21]
  say pip SAD "He sleeps better when someone / else is in the room." [HEART]
  [HOLD 13]
  say pip SAD "{..}So do I."
  befriend pip 3
  quest S100 done
  later 5m s100_later
  end
none:
  say pip NEUTRAL "One Plushie. From the shop. / {..}Mr. Flops is NOT a / Plushie. He's a bunny."
end

script s100_later
  say nar "(At home, Mr. Flops's bed / sits by the window. The / tea towel is folded twice.)"
  [R8]
  say nar "(Somebody folded it. {.}You / didn't.)"
end

# =============================================================================
# S101 "Night Patrol" - Pip (42) - Ch2 - escort, photo, timing
# =============================================================================
@pack quests kind=side
@mission S101 giver=42 side=hill ch=2-2 at=10,47 verbs=escort,photo chain=A step=2 later=4m reward=40 offer=s101_offer active=s101_again title="Night Patrol" hint="Walk Pip to the lamp post by the Wall at night, then take the 'proof' photo."
@pack sa_pip kind=side

script s101_offer
  say pip HAPPY "I made a badge. {.}WALL WATCH. / {..}It's a bottle cap."
  [R8]
  say pip NEUTRAL "Tonight we patrol Hillvile / Lane. To prove monsters / aren't real."
  [HOLD 8]
  say pip SAD "{..}You go first."
  choice pip "Go on night patrol?" { "Patrol time." -> yes ; "Maybe tomorrow." }
  end
yes:
  quest S101 active
  call s101_again
end

script s101_again
  [NIGHT]
  sfx SFX_CREAK
  say nar "(Night. The Wall hums. Pip / holds the back of your / shirt.)"
  say pip NEUTRAL "Lamp post by the Wall. / Then home. {.}Stay close."
  verb escort pip cell=52,23 then=s101_lamp
end

script s101_lamp
  jlt v1 1 lost
  npc pip EMOTE EMO_SWEAT
  say pip SHOCK "{!}...It's dark here."
  [R8]
  say pip NEUTRAL "Take the proof photo. Me, / the Wall, no monsters. / {.}Face me. Step back. A."
  verb photo pip cell=53,23 then=s101_photo
  end
lost:
  say pip SAD "You walked SO fast. {..}I / went home. {.}Ask me again."
end

script s101_photo
  sfx SFX_SPARKLE
  say nar "(The polaroid slides out. / Pip, small and brave. The / fog behind her.)"
  [HOLD 13]
  say nar "(Two red dots in the fog. / Far away. Not moving.)"
  [HOLD 13]
  say pip NEUTRAL "Those are... fireflies."
  [R8]
  say pip SAD "Breathe with me. {.}Nana says / count the heartbeats."
  push timing pattern=three skin=heart
  say pip NEUTRAL "Big, {.}red, {.}fireflies."
  [R8]
  say pip SHOCK "{big}Patrol's over.{/}"
  npc pip FLEE 1
  sfx SFX_WHOOSH
  [R21]
  [MUTE 21]
  say nar "(On the way back, the fog / giggles. {..}In Pip's voice.)"
  [HOLD 21]
  say nar "(Pip doesn't notice. {..}You / do.)"
  reward 40 furn=wall_watch_badge
  befriend pip 3
  quest S101 done
  later 4m s101_later
end

script s101_later
  say nar "(The daycare's drawing wall / has a new crayon page: / FIREFLIES CAN BE RED NOW.)"
end

# =============================================================================
# S102 "Flops the 1st Rides Again" - Pip (42) - Ch3-4 after the beach nest
# combat (Breach 4), pet, weld, housing
# =============================================================================
@pack quests kind=side
@mission S102 giver=42 side=hill ch=3-4 after=BEAT_EP3_3 at=10,47 verbs=combat,pet,weld,housing chain=A step=3 later=6m reward=80 offer=s102_offer active=s102_again title="Flops the 1st Rides Again" hint="Find Mr. Flops's twin in the Plushie Factory (Breach 4); Rivet can mend him."
@pack sa_pip kind=side

string S102_C1 "Flops the 1st: somewhere in the Factory."
string S102_C2 "Don't aim at the bunny. He hops away."

script s102_offer
  say pip SAD "Mr. Flops is home. {.}But his / twin isn't."
  [R8]
  say pip SAD "Flops the 1st. {..}He's still / on the Factory conveyor. / Half bunny. Half demon."
  [HOLD 13]
  say pip NEUTRAL "Half bunny is still a / bunny."
  choice pip "Rescue Flops the 1st?" { "I'll bring him back." -> yes ; "Not yet." }
  end
yes:
  quest S102 active
  call s102_again
end

script s102_again
  say pip NEUTRAL "He hops away when you aim. / {..}Stop aiming and he comes / close."
  push combat MAPID_PLUSHIE_FACTORY captions=S102_C1,S102_C2
  jlt v0 1 retreat
  sfx SFX_SQUEAK
  say nar "(A half-demon bunny peeks / out of a crate. One ear / stitched, one ear on fire.)"
  push pet
  say nar "(@E sniffs him. {.}He sniffs / @E. {..}They are friends / now. That's the rule.)"
  say rivet NEUTRAL "Ears? {.}Give him here. I / weld, I can stitch."
  push workshop
  sfx SFX_WELD
  say nar "(Rivet sews both ears back. / Pip adds a felt heart. And / one on Mr. Flops.)"
  [R13]
  say pip NEUTRAL "He has one demon eye."
  [R8]
  say nar "(Pip puts a sticker over it.)"
  [R8]
  sfx SFX_TINK
  say nar "(The sticker blinks.)"
  [R13]
  say pip NEUTRAL "{..}He's expressive."
  [HOLD 13]
  [MUTE 21]
  say pip HAPPY "He's yours. {.}For when I'm / not here to be brave at / you." [HEART]
  reward 80 furn=flops_the_1st
  home PLACE_GIFT furn=flops_the_1st
  befriend pip 4
  quest S102 done
  later 6m s102_later
  end
retreat:
  say pip SAD "He's still in there. {..}He's / waiting. Bunnies are good at / waiting."
end

script s102_later
  say nar "(Flops the 1st sits by @E's / bed. Every so often his / sticker blinks.)"
  [R8]
  say 542 NEUTRAL "Whoever did that ear work: / {.}beautiful. {..}Tell them / Sutura said so."
end

# =============================================================================
# S124 "Where Was I?" - Grampy Bean (88) - Ch1-2 - find, talk, pet
# =============================================================================
@pack quests kind=side
@mission S124 giver=88 side=hill ch=1-2 after=BEAT_EP1_4 at=40,47 verbs=find,pet chain=G step=1 later=5m reward=20 offer=s124_offer active=s124_again title="Where Was I?" hint="Help @E sniff out Grampy's glasses, trowel and seed packet."
@pack sa_pip kind=side

script s124_offer
  say grampy NEUTRAL "I've lost my glasses. And my / trowel. {.}And the thing I / was about to say."
  [HOLD 13]
  say grampy NEUTRAL "{..}It was important."
  [R8]
  say grampy HAPPY "Your little @E finds things, / doesn't it?"
  choice grampy "Lend Grampy @E's nose?" { "Go on, @E!" -> yes ; "Later." }
  end
yes:
  quest S124 active
  call s124_again
end

script s124_again
  say nar "(@E sniffs the air. {.}Then / your shoe. {.}Then the air / again, harder.)"
  verb find cell=41,44 then=s124_glasses
end

script s124_glasses
  sfx SFX_SQUEAK
  say nar "(The glasses. Inside a / tomato. {..}The tomato is fine / about it.)"
  verb find cell=33,22 then=s124_trowel
end

script s124_trowel
  sfx SFX_SPLASH
  say nar "(The trowel is in the fountain. / @E reaches in.)"
  [R8]
  say nar "(The fountain giggles.)"
  [HOLD 13]
  verb find cell=44,45 then=s124_packet
end

script s124_packet
  sfx SFX_SPARKLE
  say nar "(A seed packet in the flower / bed. The writing on it is / Hellvile writing.)"
  say grampy HAPPY "Ah! My glasses!"
  [R5]
  say nar "(He puts them on. Over his / other glasses.)"
  [R8]
  say grampy HAPPY "Better."
  [R13]
  say grampy NEUTRAL "This packet... {..}a little / mouse-person who whistled. / Gave me these. Long ago."
  [HOLD 21]
  say grampy NEUTRAL "{..}What was I saying?"
  [MUTE 13]
  say nar "(He tucks the empty packet / in his hat. In case he / remembers who to thank.)" [HEART]
  reward 20 furn=tomato_planter
  befriend grampy 3
  quest S124 done
  later 5m s124_later
end

script s124_later
  sfx SFX_POP2
  say nar "(The Tomato Planter at home / has a tomato on it. {.}It / wasn't there this morning.)"
end

# =============================================================================
# S125 "The Face Tomatoes" - Grampy Bean (88) - Ch3 after Ep 3.5 (saved)
# =============================================================================
@pack quests kind=side
@mission S125 giver=88 side=hill ch=3-3 after=BEAT_EP3_5 at=40,47 verbs=fetch,find chain=G step=2 later=5m reward=50 offer=s125_offer active=s125_again title="The Face Tomatoes" hint="Pick Grampy's eyed tomatoes; bury the rest by the Wall, facing Hellvile."
@pack sa_pip kind=side

script s125_offer
  say grampy NEUTRAL "The tomatoes still have / eyes. {.}From the... you know. / The thing in my head."
  [R8]
  say grampy NEUTRAL "Pick them before they get / opinions."
  choice grampy "Pick the face tomatoes?" { "On it." -> yes ; "Not now." }
  end
yes:
  quest S125 active
  call s125_again
end

script s125_again
  say nar "(Nine tomatoes. Each one / blinks when you look away.)"
  sfx SFX_TIK
  [R2]
  sfx SFX_TIK
  [R2]
  sfx SFX_TIK
  say grampy NEUTRAL "Three for Dr. Bloom. {.}The / rest by the Wall. Facing / Hellvile. {..}Trust me."
  verb find cell=54,30 then=s125_bury
end

script s125_bury
  say nar "(You bury six tomatoes by the / Wall. @E eats the seventh.)"
  [HOLD 13]
  sfx SFX_BEEP
  say nar "beep."
  [R8]
  say nar "(For a moment @E's eyes are / tomatoes.)"
  [R13]
  say nar "(...it's fine.)"
  [R21]
  [MUTE 21]
  say nar "{w}thank you"
  [HOLD 21]
  say grampy HAPPY "You heard that? {.}That's not / my voice." [HEART]
  [R8]
  say grampy HAPPY "{..}Xe's still over there."
  reward 50 furn=eye_tomato
  befriend grampy 3
  quest S125 done
  later 5m s125_later
end

script s125_later
  say nar "(Overnight, sunflowers / sprout along the Wall. All / of them face Hellvile.)"
end

# =============================================================================
# S126 "Two Gardens" - Sootkin (525, Hellvile) + Grampy Bean - Ch4-5
# fetch, housing (the Park Plot), talk
# =============================================================================
@pack quests kind=side
@mission S126 giver=525 side=hell ch=4-5 at=41,35 verbs=fetch,housing chain=G step=3 later=8m reward=100 offer=s126_offer active=s126_again title="Two Gardens" trophy="Green Thumbs" hint="Bring Sootkin Flowers from Hillvile for the shared bed on the old Wall line."
@pack sa_pip kind=side

script s126_offer
  say sootkin NEUTRAL "My sunflower is under a gate / that's coming down."
  [R8]
  say sootkin SAD "Grampy wants to move it. / To a bed on the old Wall / line. {..}Both sides."
  [HOLD 13]
  say sootkin SAD "I've never moved it. {.}What if / it doesn't survive?"
  choice sootkin "Build the shared bed?" { "It'll survive." -> yes ; "Give it time." }
  end
yes:
  quest S126 active
  call s126_again
end

script s126_again
  native CB_SIDE_HAVE ITEM_FLOWERS
  jlt v0 1 need
  take ITEM_FLOWERS
  say nar "(Hillvile soil. Hellvile / ash. A bed of four tomatoes / and one sunflower.)"
  reward 100 furn=shared_flower_bed
  push home -4
  home DECORATE_FOR 8
  say grampy HAPPY "Thank you for the seeds, / Sooty."
  [R8]
  say grampy HAPPY "Sultry. {.}Toaster."
  [R8]
  say sootkin HAPPY "{..}Yes?"
  [HOLD 21]
  [MUTE 21]
  say sootkin SAD "...You remembered." [HEART]
  [HOLD 13]
  say grampy HAPPY "I remember everything. {..}Where / are my glasses?"
  sfx SFX_PLUCK
  [R8]
  sfx SFX_PLUCK
  say nar "(They whistle together. The / sunflower turns to listen.)"
  befriend sootkin 4
  befriend grampy 2
  quest S126 done
  later 8m s126_later
  end
need:
  say sootkin NEUTRAL "Flowers from your side first. / {.}So the bed knows it's both."
end

script s126_later
  say nar "(A tomato in your mailbox. A / sunflower seed stuck to it. / No note. {.}It's a note.)"
end
