# side_a/d_nana.dsc - Chain D "Nana Puddin, Charlotte and Scorchmello" (S199
# S110 S111 S112 S113; cross-border) (owner: S4; story_final.md sec.14.1).
# Nana and Charlotte baked together before the Wall; every Thursday since,
# a muffin through a loose brick.

# =============================================================================
# S199 "The Loose Brick" - Nana Puddin (1) - Ch1-2 (night) - deliver, find
# =============================================================================
@pack quests kind=side
@mission S199 giver=1 side=hill ch=1-2 after=BEAT_EP1_5 at=25,17 verbs=deliver,find chain=D step=1 later=7m reward=30 offer=s199_offer active=s199_again title="The Loose Brick" hint="After dark, put Nana's muffin in the brick that wiggles, by the Wall."
@pack sa_nana kind=side minutes=95

script s199_offer
  say nana NEUTRAL "Sweet pea. {.}A favour."
  [R8]
  say nana NEUTRAL "After dark, put this muffin / in the brick that wiggles. / In the Wall."
  [HOLD 13]
  say nana SAD "And don't wait."
  choice nana "Take the muffin?" { "After dark." -> yes ; "Not tonight." }
  end
yes:
  quest S199 active
  give ITEM_CUPCAKE
  call s199_again
end

script s199_again
  [NIGHT]
  sfx SFX_CREAK
  say nar "(Night. The Wall breathes / out cold air. {.}One brick / wiggles.)"
  verb find cell=54,26 then=s199_brick
end

script s199_brick
  native CB_SIDE_HAVE ITEM_CUPCAKE
  jlt v0 1 eaten
  take ITEM_CUPCAKE
  sfx SFX_THUD
  say nar "(You push the muffin into the / gap.)"
  [MUTE 26]
  [HOLD 26]
  say nar "(You wait anyway.)"
  [HOLD 26]
  [FLASH W8,3]
  shake SHK_TAP
  sfx SFX_CREAK
  say nar "(A hand with soot on it / takes the muffin. From the / other side.)"
  [HOLD 21]
  sfx SFX_THUD
  say nar "(A bag of cookies is pushed / back through. {.}Ninety / percent charcoal.)"
  [R21]
  [MUTE 21]
  say nana SAD "Every Thursday. {.}She never / missed one." [HEART]
  [R13]
  say nana SAD "{..}Neither did I."
  reward 30 ITEM_CHARRED 3 furn=cookie_tin
  befriend nana 3
  quest S199 done
  later 7m s199_later
  end
eaten:
  say nar "(You had a muffin. {.}You don't / now. {..}Nana will bake / another.)"
  give ITEM_CUPCAKE
end

script s199_later
  say nar "(The cookie tin at home has / one burnt cookie in it. / You didn't put it there.)"
end

# =============================================================================
# S110 "The Secret Ingredient" - Nana Puddin (1) - Ch1 - cooking, fetch
# =============================================================================
@pack quests kind=side
@mission S110 giver=1 side=hill ch=1-2 after=BEAT_EP1_4 at=25,17 verbs=cooking chain=D step=2 later=5m reward=30 offer=s110_offer active=s110_again title="The Secret Ingredient" hint="Bake with Nana, then taste every batch of her melon bread."
@pack sa_nana kind=side

script s110_offer
  say nana NEUTRAL "My melon bread isn't right. / {.}Hasn't been for ten years."
  [R8]
  say nana HAPPY "Taste-test five batches / for me?"
  choice nana "Be Nana's taste-tester?" { "With pleasure." -> yes ; "I'm full." }
  end
yes:
  quest S110 active
  call s110_again
end

script s110_again
  push cooking 0
  say nana NEUTRAL "Batch one."
  [R13]
  choice nana "Batch one is..." { "Sweeter?" ; "Saltier?" ; "More melon?" }
  say nana NEUTRAL "Batch two."
  [R13]
  sfx SFX_EAT
  [R13]
  say nana NEUTRAL "Batch three."
  [R13]
  sfx SFX_EAT
  [R13]
  say nana NEUTRAL "Batch four."
  [R13]
  sfx SFX_EAT
  [R13]
  say nana HAPPY "And batch five."
  [HOLD 13]
  say nar "(Batch five is identical to / batch one.)"
  [R8]
  say nana HAPPY "I know. {..}I just like / watching you eat."
  [R13]
  say nana NEUTRAL "It's missing bone flour. / {.}Can't get it anymore. {..}Don't / ask why."
  [R21]
  [MUTE 21]
  say nar "(The recipe card has a second / name on it, scribbled out.)" [HEART]
  [HOLD 13]
  say nana SAD "Charlotte and I used to bake / together. {..}Different sides / of the park."
  [R13]
  say nana HAPPY "Eat up."
  reward 30 ITEM_PANCAKES 3 furn=nanas_recipe_card
  befriend nana 3
  quest S110 done
  later 5m s110_later
end

script s110_later
  say nar "(Nana's recipe card hangs in / your kitchen. The scribbled / name is a C.)"
end

# =============================================================================
# S111 "Muffin Recall" - Nana Puddin (1) - Ch2 - fetch, find, talk
# =============================================================================
@pack quests kind=side
@mission S111 giver=1 side=hill ch=2-2 after=BEAT_EP2_1 at=25,17 verbs=find chain=D step=3 later=5m reward=40 offer=s111_offer active=s111_again title="Muffin Recall" hint="Get the purple muffins back from Oma Fluff and Zoomzoom before they bite."
@pack sa_nana kind=side

script s111_offer
  say nana SHOCK "A batch came out PURPLE."
  [R8]
  say nana SAD "I sold six before I noticed. / {..}Oma Fluff has one. / Zoomzoom too."
  choice nana "Recall the muffins?" { "Before they bite!" -> yes ; "Later." }
  end
yes:
  quest S111 active
  call s111_again
end

script s111_again
  say nana NEUTRAL "Oma's by her bench on the / lane. {.}Zoomzoom is... / everywhere."
  verb find cell=7,10 then=s111_oma
end

script s111_oma
  cast 27 6 10 1
  npc 27 FREEZE
  say nar "(Oma Fluff, mid-bite.)"
  [HOLD 8]
  say 27 NEUTRAL "I was only looking at it."
  npc 27 THAW
  sfx SFX_PICKUP
  verb find cell=18,9 then=s111_zoom
end

script s111_zoom
  cast 8 17 9 1
  npc 8 HOP
  say 8 HAPPY "We ate it in under four / seconds."
  [R8]
  say 8 HAPPY "We are fast."
  [HOLD 13]
  say nar "(Zoomzoom says 'we' for the / rest of the day.)"
  [R21]
  [MUTE 21]
  say nana NEUTRAL "Back in the basket. {.}All / of them."
  [R13]
  say nana SHOCK "{..}They're warm."
  [HOLD 13]
  say nana NEUTRAL "I baked those yesterday."
  sfx SFX_CREAK
  say nar "(She puts the lid on. The lid / lifts once by itself.)"
  [HOLD 21]
  reward 40 ITEM_CUPCAKE
  befriend nana 2
  quest S111 done
  later 5m s111_later
end

script s111_later
  say 8 HAPPY "We have a joke for the / festival. {.}It's one word."
  [R8]
  say 8 HAPPY "{..}We."
end

# =============================================================================
# S112 "The Other Half of the Card" - Charlotte (566, Hellvile) - Ch4
# deliver, cooking
# =============================================================================
@pack quests kind=side
@mission S112 giver=566 side=hell ch=4-5 after=BEAT_EP4_2 at=20,22 verbs=deliver,cooking chain=D step=4 later=6m reward=80 offer=s112_offer active=s112_again title="The Other Half of the Card" hint="Carry Charlotte's half of the recipe card to Nana. Then bake."
@pack sa_nana kind=side

script s112_offer
  say 566 HAPPY "My cookies are ninety / percent charcoal. {.}And they / love you."
  [R8]
  say 566 SAD "I have half a recipe card. / {..}Torn down the middle."
  choice 566 "Take it to Nana?" { "Gladly." -> yes ; "Not today." }
  end
yes:
  quest S112 active
  call s112_again
end

script s112_again
  say nar "(Half a card. Half a melon / bread. Half a name: CHARLO.)"
  verb deliver nana cell=25,17 fragile then=s112_nana   # map=hill
end

script s112_nana
  npc nana STILL
  say nar "(Nana goes very still.)"
  [HOLD 21]
  sfx SFX_TINK
  say nar "(She tapes the two halves / together.)"
  push cooking 0x600
  say nar "(The old Mingle bagels: / cloud flour AND bone flour. / Warm. {.}Right.)"
  verb deliver 566 then=s112_burn
end

script s112_burn
  sfx SFX_THUD
  say nar "(Charlotte puts the perfect / loaf back in the oven. On / purpose.)"
  [R13]
  say 566 HAPPY "So it tastes like home."
  [R8]
  say nana ANGRY "{big}CHARLOTTE.{/}"
  [R8]
  say 566 NEUTRAL "It's a texture."
  [R21]
  [MUTE 21]
  say nana SAD "I baked it wrong on purpose. / {..}For ten years." [HEART]
  [HOLD 13]
  say nana SAD "So I'd remember it was / missing something."
  reward 80 furn=two_sided_recipe
  befriend 566 4
  befriend nana 2
  quest S112 done
  later 6m s112_later
end

script s112_later
  say nar "(The bakery sign has a second / line now: & CHARLOTTE.)"
end

# =============================================================================
# S113 "Bake for a Thousand" - Scorchmello (669, Hellvile) + Nana - Ch5
# cooking, fetch, minigame
# =============================================================================
@pack quests kind=side
@mission S113 giver=669 side=hell ch=5-5 at=17,40 verbs=fetch,cooking chain=D step=5 later=6m reward=200 offer=s113_offer active=s113_again title="Bake for a Thousand" trophy="Three Bakers" hint="Bring a Cupcake from Hillvile and a Brimstone Bagel from Hellvile."
@pack sa_nana kind=side

script s113_offer
  say 669 HAPPY "The Unity Loaf! {.}One loaf. / A thousand slices."
  [R8]
  say 669 NEUTRAL "Three bakers. One oven. / {..}No agreement about toast."
  choice 669 "Help bake the Unity Loaf?" { "Both sides!" -> yes ; "Later." }
  end
yes:
  quest S113 active
  say 669 HAPPY "Something sweet from your / side. Something burnt from / mine."
end

script s113_again
  native CB_SIDE_HAVE ITEM_CUPCAKE
  jlt v0 1 need
  native CB_SIDE_HAVE ITEM_BRIMSTONE_BAGEL
  jlt v0 1 need
  take ITEM_CUPCAKE
  take ITEM_BRIMSTONE_BAGEL
  push cooking 0x100
  say nar "(Three grannies shout at / once. The oven is on fire. / That's allowed.)"
  [R8]
  say nar "(Scorchmello toasts the / crust black. Nana scrapes / it. Charlotte puts it back.)"
  [R13]
  say nar "(On the Unity Stage, they cut / the loaf.)"
  [HOLD 13]
  sfx SFX_POP2
  say nar "{big}*tink*{/} {.}(There's a key / inside.)"
  [R8]
  say nar "(Everyone turns to Nana.)"
  [HOLD 13]
  say nana NEUTRAL "{d}Habit."
  [R21]
  [MUTE 21]
  say tallyworn NEUTRAL "I'll hang it on the old / gatepost. Where there's no / lock." [HEART]
  [HOLD 13]
  say tallyworn HAPPY "In case anyone's ever scared / again. {..}So they can see / it's open."
  reward 200 furn=unity_loaf
  befriend 669 4
  quest S113 done
  later 6m s113_later
  end
need:
  say 669 NEUTRAL "A Cupcake from Nana's. A / Brimstone Bagel from ours. / {..}Both, or it's just a loaf."
end

script s113_later
  jif SF_GRUMBLE_DEAD quiet
  say grumble HAPPY "Any leftovers? {.}From the / loaf?"
  [R8]
  say nar "(There are none.)"
  [R13]
  say grumble HAPPY "{..}Any leftovers?"
  end
quiet:
  say nar "(The Unity Loaf sits under / glass in your house. @E / sniffs it every morning.)"
end
