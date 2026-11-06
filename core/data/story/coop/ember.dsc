# coop/ember.dsc - Ember, the Hearth Imp: Ch1 side quest "Wrong Side of the Mat"
# (MP1 content; docs/COOP_HEROES.md). A co-op hero's side arc, never the plot: it
# sets no story flag. The speaker is `target`, the co-op housemate the player is
# talking to (a co-op hero's resident id is a household id, not a roster id).
# story_coop_tick (story.c) starts each hero's next quest, one per game day. All twelve co-op
# scripts (Ember, Rook, Wick x Ch1..Ch4) live in this one pack so no pack id moves (save.lock).
# Every quest ends in a reward (H$ and a small item, never a plot item) and sets no story flag.
@pack zc_embr kind=episode chapter=0 minutes=0

script coop_ember_1
  say target SAD "An hour on this mat. {.}/It says WELCOME. {.}It lies."
  [R8]
  say target NEUTRAL "I brought a recipe and a / letter. {.}Nobody reads / the letter."
  [HOLD 8]
  say target HAPPY "A key would be nice. {.}I / hear they come in muffins."
  choice target "Help her get in?" { "Check the muffin." -> muffin ; "Knock. Politely." -> knock ; "Not now." }
  end
muffin:
  say nar "(You break the housewarming / muffin open. {.}There is / a key in it.)"
  [R13]
  say target SHOCK "Of COURSE there's a key. {!}"
  [R5]
  say target HAPPY "In Hellvile that's a / Tuesday. {.}Come in. I'll / burn something for you."
  befriend nana 2
  reward 20 ITEM_CUPCAKE
  end
knock:
  say nar "(You knock. {.}It opens on / the second knock, the / polite one.)"
  [R8]
  say target HAPPY "It was never locked. {.}I / pushed. It's a PULL door."
  [R13]
  say target NEUTRAL "Do not tell the town."
  befriend nana 2
  reward 20 ITEM_CUPCAKE
end

script coop_ember_2
  say target NEUTRAL "Do you smell that? {.}Sweet, / and wrong. {.}It's coming / from the new shop."
  choice target "Follow the smell?" { "Bring samples." -> a ; "Taste it first." -> b ; "Not now." }
  end
a:
  say target HAPPY "Three samples. {.}Grape / will never know. {.}Now / hush. I'm cooking."
  say nar "(The room goes very quiet. / Somebody cries a little.)"
  befriend steamy 2
  reward 30 ITEM_SALAD
  end
b:
  say target SHOCK "You ATE the evidence?"
  say target HAPPY "...Fair. {.}It needs / salt. I'll fix it."
  befriend steamy 2
  reward 30 ITEM_SALAD
end

script coop_ember_3
  say target SHOCK "Boards up! {.}The red night / is here. {.}The stove / stays lit."
  say target NEUTRAL "Pilot light. {.}Three / waves. {.}Hold the door."
  choice target "Keep the stoves lit?" { "Hold the door." -> a ; "Feed the line." -> b ; "Not now." }
  end
a:
  say nar "(Three waves come. / The pilot light never / blinks.)"
  say target HAPPY "Whoever comes in / eats. {.}That's the / house rule."
  befriend grampy 2
  reward 30 ITEM_CUPCAKE
  end
b:
  say target HAPPY "Casserole for everyone. / It is not a trap."
  say nar "(Nobody checks the exits.)"
  befriend grampy 2
  reward 30 ITEM_CUPCAKE
end

script coop_ember_4
  say target HAPPY "One dish from each / side of the Wall. {.}And / two rivals at one table."
  say target NEUTRAL "Nobody checks the / exits this time."
  choice target "One table, both sides?" { "Seat the rivals." -> a ; "Cook both dishes." -> b ; "Not now." }
  end
a:
  say nar "(They sit. {.}They glare. / They pass the salt.)"
  say target HAPPY "Sit. Sit! {.}It's not / a trap, it's a casserole."
  befriend mayor 2
  home GRANT furn=teacup_table
  reward 30 ITEM_SKULL_TRINKET
  end
b:
  say target HAPPY "Hellvile on the left. {.}Hilltop / on the right. {.}Eat."
  say nar "(The table goes quiet / in the good way.)"
  befriend mayor 2
  home GRANT furn=teacup_table
  reward 30 ITEM_SKULL_TRINKET
end

script coop_rook_1
  say target NEUTRAL "Not asleep. {.}Just / thorough. {.}Four things / are wrong in town."
  say target NEUTRAL "A door, a smile, / a shadow, a fountain."
  choice target "Help with the log?" { "Write them up." -> a ; "Check the fountain." -> b ; "Not now." }
  end
a:
  say nar "(Four lines in the log. / The fountain hums.)"
  say target HAPPY "Write it down. {.}If it's / nothing, it's a very / small nothing."
  befriend sootkin 2
  reward 30 ITEM_CANDY_EYEBALLS
  end
b:
  say target SHOCK "It hums. {.}You heard it too?"
  say target HAPPY "Then I'm not wrong."
  befriend sootkin 2
  reward 30 ITEM_CANDY_EYEBALLS
end

script coop_rook_2
  say target NEUTRAL "Sit up with me. {.}Things / move at night."
  say target SHOCK "The lamp. {.}The chair. / The lamp again."
  choice target "Watch the house?" { "Leave them be." -> a ; "Mark the spots." -> b ; "Not now." }
  end
a:
  say target NEUTRAL "Three objects. {.}Exactly / where they were left."
  say target HAPPY "There's a pattern. {.}Look."
  befriend hattie 2
  reward 30 ITEM_SALAD
  end
b:
  say nar "(You chalk each spot. / By dawn, one has moved.)"
  say target SHOCK "See? {.}Thorough."
  befriend hattie 2
  reward 30 ITEM_SALAD
end

script coop_rook_3
  say target NEUTRAL "The roof is mine. {.}I'll / call the next wave / a minute early."
  say target NEUTRAL "Board the east side."
  choice target "Heed the watch?" { "Board the east." -> a ; "Board the west." -> b ; "Not now." }
  end
a:
  say nar "(You board it. {.}The wave / hits the west.)"
  say target SHOCK "West! {.}I said WEST!"
  befriend anvil 2
  reward 30 ITEM_CUPCAKE
  end
b:
  say nar "(You board the west. / The wave hits the east. / The house holds.)"
  say target HAPPY "Close enough. {.}Log it."
  befriend anvil 2
  reward 30 ITEM_CUPCAKE
end

script coop_rook_4
  say target NEUTRAL "At the Wall I read / the whole log. {.}Out / loud."
  say target SAD "Nobody ever / wrote back."
  choice target "Last entry?" { "Write back." -> a ; "Listen." -> b ; "Not now." }
  end
a:
  say nar "(You write one line / on the last page.)"
  say target HAPPY "Somebody wrote back. {.}I / have to sit down."
  befriend mayor 2
  home GRANT furn=cherry_clock
  reward 30 ITEM_SKULL_TRINKET
  end
b:
  say nar "(The park goes quiet / and listens to an owl.)"
  say target HAPPY "I should have / said so sooner."
  befriend mayor 2
  home GRANT furn=cherry_clock
  reward 30 ITEM_SKULL_TRINKET
end

script coop_wick_1
  say target HAPPY "Ooh. {.}Shiny. {.}No. / Home. {.}Home is shiny."
  say target NEUTRAL "Four lights. {.}Help me / walk past them."
  choice target "Lead them home?" { "Hang a lantern." -> a ; "Walk them home." -> b ; "Not now." }
  end
a:
  say nar "(You hang one lantern / on a string. {.}It's / theirs.)"
  say target HAPPY "It's very small. {.}I / love it."
  befriend lulla 2
  reward 30 ITEM_CANDY_EYEBALLS
  end
b:
  say nar "(Four lights go by. / Wick only looks twice.)"
  say target HAPPY "Home. {.}Home is shiny."
  befriend lulla 2
  reward 30 ITEM_CANDY_EYEBALLS
end

script coop_wick_2
  say target SHOCK "The vape glow. {.}It's / so pretty. {.}I'm / walking. {.}Help."
  say target SAD "I feel fuzzy and / far away."
  choice target "Get there first?" { "Steer them out." -> a ; "Hug it out." -> b ; "Not now." }
  end
a:
  say nar "(You get there first / and steer them out.)"
  say target HAPPY "The fog is lifting. {.}Thank / you."
  befriend haze 2
  reward 30 ITEM_SALAD
  end
b:
  say nar "(An exorcism hug. {.}The / fuzz goes down a notch.)"
  say target HAPPY "Better. {.}Brighter, but / the good kind."
  befriend haze 2
  reward 30 ITEM_SALAD
end

script coop_wick_3
  say target NEUTRAL "I'll stand at the door / with the lantern. {.}You / cross by my light."
  say target NEUTRAL "Keep it lit till dawn."
  choice target "Hold the light?" { "Cross the street." -> a ; "Stay with them." -> b ; "Not now." }
  end
a:
  say nar "(The household crosses / by lantern light.)"
  say target HAPPY "I didn't move. {.}Not / once."
  befriend bloom 2
  reward 30 ITEM_CUPCAKE
  end
b:
  say nar "(You hold the door / until the sky goes grey.)"
  say target HAPPY "Dawn. {.}Look. {.}It's / even shinier."
  befriend bloom 2
  reward 30 ITEM_CUPCAKE
end

script coop_wick_4
  say target HAPPY "The festival. {.}I made / a lantern. {.}A bad / small one."
  say target NEUTRAL "Chosen. {.}By me."
  choice target "Light the lantern?" { "Let it go up." -> a ; "Hold it a moment." -> b ; "Not now." }
  end
a:
  say nar "(It lifts off, crooked / and tiny. {.}Everyone / watches.)"
  say target HAPPY "I've got it. {.}I've / got it. {.}It's very small."
  befriend fuzz 2
  home GRANT furn=honey_lamp
  reward 30 ITEM_SKULL_TRINKET
  end
b:
  say nar "(You hold it with them. / Then let go together.)"
  say target HAPPY "My own light."
  befriend fuzz 2
  home GRANT furn=honey_lamp
  reward 30 ITEM_SKULL_TRINKET
end
