# side_a/b_rivet.dsc - Chain B "Rivet, Anvil, Sprocket and Bolt" (S103 S104
# S105 S202 S203 S106; cross-border) (owner: S4; story_final.md sec.14.1).
# Rivet is she, Bolt is xe (sec.13b). "Don't make it weird" is Rivet's.

# =============================================================================
# S103 "Days Since Last Fire" - Rivet (13) - Ch2 after Ep 2.6 - weld, housing
# =============================================================================
@pack quests kind=side
@mission S103 giver=13 side=hill ch=2-2 after=BEAT_EP2_6 at=53,10 verbs=weld,housing chain=B step=1 later=10m reward=0 offer=s103_offer active=s103_again title="Days Since Last Fire" hint="Weld a second safety sign with Rivet and hang it in your house."
@pack sa_rivet kind=side minutes=110

script s103_offer
  say rivet ANGRY "Chief Spark says my sign is / a safety hazard. {.}Because it / always says zero."
  [R8]
  say rivet NEUTRAL "So. {..}A second sign. At YOUR / house. So the average goes / up."
  [HOLD 8]
  say rivet NEUTRAL "That's maths."
  choice rivet "Weld a second sign?" { "That's maths." -> yes ; "Later." }
  end
yes:
  quest S103 active
  call s103_again
end

script s103_again
  say rivet NEUTRAL "Three parts. Don't wobble. / {..}Don't make it weird."
  push workshop
  sfx SFX_WELD
  [R5]
  sfx SFX_BEEP
  say nar "(A spark lands on @E.)"
  [R8]
  say nar "beep."
  [R13]
  say nar "(...fine.)"
  reward 0 furn=days_since_fire
  home PLACE_GIFT furn=days_since_fire
  push home -1
  say nar "(The sign is up. It counts / real days. {.}It says 7.)"
  [R13]
  sfx SFX_DOOR
  say rivet NEUTRAL "Just checking the hinge."
  [R8]
  say nar "(Rivet's tail is on fire.)"
  [HOLD 13]
  sfx SFX_TIK
  say nar "{big}7{/}"
  [R2]
  sfx SFX_GLITCH
  say nar "{big}-{/}"
  [R2]
  sfx SFX_GLITCH
  say nar "{big}-{/}"
  [R3]
  shake SHK_SLAM
  sfx SFX_THUD
  say nar "{big}0{/}" [PUNCH]
  [HOLD 13]
  say 99 NEUTRAL "{d}I heard that."
  [R21]
  [MUTE 21]
  say rivet SAD "Mum's forge had one. {.}It / read 0 every day." [HEART]
  [R8]
  say rivet SAD "We were proud of it."
  [R8]
  say rivet ANGRY "Don't make it weird."
  befriend rivet 3
  quest S103 done
  later 10m s103_later
end

script s103_later
  sfx SFX_BLIP
  say nar "(A note from Rivet under / your door: 'a personal best. / don't make it weird.')"
end

# =============================================================================
# S104 "Letters Not Sent" - Rivet (13) - Ch3 (the church) - find, combat, deliver
# =============================================================================
@pack quests kind=side
@mission S104 giver=13 side=hill ch=3-3 after=BEAT_EP3_1 at=53,10 verbs=find,combat,deliver chain=B step=2 later=6m reward=60 offer=s104_offer active=s104_again title="Letters Not Sent" hint="Get Rivet's toolbox back from the nest in the Vape Cellar."
@pack sa_rivet kind=side

string S104_C1 "A toolbox. A demon is sitting on it."
string S104_C2 "Don't scratch the paint. It's Rivet's."

script s104_offer
  say rivet NEUTRAL "I write my parents a letter / every month. {..}Ten years."
  [HOLD 13]
  say rivet SAD "Posted none. {.}They're in a / toolbox. In my workshop."
  [R8]
  say rivet ANGRY "Which is a NEST now. {..}The / demons dragged it down to / the Cellar."
  choice rivet "Get the toolbox back?" { "I'll get it." -> yes ; "Not yet." }
  end
yes:
  quest S104 active
  call s104_again
end

script s104_again
  say rivet NEUTRAL "Workshop first. See where / they took it."
  verb find cell=52,11 then=s104_found
end

script s104_found
  sfx SFX_CREAK
  say nar "(Scrape marks lead from the / workshop door to the Cloud / Nine cellar hatch.)"
  push combat MAPID_VAPE_CELLAR captions=S104_C1,S104_C2
  jlt v0 1 lost
  sfx SFX_PICKUP
  say nar "(The toolbox. A demon was / using it as a stool. {.}It / was a comfy stool.)"
  verb deliver rivet cell=53,10 fragile then=s104_read
  end
lost:
  say nar "(The toolbox is still down / there. {..}Rivet will wait. / She's good at waiting.)"
end

script s104_read
  say nar "(Rivet opens the toolbox. 120 / letters.)"
  [R8]
  say nar "(All of them begin: 'Dear / Mum and Dad, don't make it / weird.')"
  [HOLD 13]
  say rivet NEUTRAL "I had a format."
  [R13]
  say rivet NEUTRAL "{..}Letter one. I was / fourteen."
  [MUTE 21]
  [HOLD 21]
  say rivet SAD "'I saw what they did with / the fountain. I ran because / I couldn't fix it.'" [HEART]
  [HOLD 21]
  say rivet SAD "'I'm learning to fix things.'"
  [R26]
  choice rivet "Carry them over the Wall?" { "When it opens." -> yes ; "Your call." }
yes:
  say rivet ANGRY "No."
  [HOLD 13]
  say rivet SAD "{..}Yes."
  reward 60 furn=rivets_toolbox
  home PLACE_GIFT furn=rivets_toolbox
  befriend rivet 3
  quest S104 done
  later 6m s104_later
end

script s104_later
  say nar "(The toolbox in your house / has a letter sticking out. / It isn't addressed yet.)"
end

# =============================================================================
# S105 "Two Marks" - Anvil (595, Hellvile) - Ch4 after Ep 4.3 - weld, fetch
# =============================================================================
@pack quests kind=side
@mission S105 giver=595 side=hell ch=4-5 after=BEAT_EP4_3 at=31,9 verbs=fetch,weld,timing chain=B step=3 later=5m reward=100 offer=s105_offer active=s105_again title="Two Marks" hint="Bring Anvil 3 Scrap, then weld the new hammer mark to her rhythm."
@pack sa_rivet kind=side

script s105_offer
  say anvil NEUTRAL "The fountain pipes. {.}They / carry my old hammer mark."
  [R8]
  say anvil SAD "I want a new one before they / re-plumb them. {..}Hers and / mine. Together."
  [R8]
  say anvil NEUTRAL "Three Scrap. Salvagia has / some, if you can haggle with / a raccoon."
  choice anvil "Help stamp the new mark?" { "Count me in." -> yes ; "Later." }
  end
yes:
  quest S105 active
  say anvil HAPPY "Good. {.}The forge is hot."
end

script s105_again
  native CB_SIDE_HAVE ITEM_SCRAP
  jlt v0 3 need
  take ITEM_SCRAP 3
  say anvil NEUTRAL "Now the mark. {.}Pick one."
  choice anvil "Which mark?" { "An anvil." ; "A rivet." ; "Both, crossed." ; "A heart." }
  say anvil HAPPY "Good choice. {..}You're / Rivet's hands. I call the / rhythm."
  push timing pattern=three skin=bell
  sfx SFX_WELD
  say nar "(The mark goes into the / first pipe. For one second / the sludge runs clear.)"
  [R8]
  sfx SFX_TINK
  say nar "(Anvil tests the weld with a / spoon.)"
  [R8]
  say anvil HAPPY "It's a REALLY good spoon."
  [R8]
  say rivet NEUTRAL "Mum."
  [R5]
  say anvil ANGRY "It IS."
  [R21]
  [MUTE 13]
  say anvil SAD "I'm keeping the old mark too." [HEART]
  [R8]
  say anvil NEUTRAL "I don't want to forget what / they did with it. {..}I want to / remember what we did after."
  reward 100 furn=forge_iron_bed
  befriend anvil 4
  befriend rivet 2
  quest S105 done
  later 5m s105_later
  end
need:
  say anvil NEUTRAL "Three Scrap. {.}Salvagia's yard / or Rivet's workshop. {..}Say / hi to the raccoon for me."
end

script s105_later
  sfx SFX_TINK
  say nar "(Everything you weld from / now on carries a tiny stamp: / two marks, crossed.)"
end

# =============================================================================
# S202 "The Door" - Anvil (595) - Ch4 after Ep 4.3 - housing, talk
# =============================================================================
@pack quests kind=side
@mission S202 giver=595 side=hell ch=4-5 after=BEAT_EP4_3 at=31,9 verbs=housing chain=B step=4 later=5m reward=0 offer=s202_offer active=s202_again title="The Door" hint="Let Anvil look at your cottage's squeaky front door."
@pack sa_rivet kind=side

script s202_offer
  say anvil NEUTRAL "I hear your front door / squeaks."
  [HOLD 8]
  say anvil NEUTRAL "I'd like to see the hinge."
  choice anvil "Show her the door?" { "Come on over." -> yes ; "Some other time." }
  end
yes:
  quest S202 active
  call s202_again
end

script s202_again
  say nar "(Anvil crosses the old Wall / line for the first time in / eleven years. For a hinge.)"
  push home -1
  sfx SFX_CREAK
  say nar "(She crouches by your door. / Looks at the hinge for a / long time.)"
  [HOLD 13]
  say nar "(A tiny anvil, stamped in the / metal.)"
  [R13]
  say anvil NEUTRAL "I made this. Before the Wall. / {.}For the Mingle houses."
  [R13]
  sfx SFX_CREAK
  say anvil HAPPY "It still squeaks? {..}Good."
  [R8]
  say nar "(Rivet oils it. Anvil un-oils / it. Rivet oils it.)"
  sfx SFX_SQUEAK
  [R8]
  say nar "(Mid-argument, the door / squeaks by itself.)"
  [HOLD 8]
  say rivet NEUTRAL "...Leave it."
  [R2]
  say anvil NEUTRAL "...Leave it."
  [R21]
  [MUTE 21]
  say anvil SAD "I left the squeak in on / purpose." [HEART]
  [HOLD 13]
  say anvil SAD "So you'd know when someone / comes home."
  reward 0 furn=anvils_hinge
  befriend anvil 3
  quest S202 done
  later 5m s202_later
end

script s202_later
  sfx SFX_CREAK
  say nar "(The door squeaks a quarter / beat before the next knock. / Somebody always comes home.)"
end

# =============================================================================
# S203 "A Spoon for Mum" - Rivet (13, in Hellvile) - Ch4 - deliver, talk
# =============================================================================
@pack quests kind=side
@mission S203 giver=13 side=hell ch=4-5 after=BEAT_EP4_3 at=29,9 verbs=deliver chain=B step=5 later=4m reward=50 offer=s203_offer active=s203_again title="A Spoon for Mum" hint="Carry Rivet's best spoon to Anvil at the forge."
@pack sa_rivet kind=side

script s203_offer
  say rivet NEUTRAL "This is my best spoon."
  [HOLD 13]
  say rivet SAD "I can't carry it over there / myself. {..}Don't ask why. / {.}Don't make it weird."
  choice rivet "Carry the spoon?" { "Carefully." -> yes ; "Not now." }
  end
yes:
  quest S203 active
  call s203_again
end

script s203_again
  say nar "(The spoon is light. It feels / heavy.)"
  verb deliver anvil cell=27,11 fragile then=s203_given
end

script s203_given
  say nar "(Anvil takes the spoon.)"
  [HOLD 13]
  sfx SFX_DOOR
  say nar "(She opens a drawer: a tiny / bent spoon. Rivet's. Age / six.)"
  [R13]
  say anvil NEUTRAL "I have one."
  [HOLD 13]
  say anvil SAD "{..}I'll have two."
  [R8]
  say 596 NEUTRAL "(turns a page, not reading)"
  [R5]
  say 597 HAPPY "He's on chapter nine."
  [R21]
  [MUTE 26]
  say nar "(She puts the two spoons / side by side on the plate / that was set for ten years.)" [HEART]
  [HOLD 26]
  reward 50 furn=two_spoons
  befriend anvil 3
  befriend rivet 3
  quest S203 done
  later 4m s203_later
end

script s203_later
  say rivet NEUTRAL "Mum's got two now."
  [HOLD 8]
  say rivet NEUTRAL "...Don't make it weird."
end

# =============================================================================
# S106 "Bolt's First Weld" - Bolt (597, Hellvile, KID) - Ch4-5 - escort, weld
# =============================================================================
@pack quests kind=side
@mission S106 giver=597 side=hell ch=4-5 after=BEAT_EP4_6 at=35,11 verbs=escort,weld,housing chain=B step=6 later=7m reward=50 offer=s106_offer active=s106_again title="Bolt's First Weld" hint="Walk Bolt to the forge without xem lighting anything. Then weld."
@pack sa_rivet kind=side

script s106_offer
  say 597 HAPPY "I want to weld. Like Rivet."
  [R8]
  say nar "(Bolt holds up a torch. {..}It / is Rivet's torch.)"
  [HOLD 13]
  say rivet SHOCK "{!}Where did you GET that?"
  [R8]
  say anvil NEUTRAL "Let xem."
  choice 597 "Teach Bolt to weld?" { "Let xem." -> yes ; "Maybe next year." }
  end
yes:
  quest S106 active
  call s106_again
end

script s106_again
  say 597 HAPPY "To the forge! {.}I won't light / anything."
  verb escort 597 cell=31,11 then=s106_forge
end

script s106_forge
  jlt v1 1 lost
  say nar "(Bolt lit three things on / the way. Chief Spark / followed with a bucket.)"
  sfx SFX_SPLASH
  [R8]
  say 99 NEUTRAL "Cross-border fire safety."
  push workshop
  sfx SFX_WELD
  say nar "(A stool. {.}It wobbles. It is / perfect.)"
  [R8]
  say 597 HAPPY "For YOUR house. Yours gets / visitors."
  [R8]
  say nar "(Bolt's little sign reads: / DAYS SINCE LAST FIRE: 0. / Xe painted it first.)"
  [HOLD 13]
  say rivet NEUTRAL "{..}That's my kid."
  [R21]
  [MUTE 21]
  say nar "(Rivet stands exactly where / she stood at six, holding / the light for Anvil.)" [HEART]
  [HOLD 21]
  say anvil SAD "Look at you. {..}You're the one / holding the light now."
  reward 50 furn=bolts_stool
  home PLACE_GIFT furn=bolts_stool
  befriend 597 4
  quest S106 done
  later 7m s106_later
  end
lost:
  say 597 SAD "You walked off! {.}I lit a / bush. {..}Ask me again."
end

script s106_later
  jif SF_GRUMBLE_DEAD quiet
  say grumble HAPPY "Eat your vegetables, kid."
  [R8]
  say grumble NEUTRAL "{..}Not those. Those are / mine."
  [R5]
  say grumble NEUTRAL "grrrbl." [GURGLE]
  end
quiet:
  say nar "(Bolt's stool wobbles in your / house. {.}It wobbles happily.)"
end
