# coop/ember.dsc - Ember, the Hearth Imp: Ch1 side quest "Wrong Side of the Mat"
# (MP1 content; docs/COOP_HEROES.md). A co-op hero's side arc, never the plot: it
# sets no story flag. The speaker is `target`, the co-op housemate the player is
# talking to (a co-op hero's resident id is a household id, not a roster id).
# Nothing in the game starts this script yet: coop_ember_1 is the scene, the
# trigger and the saved "done" bit are the next step (docs/COOP_HEROES.md).
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
  end
knock:
  say nar "(You knock. {.}It opens on / the second knock, the / polite one.)"
  [R8]
  say target HAPPY "It was never locked. {.}I / pushed. It's a PULL door."
  [R13]
  say target NEUTRAL "Do not tell the town."
end
