# core/tests/story_register/bad.dsc - the register lint's seeded bad tree
# (E5; core/tools/story_register.py --selftest, run by make -C core
# story-check). Every line marked "expect: RULE" must be reported with
# that rule, and nothing else may be: the clean scripts at the end are the
# false-positive controls. Never compiled into the game (textpack.py reads
# core/data/story only).
@pack regbad kind=side

# K1: an adult swears in Pip's scene
script rb_swear
  say pip HAPPY "Patrol rules. No running."
  say rivet ANGRY "Oh, damn it. {.}Fine." # expect: K1
end

# K1 + K2: Grumbleguts' come-on with a kid in the scene
script rb_love
  say grumble LOVE "Hey there, hot stuff. {.}You / look frisky." # expect: K1 K2
  say ID_WISP SAD "i'm right here."
end

# K2: the bracket form of the LOVE face, a teen in the scene (Homework, 40)
script rb_bracket
  say 40 NEUTRAL "Ugh. Adults."
  [LOVE] # expect: K2
  say vex "Sweetie. {.}Call me."
end

# K1 through a beat script the scene runs: the kid is only in the beat
beatscript rb_beat
  +0 npc pip HOP
  +5 say nar "(A sign: 'NAKED KARAOKE / NIGHT'.)" # expect: K1
  +13 end
end

script rb_runs_beat
  runbeat rb_beat
  say mayor HAPPY "Lovely day!"
end

# K1 via the giver of a mission: Pip gives it, nobody names Pip inside
@mission S100 giver=42 side=hill ch=1-1 at=1,1 verbs=find later=5m reward=0 offer=rb_offer active=rb_offer title="Bad" hint="Bad."
script rb_offer
  say nar "(The bin smells of booze.)" # expect: K1
end

# K1: a kids' scene by kid_cast.txt (@scene e14_lulla), no kid citizen in it
script e14_lulla
  say lulla HAPPY "Shh, the little ones are / napping. {.}Unlike my / hangover." # expect: K1
end

# K1: the sexual class with the pet in the scene
script rb_pet
  say nar "@E, very quietly: beep."
  say hattie HAPPY "My act tonight? {.}Sexy / accounting." # expect: K1
end

# K4: a KID citizen (Button Nose, 18) the kid cast does not list
script rb_unlisted
  say 18 HAPPY "Hi!" # expect: K4
end

# C1: the f-word over its cap of two in the whole game (adults only: no K1)
script rb_cap
  say vex ANGRY "Oh, for fuck's sake."
  say hattie ANGRY "What the fuck."
  say bloom NEUTRAL "Fucking marvellous." # expect: C1
end

# ---- controls: none of these may be reported ----------------------------
# adults may swear and flirt (within R1-R2 and the caps)
script rb_ok_adults
  say grumble LOVE "Come up and see my gate, / handsome. {.}Damn."
  say vex "Booze. Sexy booze."
end

# the pet does not bar swearing or the LOVE face, only the sexual class
script rb_ok_pet
  say nar "@E: beep."
  say mayor LOVE "Manners! Oh, I LOVE you. / Damn, I'm emotional."
end

# the place is not the oath; sugar and darlin' are not on the list
script rb_ok_kid
  say pip HAPPY "The Hell Breach! {.}Patrol / goes everywhere."
  say nana HAPPY "Sugar, darlin', a cookie / for the brave one."
end
