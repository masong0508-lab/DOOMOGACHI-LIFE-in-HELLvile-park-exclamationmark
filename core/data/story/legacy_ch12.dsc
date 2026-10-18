# legacy_ch12.dsc - the legacy port of the 28 old beats, Chapters 1-2 (owner: S2;
# S2 replaces it with ch1/ and ch2/ (Ep 1.1-2.7)). One pack, "legacy", is shared by
# legacy_ch12.dsc and legacy_ch34.dsc: the old beats' conversations
# (script l_*), choices (ch_*), narrator lines (LS_*) and HUD objectives
# (OBJ_*). The beat logic stays C glue in core/src/story_script.c, which
# runs these scripts with story_conv(SCR_*, CB_*). lint=legacy: the old
# lines page automatically and are not held to the 3 x 28 box.
@pack legacy kind=legacy chapter=1

# the authored possessions of the old beats (exorcism.c rows; story.c
# re-arms them after a load, SPEC S2 / INTEGRATION_TODO S3.M1)
@armed steamy row=0 from=C2_SCREAM to=C2_EXORCISE unless=SF_STEAMY_SAVED|SF_STEAMY_LOST   # TALLYWORN

string LS_1 "The Wall towers over you, cold and rusty. Somewhere beyond it, something giggles."
string LS_2 "A heavy hatch with a padlock shaped like a little skull. Locked. For now."
string LS_6 "Your new cottage! The door is locked. Nana Pudding has the key."
string LS_7 "A shop under construction. A sign: CLOUD NINE VAPOR - COMING SOON! Something purple flickers inside."
string LS_8 "The shop is empty. Purple light leaks from the cellar hatch outside."
string LS_9 "\"BACK IN 5 MINUTES - VEX\". The sign has said that for an hour."
string LS_12 "The egg is still warm. Step inside your cottage again when you're ready to hatch it."

# the HUD objectives (script_objective; @0..@3 are numbers)
string OBJ_C1_MAYOR "Say hi to Mayor Giggs by the fountain"
string OBJ_C1_KEY "Get your house key from Nana at Brimstone Bites"
string OBJ_C1_HOME "Go home: the pink door on Hillvile Lane (NW)"
string OBJ_C1_HATCH "Your Doomogachi is hatching!"
string OBJ_C1_PIP "Someone is crying at the playground (SW)"
string OBJ_C1_BUNNY "Find Pip's bunny plush on the beach"
string OBJ_C1_BUNNY_RETURN "Bring Mr. Flops back to Pip"
string OBJ_C1_FRIENDS "Make friends: talk to citizens (@0/3)"
string OBJ_C1_CUPCAKE_LULLA "Bring the cupcake to Miss Lulla in the park"
string OBJ_C1_CUPCAKE_NANA "Visit Nana Pudding at Brimstone Bites"
string OBJ_C1_WALL "Investigate the purple fog by the Wall (E)"
string OBJ_C2_VAPOR "A new shop by the Wall: Cloud Nine Vapor"
string OBJ_C2_WEIRD "Folks are acting weird. Ask around (@0/3)"
string OBJ_C2_SCREAM "Someone screamed in the plaza!"
string OBJ_C2_FUZZ "Get Father Fuzz at the Church of St. Cuddles"
string OBJ_C2_EXORCISE "Exorcise Steamy in the plaza"
string OBJ_C2_RIVET "Rivet wants to see you at the Workshop (E)"
string OBJ_C2_CELLAR "Investigate the cellar of Cloud Nine Vapor"
string LS_TRIES_1 "The demon clings on! Rest, then try again. @0 more failure and the soul is lost."
string LS_TRIES_N "The demon clings on! Rest, then try again. @0 more failures and the soul is lost."

script ch_exorcise_choice
  choice fuzz "Ready, @P? The odds look like @1%. Hold the holy water like you mean it." { "Begin the exorcism!" ; "Not yet..." }
end

script l_mayor_hello
  say mayor "Oh my stars! A new face! Welcome, welcome, WELCOME to HaHaland!"
  say mayor "I'm Mayor Gigglesworth. Hee hee! Everyone just calls me Mayor Giggs."
  say mayor "And you must be @P! The happiest town on Earth is SO happy to have you!"
  say mayor "That's Brimstone Bites, our bakery-cafe. Don't worry about the name. It's ironic!"
  say mayor "Down south is the Hillvile side of Hellvile Park. Flowers! Ducks! A fountain that giggles!"
  say me "Hellvile Park?"
  say mayor "Ah. Yes. The park has two sides. Ours is Hillvile, the good side. The OTHER side..."
  say mayor "...is behind the Wall. We don't talk about the other side. Hee. Hee."
  say mayor "ANYWAY! Your cottage is up on Hillvile Lane. I gave the key to Nana Pudding to keep safe."
  say mayor "Pop into Brimstone Bites and ask her for it. Tell her Mayor Giggs sent you! Toodles!"
end

script l_nana_key
  say nana "Well hello, sweet pea! You must be @P. The Mayor told me all about you."
  say nana "Your key? Oh dear. Oh my. I may have... baked it into a muffin."
  say nana "Don't fret! I found it. It only smells a teensy bit like blueberries."
  say nar "You got the House Key! It is warm and slightly sticky."
  say nana "And one more thing, dearie. Every new neighbour gets a Doomogachi!"
  say nana "It's only an egg now. Keep it warm, feed it, love it. It'll hatch into something wonderful."
  say nana "...Probably wonderful. The last one hatched into a little demon. Ha! Kidding. Mostly."
  say nar "You got a Doomogachi egg! It wiggles happily."
  say nana "Run along home now. Hillvile Lane, up the stairs north-west. The cottage with the pink door!"
end

script l_home
  say nar "Home sweet home! It smells like fresh paint and cinnamon rolls."
  say nar "The egg wobbles in your arms. Crack... crack... CRACK!"
end

script l_hatched
  say nar "@E is here! It blinks up at you with enormous, sparkly eyes."
  say nar "@E will follow you everywhere. Check on it from the START menu: feed it, clean up, play!"
  say nar "Somewhere outside, a little kid is crying. Maybe go and see?"
end

script l_pip_lost
  say pip "*sniff* ...h-hi. I'm Pip. I'm not crying. My eyes are just doing a rain."
  say pip "I lost Mr. Flops! He's my bunny plush. One ear flops and one ear doesn't."
  say pip "We were at the beach and a big wave said WHOOSH and then he was gone..."
  say me "Don't worry, Pip. I'll find him!"
  say pip "R-really? You're the bravest grown-up ever! Mr. Flops loves sand. Check the beach!"
end

script l_bunny_found
  say nar "Something fluffy is poking out of the sand..."
  say nar "It's Mr. Flops! He is a bit damp, but still smiling his stitched little smile."
end

script l_pip_thanks
  say pip "MR. FLOPS!!! You're alive! You smell like seaweed! I love you!"
  say pip "Thank you, @P! You can have my best sticker. It's a skull. Skulls are cute."
  say pip "You should make friends with EVERYONE! HaHaland has so many people. I counted to twenty."
  say nar "Talk to the citizens around town to make friends!"
end

script l_three_friends
  say nar "Three new friends! Your heart feels three sizes bigger."
  say nar "Your phone buzzes. A text from Nana Pudding: \"Come by the cafe, sweet pea! xo\""
end

script l_nana_cupcake
  say nana "Three new friends already? You're a natural, sweet pea!"
  say nana "Could you do an old sheep a favour? Take this cupcake to Miss Lulla in the park."
  say nana "She runs the daycare and she loves my cupcakes more than life itself."
  say nar "You got a Cupcake! It has a little sugar heart on top."
end

script l_lulla
  say lulla "Is that... is that a Nana Pudding cupcake? With the little sugar heart?"
  say lulla "*nom* ...I'm going to cry. I'm crying. Thank you, @P."
  say lulla "Oh, have you seen the purple fog by the Wall? The children keep pointing at it."
  say lulla "It giggles at night. Fog shouldn't giggle. Could you take a peek? Carefully!"
end

script l_fog_after
  say nar "The fog curls through the cracks in the Wall like little purple fingers..."
  say nar "A voice, sweet as syrup: \"Hillvile... come play with us... we have FLAVOURS...\""
  say mayor "@P! Step away from there! Nobody goes near the Wall. NOBODY."
  say mayor "It's just... weather. Hellvile weather. Hee. Hee. Let's all go home and have cocoa."
end

script l_ch2_open
  say nar "A few days pass. Then, one morning, a brand new shop opens right next to the Wall."
  say nar "Its sign glows purple: CLOUD NINE VAPOR. Grand opening! Free samples!"
end

script l_vex_hello
  say vex "Well, well. A fresh face! Welcome to Cloud Nine Vapor, friend. Grand opening!"
  say vex "I'm Vex. I'm from out of town. WAY out. Other-side-of-a-Wall out of town."
  say vex "Today's flavour is @V. Tastes like a hug from someone who wants something."
end

script l_vex_no
  say vex "Suit yourself. You'll come around. Everyone in Hillvile comes around."
  say nar "On your way out you notice people leaving the shop... humming. All the same note."
end

script l_vex_yes
  say nar "You take a tiny puff. The world goes soft and purple. Something giggles inside your ear."
  say vex "There it is. The Cloud Nine feeling. Come back soon, @P. We'll be waiting."
  say nar "You feel... weird. You should probably see how everyone else is doing."
end

script l_scream
  say nar "A scream echoes from the plaza! It sounds like Steamy from the bath house!"
end

script l_steamy_possessed
  say steamy "heh... heh heh... the bath house is CLOSED, little @P. Forever and ever."
  say steamy "Steamy isn't home right now. We moved in. The flavours were SO good."
  say nar "Steamy's eyes glow red. His voice sounds like three people talking at once."
  say mayor "Oh my stars, oh my STARS! Get Father Fuzz! The Church of St. Cuddles, west of the plaza!"
end

script l_fuzz_hello
  say fuzz "Bless you, child. I felt it too. A darkness has moved into our Steamy."
  say fuzz "I am Father Fuzz. Stuffed with love and a little bit of buckwheat."
  say fuzz "An exorcism is a gentle art: a prayer in rhythm, holy water, and the demon's true name."
  say fuzz "Let's practise on Mr. Huggins, my plush dummy. Follow my lead!"
end

script l_fuzz_practised
  say fuzz "Splendid! You have the paws of a healer."
  say fuzz "Now, the real thing. I'll meet you by poor Steamy in the plaza. Hurry!"
end

script l_steamy_saved
  say steamy "W-what happened? Why do I taste grape? I don't even LIKE grape..."
  say fuzz "The demon is gone. As it fled it shrieked its true name: @D."
  say fuzz "@D... that is a citizen of Hellvile. A real person, from the other side of the Wall."
  say fuzz "Someone over there is sending their worst through that vapor. Stay watchful, @P."
end

script l_steamy_lost
  say fuzz "...I'm so sorry, child. Steamy's soul is lost to the vapor."
  say fuzz "When you are ready, we can lay him to rest. Only then. Only after we tried everything."
  say fuzz "Remember the name the demon screamed: @D. A citizen of Hellvile."
end

script l_rivet_hello
  say rivet "So YOU'RE the one poking demons with holy water. Nice. I'm Rivet."
  say rivet "Yeah, yeah, I'm an imp. Ran away from Hellvile years ago. Don't make it weird."
  say rivet "Listen. I know that vapor. Lord Vapula has been cooking something big over there."
  say rivet "Holy water won't cut it if things go sideways. You need something with a trigger."
end

script l_rivet_cute
  say rivet "A bubble wand?! HA! I love it. Demons HATE being adorable."
  say rivet "Here's your armory. Weld new stuff whenever you find scrap. Mind the sparks!"
end

script l_rivet_brutal
  say rivet "Ohhh yes. A boomstick. Rivet approves. Hellsteel stock, pink grip. Classy."
  say rivet "Here's your armory. Weld new stuff whenever you find scrap. Mind the sparks!"
end

script l_rivet_vex
  say rivet "Oh, and hey. Your Vex guy? He just slipped into his cellar with a big glowing crate."
  say rivet "Imps know a portal crate when they see one. Go check it out. Bring the boom."
end

script l_cellar_won
  say nar "You smash the portal crate. The vape wraith shrieks and pops like a soap bubble."
  say nar "Vex is nowhere to be seen. Only a note, in curly purple handwriting:"
  say nar "\"Too late, Hillvile. The big cloud is already rising. Kisses! - V\""
  say nar "The ground starts to shake..."
end

script l_cellar_lost
  say nar "You stumble back up the stairs, coughing purple. Catch your breath and try again."
end

script ch_vex_choice
  choice vex "First puff's free. Go on. Everyone in Hillvile is trying it!" { "No thanks!" ; "...Just one puff." }
end

script ch_weapon_choice
  choice rivet "I weld. Cute or brutal, I don't judge. What's your style?" { "Cute! (Bubble Wand)" ; "Brutal! (Pump Shotgun)" }
end

script ch_fog_choice
  choice nar "Purple fog seeps through the Wall. It smells like grape candy." { "Peek through the fog" ; "Back away" }
end

script ch_cellar_choice
  choice nar "The cellar hatch is open. Purple light pulses below. It smells of grape and sulfur." { "Go down! (Hell Breach)" ; "Not yet" }
end

