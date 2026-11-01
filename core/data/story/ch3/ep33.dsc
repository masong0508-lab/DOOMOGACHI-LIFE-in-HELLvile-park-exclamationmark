# ch3/ep33.dsc - Chapter 3: Ep 3.3 "Four Nests" (owner: S3; story_final.md
# sec.12). The plaza, the beach, the Fun Fair and the graveyard, any order:
# an overworld nest fight (story_script.c's nest encounter), then the nest's
# set piece, then (plaza, beach, graveyard) its Hell Breach (3, 4, 5) and the
# beat after the boss. Fuzz Goes Dark is the beach nest. Each cleared nest
# lets its citizens out with one line (N33_*, picked by story_rng).
@pack ep33 kind=episode chapter=3 minutes=60

string O33_NESTS "Clear the demon nests (@0/4)"
string O33_HEART "Go down the Hell Breach and beat the nest's boss"
string O33_RIVET "Go see Rivet at the Workshop (NE)"

# ---- the nests' citizens come out (story_ch34.c picks one of six) ----
string N33_0_0 "We hid in the bread oven. Warm, dark, smells great. Would hide again."
string N33_0_1 "Nana hit a demon with a rolling pin. It said sorry. The DEMON said sorry."
string N33_0_2 "My shop has claw marks. {..}I'm going to call them 'rustic'."
string N33_0_3 "Is it over? Don't say it's over. Last time somebody said that, the sky ripped."
string N33_0_4 "I hid under the fountain's rim. The fountain was humming. Is that normal?"
string N33_0_5 "Thank you! I'd hug you but I'm still shaking. That counts as a hug, right?"
string N33_1_0 "The sea's blue again. Well, one wave's still red. We don't talk to that wave."
string N33_1_1 "A demon took my sandcastle. It built a better one. I'm conflicted."
string N33_1_2 "Pip yelled at an imp. The imp CRIED. Pip said sorry. Good kid."
string N33_1_3 "Father Fuzz walked past us and didn't say bless you. He ALWAYS says bless you."
string N33_1_4 "My paper boat came back from over the Wall. With a sticker on it. A sticker!"
string N33_1_5 "The gulls are back. They stole my chips. Thank goodness, normal crime!"
string N33_2_0 "The ferris wheel spins forwards again. I've never been so happy to be dizzy."
string N33_2_1 "The teacups had eyes in them. They winked. I'm never drinking tea again."
string N33_2_2 "I won a plushie from a possessed claw machine. It's warm. Is that bad?"
string N33_2_3 "The bumper cars say sorry when they bump again. Order is restored."
string N33_2_4 "You bonked Father Fuzz. Everyone saw. We will never forget it."
string N33_2_5 "The cotton candy is pink again! {.}Somebody ate the machine, though."
string N33_3_0 "The ghosts were so polite. One of them fixed my umbrella."
string N33_3_1 "Grandpa Puff's moustache came back for ten minutes. It looked great."
string N33_3_2 "The Great Cake of '89 rose again. We ate it. It was still a great cake."
string N33_3_3 "The Mayor's still out there by the little stone. He says he's fine."
string N33_3_4 "Mr. Wiggles waved at me. From his grave. I waved back. That's manners."
string N33_3_5 "The fog came out of a grave. Not a big grave. A small one. A kid's one."

script s33_intro
  say fuzz NEUTRAL "Four demon nests, @P. The / plaza, the beach, the Fun / Fair and the graveyard."
  [R8]
  say mayor SHOCK "Hattie and Grampy are out / there too. Standing very, / VERY still."
  [R8]
  say fuzz SAD "Leave them for now. The / nests feed their demons. / Smash the nests first."
  [R8]
  say rivet HAPPY "Weapon need teeth? Come / to the Workshop. I have / teeth. Metal ones."
end

script s33_possessed
  npc target STILL
  [MUTE 13]
  [HOLD 13]
  say target POSSESSED "We are SO comfy in here. / Come and breathe with us."
  [R8]
  say nar "Not yet. Nests first. Their / eyes follow you all the way / down the street. Rude."
  npc target THAW
end

# ---- the plaza nest: Nana and the sourdough starter ----
script s33_plaza
  sfx SFX_THUD
  [R5]
  say nar "In the bakery, Nana holds / off three demons with a / rolling pin.//Two of them are stuck in / the dough. Proofing."
  [R8]
  say nana ANGRY "Not the STARTER! That / starter is older than the / Mayor!"
  [R8]
  say nana NEUTRAL "Thank you, sweet pea. The / rest ran down the drain, / into the sewers. Sticky."
  [R8]
  say nar "HELL BREACH 3: the Sugar / Rush Sewers. Something / down there is chewing."
end

script s33_plaza_won
  sfx SFX_HAPPY
  [R5]
  say nana HAPPY "You beat the king of the / sewers! I baked you a map. / Into a pie."
  give ITEM_PUDDING 1
  say nana NEUTRAL "Eat it first, read it / second. The other way / round is just rude."
  [R8]
  say nar "The map is baked into the / crust. It shows all four / nests. In jam."
end

# ---- the beach nest: Mr. Flops, and Fuzz Goes Dark ----
script s33_beach
  sfx SFX_SQUEAK
  [R5]
  say pip SHOCK "Code red! Imps took Mr. / Flops! They went to the / Plushie Factory!"
  [R8]
  say nar "Back at the church, the / door is wide open. Father / Fuzz's hymnal is gone."
  [MUTE 21]
  [HOLD 21]
  say nar "(A trail of stuffing leads / to the Factory. Father Fuzz / went alone.)" [HEART]
  [R13]
  say pip SAD "He went to get a new Mr. / Huggins. {..}There's no / spare. There never is."
  [R8]
  say nar "HELL BREACH 4: the Plushie / Factory. The foreman sings / along with the conveyor."
end

beatscript b33_rip
  +0 shake SHK_SLAM ; sfx SFX_THUD ; cam CAM_ZOOM 320
  +5 sfx SFX_SCRATCH
  +8 insert 1 4
  +13 mute 41 ; sfx SFX_TIK
  +21 label L_PUNCH
L_PUNCH:
  +0 caption "(It's buckwheat. It was / always buckwheat.)"
  +26 nobutton ; cam CAM_RESET
  +29 end
end

script s33_speech
  say nar "FLUFFNUTS: We make 'em. / We don't mend 'em."
  sfx SFX_SCRATCH
  [R5]
  say nar "He tears Mr. Huggins the / 6th clean in half on the / conveyor."
  runbeat b33_rip
  [MUTE 41]
  [HOLD 26]
  say nar "After the boss, Father Fuzz / sits on the conveyor with / half a plush in each hand."
  [R8]
  say fuzz SAD "I hit a demon with a / hymnal, @P. It worked. / The hug didn't." [HEART]
  [HOLD 26]
  say fuzz SAD "What am I FOR?"
  [R21]
  say fuzz SAD "Buckwheat. He was always / buckwheat. I KNEW that."
  [R13]
  say fuzz NEUTRAL "Faith is a stuffing. You / don't notice it until / something tears." [HEART]
  [HOLD 26]
  give ITEM_HOLY_WATER 2
  say fuzz SAD "You do them. The exorcisms. / I'll... I'll watch the / kids."
  [R8]
  say nar "He walks home. Slowly. It's / running away, in slow / motion."
  music SONG_INVASION
  [R13]
  sfx SFX_SQUEAK
  [R5]
  say nar "At the beach, Pip catches / Mr. Flops out of the air. / A perfect dive."
  [R8]
  say pip HAPPY "I was brave!"
  [R8]
  say pip SLEEPY "...I need a nap."
end

script s33_bell
  sfx SFX_BELL
  [R8]
  sfx SFX_BELL
  [R5]
  say nar "The church bell rings at / the wrong time. Father Fuzz / is ringing it for the kids."
  [R13]
  say nar "{d}(bedtime.)"
end

# ---- the Fun Fair nest: the rides are possessed ----
script s33_park
  sfx SFX_GLITCH
  [R5]
  say nar "The ferris wheel spins / backwards. The teacups are / full of eyes. Free refills."
  [R8]
  say nar "One Whack-an-Imp machine / survived. Its sign: DO / NOT BONK FATHER FUZZ."
  [R8]
  push timing pattern=three skin=thump
  sfx SFX_BOING
  [R8]
  say nar "You bonk Father Fuzz. / He was standing RIGHT next / to the machine."
  [R8]
  say fuzz HAPPY "Seven hundred years of / training and I lose to a / mallet. {..}Ow. Bless you."
end

# (after the beach: Fuzz has gone dark, the last joke is gone too)
script s33_park_dark
  sfx SFX_GLITCH
  [R5]
  say nar "The ferris wheel spins / backwards. The teacups are / full of eyes. Free refills."
  push timing pattern=three skin=thump
  sfx SFX_BOING
  [R8]
  say nar "You bonk Father Fuzz. / He was standing RIGHT next / to the machine."
  [R8]
  say fuzz SAD "...Ow."
  [R8]
  say nar "No joke. Not even a pun. / He rubs his head and goes / back to the kids."
end

# ---- the graveyard nest: the Mayor at the little stone ----
script s33_grave
  sfx SFX_CHOIR
  [R5]
  say nar "The joke graves rise as cute / ghosts: Mr. Wiggles, Grandpa / Puff's moustache, a cake."
  [R8]
  say nar "Mr. Huggins the 1st rises / in pieces. He waves an arm. / The arm waves back."
  [MUTE 13]
  [HOLD 21]
  say nar "(The fog pours OUT of one / small stone: FLUFFY THE / CLOUD. DRIFTED.)" [HEART]
  [R13]
  say nar "The Mayor is here. Alone, / in the middle of the nest, / not hiding. He has a shovel."
  [R8]
  say nar "He does not use it."
  [R8]
  say mayor SAD "I'm not hiding. I'm... / visiting."
  [R21]
  say mayor SAD "Clear them off him, / would you."
  [R8]
  sfx SFX_BEEP
  [R5]
  say nar "@E: beep."
  [R3]
  say nar "The nearest imp backs off. / From the grave, the fog / beeps back. Lower."
  [MUTE 14]
  [R13]
  jif SF_STEAMY_LOST lost
  say steamy HAPPY "Hose! Hose! I brought a / HOSE! Point me at a demon!"
  jmp go
lost:
  say steamy HAPPY "I'm keeping the dead warm / in my bathtub. It's nice, / actually. Cold out."
go:
  say nar "HELL BREACH 5: Graveyard / Shift. Somebody down there / is weeping. Professionally."
end

script s33_grave_won
  sfx SFX_SAD
  [R5]
  say nar "The Weeping Widow finds her / plushie at last. She was / sitting on it. Since 1887."
  [R8]
  say mayor NEUTRAL "Thank you, @P. He's... / it's just a joke stone. / Hee. Hee."
  [MUTE 8]
  [R8]
  say mayor SAD "{..}It's just a stone."
end

# ---- between the nests ----
script s33_rivet_call
  say rivet NEUTRAL "Psst, @P! Two nests to go. / Swing by the Workshop and / I'll beef up your gear."
  [R8]
  say rivet NEUTRAL "Oh, and my sign says zero / days since the last fire. / It's on fire."
  [R8]
  say rivet NEUTRAL "{d}It's a different fire."
end

script s33_rivet_upgrade
  sfx SFX_WELD
  [R5]
  say rivet HAPPY "Look at you, all singed and / heroic. Let's make that / weapon meaner. Or cuter."
end

script s33_retreat
  say nar "You back off to catch your / breath. The nest isn't going / anywhere. Nests are patient."
end

script s33_mayor_idle
  say mayor SAD "Shovel? What shovel? This / is a... very long teaspoon. / {.}For tea. Hee. Hee."
end
