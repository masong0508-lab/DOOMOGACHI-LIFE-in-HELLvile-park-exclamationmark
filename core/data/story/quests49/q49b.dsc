# quests49/q49b.dsc - the old side quests #25-#48, re-timed (owner: S4;
# the format is q49a.dsc's: an offer that ends on the accept choice, and a
# thank-you after which story_side.c pays the table's reward).
@pack q49b kind=side minutes=60

# ---- #25 Mr. Biscuit (Wobbles 127) -------------------------------------------
script q25_offer
  say 127 SAD "{.}*hic* {..}Mr. Biscuit ran / away when the sky went / red!"
  [R8]
  say 127 SAD "He's a puppy. {.}He's THIS / big."
  [HOLD 13]
  say nar "(Wobbles holds two fingers / very close together.)"
  choice 127 "Find Mr. Biscuit?" { "Sure, I'll help!" ; "Maybe later" }
end

script q25_thanks
  sfx SFX_SQUEAK
  say 127 HAPPY "{big}MR. BISCUIT!{/} *hug*"
  [R5]
  say 127 HAPPY "Thank you thank you thank / you thank you thank you!"
  [R8]
  say 127 NEUTRAL "{..}*hic*"
end

# ---- #26 Spider Ride (Sr. Lumen 354) -----------------------------------------
script q26_offer
  say 354 NEUTRAL "Something with too many legs / crawled out of the Fun Fair."
  [R13]
  say 354 SHOCK "Spiders made of cloud, / child. {..}They are riding / the teacups."
  [HOLD 8]
  say 354 ANGRY "Purge them. {.}Gently, if / possible. {..}It will not / be possible."
  choice 354 "Clear the spiders?" { "Sure, I'll help!" ; "Maybe later" }
end

script q26_thanks
  sfx SFX_HOLY
  say 354 HAPPY "The children can ride again."
  [R8]
  say 354 NEUTRAL "Take this salt and my old / spider swatter medal. {..}I / swatted a lot of spiders."
end

# ---- #27 Spire Scout (Updraft 17) --------------------------------------------
script q27_offer
  sfx SFX_WHOOSH
  say 17 SHOCK "My balloon got sucked toward / that cloud tower! {!}With / my lunch in it!"
  [R8]
  say 17 NEUTRAL "Get me a close-up photo so / I can plan a rescue / flight."
  [HOLD 8]
  say 17 SAD "{..}For the lunch."
  choice 17 "Photograph the Spire?" { "Sure, I'll help!" ; "Maybe later" }
end

script q27_thanks
  say 17 SICK "Oof. It's even uglier up / close."
  [R8]
  say 17 HAPPY "Thanks! Here, my sky scout / wings pin. {.}Mind the / updrafts. I'm one."
end

# ---- #28 Stay Awake, Guards! (Valkyrie 86) -----------------------------------
script q28_offer
  say 86 SLEEPY "The royal guard hasn't slept / since the sky ripped."
  [R13]
  say 86 SLEEPY "{..}I am the royal guard."
  [R8]
  say 86 ANGRY "Two Energy Drinks and I'll / hold the plaza till dawn!"
  choice 86 "Bring 2 Energy Drinks?" { "Sure, I'll help!" ; "Maybe later" }
end

script q28_thanks
  sfx SFX_EAT
  say 86 SHOCK "{big}HUZZAH!{/} {!}The guard / stands firm!"
  [R8]
  say 86 HAPPY "Take this hellsteel. We / pried it off a demon. {..}He / was very rude about it."
end

# ---- #29 Comfort Food (Sandwichy 4 -> Hootenanny 26) -------------------------
script q29_offer
  say sandwichy SAD "Hootenanny has kept night / watch for three days / straight."
  [R8]
  say sandwichy HAPPY "Bring him my onigiri? Extra / pickle. {.}West lawn."
  choice sandwichy "Deliver the onigiri?" { "Sure, I'll help!" ; "Maybe later" }
end

script q29_thanks
  say sandwichy HAPPY "He ate it in one bite and / said 'hoo'."
  [HOLD 8]
  say sandwichy NEUTRAL "{..}That means thank you. / Here's one for you too."
end

# ---- #30 Save the Mascot (Twisty 43 -> Hugsley 20) ---------------------------
script q30_offer
  sfx SFX_SQUEAK
  say 43 SHOCK "Hugsley the town mascot / breathed the cloud."
  [R8]
  say 43 SHOCK "Now he hugs TOO HARD. {!}He / popped three of my / balloons. On purpose."
  [HOLD 8]
  say 43 SAD "Please exorcise him before / he hugs a bus."
  choice 43 "Save Hugsley?" { "Sure, I'll help!" ; "Maybe later" }
end

script q30_thanks
  sfx SFX_HAPPY
  say 43 HAPPY "He's back to normal hugs! / {.}Normal, wonderful hugs."
  [R8]
  say 43 NEUTRAL "Here, a balloon animal. / {..}It's a plushie. {.}Close / enough."
end

# ---- #31 Postcard from the Fountain (Clinker 753) ----------------------------
script q31_offer
  say 753 NEUTRAL "I write poems on the gate / in chalk."
  [R8]
  say 753 SAD "I've never seen a fountain / that giggles. {..}Our / fountains only sigh."
  [HOLD 8]
  say 753 HAPPY "Could you bring me a photo / of yours?"
  choice 753 "Photograph the fountain?" { "Sure, I'll help!" ; "Maybe later" }
end

script q31_thanks
  [HOLD 13]
  say 753 SHOCK "A fountain... {..}that laughs."
  [R8]
  say 753 HAPPY "I'm going to write forty / poems about this. {.}Thank / you."
end

# ---- #32 Charcoal Cookies (Charlotte 566 -> Nana 1) --------------------------
script q32_offer
  say 566 HAPPY "I baked for Nana Puddin! / {.}The famous Nana Puddin!"
  [R8]
  say 566 NEUTRAL "They're only a little bit / charcoal. {..}A little bit / is ninety percent."
  [HOLD 8]
  say 566 LOVE "Would you deliver them?"
  choice 566 "Deliver the bagels?" { "Sure, I'll help!" ; "Maybe later" }
end

script q32_thanks
  sfx SFX_LOVE
  say 566 SHOCK "She said they were 'crunchy / and full of love'! {!}She / wants to swap recipes!"
  [R8]
  say 566 HAPPY "Have some angel cake. {..}I / didn't make it. You can / eat it."
end

# ---- #33 The Florist's Apprentice (Morguelet 582 -> Bluebell 29) -------------
script q33_offer
  say 582 SHOCK "Shh. {.}I want to be a / florist."
  [HOLD 13]
  say 582 SAD "A HILLVILE florist. {..}Not a / funeral one."
  [R8]
  say 582 NEUTRAL "Could you ask Bluebell if / she'd take a ghoul / apprentice?"
  choice 582 "Ask Bluebell for her?" { "Sure, I'll help!" ; "Maybe later" }
end

script q33_thanks
  sfx SFX_SPARKLE
  say 582 SHOCK "She said YES?!"
  [R8]
  say 582 HAPPY "I'm going to cry black tears / of joy. {..}Here, flowers, / and a flower friend ribbon!"
end

# ---- #34 Game Night (Graveley 730) -------------------------------------------
script q34_offer
  say 730 NEUTRAL "I lowered the rent for the / families in my crypts."
  [HOLD 8]
  say 730 ANGRY "Don't look at me like that."
  [R8]
  say 730 SAD "Now the kids need something / fun. {.}A board game, maybe?"
  choice 730 "Bring a Board Game?" { "Sure, I'll help!" ; "Maybe later" }
end

script q34_thanks
  say 730 HAPPY "Ha! The little ghouls will / play for a century."
  [R8]
  say 730 ANGRY "Thank you. {..}Don't tell / anyone I'm nice."
end

# ---- #35 Lullaby for Clouds (Ploddy 701) -------------------------------------
script q35_offer
  say 701 SLEEPY "I hum lullabies to the / clouds I haul."
  [R13]
  say 701 NEUTRAL "I've heard Hillvile has a / Concert Hall. {..}With a / roof."
  [R8]
  say 701 HAPPY "Could you go hear it for / me? And tell me?"
  choice 701 "Visit the Concert Hall?" { "Sure, I'll help!" ; "Maybe later" }
end

script q35_thanks
  [MUTE 21]
  say 701 HAPPY "Real music. {.}In a real / hall."
  [HOLD 13]
  say 701 SLEEPY "The clouds will sleep so / well tonight. {..}Thank you, / friend."
end

# ---- #36 Squeaky Toy Diplomacy (Big Snarl 855) -------------------------------
script q36_offer
  sfx SFX_DEMON_ROAR
  say 855 ANGRY "{big}GRR.{/} {.}Nobody gets into / the Arena without a squeaky / toy."
  [R8]
  say 855 NEUTRAL "Rules is rules."
  [HOLD 13]
  say 855 SAD "{..}Can I keep it after?"
  choice 855 "Bring Big Snarl a plushie?" { "Sure, I'll help!" ; "Maybe later" }
end

script q36_thanks
  sfx SFX_SQUEAK
  [R3]
  sfx SFX_SQUEAK
  [R3]
  sfx SFX_SQUEAK
  [HOLD 13]
  say 855 NEUTRAL "{..}Ahem."
  [R8]
  say 855 HAPPY "You're on the list. FOREVER. / {.}Have a steak."
end

# ---- #37 Last Puff (Dirgelet 766 -> Hazey 788) -------------------------------
script q37_offer
  say 766 SAD "My friend Hazey still / heralds Vapula's smoke."
  [R13]
  say 766 SAD "He can't stop. {..}Something's / still inside him."
  [MUTE 13]
  [HOLD 13]
  say 766 SAD "Please?"
  choice 766 "Free Hazey?" { "Sure, I'll help!" ; "Maybe later" }
end

script q37_thanks
  sfx SFX_CHOIR
  say 766 HAPPY "He coughed out the last of / it and then he SANG."
  [R8]
  say 766 HAPPY "{..}My friend is back. / Thank you!"
end

# ---- #38 Counting Together (Tallyworn 712 -> Tock 104) -----------------------
script q38_offer
  say tallyworn NEUTRAL "I counted days for ten / years."
  [HOLD 13]
  say tallyworn NEUTRAL "They say a clockmaker in / Hillvile counts seconds. / {..}Tock."
  [R8]
  say tallyworn SAD "Would he count with me?"
  choice tallyworn "Introduce him to Tock?" { "Sure, I'll help!" ; "Maybe later" }
end

script q38_thanks
  sfx SFX_TIK
  say tallyworn HAPPY "He said yes. {.}Every day at / noon, we count together."
  [R8]
  sfx SFX_TIK
  say tallyworn NEUTRAL "One."
  [R8]
  sfx SFX_TIK
  say tallyworn NEUTRAL "Two."
  [R8]
  sfx SFX_TIK
  say tallyworn HAPPY "Three. {..}Thank you."
end

# ---- #39 Echo of the Duke (Fangela 519) --------------------------------------
script q39_offer
  say 519 HAPPY "Sniff sniff. {.}There's still / an echo of Vapula's stink / up in that Spire."
  [R8]
  say 519 LOVE "A real champion would clear / it out. {..}I'm looking at / you. Champion."
  choice 519 "Clear the Spire's echo?" { "Sure, I'll help!" ; "Maybe later" }
end

script q39_thanks
  say 519 HAPPY "You smell like victory!"
  [R8]
  say 519 NEUTRAL "And smoke. {.}Mostly victory. / Take this champion's / buckle, champ."
end

# ---- #40 Rival Bakers (Scorchmello 669 -> Crumbelina 73) ---------------------
script q40_offer
  say 669 ANGRY "Crumbelina the royal baker / said my marshmallows were / 'a crime'."
  [HOLD 8]
  say 669 SAD "They ARE a crime. {..}A crime / of passion."
  [R8]
  say 669 NEUTRAL "Could you... make peace for / me?"
  choice 669 "Make peace for him?" { "Sure, I'll help!" ; "Maybe later" }
end

script q40_thanks
  sfx SFX_HAPPY
  say 669 SHOCK "A bake-off! {!}She challenged / me to a FRIENDLY bake-off!"
  [R8]
  say 669 HAPPY "Have some of her cupcakes. / {..}They're incredible. {.}I / hate it."
end

# ---- #41 Squeak the Bat (Biscuitfang 820) ------------------------------------
script q41_offer
  say 820 SAD "*sniffle* {.}My pet bat Squeak / flew off when the gate / opened!"
  [R8]
  say 820 NEUTRAL "He likes the lake shore."
  [HOLD 8]
  say 820 SAD "{..}I bit someone. Sorry. / {.}I bite when I'm sad."
  choice 820 "Find Squeak the bat?" { "Sure, I'll help!" ; "Maybe later" }
end

script q41_thanks
  sfx SFX_SQUEAK
  say 820 HAPPY "{big}SQUEAK!{/} *happy chomp*"
  [R8]
  say 820 SHOCK "Oops, sorry. {.}You're / officially a bat buddy!"
end

# ---- #42 Wall Art (Stencilla 565) --------------------------------------------
script q42_offer
  say 565 NEUTRAL "I painted hearts on the Wall / every night. {..}For years."
  [R8]
  say 565 SAD "Before they knock it down, / could you photograph them?"
  choice 565 "Photograph the hearts?" { "Sure, I'll help!" ; "Maybe later" }
end

script q42_thanks
  [HOLD 13]
  say 565 HAPPY "My hearts will live forever / in that picture."
  [R8]
  say 565 NEUTRAL "Here, a wall artist ribbon. / {.}You've earned it."
end

# ---- #43 Encore! (Riffmaw 511) -----------------------------------------------
script q43_offer
  sfx SFX_KAZOO
  say 511 HAPPY "That festival was the best / gig of my un-life!"
  [R8]
  say 511 SHOCK "Get up on the Unity Stage / and give 'em an ENCORE! {!}"
  choice 511 "Play the encore?" { "Sure, I'll help!" ; "Maybe later" }
end

script q43_thanks
  sfx SFX_FANFARE
  say 511 HAPPY "{big}ENCORE! ENCORE!{/} You rock, / Hillvile!"
  [R8]
  say 511 NEUTRAL "Take the festival anthem / for your jukebox. {..}Play it / LOUD."
end

# ---- #44 Frog Wedding (Confetti 62: Warty 912 -> Lily Lou 82) ----------------
script q44_offer
  say 62 LOVE "Warty sings love songs to / the moon. Lily Lou grows / lilies for the pond."
  [R8]
  say 62 HAPPY "A cross-border frog wedding! / {..}I've already ordered the / flies."
  [HOLD 8]
  say 62 LOVE "Nudge him?"
  choice 62 "Nudge Warty?" { "Sure, I'll help!" ; "Maybe later" }
end

script q44_thanks
  sfx SFX_CONFETTI
  say 62 SHOCK "A wedding! {!}A WEDDING!"
  [R8]
  say 62 HAPPY "The first cross-border / wedding in Hellvile Park! / Have a ring and my pin!"
end

# ---- #45 The Big Picture (Shutterbug 46) -------------------------------------
script q45_offer
  say 46 HAPPY "Darling, I need the shot of / the century."
  [R8]
  say 46 NEUTRAL "The river, both sides, {..}no / Wall. Stand on the north / bridge and snap it."
  choice 46 "Snap the big picture?" { "Sure, I'll help!" ; "Maybe later" }
end

script q45_thanks
  [MUTE 21]
  [HOLD 21]
  say 46 SAD "No Wall in the frame. {..}Just / people."
  [R13]
  say 46 HAPPY "It's the most beautiful / thing I've ever developed." [HEART]
end

# ---- #46 Potluck Planner (Mother Nox 778) ------------------------------------
script q46_offer
  say nox HAPPY "My potluck is open to / everyone now. {.}Everyone!"
  [R8]
  say nox NEUTRAL "I need more soup. {..}Two / Soul Soups from the market, / dear?"
  choice nox "Bring 2 Soul Soup?" { "Sure, I'll help!" ; "Maybe later" }
end

script q46_thanks
  sfx SFX_EAT
  say nox HAPPY "Splendid. Hillvile brought / cake and we brought soup."
  [HOLD 13]
  say nox HAPPY "That is diplomacy. {..}Have / some cake."
end

# ---- #47 Vapula's Apology (Lord Vapula 999 -> Mayor 0) ------------------------
script q47_offer
  sfx SFX_WHOOSH
  say vapula SAD "{..}*tiny wheeze*"
  [HOLD 13]
  say vapula SAD "I'm... smaller now. {.}Humbler."
  [R8]
  say vapula NEUTRAL "Could you give these flowers / to your Mayor? {..}With my / apology?"
  choice vapula "Take him the flowers?" { "Sure, I'll help!" ; "Maybe later" }
end

script q47_thanks
  [MUTE 13]
  say vapula SHOCK "He forgave me. {..}He HUGGED / me."
  [HOLD 13]
  say vapula SAD "I didn't know a cloud could / feel this... dry-eyed." [HEART]
  [R8]
  say vapula HAPPY "Thank you."
end

# ---- #48 Explorer's Club (Scoutmaster 15) ------------------------------------
script q48_offer
  say 15 HAPPY "As scout leader I MUST know: / what do they sell at the / Hellvile imp market?"
  [R8]
  say 15 HAPPY "Go look! For science! {.}For / badges! {..}Mostly badges."
  choice 15 "Scout the imp market?" { "Sure, I'll help!" ; "Maybe later" }
end

script q48_thanks
  say 15 SHOCK "Spicy peppers and eyeball / candy?! {!}Incredible."
  [R8]
  say 15 HAPPY "You've earned the Cross- / Border Explorer badge! {.}I / have one too. {..}Now."
end
