# ch1/common.dsc - Chapter 1 "Welcome to HaHaland", the chapter-common pack
# (owner: S2; SPEC sec.7 S2 content, story_final.md sec.5, 6, 10). The HUD
# objectives (OBJ_*, read by core/src/story_ch12.c), the cast's idle lines
# (IDLE_*, story_talk.c), the wrongness rows' captions and scripts (W1-W6,
# W16; the rows are core/src/story_ch12.c), the region lines and the NG+
# lines. On the PS1 this pack sits in the resident common slot (txtload.c),
# so the HUD never evicts the episode pack a script runs from.
@pack ch1 kind=common chapter=1

# ---- the HUD (story_ch12.c; @0 = a count) ----
string OBJ_C1_MAYOR "Say hi to Mayor Giggs at the fountain (plaza)"
string OBJ_C1_KEY "Get your key from Nana at Brimstone Bites (NW)"
string OBJ_C1_HOME "Go home: the pink door, Hillvile Lane (NW)"
string OBJ_C1_HATCH "Stay with the egg. It's hatching! (home)"
string OBJ_C1_PIP "Find the crying kid at the playground (SW)"
string OBJ_C1_BUNNY "Search the beach for Mr. Flops (S)"
string OBJ_C1_BUNNY_RETURN "Bring Mr. Flops back to Pip (SW)"
string OBJ_C1_FRIENDS "Meet Steamy, Hattie and Grampy (park, S, @0/3)"
string OBJ_C1_FRIENDS_DONE "Get a cupcake at Brimstone Bites (NW)"
string OBJ_C1_CUPCAKE_LULLA "Take the cupcake to Miss Lulla (park, S)"
string OBJ_C1_PARTY "Be home (NW) by six. No reason. (Party.)"
string OBJ_C1_WALL "Check the thumping at the Wall (E)"
string OBJ_C1_HOME_NIGHT "Go home (NW). It's late. It's always late."

# ---- the cast's idle lines in Chapter 1 (story_talk.c) ----
string IDLE_MAYOR_1A "Tea party at four! Bring a hat. Bring a spare hat. Hats get nervous."
string IDLE_MAYOR_1B "I practise my speeches on the ducks. They laugh in all the right places."
string IDLE_NANA_1A "Cinnamon swirls, sweet pea! Fresh-ish. Fresh in spirit."
string IDLE_NANA_1B "Is @E eating? A growing Boreagotchi needs carbs. Then more carbs."
string IDLE_PIP_1A "Mr. Flops says hi! He's shy. He hides behind my ear. Well. His ear."
string IDLE_PIP_1B "I counted everyone in town for Patrol! I got to twenty. Then lunch."
string IDLE_FUZZ_1A "Bless you, child! Our doors are always open. The hinge broke."
string IDLE_BLOOM_1A "Hello. Sorry, cold hands. A doctor thing. {..}The stethoscope is worse."
string IDLE_LULLA_1A "Shh! The little ones are napping. Wake them and I will end you. Gently."
string IDLE_STEAMY_1A "Bath house is open! Steam, bubbles, ducks. Mostly ducks."
string IDLE_HATTIE_1A "What's brown and sticky? A stick! {.}Nobody laughs at that one. Except me."
string IDLE_GRAMPY_1A "These tomatoes won't pick themselves, Toaster. {..}You ARE Toaster, right?"
string IDLE_MASCOTTY_1A "*muffled mascot noises*"

# ---- wrongness (story_ch12.c rows; captions appear, never pop) ----
string W1_CAP "The fountain giggles. Half / a second late. Bad dub."
string W2_CAP "Under the sign's 500, old / paint shows through: 1000. / Somebody rounded."
string W3_CAP "Every flower leans toward / the Wall. Even the plastic / one."
string W4_CAP "Everybody bobs to the / music. The mascot doesn't. / It's on break. Forever."
string W5_CAP "One lamp post faces the / Wall. It's either curious / or quitting."
string W6_CAP "THUMP. THUMP. THUMP. / {..}THUMP."

script w1_again
  [R5]
  sfx SFX_BUBBLE
  say nar "(...and again, on time. / Show-off.)"
  say nar "{d}...Hee."
end

script w3_pick
  say nar "(You pick one.)"
  [R13]
  sfx SFX_CREAK
  say nar "(It turns in your hand to / face the Wall. Rude. You / were RIGHT here.)"
end

script w4_wave
  native CB_C12 C12A_MASCOTTY
  [R21]
  say nar "(The mascot waves again. / Even slower. Saving its / energy for something.)"
end

# ---- the joke grave (W16, the graveyard in Chapter 1) ----
script c1_grave
  sfx SFX_SQUEAK
  say nar "(A row of tiny graves. The / first: REST IN PIECES, / MR. HUGGINS THE 1st.)"
  [R8]
  say nar "(A small stone at the end, / in a child's hand:)"
  [MUTE 13]
  [HOLD 13]
  say nar "{d}FLUFFY THE CLOUD. / DRIFTED." [HEART]
  [R13]
  say nar "(Behind you, someone stops / gobbling. {..}Then starts / again, casually.)"
  setf SF_VISITED_GRAVE
end

script c1_grave_again
  say nar "(Fluffy the cloud. Drifted. / Someone has left a cupcake / on the stone.)"
end

# ---- region lines (the first time you walk in, Chapter 1) ----
script rg_wall
  say nar "(The Wall. Rusty and very / tall. Something on the / other side giggles.)"
  [R8]
  say nar "SIGN: NOTHING TO SEE! / LOOK AT THE FOUNTAIN! / -MAYOR"
  setf SF_WALL_HINT
end

script rg_vape
  say nar "SIGN: CLOUD NINE VAPOR. / COMING SOON!"
  [R13]
  say nar "(Something purple flickers / inside. When you look, it / pretends to be a lamp.)"
end

script rg_gate
  say nar "(A huge iron gate with / seven padlocks and a very / stern sign: KEEP OUT.)"
  [R8]
  say nar "Under it, smaller: / FOREVER. Smaller still: / PLEASE."
end

script rg_home_locked
  say nar "(Your new cottage! The pink / door is locked. Nana at the / bakery has the key.)"
end
