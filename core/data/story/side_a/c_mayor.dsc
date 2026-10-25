# side_a/c_mayor.dsc - Chain C "Mayor Giggs" (S196 S107 S108 S197 S198 S109;
# Hillvile, then cross-border) (owner: S4; story_final.md sec.14.1). The
# Mayor's "Hee. Hee." is a wound, named only in S198; until then it is a
# laugh line, and it is never mocked.

# =============================================================================
# S196 "Flowers for a Cloud" - Mayor Giggs (0) - Ch1 - fetch, deliver
# =============================================================================
@pack quests kind=side
@mission S196 giver=0 side=hill ch=1-2 after=BEAT_EP1_2 at=29,8 verbs=fetch,deliver chain=C step=1 later=6m reward=30 offer=s196_offer active=s196_again title="Flowers for a Cloud" hint="Bring Flowers to the little grave in the graveyard for the Mayor."
@pack sa_mayor kind=side minutes=100

script s196_offer
  say mayor HAPPY "Town beautification! {.}Hee. / Hee."
  [R8]
  say mayor NEUTRAL "Flowers. For one grave. / {..}Specifically."
  [HOLD 8]
  say mayor HAPPY "Any flowers. The shop has / them, or the flower beds."
  choice mayor "Take flowers to a grave?" { "Of course." -> yes ; "Later." }
  end
yes:
  quest S196 active
  say mayor NEUTRAL "The little one. {.}By the / fence. You'll know it."
end

script s196_again
  native CB_SIDE_HAVE ITEM_FLOWERS
  jlt v0 1 none
  say nar "(The graveyard. The little one / by the fence.)"
  verb find cell=8,34 then=s196_grave
  end
none:
  say mayor NEUTRAL "Flowers first. {.}Any kind. / {..}Not grape-coloured."
end

script s196_grave
  take ITEM_FLOWERS
  say nar "(A small stone. In a child's / hand: FLUFFY THE CLOUD. / DRIFTED.)"
  [HOLD 13]
  cast mayor 9 33 1
  sfx SFX_BLIP
  say nar "(The Mayor is already here.)"
  [SAD]
  say mayor "Oh! {.}I'm, ah. Lost."
  [R8]
  say mayor HAPPY "I'm inspecting the... gravel. / {..}Hee. Hee."
  [HOLD 13]
  say nar "(He inspects it. {..}It is / gravel.)"
  [R21]
  [MUTE 21]
  say nar "(You place the flowers. He / watches them for exactly / eight beats.)" [HEART]
  [R8]
  say nar "(The handwriting on the / stone is the handwriting on / his speech notes.)"
  reward 30 furn=small_cloud_stone
  befriend mayor 3
  quest S196 done
  later 6m s196_later
end

script s196_later
  say nar "(The flowers on the little / grave have been changed. / Nobody did it in daylight.)"
end

# =============================================================================
# S107 "Speech Notes" - Mayor Giggs (0) - Ch1 - find, talk
# =============================================================================
@pack quests kind=side
@mission S107 giver=0 side=hill ch=1-2 after=BEAT_EP1_4 at=29,8 verbs=find chain=C step=2 later=5m reward=20 offer=s107_offer active=s107_again title="Speech Notes" hint="Find the Mayor's speech pages: the fountain, the bakery, Mascotty."
@pack sa_mayor kind=side

script s107_offer
  say mayor SHOCK "My welcome speech! {!}Gone!"
  [R8]
  say mayor NEUTRAL "I've given it four hundred / times. {..}I can't remember / a word."
  choice mayor "Find his speech notes?" { "I'll find them." -> yes ; "Not now." }
  end
yes:
  quest S107 active
  call s107_again
end

script s107_again
  say mayor NEUTRAL "The fountain. The bakery. / And, ah. {.}Mascotty might / have one."
  verb find cell=34,23 then=s107_p1
end

script s107_p1
  sfx SFX_SPLASH
  say nar "(Page one, by the fountain. / In the margin, where a / sentence was: 'Hee. Hee.')"
  verb find cell=25,16 then=s107_p2
end

script s107_p2
  sfx SFX_SPARKLE
  say nar "(Page two, under the bakery / bench. Flour on it. 'Hee. / Hee.' in the margin.)"
  say nar "(Page three: Mascotty has / it. In her paw. {.}She hasn't / moved.)"
  verb find cell=40,28 then=s107_p3
end

script s107_p3
  npc mascotty STILL
  say nar "(You stand very still beside / her.)"
  [HOLD 21]
  sfx SFX_CREAK
  say nar "(Her paw opens.)"
  [R13]
  say mayor HAPPY "My speech! {.}Ahem."
  [R8]
  say mayor HAPPY "Welcome to HaHaland, where / {.}{.}{.}hee. hee."
  [HOLD 13]
  say mayor HAPPY "It kills every time."
  [R13]
  say nar "(There's a fourth page. A / different hand: 'You never / asked where it went.')"
  [MUTE 14]
  [HOLD 14]
  say mayor NEUTRAL "{..}Old draft."
  reward 20 furn=framed_speech
  befriend mayor 2
  quest S107 done
  later 5m s107_later
end

script s107_later
  say nar "(The framed speech in your / house is one line long. The / frame is very nice.)"
end

# =============================================================================
# S108 "The Locked Drawer" - Mayor Giggs (0) - Ch2 - find, fetch, talk
# =============================================================================
@pack quests kind=side
@mission S108 giver=0 side=hill ch=2-2 after=BEAT_EP2_1 at=29,8 verbs=fetch,deliver chain=C step=3 later=5m reward=40 offer=s108_offer active=s108_again title="The Locked Drawer" hint="Keysie has the spare key to the Mayor's drawer. She likes Coffee."
@pack sa_mayor kind=side

script s108_offer
  say mayor NEUTRAL "Town Hall's bottom drawer is / locked."
  [HOLD 8]
  say mayor SHOCK "And it makes a noise."
  [R8]
  say mayor HAPPY "Would you open it? {.}So I / don't have to? {..}Hee."
  choice mayor "Open the drawer?" { "I'll open it." -> yes ; "Not today." }
  end
yes:
  quest S108 active
  say mayor NEUTRAL "Keysie has a spare key for / everyone. {.}I asked her to / lose this one."
end

script s108_again
  native CB_SIDE_HAVE ITEM_COFFEE
  jlt v0 1 none
  say nar "(Keysie works on coffee. Take / her one.)"
  verb deliver 21 cell=39,9 then=s108_key
  end
none:
  say mayor NEUTRAL "Keysie only 'finds' things / after coffee. {..}Brimstone / Bites has coffee."
end

script s108_key
  take ITEM_COFFEE
  say 21 HAPPY "Oh! {.}Look what I found."
  [R8]
  say 21 NEUTRAL "{..}A key I definitely lost."
  sfx SFX_DOOR
  say nar "(The drawer: a child's / drawing of two kids on a / blanket, and bone flour.)"
  say nar "(A cupcake eleven years old. / Perfectly preserved. {.}And a / frayed string with a loop.)"
  [R8]
  say nar "(The noise is the cupcake. / {..}It giggles.)"
  [HOLD 8]
  sfx SFX_SQUEAK
  [R8]
  say mayor HAPPY "It's a fresh one."
  [R13]
  say mayor NEUTRAL "It is not a fresh one."
  [R21]
  [MUTE 21]
  say nar "(The drawing is signed G + / N. He puts it back. He / doesn't lock the drawer.)" [HEART]
  [HOLD 13]
  say nar "(He puts the string in his / pocket without a word.)"
  reward 40
  befriend mayor 3
  befriend 21 2
  quest S108 done
  later 5m s108_later
end

script s108_later
  say nar "(A framed child's drawing / in the Mayor's office: two / kids on a blanket.)"
end

# =============================================================================
# S197 "The Plaque" - Mayor Giggs (0) - Ch2 - fetch, find
# =============================================================================
@pack quests kind=side
@mission S197 giver=0 side=hill ch=2-2 after=BEAT_EP2_3 at=29,8 verbs=fetch,find chain=C step=4 later=6m reward=30 offer=s197_offer active=s197_again title="The Plaque" hint="Polish the Wall's foundation plaque with Soap. For appearances."
@pack sa_mayor kind=side

script s197_offer
  say mayor HAPPY "The Wall's plaque needs a / polish. {.}For appearances."
  [R8]
  say mayor NEUTRAL "Soap will do. {..}Don't read / it. Just polish it."
  choice mayor "Polish the plaque?" { "Just polish it." -> yes ; "Later." }
  end
yes:
  quest S197 active
end

script s197_again
  native CB_SIDE_HAVE ITEM_SOAP
  jlt v0 1 none
  say nar "(The plaque is at the foot of / the Wall, by the lamp.)"
  verb find cell=54,28 then=s197_plaque
  end
none:
  say mayor NEUTRAL "Soap. {.}The shop. {..}Or the / hospital, it's cheaper there."
end

script s197_plaque
  take ITEM_SOAP
  say nar "(ERECTED FOR THE SAFETY OF... / The rest is under grime.)"
  sfx SFX_SQUEAK
  [R13]
  sfx SFX_SPLASH
  [R2]
  sfx SFX_SQUEAK
  [R13]
  sfx SFX_SPLASH
  [R2]
  sfx SFX_SQUEAK
  [R13]
  sfx SFX_SPLASH
  say nar "(The polish squeaks. The / fountain answers every / squeak, a beat late.)"
  [R8]
  cast mayor 53 28 1
  npc mayor FREEZE
  say nar "(A hand lands on yours.)"
  [HOLD 13]
  say mayor NEUTRAL "That's enough. {.}That's / plenty. {..}Hee."
  npc mayor THAW
  npc mayor WALK_TO cell=29,9
  [R21]
  sfx SFX_SQUEAK
  say nar "(You keep polishing. A / corner of a signature. In / crayon- coloured ink.)"
  [MUTE 14]
  [HOLD 14]
  reward 30 furn=polish_tin
  befriend mayor 2
  quest S197 done
  later 6m s197_later
end

script s197_later
  say nar "(The plaque shines. Under it / the crayon signature is a / little bigger than before.)"
end

# =============================================================================
# S198 "Tea for a Tic" - Mayor Giggs (0) - Ch3 (the church) - fetch, talk
# =============================================================================
@pack quests kind=side
@mission S198 giver=0 side=hill ch=3-3 after=BEAT_EP3_2 at=29,8 verbs=fetch chain=C step=5 later=5m reward=40 offer=s198_offer active=s198_again title="Tea for a Tic" hint="Bring the Mayor something warm to drink. Nana says tea. Coffee will do."
@pack sa_mayor kind=side

script s198_offer
  say mayor HAPPY "Hee. Hee. Hee. Hee."
  [R8]
  say mayor HAPPY "Hee. Hee. Hee. {..}Hee."
  [R8]
  say nana NEUTRAL "He can't stop. {.}Tea. Bring / him something warm."
  choice nana "Fetch something warm?" { "Right away." -> yes ; "In a bit." }
  end
yes:
  quest S198 active
end

script s198_again
  native CB_SIDE_HAVE ITEM_COFFEE
  jlt v0 1 none
  take ITEM_COFFEE
  say nar "(Mocha Latte brewed it from / the last tin in town. She / drew a cloud in the foam.)"
  [R8]
  say nar "(He drinks around the cloud.)"
  [R13]
  say nar "(The cloud is the last thing / in the cup.)"
  [MUTE 41]
  [HOLD 26]
  say nar "(The hee stops.)"
  [HOLD 26]
  say mayor SAD "That's worse." [HEART]
  [R13]
  say mayor SAD "{..}Hee."
  reward 40 furn=cloud_foam_teacup
  befriend mayor 3
  quest S198 done
  later 5m s198_later
  end
none:
  say mayor HAPPY "Hee. {..}Hee. {..}Something warm, / Nana says. Hee."
end

script s198_later
  say nar "(An empty cup on a pew. The / foam cloud dried on the / inside.)"
end

# =============================================================================
# S109 "The Apology Tour" - Mayor Giggs (0) - Ch4-5 after Ep 4.6
# escort, talk, cooking
# =============================================================================
@pack quests kind=side
@mission S109 giver=0 side=hill ch=4-5 after=BEAT_EP4_6 at=29,8 verbs=cooking,deliver chain=C step=6 later=8m reward=150 offer=s109_offer active=s109_again title="The Apology Tour" hint="Bake with the Mayor, then take his first apology cupcake to Tallyworn."
@pack sa_mayor kind=side

script s109_offer
  say mayor NEUTRAL "I want to apologise to every / Hellvile citizen. {.}In / person."
  [R8]
  say mayor HAPPY "There are five hundred. {..}I / have a wagon."
  choice mayor "Join the apology tour?" { "Let's bake." -> yes ; "Not yet." }
  end
yes:
  quest S109 active
  call s109_again
end

script s109_again
  push cooking 0x100
  sfx SFX_HAPPY
  say nar "(Five hundred cupcakes. One / wagon. The wagon creaks.)"
  [R8]
  say nar "(At every stop he starts a / speech. Someone takes a / cupcake: 'yeah, fine.')"
  [R13]
  say mayor HAPPY "Five hundred citizens. {.}Zero / speeches finished!"
  [R8]
  say 531 NEUTRAL "Itemised receipt: apology, / partial, times one."
  [R13]
  say mayor NEUTRAL "One more. {.}Tallyworn. {..}Take / him this one, would you?"
  verb deliver tallyworn cell=30,9 then=s109_hand
end

script s109_hand
  say tallyworn NEUTRAL "{..}No cupcake."
  [HOLD 13]
  say tallyworn NEUTRAL "A handshake."
  [R8]
  say nar "(The Mayor holds it a beat too / long.)"
  [MUTE 21]
  [HOLD 21]
  say tallyworn HAPPY "...Day one." [HEART]
  [R13]
  sfx SFX_KAZOO
  say nar "(Honkers honks at him.)"
  [R5]
  say mayor HAPPY "Ha!"
  [HOLD 8]
  say nar "(A real one. {.}Nobody moves.)"
  music SONG_HEEHEE_JINGLE
  reward 150 furn=mayoral_sash
  befriend mayor 4
  befriend tallyworn 3
  quest S109 done
  later 8m s109_later
end

script s109_later
  sfx SFX_SPLASH
  say nar "(The fountain giggles. {.}Right / on time.)"
end
