# quests49/q49a.dsc - the old side quests #0-#24, re-timed (owner: S4;
# docs/design/story/SPEC.md sec.7 S4; comic_timing.md sec.6.2). Each quest
# has an offer (it ends on the accept choice: option 0 takes the quest,
# story_side.c accept_choice) and a thank-you (the reward is the table's,
# paid by story_side.c finish() when the script ends). The titles and hints
# are in index.dsc. Timing: one gag per 3-6 lines, gaps on the ladder, a
# hold before every topper, a sting or a sound on every visual beat.
# W5 (script rewrite, docs/design/story/SCRIPT_STYLE.md): the first box of
# every offer says who, what and where; one joke, one button; the accept
# option names the action; pauses only before a turn (C5); lines written to
# 27 columns (the portrait box, §8).
@pack q49a kind=side minutes=60

# ---- #0 Mail Mix-Up (Lilypad Lou 97 -> Valentina 52) -------------------------
script q0_offer
  sfx SFX_SPLASH
  say 97 SHOCK "Ribbit! Valentina's love / letter got stuck to my / hat. She's in the plaza."
  [R8]
  say 97 NEUTRAL "I licked the stamp. I lick / every stamp. It's a frog / thing."
  [HOLD 13]
  say 97 HAPPY "Undelivered mail makes me / sad. {.}Then hungry. Please / hurry."
  choice 97 "Deliver the love letter?" { "Deliver it" ; "Maybe later" }
end

script q0_thanks
  sfx SFX_LOVE
  say 97 HAPPY "She read it and FAINTED. / From joy! Some of it was / the spit."
  [R8]
  say 97 LOVE "Flowers, and my best / Lovebird Stamp. It's only / been licked twice."
end

# ---- #1 Coffee for Bluebell (29) ----------------------------------------------
script q1_offer
  say 29 SLEEPY "I've been up since dawn / making a tulip heart for / the window."
  [R13]
  say 29 SLEEPY "It's a very small heart. / It's one tulip."
  [HOLD 8]
  say 29 SAD "I need a coffee from / Brimstone Bites. {.}I'm / wilting. Professionally."
  choice 29 "Bring Bluebell a coffee?" { "Fetch it" ; "Maybe later" }
end

script q1_thanks
  sfx SFX_EAT
  say 29 HAPPY "{big}*slurp*{/} My petals are / perking up!"
  [R8]
  say 29 HAPPY "Take this Tulip Pin. I made / it from a real tulip. / {.}Mostly."
end

# ---- #2 Lost Kitten (Barkley 96) ----------------------------------------------
script q2_offer
  say 96 SHOCK "Woof! I was walking Sir / Whiskers and he walked ME. / Into a hedge."
  [R8]
  say 96 SAD "Then he vanished. Like a / cat. {.}Because he is one."
  [HOLD 8]
  say 96 NEUTRAL "He likes high places. Can / you look up on Hillvile / Lane?"
  choice 96 "Find Sir Whiskers?" { "Find him" ; "Maybe later" }
end

script q2_thanks
  sfx SFX_HAPPY
  say 96 HAPPY "SIR WHISKERS! You fuzzy / little gremlin!"
  [R5]
  say 96 HAPPY "Thank you! Here's my / dog-walking money."
  [R13]
  say 96 NEUTRAL "Cat-walking money. {.}He / took a cut."
end

# ---- #3 Say Cheese! (Shutterbug 46) -------------------------------------------
script q3_offer
  say 46 HAPPY "Darling! I'm making a / postcard of HaHaland, and / I need the fountain."
  [R5]
  say 46 HAPPY "Stand in front, face it, / press A. I'd do it myself, / but I'm famous."
  [R8]
  say 46 NEUTRAL "If it giggles in the shot, / {.}that's a feature."
  choice 46 "Photograph the fountain?" { "Photograph it" ; "Maybe later" }
end

script q3_thanks
  sfx SFX_SPARKLE
  say 46 LOVE "The sparkle! The light! / You have the eye, darling. / Both of them!"
  [R8]
  say 46 HAPPY "Your model fee. I mean / photographer fee. Don't / ask for more."
end

# ---- #4 Cupid's Little Helper (Smoochy 117: Freckles 11 -> Skipper 25) ---------
script q4_offer
  say 117 LOVE "Psst! Freckles has a HUGE / crush on Skipper the / paper kid."
  [R8]
  say 117 HAPPY "She's in the park, / practising 'hi' on a tree. / The tree said no."
  [HOLD 8]
  say 117 HAPPY "Give her a pep talk? / {.}Before she settles for / the tree."
  choice 117 "Help Freckles confess?" { "Give a pep talk" ; "Maybe later" }
end

script q4_thanks
  sfx SFX_LOVE
  say 117 LOVE "Love is in the air! Or at / least a lot of nervous / giggling."
  [R8]
  say 117 LOVE "Thanks, partner. Take / this perfume. Love is / mostly smelling nice."
end

# ---- #5 Strong Bones (Bonejangles 5) -----------------------------------------
script q5_offer
  sfx SFX_BELL
  say 5 HAPPY "Clack! I teach xylophone. / I AM a xylophone. Mostly / bones. Partly servo."
  [R8]
  say 5 NEUTRAL "Two Milks keep my ribs / ringing true. Calcium. / {.}It's in my user manual."
  choice 5 "Bring Bonejangles 2 Milk?" { "Fetch them" ; "Maybe later" }
end

script q5_thanks
  sfx SFX_EAT
  say 5 HAPPY "{big}*glug glug*{/} It's / leaking out of my ribs. / That's normal. I think."
  [R5]
  sfx SFX_BELL
  [R3]
  sfx SFX_BELL
  [R3]
  sfx SFX_BELL
  say 5 HAPPY "Hear that? Perfect pitch! / You saved my tibia solo. / Clack clack!"
end

# ---- #6 Bedtime Stories (Story Owl 16) ----------------------------------------
script q6_offer
  say 16 HAPPY "Hoo! The little ones want / a new bedtime story."
  [R8]
  say 16 NEUTRAL "The Library in the park / has 'The Very Hungry / Demon'. Fetch it?"
  [HOLD 8]
  say 16 HAPPY "It has a happy ending. / {.}Mostly."
  choice 16 "Visit the Library?" { "Go to the Library" ; "Maybe later" }
end

script q6_thanks
  sfx SFX_SPARKLE
  say 16 HAPPY "Hoo hoo! The kids will love / it. The demon says please."
  [R8]
  say 16 NEUTRAL "A hymn book and a Bookworm / Bookmark. The worm is / retired. {.}Don't wake him."
end

# ---- #7 Ribbon Cutting (Ribbonetta 83) ----------------------------------------
script q7_offer
  say 83 HAPPY "I cut ribbons for a / living! The Fun Fair's / big top opens today."
  [R8]
  say 83 NEUTRAL "But it needs an official / first visitor before I / can snip. It's the law."
  [HOLD 8]
  say 83 HAPPY "My law. {.}I made it up. / I make up a lot of laws."
  choice 83 "Visit the big top?" { "Go see it" ; "Maybe later" }
end

script q7_thanks
  sfx SFX_TINK
  say 83 HAPPY "{big}SNIP!{/} The Fun Fair is / officially fun! By law!"
  [R8]
  say 83 NEUTRAL "Your visitor fee. Don't / spend it on ribbon. Ribbon / is MY thing."
end

# ---- #8 Grandma's Glasses (Oma Fluff 27) --------------------------------------
script q8_offer
  say 27 HAPPY "Dearie! I've lost my / reading glasses. I'd look / myself, but. You know."
  [R8]
  say 27 NEUTRAL "Last I saw them I was / napping on the beach."
  [HOLD 13]
  say 27 SLEEPY "Or was that 1974."
  choice 27 "Find Oma's glasses?" { "Find them" ; "Maybe later" }
end

script q8_thanks
  sfx SFX_SPARKLE
  say 27 SHOCK "Oh! I can see you now! / You're shorter than you / sounded."
  [R8]
  say 27 HAPPY "A pudding, and my old / locket. There's a cake in / it. {.}Don't ask how."
end

# ---- #9 Pier Panorama (Tidepool 112) ------------------------------------------
script q9_offer
  sfx SFX_SPLASH
  say 112 HAPPY "Duuude. The surf club wall / needs a photo from the / very end of the pier."
  [R8]
  say 112 HAPPY "Gnarliest view in / HaHaland. Don't fall in. / {.}Or do. Also gnarly."
  choice 112 "Snap the pier panorama?" { "Photograph it" ; "Maybe later" }
end

script q9_thanks
  sfx SFX_SPARKLE
  say 112 SHOCK "Radical shot! {!}You're / officially in the surf / club now, dude!"
  [R8]
  say 112 NEUTRAL "Membership is one shell. / You owe me a shell."
end

# ---- #10 Clean Sweep (Squishy 39) ---------------------------------------------
script q10_offer
  sfx SFX_BUBBLE
  say 39 HAPPY "Blorp! I wash every window / in Hillvile Towers. With / my whole body."
  [R8]
  say 39 SAD "But I'm out of soap. Now / I'm just a damp feeling on / the glass."
  [HOLD 8]
  say 39 NEUTRAL "Three Soaps from the shop, / please!"
  choice 39 "Bring Squishy 3 Soap?" { "Fetch them" ; "Maybe later" }
end

script q10_thanks
  sfx SFX_BUBBLE
  [R3]
  sfx SFX_BUBBLE
  say 39 HAPPY "Sparkle sparkle! You can / see your reflection in me / now."
  [R8]
  say 39 HAPPY "Look. It's you. {.}Hi. / You look tired."
end

# ---- #11 Choir Practice (Hallelujah 142) -------------------------------------
script q11_offer
  say 142 SHOCK "{big}HALLELUJAH!{/}" [SHOUT]
  [R8]
  say 142 NEUTRAL "Sorry. I mean, hello. / That's just how I say / hello."
  [R5]
  say 142 HAPPY "The Concert Hall needs its / echo tested. Go in and / SING! LOUDLY!"
  choice 142 "Test the hall's echo?" { "Go and sing" ; "Maybe later" }
end

script q11_thanks
  sfx SFX_CHOIR
  say 142 HAPPY "I heard you from here! / Beautiful!"
  [R8]
  say 142 NEUTRAL "Terrible!"
  [R5]
  say 142 HAPPY "Beautiful! Here's a hymn / for your jukebox. Sing it / worse. I dare you."
end

# ---- #12 Smoke Signals (Drizzle 57) -------------------------------------------
script q12_offer
  say 57 SHOCK "The weather's all wrong! / A purple cloud is sitting / on Cloud Nine Vapor."
  [R8]
  say 57 NEUTRAL "Get me a photo? It's NOT / on my forecast. Nothing / purple is ever on it."
  choice 57 "Photograph the cloud?" { "Photograph it" ; "Maybe later" }
end

script q12_thanks
  say 57 SHOCK "Look at that cloud..."
  [HOLD 13]
  [MUTE 13]
  say 57 SHOCK "It's SMILING."
  [R8]
  say 57 SAD "That's not weather. That's / a personality. Here, a / Storm Chaser badge."
end

# ---- #13 Steam Cleaning (Lather 41 -> Steamy 2) -------------------------------
script q13_offer
  sfx SFX_BUBBLE
  say 41 SAD "Poor Steamy still smells / of grape after her... / ordeal. Why GRAPE?"
  [R8]
  say 41 HAPPY "Take her my lavender soap? / It's very calming. It / calms me just holding it."
  choice 41 "Take Steamy the soap?" { "Deliver it" ; "Maybe later" }
end

script q13_thanks
  say 41 HAPPY "She's back at the bath / house, humming!"
  [R8]
  say 41 NEUTRAL "Real humming. Not demon / humming. I checked. Have / some soap. You need it."
end

# ---- #14 Holy Supplies (Sr. Lumen 354) ----------------------------------------
script q14_offer
  sfx SFX_HOLY
  say 354 NEUTRAL "The old scrolls warn of / more possessions, child. / I need Holy Water."
  [R13]
  say 354 NEUTRAL "Two flasks. The church / sells it. Yes, we sell it. / Faith has overheads."
  [R8]
  say 354 HAPPY "The scrolls also say 'buy / two, get faith free'. / {.}I wrote that one."
  choice 354 "Bring 2 Holy Water?" { "Fetch them" ; "Maybe later" }
end

script q14_thanks
  sfx SFX_HOLY
  say 354 HAPPY "Blessed be. Take this / rosary."
  [R8]
  say 354 NEUTRAL "It once made a demon / sneeze for an hour. We / were all very impressed."
end

# ---- #15 Scrap Science (Prof. Fizz 69) ----------------------------------------
script q15_offer
  say fizz HAPPY "Squeak! I'm inventing a / vapor detector! It will / detect vapor. Hopefully."
  [R8]
  say fizz NEUTRAL "I need three bits of / scrap. Rivet sells some. / Or punch a demon for it!"
  choice fizz "Bring Prof. Fizz 3 Scrap?" { "Fetch them" ; "Maybe later" }
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
  say fizz NEUTRAL "It beeps at everything. / Especially me. Take / some gears."
end

# ---- #16 Streamer Scream (Pillowpuff 23 -> Lagspike 14) -----------------------
script q16_offer
  say 23 SHOCK "My client Lagspike vaped / ALL night on stream. Now / he's possessed."
  [R8]
  say 23 SHOCK "His chat is just screaming. / So is he. Views are up."
  [HOLD 8]
  say 23 SAD "Exorcise him? Gently? He's / by the graveyard. {.}Clip / it if you can."
  choice 23 "Help Lagspike?" { "Exorcise him" ; "Maybe later" }
end

script q16_thanks
  sfx SFX_HAPPY
  say 23 HAPPY "He's streaming normal cat / videos again! Views are / down. I'm fine."
  [R8]
  say 23 NEUTRAL "A chill pill and a Golden / Hug Token. {.}Don't stream / them."
end

# ---- #17 No Vapes at Daycare (Miss Lulla 36) ----------------------------------
script q17_offer
  say 36 SHOCK "Someone dropped a vape pen / at the playground! Find / it before the kids do!"
  [R8]
  say 36 SHOCK "They think it's a magic / wand. {!}Bonnie already tried / to turn me into a pony."
  [HOLD 8]
  say 36 SAD "I'm too old to be a pony. / I've checked."
  choice 36 "Find the vape pen?" { "Find it" ; "Maybe later" }
end

script q17_thanks
  sfx SFX_THUD
  say 36 ANGRY "Into the bin it goes. / Forever. The bin is going / in a bigger bin."
  [R13]
  say 36 HAPPY "Thank you, @P. The kids / are safe. {.}And I'm still / not a pony."
end

# ---- #18 A Taste of Home (Rivet 13) ------------------------------------------
script q18_offer
  say rivet NEUTRAL "Don't laugh."
  [HOLD 13]
  say rivet SAD "I miss Hellvile candy. / The shop sells Candy / Eyeballs now. Get me two?"
  [R8]
  say rivet NEUTRAL "For morale. Imp morale. / It's a medical thing. / {.}Shut up."
  choice rivet "Bring 2 Candy Eyeballs?" { "Fetch them" ; "Maybe later" }
end

script q18_thanks
  sfx SFX_EAT
  say rivet NEUTRAL "{big}*crunch*{/}"
  [HOLD 13]
  say rivet SAD "Just like Mum's."
  [R8]
  say rivet ANGRY "Ahem. Take some weld rods. / Tell anyone I'm sentimental / and I'll weld YOU."
end

# ---- #19 Knightly Hearts (Valentina 52: Dame Fluff 9 -> Valkyrie 86) ----------
script q19_offer
  say 52 LOVE "Darling, Dame Fluff and / Valkyrie have been sparring / for YEARS."
  [R8]
  say 52 HAPPY "Every duel ends in a hug / and a nosebleed. It's / obviously love. Or rabies."
  [HOLD 8]
  say 52 LOVE "Go nudge Dame Fluff. West / lawn! {.}Wear a helmet."
  choice 52 "Nudge Dame Fluff?" { "Nudge her" ; "Maybe later" }
end

script q19_thanks
  sfx SFX_LOVE
  say 52 LOVE "They're holding paws! With / gauntlets on! It sounds / like cutlery falling."
  [R8]
  say 52 HAPPY "Take this ring, and my / Cupid's Arrow Pin. {.}Aim / with care."
end

# ---- #20 Pip's Bravery Badge (Pip 42) ----------------------------------------
script q20_offer
  say pip HAPPY "Mr. Flops needs a picture / of the scary Wall! For his / bravery badge!"
  [R8]
  say pip SHOCK "He's too scared to take / it. I'm NOT scared. Patrol / rules."
  [HOLD 13]
  say pip NEUTRAL "You do it though."
  choice pip "Photograph the Wall?" { "Photograph it" ; "Maybe later" }
end

script q20_thanks
  sfx SFX_SPARKLE
  say pip SHOCK "Whoa! {!}Spooky!"
  [R8]
  say pip NEUTRAL "Mr. Flops fainted. He's / fine. He's a plushie. He / faints professionally."
  [R5]
  say pip HAPPY "Here! A plushie for being / brave, and Mr. Flops ears!"
end

# ---- #21 Fire Drill (Chief Spark 99) -----------------------------------------
script q21_offer
  sfx SFX_DEMON_ROAR
  say 99 SHOCK "{big}WOOF!{/} A big brute / demon is sitting on my / fire hydrant by the plaza!"
  [R8]
  say 99 SAD "I'm a fire chief, not a / demon chief! {.}And that's / MY hydrant."
  choice 99 "Fight the brute?" { "Fight it" ; "Maybe later" }
end

script q21_thanks
  sfx SFX_SPLASH
  say 99 HAPPY "The hydrant is FREE! You're / an Honorary Firefighter / now, kid."
  [R8]
  say 99 NEUTRAL "Here's some hellsteel and / a badge. {.}The badge is / slightly on fire."
end

# ---- #22 Barricade the Bakery (Nana 1) ---------------------------------------
script q22_offer
  say nana SHOCK "Sweet pea! The demons are / after my sourdough / starter!"
  [R8]
  say nana ANGRY "Bring me three bits of / scrap and I'll barricade / the cafe."
  [HOLD 8]
  say nana NEUTRAL "Nobody touches Nana's / starter. {.}It's older than / the Mayor. And meaner."
  choice nana "Bring Nana 3 Scrap?" { "Fetch them" ; "Maybe later" }
end

script q22_thanks
  sfx SFX_WELD
  say nana HAPPY "Perfect! Nothing gets / between Nana and her / starter."
  [R8]
  say nana HAPPY "Have some emergency / cupcakes. Every cupcake is / an emergency, sweet pea."
end

# ---- #23 Medic! (Dr. Bloom 111) ----------------------------------------------
script q23_offer
  say bloom SAD "Scrapes. Burns. One / patient is mostly eyebrows / now. I'm out of medicine."
  [R8]
  say bloom NEUTRAL "Bring me two Medicine. My / cold hands are steady. / {.}They're always cold."
  choice bloom "Bring 2 Medicine?" { "Fetch them" ; "Maybe later" }
end

script q23_thanks
  say bloom HAPPY "Everyone will be patched / up. Even the eyebrows."
  [R13]
  say bloom NEUTRAL "You look tired. Drink / these. Doctor's orders. / {.}Not medical advice."
end

# ---- #24 Hymn of Hope (Cantor Coo 426 -> Bellwether 91) ----------------------
script q24_offer
  sfx SFX_BELL
  say cantor SAD "Bellwether rang the bells / too close to the purple / cloud. Now she's possessed."
  [R8]
  tapestop 5
  say cantor SHOCK "And the bells toll / BACKWARDS. Listen."
  [HOLD 21]
  [MUTE 21]
  say cantor SHOCK "They're un-ringing. Free / her before they un-ring / the church."
  choice cantor "Free Bellwether?" { "Exorcise her" ; "Maybe later" }
end

script q24_thanks
  sfx SFX_BELL
  [R8]
  sfx SFX_BELL
  say cantor HAPPY "Listen! The bells ring / forward again!"
  [R8]
  say cantor NEUTRAL "Take some blessed salt. / Salt the road. Salt the / bells. It all needs salt."
end
