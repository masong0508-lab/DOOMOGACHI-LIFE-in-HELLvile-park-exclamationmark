# ch1/ep15.dsc - Ep 1.5 "Housewarming" (owner: S2; story_final.md sec.10).
# The house becomes yours; the live-placement party; the first clear scare
# at the window. Legacy beat C1_CUPCAKE: stage 0 Nana's cupcake, 1 Miss
# Lulla, 2 home for six. Nine gifts through HOME PLACE_GIFT (each tagged
# "gift from X" by the catalog's giver; the house drops NEW gifts in when
# it opens), then the toast, the ripple, the window.
@pack ep15 kind=episode chapter=1 minutes=25

script e15_nana
  say nana HAPPY "Three new friends already? / You're a natural, sweet pea!"
  say nana "Do an old sheep a favour: / take this cupcake to Miss / Lulla in the park."
  give ITEM_CUPCAKE
  setf SF_CUPCAKE_GIVEN
  sfx SFX_PICKUP
  say nar "(You got a cupcake. It has / a little sugar heart.)"
  [R13]
  [R8]
  say nana HAPPY "And be home at six! / {.}No reason! Shoo!"
  native CB_C12 C12A_STEP 1
end

script e15_lulla
  say lulla SHOCK "Is that... a Nana Pudding / cupcake? With the little / sugar heart?"
  take ITEM_CUPCAKE
  sfx SFX_EAT
  say lulla LOVE "*nom* ...I'm going to cry. / I'm crying. Thank you, @P."
  say lulla NEUTRAL "The children drew pictures / today. Look."
  [R13]
  say nar "(Eleven drawings. Every one / has a purple cloud in it, / wearing a crown.)"
  [R8]
  say lulla NEUTRAL "They all drew the same / friend. I don't know who / he is."
  say lulla "And the fog by the Wall / giggles at night. Fog / shouldn't giggle."
  befriend lulla 4
  reward 40
  native CB_C12 C12A_STEP 2
end

script e15_early
  say nar "(The curtains are drawn. / Somebody inside goes / SHHH.)"
end

# nine guests at 8 frames each, then the ripple
beatscript b15_arrivals
  +0 sfx SFX_THUD
  +8 sfx SFX_THUD
  +16 sfx SFX_THUD
  +24 sfx SFX_THUD
  +32 sfx SFX_THUD
  +40 sfx SFX_THUD
  +48 sfx SFX_THUD
  +56 sfx SFX_THUD
  +64 sfx SFX_THUD
  +69 npc mayor RIPPLE 6
  +77 end
end

beatscript b15_window
  +0 npc mascotty STILL ; mute 41
  +21 caption "{d}Ignore that one."
  +42 label L_PUNCH
L_PUNCH:
  +0 npc mascotty HOP ; nobutton
  +8 npc mascotty EMOTE EMO_HEART
  +21 iris_out 0 69
  +34 end
end

script e15_party
  native CB_C12 C12A_TIME_AT 18
  sfx SFX_DOOR
  say nar "(Six o'clock. You open the / door, and...)"
  music SONG_FESTIVAL
  shake SHK_POW
  say nar "{big}SURPRISE!{/}"
  [R21]
  cast mayor 3 8
  cast nana 5 8
  cast pip 6 9
  cast fuzz 2 8
  cast bonejangles 7 8
  cast lulla 8 9
  cast bloom 4 9
  runbeat b15_arrivals
  home PLACE_GIFT furn=nanas_rug
  say nana HAPPY "A rug. Every home needs one / thing that's been walked / on with love."
  home PLACE_GIFT furn=pips_bee_drawing
  say pip HAPPY "I drew you a bee! It's you. / You're the bee."
  home PLACE_GIFT furn=fuzz_cushion
  say fuzz HAPPY "A cushion. Cuddle-proofed. / I tested it. Extensively."
  [R13]
  home PLACE_GIFT furn=kevins_bust
  say nar "(A bust of someone called / Kevin, delivered by Kevin. / He leaves before you ask.)"
  home PLACE_GIFT furn=xylophone_table
  say bonejangles HAPPY "An end-table! It's also a / xylophone. Clack!"
  [R8]
  home PLACE_GIFT furn=lulla_nightlight
  say lulla HAPPY "A nightlight. For you, not / the egg. Everyone needs / one."
  [R8]
  home PLACE_GIFT furn=mayor_photo
  say mayor HAPPY "A framed photo of me! For / when you miss me. Which / will be constantly!"
  [R13]
  home PLACE_GIFT furn=bloom_thermometer
  say bloom NEUTRAL "A thermometer. Decorative. / {..}Mostly decorative."
  [R13]
  home PLACE_GIFT furn=rivet_coat_hook
  say nar "(On the step: a welded coat / hook, and a note: don't / make it weird.)"
  say nar "(Put your presents where / you like. Everyone wants / to see where theirs goes.)"
  push home -1
  say nar "(@E climbs onto Kevin's bust / and sits there like it / was always the plan.)"
  [R13]
  say mayor HAPPY "A toast! To @P! To new / friends! To fluffy--"
  npc mayor FREEZE
  [MUTE 13]
  [HOLD 13]
  say mayor NEUTRAL "--cupcakes. Fluffy / cupcakes."
  npc mayor THAW
  [R8]
  say nar "(Nana looks at him for / exactly as long as it takes / him to notice.)"
  [R13]
  npc mayor RIPPLE 6
  say nar "(Everybody waves goodbye in / a ripple, and goes home.)"
  cast mayor 0 0 0x80
  cast nana 0 0 0x80
  cast pip 0 0 0x80
  cast fuzz 0 0 0x80
  cast bonejangles 0 0 0x80
  cast lulla 0 0 0x80
  cast bloom 0 0 0x80
  sfx SFX_DOOR
  [NIGHT]
  [MUTE 21]
  [HOLD 21]
  cast mascotty 6 8
  runbeat b15_window
  [HOLD 21]
  iris off
  cast mascotty 0 0 0x80
  native CB_C12 C12A_EP_DONE BEAT_EP1_5
  beat C1_WALL
end
