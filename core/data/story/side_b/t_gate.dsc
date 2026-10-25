# side_b/t_gate.dsc - Chain T "Feed the Gate" (S183 S184 S207 S185 S186)
# (owner: S5; story_final.md sec.14.1; GRUMBLEGUTS_AND_DIRECTOR_NOTES.md,
# GRUMBLEGUTS_TONE.md). He kept the sealed gate ten years on the promise of
# a feast; only his stomach ever listened. A regular at Brimstone Bites, a
# line cook, a dessert for two, and the first visitors his gate ever had.
# Bawdy and suggestive with adults, a grumpy uncle with kids; GURGLE is
# plain text (big gurgles get the record scratch).
#
# S186 "The Feast of Rebuilding" is the chain's fifth part. Its @mission
# row is here, so the side table, the chain and the chapter counts are
# whole (test_side_table.c), but it has no scripts, giver spot or quest-log
# title yet: the Feast package (SPEC sec.7 S5 "the Feast", C9, C16, R15)
# writes them with the set piece. Until then nobody offers it.

# =============================================================================
# S183 "First Hellvile Regular" - Grumbleguts (729) + Mother Nox (778), at
# Brimstone Bites - Ch4 after Ep 4.6 - cooking, talk (absorbs heart S104)
# =============================================================================
@pack quests kind=side
@mission S183 giver=729 side=hill ch=4-5 after=BEAT_EP4_6 at=27,17 verbs=cooking chain=T step=1 later=6m reward=80 offer=s183_offer active=s183_again title="First Hellvile Regular" hint="Grumbleguts ordered the whole menu at Brimstone Bites. Help in the kitchen."
@pack sb_gate kind=side minutes=95

script s183_offer
  sfx SFX_THUD
  say nar "(Grumbleguts has sat down at / Brimstone Bites. {.}He has / ordered the menu.)"
  [R8]
  say nar "(The whole menu.)"
  [R8]
  say grumble LOVE "Brimstone Bites! In / HILLVILE. {.}It's ironic, / sugar. I love ironic."
  [R13]
  say 128 SHOCK "We're out of everything. / {..}Since an hour ago. He's / still ordering."
  [HOLD 8]
  say 128 SAD "The restaurant would like / him to stay. {.}The kitchen / would like help."
  choice 128 "Help in the kitchen?" { "Apron on." -> yes ; "Later." }
  end
yes:
  quest S183 active
  call s183_again
end

script s183_again
  say 128 NEUTRAL "Three Bites dishes. {.}Bagels / first."
  push cooking 0x600
  native CB_SIDE_GRUMBLE GRB_DISH
  say nar "(He eats with his back to / the room. The camera cuts / back. He is bigger.)"
  [R8]
  say grumble NEUTRAL "Six. {.}Needs salt."
  push cooking 0x1100
  native CB_SIDE_GRUMBLE GRB_DISH
  say nar "(The camera cuts back. {.}He is / bigger.)"
  [R8]
  say grumble HAPPY "Eight. {.}Needs more."
  [R13]
  sfx SFX_KAZOO
  say nar "(Honkers offers him a napkin.)"
  [R8]
  say grumble LOVE "Well, hello there, crisp / and folded."
  [R8]
  say nar "(He is flirting with the / napkin. {..}The napkin is not / interested.)"
  push cooking 0x500
  native CB_SIDE_GRUMBLE GRB_DISH
  say nar "(The camera cuts back. {.}He is / much bigger.)" [PUNCH]
  [R21]
  say nox NEUTRAL "My bone soup against your / best dish. {.}Let the belly / judge."
  [R8]
  sfx SFX_EAT
  say nar "(He eats both. With effort. / {.}A lot of effort.)"
  [R13]
  say nar "GURGLE: gurgle." [GURGLE]
  [HOLD 8]
  say grumble NEUTRAL "The belly says... {..}tie. {r}"
  [R8]
  say nar "(He turns to leave, rear first, / before anyone can argue.)"
  [R8]
  say nox NEUTRAL "The belly is a coward."
  [R8]
  say nar "GURGLE: gurgle?" [GURGLE]
  [R13]
  say nox NEUTRAL "...Fair."
  [R21]
  [MUTE 21]
  say grumble SAD "He said there'd be a feast / when the gate opened." [HEART]
  [HOLD 13]
  say grumble SAD "{..}Gate's open."
  [HOLD 13]
  say nar "(He looks at the empty plates.)"
  [HOLD 13]
  say grumble HAPPY "Huh. {.}He was right."
  [R8]
  say nar "GURGLE: gurgle." [GURGLE]
  [R13]
  say nar "(He pays with the gate key. / Taco Yaki frames it.)"
  reward 80 furn=framed_gate_key
  home PLACE_GIFT furn=framed_gate_key
  befriend grumble 3
  befriend 128 2
  befriend nox 1
  quest S183 done
  later 6m s183_later
end

script s183_later
  say nar "(The framed gate key in your / house has a second, smaller / key hung on it. {..}By nobody.)"
  [R8]
  say nar "(It's the key to the / gatehouse.)"
end

# =============================================================================
# S184 "Line Cook" - Taco Yaki (128) + Grumbleguts - Ch4-5
# cooking, minigame, timing
# =============================================================================
@pack quests kind=side
@mission S184 giver=128 side=hill ch=4-5 after=BEAT_EP4_6 at=23,17 verbs=minigame,timing chain=T step=2 later=6m reward=100 offer=s184_offer active=s184_again title="Line Cook" hint="Two burners, one apprentice who eats the ingredients. Flip on the beat."
@pack sb_gate kind=side

script s184_offer
  say 128 NEUTRAL "He wants to cook. {.}Not just / eat. COOK."
  [R8]
  say 128 SAD "I've never had an apprentice / who ate the ingredients. {..}I'm / about to."
  [HOLD 8]
  say grumble LOVE "I'm great with my hands, / darlin'. {.}Ask anyone. Don't / ask anyone."
  choice 128 "Run the duet?" { "Two burners. Go." -> yes ; "Later." }
  end
yes:
  quest S184 active
  call s184_again
end

script s184_again
  say 128 NEUTRAL "Flip on the beat. {.}I'll do / the left burner. He does the / right one."
  push minigame SMG_B_DUET
  jlt v0 1 again
  [R13]
  sfx SFX_BEEP
  say nar "(Beep. Beep. Beep. A forklift / backs in with the restock.)"
  [R8]
  say grumble NEUTRAL "I only had a light snack."
  [R8]
  say 128 NEUTRAL "That's the week's." [PUNCH]
  [R21]
  say nar "(His first dish, served to / someone else: Lady Smog.)"
  [R8]
  say smog HAPPY "...It's lovely."
  [R8]
  say nar "(He sucks his gut in.)"
  [HOLD 13]
  sfx SFX_BOING
  say nar "(It springs out. {r})"
  [R8]
  say nar "(GURGLE, for once, says / nothing. {.}It's full.)"
  [R21]
  [MUTE 21]
  say grumble SAD "Nobody ever ate anything I / made." [HEART]
  [HOLD 8]
  say grumble SAD "{..}Nobody ever came up to the / gate."
  [HOLD 13]
  say 128 HAPPY "They're coming now."
  [R8]
  say nar "(The queue is out the door.)"
  reward 100 furn=chefs_hat_xxl
  home PLACE_GIFT furn=chefs_hat_xxl
  befriend grumble 3
  befriend 128 3
  befriend smog 1
  quest S184 done
  later 6m s184_later
  end
again:
  say 128 SAD "Half the takoyaki is on the / ceiling. {.}The other half is / in him. Again?"
end

script s184_later
  say nar "(Flossie flosses him after / every shift now.)"
  [R8]
  say nar "(He lets her.)"
end

# =============================================================================
# S207 "Someone for Dessert" - Grumbleguts (729) - Ch5 after S184
# deliver, talk (grafted heart S105)
# =============================================================================
@pack quests kind=side
@mission S207 giver=729 side=hill ch=5-5 at=27,17 verbs=deliver chain=T step=3 later=6m reward=80 offer=s207_offer active=s207_again title="Someone for Dessert" hint="Grumbleguts made dessert for two. Find anyone to eat the other one."
@pack sb_gate kind=side

script s207_offer
  say grumble NEUTRAL "I made dessert. {.}For two."
  [R8]
  say grumble SAD "Find anyone to eat the other / one, would you? {..}Anyone."
  choice grumble "Find a dessert date?" { "Anyone at all?" -> yes ; "Later." }
  end
yes:
  quest S207 active
  call s207_again
end

script s207_again
  say nar "(The dessert is a cake / shaped like his own rear. / {.}It's very accurate.)"
  verb deliver smog cell=49,17 then=s207_smog   # map=hell
end

script s207_smog
  say smog NEUTRAL "That's... {.}No. Thank you. {r}"
  verb deliver 519 cell=44,30 then=s207_fang   # map=hell
end

script s207_fang
  say 519 NEUTRAL "I've seen that rear. {.}I've / seen enough of it. {r}"
  verb deliver haze cell=44,13 then=s207_haze   # map=hell
end

script s207_haze
  say haze NEUTRAL "Is it plain water flavour? / {..}No? {.}Then no. {r}"
  verb deliver 555 cell=13,37 then=s207_solemn   # map=hell
end

script s207_solemn
  say 555 NEUTRAL "Someone should."
  [R13]
  say nar "(Mr. Solemn sits down. He / has not smiled in two / hundred years.)"
  [R8]
  say nar "(He eats the cake without / comment.)"
  [MUTE 26]
  [HOLD 26]
  say nar "(They eat in silence.)"
  [HOLD 26]
  say 555 NEUTRAL "Close."
  [R8]
  say grumble NEUTRAL "To what?"
  [R13]
  say 555 NEUTRAL "...Once." [HEART]
  [NO BUTTON]
  [HOLD 26]
  say nar "(Next morning he takes Nox's / line-cook job. {.}And wears the / tight shirt.)"
  reward 80 furn=big_chair
  home PLACE_GIFT furn=big_chair
  befriend grumble 3
  befriend 555 2
  quest S207 done
  later 6m s207_later
end

script s207_later
  sfx SFX_CREAK
  say nar "(The Big Chair in your house / creaks. Three citizens fit / in it. {.}They slide off.)"
end

# =============================================================================
# S185 "Come Up and See My Gate" - Grumbleguts (729) - Ch5
# housing, deliver, talk
# =============================================================================
@pack quests kind=side
@mission S185 giver=729 side=hell ch=5-5 at=38,27 verbs=housing,deliver chain=T step=4 later=6m reward=150 offer=s185_offer active=s185_again title="Come Up and See My Gate" trophy="Come Up Sometime" hint="Decorate Grumbleguts's gatehouse for visitors, then invite three guests."
@pack sb_gate kind=side

script s185_offer
  say grumble LOVE "The gate's fixed. {.}Anvil and / Rivet welded the crack. Come / up and see it sometime."
  [R8]
  say grumble NEUTRAL "I mean it this time. {.}I want / to decorate the gatehouse. / For visitors."
  [HOLD 13]
  say grumble SAD "Nobody's ever visited. {..}I / have flowers, a heart from / Stencilla, and no idea."
  choice grumble "Decorate the gatehouse?" { "Let's make it nice." -> yes ; "Later." }
  end
yes:
  quest S185 active
  call s185_again
end

script s185_again
  say grumble NEUTRAL "Every piece is a seat. {.}And a / mirror. I insist on a mirror."
  push home 0x4B04
  home DECORATE_FOR 11
  [R13]
  say nar "(He admires himself in the / mirror. {r})"
  [R13]
  say grumble HAPPY "Guests. {.}Three. You ask, / sugar, they won't say no to / you."
  verb deliver fuzz cell=7,28 then=s185_fuzz   # map=hill
end

script s185_fuzz
  say fuzz NEUTRAL "I have a sermon. {.}And a / headache. {..}...What time?"
  verb deliver nox cell=7,13 then=s185_nox   # map=hell
end

script s185_nox
  say nox NEUTRAL "I'm busy. {.}I'll bring soup."
  verb deliver smog cell=49,17 then=s185_smog   # map=hell
end

script s185_smog
  say smog NEUTRAL "I couldn't possibly. {..}I'll / wear the hat."
  [R21]
  sfx SFX_DOOR
  say nar "(They all come anyway.)"
  [R13]
  say nar "(Every seat is sized for / him. The guests sit three to / a chair.)"
  [R8]
  say nar "(Fuzz, between Nox and Lady / Smog, is lifted off the / floor by the cushion.)"
  [HOLD 13]
  say fuzz NEUTRAL "...This is fine." [PUNCH]
  [R13]
  say nar "(Fuzz does not say 'no' once. / {.}Grumbleguts does not ask.)"
  [R21]
  [MUTE 21]
  say grumble SAD "So this is it. {.}The gate." [HEART]
  [HOLD 13]
  say grumble SAD "{..}Ten years."
  [HOLD 8]
  say nox NEUTRAL "It's a good gate."
  [HOLD 13]
  say grumble SAD "...Nobody ever said that."
  reward 150 furn=very_large_seat
  home PLACE_GIFT furn=very_large_seat
  befriend grumble 4
  befriend fuzz 1
  befriend nox 2
  befriend smog 2
  quest S185 done
  later 6m s185_later
end

script s185_later
  say nar "(Next morning a little heart / is painted on the Gate. / {..}Nobody knows who did that.)"
  [R8]
  say nar "(Everybody knows.)"
  [R13]
  say nar "(The Gate's maintenance notice: / 'visitors: 4'.)"
end

# =============================================================================
# S186 "The Feast of Rebuilding" - Grumbleguts (729) - Ch5, weekends
# (the row only; the Feast package writes the rest - see the top of file)
# =============================================================================
@pack quests kind=side
@mission S186 giver=729 side=park ch=5-5 verbs=cooking,exorcism,find,timing chain=T step=5 later=10m reward=300
