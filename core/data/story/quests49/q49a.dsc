# quests49/q49a.dsc - the old side quests #0-#24, re-timed (owner: S4;
# docs/design/story/SPEC.md sec.7 S4; comic_timing.md sec.6.2). Each quest
# has an offer (it ends on the accept choice: option 0 takes the quest,
# story_side.c accept_choice) and a thank-you (the reward is the table's,
# paid by story_side.c finish() when the script ends). The titles and hints
# are in index.dsc. Timing: one gag per 3-6 lines, gaps on the ladder, a
# hold before every topper, a sting or a sound on every visual beat.
@pack q49a kind=side minutes=60

# ---- #0 Mail Mix-Up (Lilypad Lou 97 -> Valentina 52) -------------------------
script q0_offer
  sfx SFX_SPLASH
  say 97 SHOCK "Ribbit! {.}A love letter for / Valentina got stuck to / my hat."
  [R8]
  say 97 NEUTRAL "I licked the stamp. {..}I lick / all the stamps. {..}It's a / frog thing."
  [HOLD 13]
  say 97 HAPPY "Hop it over to the plaza? / Undelivered mail is a / personal failure."
  choice 97 "Deliver the love letter?" { "Sure, I'll help!" ; "Maybe later" }
end

script q0_thanks
  sfx SFX_LOVE
  say 97 HAPPY "She read it and FAINTED. / {.}From joy! {..}Mostly from / joy."
  [R8]
  say 97 LOVE "A thank-you, and my best / lovebird stamp. {.}Lightly / licked."
end

# ---- #1 Coffee for Bluebell (29) ----------------------------------------------
script q1_offer
  say 29 SLEEPY "I've been up since dawn / arranging tulips into the / shape of a heart."
  [R13]
  say 29 SLEEPY "{..}It's a very small heart."
  [HOLD 8]
  say 29 SAD "I need coffee. {.}Please. / {..}From Brimstone Bites."
  choice 29 "Bring Bluebell a coffee?" { "Sure, I'll help!" ; "Maybe later" }
end

script q1_thanks
  sfx SFX_EAT
  say 29 HAPPY "{big}*slurp*{/} {.}My petals are / perking up!"
  [R8]
  say 29 HAPPY "Take this tulip pin. I made / it from a real tulip. / {..}Mostly."
end

# ---- #2 Lost Kitten (Barkley 96) ----------------------------------------------
script q2_offer
  say 96 SHOCK "Woof! {!}I was walking / Sir Whiskers and he walked / ME. Into a hedge."
  [R8]
  say 96 SAD "Then he vanished. {..}Like a / cat. {.}Because he is one."
  [HOLD 8]
  say 96 NEUTRAL "He likes high places. / Up on Hillvile Lane?"
  choice 96 "Find Sir Whiskers?" { "Sure, I'll help!" ; "Maybe later" }
end

script q2_thanks
  sfx SFX_HAPPY
  say 96 HAPPY "SIR WHISKERS! {.}You little / rascal!"
  [R5]
  say 96 HAPPY "Thank you, thank you, / thank you! {.}Here's my / dog-walking money."
  [R13]
  say 96 NEUTRAL "{..}Cat-walking money."
end

# ---- #3 Say Cheese! (Shutterbug 46) -------------------------------------------
script q3_offer
  say 46 HAPPY "Darling! I'm making a / postcard of HaHaland!"
  [R5]
  say 46 HAPPY "The giggling fountain. / Stand right in front, face / it, press A."
  [R8]
  say 46 NEUTRAL "If it giggles in the shot, / {..}it's a feature."
  choice 46 "Photograph the fountain?" { "Sure, I'll help!" ; "Maybe later" }
end

script q3_thanks
  sfx SFX_SPARKLE
  say 46 LOVE "Oh, the sparkles! The light! / {.}You have the eye, darling."
  [R8]
  say 46 HAPPY "Your model fee. {..}I mean / photographer fee. {.}I mean / both."
end

# ---- #4 Cupid's Little Helper (Smoochy 117: Freckles 11 -> Skipper 25) ---------
script q4_offer
  say 117 LOVE "Psst! {.}Freckles has a HUGE / crush on Skipper the paper / kid."
  [R8]
  say 117 HAPPY "She's in the park, / practising 'hi' to a tree. / {..}The tree said no."
  [HOLD 8]
  say 117 HAPPY "Give her a little pep / talk? For love?"
  choice 117 "Help Freckles confess?" { "Sure, I'll help!" ; "Maybe later" }
end

script q4_thanks
  sfx SFX_LOVE
  say 117 LOVE "Love is in the air! {..}Or / at least nervous giggling."
  [R8]
  say 117 LOVE "Thanks, partner. {.}Here: / smell nice. It's a love / tool. {.}Use it wisely."
end

# ---- #5 Strong Bones (Bonejangles 5) -----------------------------------------
script q5_offer
  sfx SFX_BELL
  say 5 HAPPY "Clack clack! I'm a xylophone / teacher AND a xylophone."
  [R8]
  say 5 NEUTRAL "Two Milks keep these bones / ringing true. {..}Calcium. / It's in the brochure."
  choice 5 "Bring Bonejangles 2 Milk?" { "Sure, I'll help!" ; "Maybe later" }
end

script q5_thanks
  sfx SFX_EAT
  say 5 HAPPY "{big}*glug glug*{/}"
  [R5]
  sfx SFX_BELL
  [R3]
  sfx SFX_BELL
  [R3]
  sfx SFX_BELL
  say 5 HAPPY "Hear that? {.}Perfect pitch! / You saved my tibia solo."
end

# ---- #6 Bedtime Stories (Story Owl 16) ----------------------------------------
script q6_offer
  say 16 HAPPY "Hoo! The little ones want / a new bedtime story."
  [R8]
  say 16 NEUTRAL "'The Very Hungry Demon'. / {.}The Library in the park / has a copy."
  [HOLD 8]
  say 16 HAPPY "It has a happy ending. / {..}Hoo. {..}Mostly."
  choice 16 "Visit the Library?" { "Sure, I'll help!" ; "Maybe later" }
end

script q6_thanks
  sfx SFX_SPARKLE
  say 16 HAPPY "Hoo hoo! The kids will / love it. {.}The demon is / very polite."
  [R8]
  say 16 NEUTRAL "Take this, and a bookworm / bookmark. {.}The worm is / retired."
end

# ---- #7 Ribbon Cutting (Ribbonetta 83) ----------------------------------------
script q7_offer
  say 83 HAPPY "I cut ribbons professionally! / {.}Snip snip!"
  [R8]
  say 83 NEUTRAL "The Fun Fair's big top / needs an official visitor / before I can snip."
  [HOLD 8]
  say 83 HAPPY "That's the law. {..}My law. / I made it up."
  choice 83 "Visit the big top?" { "Sure, I'll help!" ; "Maybe later" }
end

script q7_thanks
  sfx SFX_TINK
  say 83 HAPPY "{big}SNIP!{/} {.}The Fun Fair is / officially fun!"
  [R8]
  say 83 NEUTRAL "Your official visitor fee. / {..}Don't spend it all on / ribbon."
end

# ---- #8 Grandma's Glasses (Oma Fluff 27) --------------------------------------
script q8_offer
  say 27 HAPPY "Dearie! I've lost my / reading glasses!"
  [R8]
  say 27 NEUTRAL "Last I saw them I was / napping at the beach."
  [HOLD 13]
  say 27 SLEEPY "{..}Or was it 1974."
  choice 27 "Find Oma's glasses?" { "Sure, I'll help!" ; "Maybe later" }
end

script q8_thanks
  sfx SFX_SPARKLE
  say 27 SHOCK "Oh! I can see you now! / {.}You're lovely."
  [R8]
  say 27 HAPPY "A pudding, and my old / locket. {..}There's a cake / in it. Don't ask how."
end

# ---- #9 Pier Panorama (Tidepool 112) ------------------------------------------
script q9_offer
  sfx SFX_SPLASH
  say 112 HAPPY "Duuude. {.}The surf club wall / needs a photo from the very / end of the pier."
  [R8]
  say 112 HAPPY "The gnarliest view in / HaHaland. {..}Don't fall / in. {.}Or do. Gnarly."
  choice 112 "Snap the pier panorama?" { "Sure, I'll help!" ; "Maybe later" }
end

script q9_thanks
  sfx SFX_SPARKLE
  say 112 SHOCK "Radical shot! {!}You're / officially in the surf / club now, dude!"
  [R8]
  say 112 NEUTRAL "Membership is one shell. / {..}You owe me one shell."
end

# ---- #10 Clean Sweep (Squishy 39) ---------------------------------------------
script q10_offer
  sfx SFX_BUBBLE
  say 39 HAPPY "Blorp! I wash every window / in Hillvile Towers."
  [R8]
  say 39 SAD "I'm out of soap. {..}And / full of pride. {.}Mostly / pride."
  [HOLD 8]
  say 39 NEUTRAL "Three Soaps from the shop, / please!"
  choice 39 "Bring Squishy 3 Soap?" { "Sure, I'll help!" ; "Maybe later" }
end

script q10_thanks
  sfx SFX_BUBBLE
  [R3]
  sfx SFX_BUBBLE
  say 39 HAPPY "Sparkle sparkle! {.}You can / see your reflection in me / now."
  [R8]
  say 39 HAPPY "Look. {..}It's you. {.}Hi."
end

# ---- #11 Choir Practice (Hallelujah 142) -------------------------------------
script q11_offer
  say 142 SHOCK "{big}HALLELUJAH!{/}" [SHOUT]
  [R8]
  say 142 NEUTRAL "{..}I mean, hello. / {.}Sorry. Hello."
  [R5]
  say 142 HAPPY "The Concert Hall needs / someone to test the echo. / Go in and SING! Loudly!"
  choice 142 "Test the hall's echo?" { "Sure, I'll help!" ; "Maybe later" }
end

script q11_thanks
  sfx SFX_CHOIR
  say 142 HAPPY "I heard you from here! / {.}Beautiful!"
  [R8]
  say 142 NEUTRAL "{..}Terrible!"
  [R5]
  say 142 HAPPY "Beautiful! {.}Here's a hymn / for your jukebox."
end

# ---- #12 Smoke Signals (Drizzle 57) -------------------------------------------
script q12_offer
  say 57 SHOCK "The weather's all wrong. / {.}A purple cloud is sitting / over Cloud Nine Vapor."
  [R8]
  say 57 NEUTRAL "It's NOT on my forecast. / {..}Nothing purple is ever / on my forecast."
  choice 57 "Photograph the cloud?" { "Sure, I'll help!" ; "Maybe later" }
end

script q12_thanks
  say 57 SHOCK "Look at that cloud..."
  [HOLD 13]
  [MUTE 13]
  say 57 SHOCK "{..}It's SMILING."
  [R8]
  say 57 SAD "That's not meteorology. / That's evil. {.}Here, a storm / chaser badge. Stay dry."
end

# ---- #13 Steam Cleaning (Lather 41 -> Steamy 2) -------------------------------
script q13_offer
  sfx SFX_BUBBLE
  say 41 SAD "Poor Steamy still smells of / grape after her... ordeal."
  [R8]
  say 41 HAPPY "Bring her my lavender soap? / {.}It's very calming. It / calms me just holding it."
  choice 41 "Take Steamy the soap?" { "Sure, I'll help!" ; "Maybe later" }
end

script q13_thanks
  say 41 HAPPY "She's back at the bath / house, humming!"
  [R8]
  say 41 NEUTRAL "Real humming. {..}Not demon / humming. {.}I checked. / Bless you!"
end

# ---- #14 Holy Supplies (Sr. Lumen 354) ----------------------------------------
script q14_offer
  sfx SFX_HOLY
  say 354 NEUTRAL "The old scrolls warn of / more possessions."
  [R13]
  say 354 NEUTRAL "We need Holy Water, two / flasks. {.}The church sells / it, child."
  [R8]
  say 354 HAPPY "The scrolls also say 'buy / two, get faith free'. {..}I / wrote that one."
  choice 354 "Bring 2 Holy Water?" { "Sure, I'll help!" ; "Maybe later" }
end

script q14_thanks
  sfx SFX_HOLY
  say 354 HAPPY "Blessed be. {.}Take this / rosary."
  [R8]
  say 354 NEUTRAL "It once made a demon sneeze / for an hour. {..}We were / all very impressed."
end

# ---- #15 Scrap Science (Prof. Fizz 69) ----------------------------------------
script q15_offer
  say fizz HAPPY "Squeak! I'm inventing a / vapor detector!"
  [R8]
  say fizz NEUTRAL "I need three bits of scrap. / {.}Rivet sells some, or win / them in fights!"
  choice fizz "Bring Prof. Fizz 3 Scrap?" { "Sure, I'll help!" ; "Maybe later" }
end

script q15_thanks
  sfx SFX_WELD
  say fizz SHOCK "{big}EUREKA!{/} It beeps!"
  [R5]
  sfx SFX_BEEP
  [R3]
  sfx SFX_BEEP
  [R3]
  sfx SFX_BEEP
  say fizz NEUTRAL "{..}It beeps at everything. / {.}Take some gears for your / welding."
end

# ---- #16 Streamer Scream (Pillowpuff 23 -> Lagspike 14) -----------------------
script q16_offer
  say 23 SHOCK "My client Lagspike vaped / ALL night on stream."
  [R8]
  say 23 SHOCK "Now his chat is only / screaming. {..}And so is he."
  [HOLD 8]
  say 23 SAD "Can you help him? Gently? / He's by the graveyard."
  choice 23 "Help Lagspike?" { "Sure, I'll help!" ; "Maybe later" }
end

script q16_thanks
  sfx SFX_HAPPY
  say 23 HAPPY "He's streaming cat videos / again! {.}Normal cat / videos."
  [R8]
  say 23 NEUTRAL "A chill pill and a golden / hug token. {..}Don't stream / them."
end

# ---- #17 No Vapes at Daycare (Miss Lulla 36) ----------------------------------
script q17_offer
  say 36 SHOCK "Someone dropped a vape pen / at the playground!"
  [R8]
  say 36 SHOCK "The kids think it's a magic / wand. {!}Bonnie tried to / turn me into a pony."
  [HOLD 8]
  say 36 SAD "Please find it before they / do!"
  choice 36 "Find the vape pen?" { "Sure, I'll help!" ; "Maybe later" }
end

script q17_thanks
  sfx SFX_THUD
  say 36 ANGRY "Into the bin it goes. / {.}Forever."
  [R13]
  say 36 HAPPY "Thank you, @P. {..}The kids / are safe. {.}I am still / not a pony."
end

# ---- #18 A Taste of Home (Rivet 13) ------------------------------------------
script q18_offer
  say rivet NEUTRAL "Don't laugh."
  [HOLD 13]
  say rivet SAD "I miss Hellvile candy. / {.}The shop sells Candy / Eyeballs now."
  [R8]
  say rivet NEUTRAL "Two packs? {.}For morale. / {..}Imp morale."
  choice rivet "Bring 2 Candy Eyeballs?" { "Sure, I'll help!" ; "Maybe later" }
end

script q18_thanks
  sfx SFX_EAT
  say rivet NEUTRAL "{big}*crunch*{/}"
  [HOLD 13]
  say rivet SAD "{..}Just like Mum's."
  [R8]
  say rivet ANGRY "Ahem. Thanks. Have some weld / rods. {.}Don't tell anyone / I'm sentimental."
end

# ---- #19 Knightly Hearts (Valentina 52: Dame Fluff 9 -> Valkyrie 86) ----------
script q19_offer
  say 52 LOVE "Dame Fluff and Valkyrie / have been sparring for / YEARS, darling."
  [R8]
  say 52 HAPPY "Every duel ends in a hug / and a nosebleed. {..}It's / obviously love."
  [HOLD 8]
  say 52 LOVE "Go nudge Dame Fluff for me. / West lawn!"
  choice 52 "Nudge Dame Fluff?" { "Sure, I'll help!" ; "Maybe later" }
end

script q19_thanks
  sfx SFX_LOVE
  say 52 LOVE "They're holding paws! / {.}With gauntlets on!"
  [R8]
  say 52 HAPPY "Take this ring, and my / Cupid's Arrow pin. {..}Aim / with care."
end

# ---- #20 Pip's Bravery Badge (Pip 42) ----------------------------------------
script q20_offer
  say pip HAPPY "Mr. Flops wants a picture of / the scary Wall!"
  [R8]
  say pip SHOCK "He's too scared. {.}I'm NOT / scared."
  [HOLD 13]
  say pip NEUTRAL "{..}You do it though."
  choice pip "Photograph the Wall?" { "Sure, I'll help!" ; "Maybe later" }
end

script q20_thanks
  sfx SFX_SPARKLE
  say pip SHOCK "Whoa! {!}Spooky!"
  [R8]
  say pip NEUTRAL "Mr. Flops fainted. {..}He's / fine. He's a plushie."
  [R5]
  say pip HAPPY "A plushie for being brave, / and Mr. Flops ears!"
end

# ---- #21 Fire Drill (Chief Spark 99) -----------------------------------------
script q21_offer
  sfx SFX_DEMON_ROAR
  say 99 SHOCK "{big}WOOF!{/} {!}A big brute demon / is sitting on my fire / hydrant!"
  [R8]
  say 99 SAD "I'm a fire chief, not a / demon chief! {.}Help!"
  choice 99 "Fight the brute?" { "Sure, I'll help!" ; "Maybe later" }
end

script q21_thanks
  sfx SFX_SPLASH
  say 99 HAPPY "The hydrant is FREE! {.}You're / an honorary firefighter / now, kid."
  [R8]
  say 99 NEUTRAL "Here's your badge. {..}It / is slightly on fire."
end

# ---- #22 Barricade the Bakery (Nana 1) ---------------------------------------
script q22_offer
  say nana SHOCK "Sweet pea! {!}The demons are / after my sourdough / starter!"
  [R8]
  say nana ANGRY "Three bits of scrap and I / can barricade the cafe."
  [HOLD 8]
  say nana NEUTRAL "Nobody touches Nana's / starter. {..}It's older / than the Mayor."
  choice nana "Bring Nana 3 Scrap?" { "Sure, I'll help!" ; "Maybe later" }
end

script q22_thanks
  sfx SFX_WELD
  say nana HAPPY "Perfect! {.}Nothing gets / between Nana and her / starter."
  [R8]
  say nana HAPPY "Have some emergency / cupcakes. {..}Every cupcake / is an emergency."
end

# ---- #23 Medic! (Dr. Bloom 111) ----------------------------------------------
script q23_offer
  say bloom SAD "So many scrapes and singed / fur. {..}I'm running low."
  [R8]
  say bloom NEUTRAL "Two Medicine, please. {.}My / cold hands are steady, at / least."
  choice bloom "Bring 2 Medicine?" { "Sure, I'll help!" ; "Maybe later" }
end

script q23_thanks
  say bloom HAPPY "Thank you. {.}Everyone will be / patched up."
  [R13]
  say bloom NEUTRAL "You look tired. {..}Drink / these. Doctor's orders."
end

# ---- #24 Hymn of Hope (Cantor Coo 426 -> Bellwether 91) ----------------------
script q24_offer
  sfx SFX_BELL
  say cantor SAD "Bellwether rang the bells / too close to the cloud."
  [R8]
  tapestop 5
  say cantor SHOCK "Now they toll BACKWARDS. / {..}Listen."
  [HOLD 21]
  [MUTE 21]
  say cantor SHOCK "{..}They're un-ringing. / Free her, please!"
  choice cantor "Free Bellwether?" { "Sure, I'll help!" ; "Maybe later" }
end

script q24_thanks
  sfx SFX_BELL
  [R8]
  sfx SFX_BELL
  say cantor HAPPY "Listen! {.}The bells ring / forward again!"
  [R8]
  say cantor NEUTRAL "Take some blessed salt for / the road. {..}Salt the road / if you like."
end
