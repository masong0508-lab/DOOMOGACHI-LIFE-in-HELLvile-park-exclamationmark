# legacy_ch34.dsc - the legacy port of the 28 old beats, Chapters 3-4 (owner: S2;
# handed to S3, who replaces it with ch3/ and ch4/). One pack, "legacy", is shared by
# legacy_ch12.dsc and legacy_ch34.dsc: the old beats' conversations
# (script l_*), choices (ch_*), narrator lines (LS_*) and HUD objectives
# (OBJ_*). The beat logic stays C glue in core/src/story_script.c, which
# runs these scripts with story_conv(SCR_*, CB_*). lint=legacy: the old
# lines page automatically and are not held to the 3 x 28 box.
@pack legacy kind=legacy chapter=3

# the authored possessions of the old beats (exorcism.c rows; story.c
# re-arms them after a load). 40 / 41 = SF_C3_EXO0 + 0 / 1 (Hattie, Grampy).
@armed hattie row=2 from=C3_ERUPTION to=C3_NESTS unless=40       # HAZE
@armed grampy row=3 from=C3_ERUPTION to=C3_NESTS unless=41       # SOOTKIN
@armed smog row=4 from=C4_GATE to=C4_HELP unless=SF_C4_LADY_SMOG # FLUFFY

string LS_3 "The cloud tower is sealed by a wall of screaming vapor. Clear the nests first!"
string LS_4 "The old hell gate is sealed with flowers now. Sootkin waters them every morning."
string LS_5 "The sealed gate hums. Something on the other side is very, very bored."
string LS_10 "The gate in the Wall is jammed shut by writhing vapor. Not yet."
string LS_11 "A huge iron gate, sealed with seven padlocks and one very stern sign: KEEP OUT. FOREVER."
string LS_13 "The demon holds on! Try again when you're ready."
string LS_14 "The smoke clings on. Lady Smog coughs, \"...again... please...\""
string LS_15 "You retreat to catch your breath. The nest is still there..."
string LS_16 "Lord Vapula's laughter rolls down the cloud stairs. Gather yourself and climb again."

# the HUD objectives (script_objective; @0..@3 are numbers)
string OBJ_C3_ERUPTION "Survive!"
string OBJ_C3_NESTS "Clear demon nests @0/@1, exorcise @2/@3"
string OBJ_C3_SPIRE "Climb Cloud Nine Spire. Defeat Lord Vapula!"
string OBJ_C4_GATE "Cross into Hellvile through the Wall gate (E)"
string OBJ_C4_TALLY "Talk to Tallyworn at the gate plaza"
string OBJ_C4_PRISONERS "Meet Hellvile's citizens (@0/5)"
string OBJ_C4_NOX "Meet Mother Nox at her chapel (N)"
string OBJ_C4_HELP "Help Sootkin, Lady Smog and Anvil (@0/3)"
string OBJ_C4_SUMMIT "Bring the Mayor and Mother Nox together"
string OBJ_C4_MAYOR "Fetch Mayor Giggs from HaHaland's Wall gate"
string OBJ_C4_FESTIVAL "The festival! Chat with folks (@0/4)"
string OBJ_C4_STAGE "Everyone is gathering at the Unity Stage (N)"
string OBJ_DONE "Free play! Explore and help out"

script ch_travel_from_hh
  choice nar "The gate in the Wall stands open." { "Go to Hellvile" ; "Go to the united park" ; "Stay here" }
end

# before the festival the park option is not offered (the old 2-option menu)
script ch_travel_from_hh2
  choice nar "The gate in the Wall stands open." { "Go to Hellvile" ; "Go to the united park" }
end

script l_invaded
  say mayor "@P! Oh thank goodness! The sky RIPPED and DEMONS came out of the CLOUD!"
  say mayor "Everyone's hiding in the church with Father Fuzz! The demons built nests all over town!"
  say mayor "Hattie Haha and Grampy Bean breathed the big cloud... they're not themselves anymore."
  say mayor "Please. Clear the nests. Save our friends. Then... that tower of cloud by the Wall."
  say nar "Walk into a demon nest to fight. Talk to the possessed to exorcise them."
end

script l_nest_cleared
  say nar "Nest cleared! The citizens hiding nearby peek out and cheer."
end

script l_rivet_call
  say rivet "Psst, @P! Got scrap? Swing by the workshop and I'll beef up your gear!"
end

script l_rivet_upgrade
  say rivet "Look at you, all singed and heroic. Let's make that weapon meaner. Or cuter. Or both!"
end

script l_all_nests
  say nar "The last nest falls silent. Above Cloud Nine Vapor, the cloud tower pulses angrily."
  say fuzz "That tower is the heart of it, @P. Lord Vapula himself waits at the top. Go with love."
end

script l_spire_base
  say nar "The shop has become a tower of churning vapor. Cloud stairs spiral up into the storm."
  say nar "Somewhere far above, a voice like a thunderhead laughs: \"COME UP, LITTLE SNACK.\""
end

script l_spire_won
  say nar "Lord Vapula, Duke of the Ninth Cloud, lets out one last enormous wheeze..."
end

script l_possessed_c3
  say nar "@T stares through you. Their eyes glow red, and the voice is not theirs."
  say nar "\"The cloud is SO comfy. Why don't you come up and breathe with us?\""
end

script l_c3_saved
  say nar "@T gasps and blinks. \"Where... where am I? Why is the sky RED?\""
  say nar "The demon @D fled back toward the Wall. @T hurries off to the church."
end

script l_c3_lost
  say nar "The light leaves @T's eyes. The demon took everything with it."
  say fuzz "We tried, @P. We tried everything. Now we can let them rest."
end

script l_c4_open
  say mayor "You did it! The cloud is GONE! The sky is pink again! I'm going to cry! I'm crying!"
  say fuzz "Vapula is defeated... but the gate in the Wall has cracked open, @P."
  say fuzz "Every demon name we heard belonged to a citizen of Hellvile. Were they ever truly our enemies?"
  say rivet "They weren't. Most of them are tired folks Vapula worked half to death. My mum's over there."
  say rivet "Go on, hero. Walk through the gate in the Wall. Meet the neighbours."
end

script l_tally
  say tallyworn "...You came through the gate. You actually came through the gate."
  say tallyworn "I've been counting. 3,652 days pumping vapor at the gate for Lord Vapula."
  say tallyworn "He's gone? Really gone? Then... we're free? We were never allowed to cross."
  say tallyworn "Please, talk to people. They're scared. They think Hillvile folks are monsters."
end

script l_word_spreads
  say nar "Word spreads through Hellvile: a hero from Hillvile is in town, and they're... NICE?"
  say nar "Someone says Mother Nox wants to see you at her chapel, north of the gate plaza."
end

script l_nox_hello
  say nox "So you are the one who popped Vapula like a balloon. I am Mother Nox."
  say nox "I host the potluck. And I suppose I lead this sorry, lovely little town."
  say nox "Vapula held us all hostage. The vapor pumps, the gate, the possessions... his orders."
  say nox "Three of mine need help before Hellvile can trust Hillvile again."
  say nox "Sootkin by the gate, who keeps a secret. Lady Smog, still choking on Vapula's smoke."
  say nox "And Anvil at the forge, who misses her daughter so much she forgets to eat."
  say nox "Help them, and I will meet your Mayor at the Wall."
end

script l_sootkin_ask
  say sootkin "Oh! A Hillvile person! Please don't tell anyone, but... I grow a sunflower under the gate."
  say sootkin "It has never seen real sunshine. Could you bring it some Hillvile flowers for company?"
  say nar "Pick some at a flower bed in HaHaland, then bring them to Sootkin."
end

script l_sootkin_thanks
  say sootkin "Oh! Oh, they're so BRIGHT! Look, sunny, you have friends now!"
  say sootkin "Thank you, @P. I'm going to plant them all around the gate. Right where the smoke was."
end

script l_smog
  say nar "Lady Smog coughs out puffs of black vapor. Something is still inside her, holding on."
  say nar "\"The Duke... is... ETERNAL... *cough* ...help...\""
end

script l_smog_saved
  say smog "*gasp* Air! Real air! I haven't tasted air in eleven years. It's DELICIOUS."
  say smog "I served Vapula because I was afraid. I'm not afraid anymore. Thank you, darling."
end

script l_smog_lost
  say nar "The smoke takes Lady Smog with it. Mother Nox bows her head."
end

script l_anvil
  say anvil "My Rivet ran off to Hillvile ten years ago. Couldn't stand the smoke. Couldn't stand ME, maybe."
  say rivet "...Mum?"
  say anvil "...RIVET?!"
  say nar "They hug like two vices closing. Sparks fly. Actual sparks. They are both welders."
  say rivet "I made you something, Mum. It's a spoon. It's a REALLY good spoon."
  say anvil "It's the best spoon I have ever seen. Thank you, @P. Thank you for bringing her home."
end

script l_help_done
  say nox "You did it. Sootkin is singing, Lady Smog is breathing, and Anvil is crying into a spoon."
  say nox "Fetch your Mayor, hero. I will wait at the gate plaza."
end

script l_mayor_cross
  say mayor "M-meet Hellvile's leader? On the OTHER side? Me?"
  say mayor "...Oh my stars. You walked through demons for us. I can walk through a gate."
  say mayor "Lead the way, @P! I'll bring cupcakes. Everyone likes cupcakes!"
end

script l_summit
  say nox "Mayor Gigglesworth."
  say mayor "Mother Nox. I... I brought cupcakes. Nana Pudding made them. They're cupcakes."
  say nox "...I brought bone soup."
  say mayor "...I LOVE soup."
  say nox "Then I believe we have nothing left to build a Wall for."
  say nar "Together they press their hands to the old Wall. It creaks. It groans. It..."
end

script l_festival
  say nar "The Wall is rubble. The river sparkles. And for the first time ever, the whole park is ONE park."
  say nar "Hillvile and Hellvile pour in from both sides. Somebody started a festival. Everybody came."
end

script l_stage_call
  say nar "Fireworks crackle over the Unity Stage. Everyone is gathering there!"
end

script l_stage
  say mayor "Citizens of Hillvile! Citizens of Hellvile! Citizens of... BOTH!"
  say nox "Tonight there is no Wall. There is only the park. OUR park."
  say pip "And it's all thanks to @P! And @E! And Mr. Flops!"
  say fuzz "Bless this park. Bless every single one of you. Even you, Vex."
  say vex "...Thanks, Father. I'm, uh, selling plain water vapes now. They're terrible."
  say rivet "Enough speeches! Hit it!"
end

script l_done
  say nar "THE END... of the beginning! Free play is unlocked: the united park is yours."
  say nar "Keep helping folks, hunting secrets, and making friends on both sides of the river."
end

script ch_c3_exo_choice
  choice nar "Perform the exorcism?" { "Begin the exorcism!" ; "Not yet..." }
end

script ch_smog_choice
  choice nar "Free Lady Smog from the smoke?" { "Begin the exorcism!" ; "Not yet..." }
end

script ch_spire_choice
  choice nar "Climb Cloud Nine Spire?" { "Go down! (Hell Breach)" ; "Not yet" }
end

script ch_travel_from_hv
  choice nar "Back through the gate?" { "Go to HaHaland" ; "Go to the united park" ; "Stay here" }
end

script ch_travel_from_pk
  choice nar "Where to?" { "Go to HaHaland" ; "Go to Hellvile" ; "Stay here" }
end

