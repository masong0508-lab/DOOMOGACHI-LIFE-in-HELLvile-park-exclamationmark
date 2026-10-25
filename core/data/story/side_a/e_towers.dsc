# side_a/e_towers.dsc - Chain S "Hillvile Towers, apartment 7B" (S114-S116;
# the second home) and Chain H "Steamy's bath house" (S127 S200 S128 S129;
# branches on SF_STEAMY_LOST) (owner: S4; story_final.md sec.14.1).
# Steamy is she (sec.13b). The 7B tenant who crossed back was Tallyworn.

# =============================================================================
# S114 "Apartment 7B" - Keysie (21) - Ch2 - find, housing
# =============================================================================
@pack quests kind=side
@mission S114 giver=21 side=hill ch=2-3 after=BEAT_EP2_1 at=39,9 verbs=find,housing chain=S step=1 later=6m reward=0 offer=s114_offer active=s114_again title="Apartment 7B" hint="Keysie has the key to 7B in Hillvile Towers. Clear the dust."
@pack sa_tower kind=side minutes=90

script s114_offer
  sfx SFX_TINK
  say 21 NEUTRAL "I hold a spare key for an / apartment nobody has rented / in ten years."
  [R8]
  say 21 NEUTRAL "The last tenant moved to the / other side of the park."
  [HOLD 8]
  say 21 HAPPY "Rent is whatever you've got."
  choice 21 "Take apartment 7B?" { "I'll take it." -> yes ; "Not yet." }
  end
yes:
  quest S114 active
  call s114_again
end

script s114_again
  say nar "(Seventh floor. The windows / face the Wall. Everything is / furnished in dust.)"
  home REMOVE_ONLY 1
  push home -2
  home REMOVE_ONLY 0
  say nar "(A dust sheet over the sofa / is shaped like a person.)"
  [HOLD 13]
  sfx SFX_WHOOSH
  say nar "(You pull it. {.}A sofa.)"
  [R8]
  say nar "(The sheet, on the floor, is / still shaped like a person.)"
  [R13]
  say 21 NEUTRAL "{d}It does that."
  [R21]
  [MUTE 21]
  say nar "(Tally chalk under the sill. / Three thousand six hundred / and fifty-one marks.)"
  [HOLD 21]
  say nar "(The last one is fresh.)"
  [HOLD 21]
  say 21 NEUTRAL "Keep what you find. {..}They / won't be back."
  reward 0 furn=bone_lamp
  befriend 21 3
  quest S114 done
  later 6m s114_later
end

script s114_later
  say nar "(7B's window is dark. {.}Below / it, on the sill, one fresh / chalk mark.)"
end

# =============================================================================
# S115 "Neighbours" - Gusty (92) and Curlicue (44) - Ch2-3 - housing, talk
# =============================================================================
@pack quests kind=side
@mission S115 giver=92 side=hill ch=2-3 after=BEAT_EP2_2 at=37,9 verbs=housing chain=S step=2 later=6m reward=60 offer=s115_offer active=s115_again title="Neighbours" hint="Make 7B cosy for the Towers' cosiest-floor contest. Mascotty judges."
@pack sa_tower kind=side

script s115_offer
  say 92 HAPPY "The Towers' cosiest-floor / contest! {.}7A and 7C have / never won."
  [R8]
  say 44 HAPPY "We need 7B. {.}As an ally. / Against the ninth floor."
  [HOLD 8]
  say 92 NEUTRAL "Make it cosy. We'll lend you / something each."
  choice 92 "Join the contest?" { "Team seven!" -> yes ; "Later." }
  end
yes:
  quest S115 active
  call s115_again
end

script s115_again
  say nar "(Gusty lends a pinwheel. / Curlicue lends a hairdryer. / 'Where I can see it.')"
  push home -2
  say nar "(The judge is Mascotty.)"
  npc mascotty STILL
  [HOLD 21]
  say nar "(She stands in 7A's doorway. / Nothing.)"
  [HOLD 21]
  say nar "(7C. {..}Nothing.)"
  [HOLD 21]
  say nar "(7B.)"
  [HOLD 21]
  sfx SFX_CREAK
  say nar "(She nods. {.}Once.)"
  [R8]
  sfx SFX_THUD
  say nar "(Gusty faints.)"
  [R8]
  say 44 HAPPY "She likes the rug."
  [R13]
  say nar "(The ninth floor wins again. / A single ghost with one / candle.)"
  [R21]
  [MUTE 21]
  say 507 SAD "I didn't want to be a / bother. {..}I leave biscuits. On / the stairs." [HEART]
  [HOLD 13]
  say nar "(Nobody had ever gone up to / say thank you. {.}You do.)"
  reward 60 furn=rattlecup_teacup
  befriend 92 2
  befriend 44 2
  befriend 507 3
  quest S115 done
  later 6m s115_later
end

script s115_later
  sfx SFX_TINK
  say nar "(Midnight. Rattlecup's / teacup rattles. {.}There's a / biscuit in the saucer.)"
end

# =============================================================================
# S116 "Room With a View" - Keysie (21) - Ch4-5 - housing, photo, talk
# =============================================================================
@pack quests kind=side
@mission S116 giver=21 side=hill ch=4-5 after=BEAT_EP4_1 at=39,9 verbs=housing,photo chain=S step=3 later=6m reward=100 offer=s116_offer active=s116_again title="Room With a View" trophy="Landlord" hint="Get 7B ready for its old tenant, then take Keysie's brochure photo."
@pack sa_tower kind=side

script s116_offer
  say 21 HAPPY "With the Wall gone, 7B faces / Hellvile Park."
  [R8]
  say 21 NEUTRAL "I'd like a photo. {.}For the / brochure."
  [HOLD 13]
  say 21 NEUTRAL "{..}There has never been a / brochure."
  choice 21 "Shoot the brochure?" { "Say cheese." -> yes ; "Later." }
  end
yes:
  quest S116 active
  call s116_again
end

script s116_again
  say 21 NEUTRAL "And the old tenant is coming / to see his flat. {.}Tidy it?"
  push home -2
  home DECORATE_FOR 6
  say nar "(Tallyworn sits in the window / of his old flat.)"
  [HOLD 26]
  say tallyworn NEUTRAL "Can I leave one thing?"
  [R8]
  say tallyworn NEUTRAL "The chalk. {.}On the sill."
  [MUTE 21]
  [HOLD 21]
  say tallyworn SAD "Someone else might need to / count something." [HEART]
  [HOLD 13]
  say 21 HAPPY "Now the brochure! Face me. / Step back. A."
  verb photo 21 cell=40,9 then=s116_photo
end

script s116_photo
  sfx SFX_SPARKLE
  say nar "(The brochure: one photo, / one line. 'Hillvile Towers: / now with a view.')"
  [R8]
  say 21 NEUTRAL "It used to say 'now with a / Wall'."
  reward 100 furn=tally_wall
  befriend 21 3
  befriend tallyworn 2
  quest S116 done
  later 6m s116_later
end

script s116_later
  say nar "(The brochure's second / printing has a second line: / 'and a chalk.')"
end

# =============================================================================
# S127 "Every Duck Has a Name" - Steamy (2) - Ch1-2 - find, talk
# =============================================================================
@pack quests kind=side
@mission S127 giver=2 side=hill ch=1-2 after=BEAT_EP1_4 at=27,43 verbs=find chain=H step=1 later=5m reward=30 offer=s127_offer active=s127_again title="Every Duck Has a Name" hint="One of Steamy's ducks isn't hers. Take it to the Wall after dark."
@pack sa_tower kind=side

script s127_offer
  sfx SFX_SQUEAK
  say steamy NEUTRAL "I named every duck I own. / {.}Except this one."
  [R8]
  say steamy SAD "I don't remember buying it. / {..}And I can't throw out a / duck."
  choice steamy "Solve the duck mystery?" { "On the case." -> yes ; "Later." }
  end
yes:
  quest S127 active
  call s127_again
end

script s127_again
  say nar "(Three suspects. Each squeaks / their own duck as an alibi.)"
  sfx SFX_SQUEAK
  say 64 HAPPY "Squeaker. {.}Clown. Mine's / called Honk."
  [R13]
  sfx SFX_SQUEAK
  say 112 HAPPY "Tidepool. Mine's called / Duuude."
  [R13]
  sfx SFX_SQUEAK
  say pip HAPPY "Pip. This is Mr. Flops."
  [R13]
  say pip NEUTRAL "{..}He's a duck today."
  [R8]
  say steamy NEUTRAL "At night it squeaks at the / Wall. {.}By itself."
  [NIGHT]
  verb find cell=54,40 then=s127_wall
end

script s127_wall
  say nar "(You set the duck on the foot / of the Wall.)"
  [MUTE 21]
  [HOLD 13]
  sfx SFX_SQUEAK
  [R3]
  sfx SFX_SQUEAK
  [R3]
  sfx SFX_SQUEAK
  [R13]
  sfx SFX_SQUEAK
  say nar "(Something squeaks back. Three, / then one.)"
  [HOLD 21]
  say nar "(The fog giggles. {..}In a / frog's voice.)"
  [HOLD 13]
  say steamy NEUTRAL "...That one's not mine / either."
  [R13]
  say steamy HAPPY "I'm keeping it. {.}Its name / is Guest."
  reward 30 furn=rubber_duck_guest
  befriend steamy 3
  quest S127 done
  later 5m s127_later
end

script s127_later
  sfx SFX_SQUEAK
  say nar "(Somewhere, a duck squeaks / at the Wall. {..}Three, then / one.)"
end

# =============================================================================
# S200 "Night Shift at the Bath-House" - Lather (41) - Ch2 after Ep 2.4 (night)
# find; no music for the whole quest; [NO GAG]
# =============================================================================
@pack quests kind=side
@mission S200 giver=41 side=hill ch=2-2 after=BEAT_EP2_4 at=21,26 verbs=find chain=H step=2 later=8m reward=30 offer=s200_offer active=s200_again title="Night Shift at the Bath-House" hint="Fetch Lather's soap from the closed bath house. Don't look at the water."
@pack sa_tower kind=side

script s200_offer
  say 41 SAD "The bath house is closed / since Steamy was taken."
  [R8]
  say 41 NEUTRAL "I left my soap inside. {.}Just / grab it."
  [HOLD 13]
  say 41 NEUTRAL "Don't look at the water."
  choice 41 "Fetch the soap?" { "I won't look." -> yes ; "Not at night." }
  end
yes:
  quest S200 active
  call s200_again
end

script s200_again
  [NIGHT]
  [SILENCE]
  [NO GAG]
  say nar "(Night. The bath house door / is open a crack.)"
  verb find cell=24,44 then=s200_in
end

script s200_in
  say nar "(The ducks in the main tub all / face the door.)"
  [HOLD 21]
  sfx SFX_SPLASH
  [R3]
  sfx SFX_SPLASH
  [R8]
  say nar "(The soap is on the edge of / the tub.)"
  [HOLD 13]
  tapestop 5
  sfx SFX_BUBBLE
  say nar "(The water giggles once. Low. / Much too low.)"
  [HOLD 26]
  sfx SFX_DOOR
  say nar "(Outside, on the step, one duck. / Facing the Wall.)"
  [HOLD 21]
  mute 0
  native CB_SIDE_MUSIC
  reward 30 ITEM_SOAP furn=facing_duck
  quest S200 done
  later 8m s200_later
end

script s200_later
  jif SF_STEAMY_LOST lost
  say nar "(Morning. Steamy has lined / the ducks up. All of them / face away from the Wall.)"
  end
lost:
  say nar "(Morning. Lather has lined / the ducks up. All of them / face away from the Wall.)"
end

# =============================================================================
# S128 "Grand Re-Opening" (saved) / "Steam Rises" (lost) - Steamy (2) or her
# ghost - Ch3 - fetch, cooking (soap), housing (her bath house)
# =============================================================================
@pack quests kind=side
@mission S128 giver=41 side=hill ch=3-3 after=BEAT_EP3_1 at=21,26 verbs=fetch,housing chain=H step=3 later=6m reward=60 offer=s128_offer active=s128_again title="Grand Re-Opening" hint="Bring 3 Soap to reopen the bath house for the people in the church."
@pack sa_tower kind=side

script s128_offer
  jif SF_STEAMY_LOST ghost
  say steamy HAPPY "I want the bath house open. / For the people in the church."
  [R8]
  say steamy NEUTRAL "Clean is a kind of brave."
  jmp ask
ghost:
  say 41 SAD "The water at the bath house / is warm again. {..}Nobody lit / the boiler."
  [HOLD 13]
  say 41 NEUTRAL "I think she wants it open. / {.}From the other side of the / water."
ask:
  choice nar "Reopen the bath house?" { "Clean is brave." -> yes ; "Later." }
  end
yes:
  quest S128 active
  say 41 NEUTRAL "Three Soap. {.}Rivet's welding / a sign."
end

script s128_again
  native CB_SIDE_HAVE ITEM_SOAP
  jlt v0 3 need
  take ITEM_SOAP 3
  say nar "(Towels, ducks, and a sign / Rivet welded: NO DEMONS.)"
  push home 0x4004
  home DECORATE_FOR 5
  say nar "(First bath: the daycare kids.)"
  [R8]
  say nar "(Mascotty arrives for a / bath. She gets in with the / head on.)"
  [HOLD 21]
  sfx SFX_SPLASH
  say nar "(The head floats. {..}Mascotty / does not.)"
  [R13]
  say nar "(Everyone stares at the / surface.)"
  [HOLD 13]
  sfx SFX_SPLASH
  say nar "(She surfaces. Head back on. / Nobody says anything.)"
  [R21]
  [MUTE 21]
  jif SF_STEAMY_LOST lost
  say steamy SAD "There was someone in my head / who hadn't been warm in ten / years." [HEART]
  [HOLD 13]
  say steamy HAPPY "I keep it hotter now."
  jmp done
lost:
  say nar "(Steam on the mirror. Letters / appear: WARM ENOUGH?)" [HEART]
  [HOLD 21]
  choice nar "Warm enough?" { "Yes." }
done:
  reward 60 furn=steamys_tub
  befriend 41 2
  quest S128 done
  later 6m s128_later
  end
need:
  say 41 NEUTRAL "Three Soap. {.}The shop, or the / hospital."
end

script s128_later
  say nar "(The bath house windows / glow. Somebody keeps the / water exactly warm enough.)"
end

# =============================================================================
# S129 "Mixed Bathing" - Steamy (2) or her ghost + Scaldie (513, Hellvile)
# Ch4-5 - talk, fetch, concert
# =============================================================================
@pack quests kind=side
@mission S129 giver=513 side=hell ch=4-5 after=BEAT_EP4_2 at=44,19 verbs=fetch,timing chain=H step=4 later=6m reward=120 offer=s129_offer active=s129_again title="Mixed Bathing" hint="Bring Scaldie a Coffee and settle the bath temperature: on the beat."
@pack sa_tower kind=side

script s129_offer
  say 513 ANGRY "The first cross-border bath / night! {.}I want it HOTTER."
  [R8]
  say 513 NEUTRAL "Your Steamy wants 'warm'. / {..}The ducks are neutral."
  choice 513 "Broker the bath night?" { "I'll broker it." -> yes ; "Later." }
  end
yes:
  quest S129 active
  say 513 NEUTRAL "A diplomatic snack first. / {.}A Coffee. Very hot."
end

script s129_again
  native CB_SIDE_HAVE ITEM_COFFEE
  jlt v0 1 need
  take ITEM_COFFEE
  say nar "(One slider. Two hands: / yours and @E's. Every beat, / the water inches warmer.)"
  push timing pattern=three skin=heart
  say 513 NEUTRAL "{..}Let me test it."
  [HOLD 13]
  shake SHK_POW
  say 513 SHOCK "{big}LUKEWARM!{/}" [SHOUT]
  [R26]
  say 513 NEUTRAL "...I'll allow it."
  [R13]
  say nar "(Twelve citizens. Six a / side. Warty sings. Guest the / duck swims to the middle.)"
  [R21]
  [MUTE 21]
  jif SF_STEAMY_LOST lost
  say steamy SAD "You were in my head. {.}You / were so tired." [HEART]
  [HOLD 13]
  say tallyworn SAD "You were so warm."
  jmp done
lost:
  say nar "(Steam on the big window: / EVERYONE'S HERE.)" [HEART]
  [HOLD 21]
done:
  reward 120
  befriend 513 3
  quest S129 done
  later 6m s129_later
  end
need:
  say 513 NEUTRAL "A Coffee. {.}Hot. HOTTER. / {..}Fine, normal hot."
end

script s129_later
  say nar "(Scaldie opened a sauna in / the back room. {.}It is on / fire. {..}It's fine.)"
end
