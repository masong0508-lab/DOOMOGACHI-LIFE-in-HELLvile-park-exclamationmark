# quests49/q49b.dsc - the old side quests #25-#48, re-timed (owner: S4;
# the format is q49a.dsc's: an offer that ends on the accept choice, and a
# thank-you after which story_side.c pays the table's reward). W5 rewrote
# the words to docs/design/story/SCRIPT_STYLE.md (see q49a.dsc's header).
@pack q49b kind=side minutes=60

# ---- #25 Mr. Biscuit (Wobbles 127) -------------------------------------------
script q25_offer
  say 127 SAD "*hic* Mr. Biscuit ran off / to the graveyard when the / sky went red! *hic*"
  [R8]
  say 127 SAD "He's my puppy. He's THIS / big."
  [HOLD 13]
  say nar "(Wobbles holds two fingers / a crumb apart. Mr. Biscuit / is a very small puppy.)"
  choice 127 "Find Mr. Biscuit?" { "Find him" ; "Maybe later" }
end

script q25_thanks
  sfx SFX_SQUEAK
  say 127 HAPPY "{big}MR. BISCUIT!{/} *hug*"
  [R5]
  say 127 HAPPY "Thank you thank you thank / you thank you thank you / thank you thank you!"
  [R8]
  say 127 NEUTRAL "*hic* {.}That one was Mr. / Biscuit."
end

# ---- #26 Spider Ride (Sr. Lumen 354) -----------------------------------------
script q26_offer
  say 354 NEUTRAL "Something with too many / legs is loose in the Fun / Fair, child."
  [R13]
  say 354 SHOCK "Spiders made of cloud. / They are riding the / teacups. Without tickets."
  [HOLD 8]
  say 354 ANGRY "Purge them. Gently, if / possible. {.}It will not be / possible."
  choice 354 "Clear the spiders?" { "Clear them out" ; "Maybe later" }
end

script q26_thanks
  sfx SFX_HOLY
  say 354 HAPPY "The children can ride / again. The teacups need / a very long bath."
  [R8]
  say 354 NEUTRAL "Salt, and my old Spider / Swatter Medal. {.}I swatted / a great many spiders."
end

# ---- #27 Spire Scout (Updraft 17) --------------------------------------------
script q27_offer
  sfx SFX_WHOOSH
  say 17 SHOCK "My balloon got sucked at / that cloud tower! With my / lunch in it!"
  [R8]
  say 17 NEUTRAL "Get me a close-up photo of / the Spire's foot, so I can / plan a rescue flight."
  [HOLD 8]
  say 17 SAD "{.}For the lunch."
  choice 17 "Photograph the Spire?" { "Photograph it" ; "Maybe later" }
end

script q27_thanks
  say 17 SICK "Oof. It's even uglier up / close. Who DESIGNED that?"
  [R8]
  say 17 HAPPY "Thanks! Here, my Sky Scout / Wings. Mind the updrafts. / {.}I'm one of them."
end

# ---- #28 Stay Awake, Guards! (Valkyrie 86) -----------------------------------
script q28_offer
  say 86 SLEEPY "The royal guard hasn't / slept since the sky / ripped open."
  [R13]
  say 86 SLEEPY "{.}I am the royal guard."
  [R8]
  say 86 ANGRY "Two Energy Drinks and I'll / hold the plaza till dawn! / Then till lunch!"
  choice 86 "Bring 2 Energy Drinks?" { "Fetch them" ; "Maybe later" }
end

script q28_thanks
  sfx SFX_EAT
  say 86 SHOCK "{big}HUZZAH!{/} The guard / stands firm! The guard can / hear colours!"
  [R8]
  say 86 HAPPY "Take this hellsteel. We / pried it off a demon. He / was very rude about it."
end

# ---- #29 Comfort Food (Sandwichy 4 -> Hootenanny 26) -------------------------
script q29_offer
  say sandwichy SAD "Hootenanny has kept night / watch for three days / straight. He's eating bugs."
  [R8]
  say sandwichy HAPPY "Take him my onigiri? Extra / pickle. He's on the west / lawn, glaring at the sky."
  choice sandwichy "Deliver the onigiri?" { "Deliver it" ; "Maybe later" }
end

script q29_thanks
  say sandwichy HAPPY "He ate it in one bite and / said 'hoo'."
  [HOLD 8]
  say sandwichy NEUTRAL "{.}That means thank you. / Here's one for you. / Say hoo."
end

# ---- #30 Save the Mascot (Twisty 43 -> Hugsley 20) ---------------------------
script q30_offer
  sfx SFX_SQUEAK
  say 43 SHOCK "Hugsley the town mascot / breathed the purple cloud. / He's possessed!"
  [R8]
  say 43 SHOCK "Now he hugs TOO HARD. He / popped three of my / balloons. On purpose."
  [HOLD 8]
  say 43 SAD "Exorcise him? He's at the / playground. {.}Before he / hugs a bus."
  choice 43 "Save Hugsley?" { "Exorcise him" ; "Maybe later" }
end

script q30_thanks
  sfx SFX_HAPPY
  say 43 HAPPY "He's back to normal hugs! / Normal, wonderful, / non-lethal hugs."
  [R8]
  say 43 NEUTRAL "Here, a balloon animal. / It's a plushie. {.}I'm out / of breath."
end

# ---- #31 Postcard from the Fountain (Clinker 753) ----------------------------
script q31_offer
  say 753 NEUTRAL "I write poems on the gate. / In chalk. I rate them. / They're all sixes."
  [R8]
  say 753 SAD "I've never seen a fountain / that giggles. Ours only / sigh. Then they leak."
  [HOLD 8]
  say 753 HAPPY "Bring me a photo of / HaHaland's fountain?"
  choice 753 "Photograph the fountain?" { "Photograph it" ; "Maybe later" }
end

script q31_thanks
  [HOLD 13]
  say 753 SHOCK "A fountain... that laughs."
  [R8]
  say 753 HAPPY "I'll write forty poems / about it. All sixes. / {.}Needs salt."
end

# ---- #32 Charcoal Cookies (Charlotte 566 -> Nana 1) --------------------------
script q32_offer
  say 566 HAPPY "I baked for Nana Puddin! / THE Nana Puddin! Could / you deliver them?"
  [R8]
  say 566 NEUTRAL "They're only a little bit / charcoal. A little bit is / ninety percent."
  [HOLD 8]
  say 566 LOVE "{.}The other ten is love."
  choice 566 "Deliver the bagels?" { "Deliver them" ; "Maybe later" }
end

script q32_thanks
  sfx SFX_LOVE
  say 566 SHOCK "She said they were 'crunchy / and full of love'! She / wants to swap recipes!"
  [R8]
  say 566 HAPPY "Have some angel cake. I / didn't make it. {.}So you / can eat it."
end

# ---- #33 The Florist's Apprentice (Morguelet 582 -> Bluebell 29) -------------
script q33_offer
  say 582 SHOCK "Shh. I want to be a / florist. Can you ask / Bluebell for me?"
  [HOLD 13]
  say 582 SAD "A HILLVILE florist. Not a / funeral one. {.}I've done / enough funerals."
  [R8]
  say 582 NEUTRAL "Ask if she'd take a ghoul / apprentice? I'm good with / dying things. Flowers die!"
  choice 582 "Ask Bluebell for her?" { "Ask Bluebell" ; "Maybe later" }
end

script q33_thanks
  sfx SFX_SPARKLE
  say 582 SHOCK "She said YES?!"
  [R8]
  say 582 HAPPY "I'm crying black tears of / joy. Here: flowers, and a / Flower Friend Ribbon!"
end

# ---- #34 Game Night (Graveley 730) -------------------------------------------
script q34_offer
  say 730 NEUTRAL "I lowered the rent for the / families in my crypts."
  [HOLD 8]
  say 730 ANGRY "Don't look at me like that."
  [R8]
  say 730 SAD "Now their kids need / something fun. Bring me a / Board Game? Not about rent."
  choice 730 "Bring a Board Game?" { "Fetch it" ; "Maybe later" }
end

script q34_thanks
  say 730 HAPPY "Ha! The little ghouls will / play it for a century. / Literally."
  [R8]
  say 730 ANGRY "Thank you. {.}Tell anyone / I'm nice and your rent / goes up."
end

# ---- #35 Lullaby for Clouds (Ploddy 701) -------------------------------------
script q35_offer
  say 701 SLEEPY "I haul clouds. I hum them / lullabies so they don't / rain on me."
  [R13]
  say 701 NEUTRAL "They say HaHaland has a / Concert Hall. With a roof. / {.}A whole roof."
  [R8]
  say 701 HAPPY "Go hear it for me? Then / tell me everything?"
  choice 701 "Visit the Concert Hall?" { "Go and listen" ; "Maybe later" }
end

script q35_thanks
  [MUTE 21]
  say 701 HAPPY "Real music. In a real / hall. Under a real roof."
  [HOLD 13]
  say 701 SLEEPY "The clouds will sleep so / well tonight. Thank you, / friend."
end

# ---- #36 Squeaky Toy Diplomacy (Big Snarl 855) -------------------------------
script q36_offer
  sfx SFX_DEMON_ROAR
  say 855 ANGRY "{big}GRR.{/} Nobody gets into / the Arena without a / squeaky toy. Bring one."
  [R8]
  say 855 NEUTRAL "Rules is rules. I wrote / them. In crayon."
  [HOLD 13]
  say 855 SAD "{.}Can I keep it after?"
  choice 855 "Bring Big Snarl a plushie?" { "Fetch it" ; "Maybe later" }
end

script q36_thanks
  sfx SFX_SQUEAK
  [R3]
  sfx SFX_SQUEAK
  [R3]
  sfx SFX_SQUEAK
  [HOLD 13]
  say 855 NEUTRAL "{.}Ahem."
  [R8]
  say 855 HAPPY "On the list. FOREVER. Have / a steak. Tell nobody / I squeaked."
end

# ---- #37 Last Puff (Dirgelet 766 -> Hazey 788) -------------------------------
script q37_offer
  say 766 SAD "My friend Hazey still / heralds Vapula's smoke. / He can't stop."
  [R13]
  say 766 SAD "Something's still inside / him. Could you exorcise / it out?"
  [MUTE 13]
  [HOLD 13]
  say 766 SAD "Please?"
  choice 766 "Free Hazey?" { "Exorcise him" ; "Maybe later" }
end

script q37_thanks
  sfx SFX_CHOIR
  say 766 HAPPY "He coughed out the last of / it, and then he SANG. / Badly. Wonderfully."
  [R8]
  say 766 HAPPY "My friend is back. Thank / you! Take some sage. He / still smells of smoke."
end

# ---- #38 Counting Together (Tallyworn 712 -> Tock 104) -----------------------
script q38_offer
  say tallyworn NEUTRAL "I counted days on a wall / for ten years. I'm good at / it. Too good."
  [HOLD 13]
  say tallyworn NEUTRAL "They say a clockmaker in / Hillvile counts seconds. / Tock."
  [R8]
  say tallyworn SAD "Would he count with me? / Introduce us?"
  choice tallyworn "Introduce him to Tock?" { "Introduce them" ; "Maybe later" }
end

script q38_thanks
  sfx SFX_TIK
  say tallyworn HAPPY "He said yes. Every day at / noon, we count together."
  [R8]
  sfx SFX_TIK
  say tallyworn NEUTRAL "One."
  [R8]
  sfx SFX_TIK
  say tallyworn NEUTRAL "Two."
  [R8]
  sfx SFX_TIK
  say tallyworn HAPPY "Three. {.}Best count in / years. Nine out of ten."
end

# ---- #39 Echo of the Duke (Fangela 519) --------------------------------------
script q39_offer
  say 519 HAPPY "Sniff sniff. There's still / an echo of Vapula's stink / up in the Spire."
  [R8]
  say 519 LOVE "A real champion would clear / it out. I'm looking at / you. {.}Champion."
  choice 519 "Clear the Spire's echo?" { "Clear it out" ; "Maybe later" }
end

script q39_thanks
  say 519 HAPPY "You smell like victory!"
  [R8]
  say 519 NEUTRAL "And smoke. Lots of smoke. / Take this Champion's / Buckle, champ."
end

# ---- #40 Rival Bakers (Scorchmello 669 -> Crumbelina 73) ---------------------
script q40_offer
  say 669 ANGRY "Crumbelina the royal baker / said my marshmallows were / 'a crime'."
  [HOLD 8]
  say 669 SAD "They ARE a crime. {.}Of / passion."
  [R8]
  say 669 NEUTRAL "Could you make peace with / her for me? I'd go, but / I'd set her on fire."
  choice 669 "Make peace for him?" { "Make peace" ; "Maybe later" }
end

script q40_thanks
  sfx SFX_HAPPY
  say 669 SHOCK "A bake-off! She challenged / me to a FRIENDLY bake-off!"
  [R8]
  say 669 HAPPY "Have some of her cupcakes. / They're incredible. / {.}I hate it."
end

# ---- #41 Squeak the Bat (Biscuitfang 820) ------------------------------------
script q41_offer
  say 820 SAD "*sniffle* My pet bat Squeak / flew off when the gate / opened!"
  [R8]
  say 820 NEUTRAL "He likes the shore of the / Lake of Fire. Can you / find him?"
  [HOLD 8]
  say 820 SAD "Also I bit someone. Sorry. / {.}I bite when I'm sad."
  choice 820 "Find Squeak the bat?" { "Find him" ; "Maybe later" }
end

script q41_thanks
  sfx SFX_SQUEAK
  say 820 HAPPY "{big}SQUEAK!{/} *happy chomp*"
  [R8]
  say 820 SHOCK "Oops. Sorry. That was / you. You're officially a / Bat Buddy now!"
end

# ---- #42 Wall Art (Stencilla 565) --------------------------------------------
script q42_offer
  say 565 NEUTRAL "I painted hearts on the / Wall every night. For / years. Nobody noticed."
  [R8]
  say 565 SAD "Before they knock it down, / could you photograph them?"
  choice 565 "Photograph the hearts?" { "Photograph them" ; "Maybe later" }
end

script q42_thanks
  [HOLD 13]
  say 565 HAPPY "My hearts will live forever / in that picture."
  [R8]
  say 565 NEUTRAL "Here, a Wall Artist Ribbon. / You've earned it. I made / it at night. Obviously."
end

# ---- #43 Encore! (Riffmaw 511) -----------------------------------------------
script q43_offer
  sfx SFX_KAZOO
  say 511 HAPPY "That festival was the best / gig of my un-life!"
  [R8]
  say 511 SHOCK "Get up on the Unity Stage / and give 'em an ENCORE! {!}I / would, but my jaw fell off."
  choice 511 "Play the encore?" { "Play the encore" ; "Maybe later" }
end

script q43_thanks
  sfx SFX_FANFARE
  say 511 HAPPY "{big}ENCORE! ENCORE!{/} You / rock, Hillvile!"
  [R8]
  say 511 NEUTRAL "Take the festival anthem / for your jukebox. Play it / LOUD. Wake the dead. Hi."
end

# ---- #44 Frog Wedding (Confetti 62: Warty 912 -> Lily Lou 82) ----------------
script q44_offer
  say 62 LOVE "Warty sings love songs to / the moon. Lily Lou grows / lilies for the pond."
  [R8]
  say 62 HAPPY "A cross-border frog / wedding! I've already / ordered the flies."
  [HOLD 8]
  say 62 LOVE "Nudge Warty to confess? / {.}The flies won't keep."
  choice 62 "Nudge Warty?" { "Nudge him" ; "Maybe later" }
end

script q44_thanks
  sfx SFX_CONFETTI
  say 62 SHOCK "A wedding! {!}A WEDDING!"
  [R8]
  say 62 HAPPY "The first cross-border / wedding in Hellvile Park! / Take a ring and my pin!"
end

# ---- #45 The Big Picture (Shutterbug 46) -------------------------------------
script q45_offer
  say 46 HAPPY "Darling, I need the shot / of the century. Both sides / of the river. No Wall."
  [R8]
  say 46 NEUTRAL "Stand on the north bridge / and snap it. I'd go, but / I cry at bridges."
  choice 46 "Snap the big picture?" { "Photograph it" ; "Maybe later" }
end

script q45_thanks
  [MUTE 21]
  [HOLD 21]
  say 46 SAD "No Wall in the frame. Just / people."
  [R13]
  say 46 HAPPY "It's the most beautiful / thing I've ever developed." [HEART]
end

# ---- #46 Potluck Planner (Mother Nox 778) ------------------------------------
script q46_offer
  say nox HAPPY "My potluck is open to / everyone now. Everyone. / Even Hillvile."
  [R8]
  say nox NEUTRAL "I need two Soul Soups from / the market. Don't ask / what's in them."
  choice nox "Bring 2 Soul Soup?" { "Fetch them" ; "Maybe later" }
end

script q46_thanks
  sfx SFX_EAT
  say nox HAPPY "Good. Hillvile brought / cake. We brought soup."
  [HOLD 13]
  say nox HAPPY "That is diplomacy. {.}Have / some cake. Not the soup."
end

# ---- #47 Vapula's Apology (Lord Vapula 999 -> Mayor 0) ------------------------
script q47_offer
  sfx SFX_WHOOSH
  say vapula SAD "*tiny wheeze*"
  [HOLD 13]
  say vapula SAD "i'm... smaller now. / humbler. a light drizzle / of a duke."
  [R8]
  say vapula NEUTRAL "give these flowers to your / mayor? with my apology? / {.}tell him it's from a cloud."
  choice vapula "Take him the flowers?" { "Deliver them" ; "Maybe later" }
end

script q47_thanks
  [MUTE 13]
  say vapula SHOCK "he forgave me. he HUGGED / me. with his WINGS."
  [HOLD 13]
  say vapula SAD "i didn't know a cloud could / feel this... dry-eyed." [HEART]
  [R8]
  say vapula HAPPY "thank you. {.}light showers / later. happy ones."
end

# ---- #48 Explorer's Club (Scoutmaster 15) ------------------------------------
script q48_offer
  say 15 HAPPY "As scout leader I MUST / know: what do they sell at / the Hellvile imp market?"
  [R8]
  say 15 HAPPY "Go look! For science! For / badges! {.}Mostly badges."
  choice 15 "Scout the imp market?" { "Go and look" ; "Maybe later" }
end

script q48_thanks
  say 15 SHOCK "Spicy peppers and eyeball / candy?! Incredible. I'm / never going."
  [R8]
  say 15 HAPPY "You've earned the Cross- / Border Explorer badge! I / have one too. {.}Now."
end
