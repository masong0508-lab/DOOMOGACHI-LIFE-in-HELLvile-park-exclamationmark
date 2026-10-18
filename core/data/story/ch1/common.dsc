# ch1/common.dsc - Chapter 1 "Welcome to HaHaland", the chapter-common pack
# (owner: S2; SPEC sec.7 S2 content, story_final.md sec.5, 6, 10). The HUD
# objectives (OBJ_*, read by core/src/story_ch12.c), the cast's idle lines
# (IDLE_*, story_talk.c), the wrongness rows' captions and scripts (W1-W6,
# W16; the rows are core/src/story_ch12.c), the region lines and the NG+
# lines. On the PS1 this pack sits in the resident common slot (txtload.c),
# so the HUD never evicts the episode pack a script runs from.
@pack ch1 kind=common chapter=1

# ---- the HUD (story_ch12.c; @0 = a count) ----
string OBJ_C1_MAYOR "Say hi to Mayor Giggs by the fountain"
string OBJ_C1_KEY "Your key is with Nana at Brimstone Bites"
string OBJ_C1_HOME "Go home: the pink door on Hillvile Lane (NW)"
string OBJ_C1_HATCH "The egg is wobbling. Stay with it!"
string OBJ_C1_PIP "Someone is crying at the playground (SW)"
string OBJ_C1_BUNNY "Find Mr. Flops on the beach"
string OBJ_C1_BUNNY_RETURN "Bring Mr. Flops back to Pip"
string OBJ_C1_FRIENDS "Meet Steamy, Hattie and Grampy (@0/3)"
string OBJ_C1_FRIENDS_DONE "Nana texted: come get a cupcake!"
string OBJ_C1_CUPCAKE_LULLA "Bring the cupcake to Miss Lulla (park)"
string OBJ_C1_PARTY "Go home for six: it's your housewarming!"
string OBJ_C1_WALL "Something is thumping at the Wall (E)"
string OBJ_C1_HOME_NIGHT "Go home. It's late. It's always late."

# ---- the cast's idle lines in Chapter 1 (story_talk.c) ----
string IDLE_MAYOR_1A "Hee hee! Isn't HaHaland just / the HAPPIEST? Say yes. / Please say yes."
string IDLE_MAYOR_1B "I practise my speeches on / the ducks. They laugh at all / the right places."
string IDLE_NANA_1A "Fresh cinnamon swirls, sweet / pea! Well, fresh-ish. / Fresh in spirit."
string IDLE_NANA_1B "Is @E eating well? A growing / Doomogachi needs carbs. / Lots of carbs."
string IDLE_PIP_1A "Mr. Flops says hi! He's shy. / He hides behind my ear. / Well. His ear."
string IDLE_PIP_1B "I counted everyone in town. / I got to twenty and then / I got hungry."
string IDLE_FUZZ_1A "Bless you, child. The church / doors are always open. / Literally. The hinge broke."
string IDLE_BLOOM_1A "Hello. Sorry about my cold / hands. It's a doctor thing. / {..}A doctor thing."
string IDLE_LULLA_1A "The little ones are napping. / Shh! Nap time is sacred / time."
string IDLE_STEAMY_1A "Bath house is open! Steam, / bubbles, ducks. Mostly / ducks."
string IDLE_HATTIE_1A "What's brown and sticky? / A stick! {.}Nobody laughs at / that one. Except me."
string IDLE_GRAMPY_1A "These tomatoes won't pick / themselves, Toaster. / {..}You ARE Toaster, right?"
string IDLE_MASCOTTY_1A "..."

# ---- wrongness (story_ch12.c rows; captions appear, never pop) ----
string W1_CAP "(The fountain giggles. / Half a second late.)"
string W2_CAP "(Someone painted over a / number on the sign. The / old one was 1000.)"
string W3_CAP "(The flowers all lean toward / the Wall. All of them.)"
string W4_CAP "(Everybody bobs to the music. / The mascot does not.)"
string W5_CAP "(One lamp post is facing / the Wall.)"
string W6_CAP "THUMP. THUMP. THUMP. / {..}THUMP."

script w1_again
  [R5]
  sfx SFX_BUBBLE
  say nar "(...and again, on time.)"
  say nar "{d}...Hee."
end

script w3_pick
  say nar "(You pick one.)"
  [R13]
  sfx SFX_CREAK
  say nar "(It leans toward the Wall / in your hand.)"
end

script w4_wave
  native CB_C12 C12A_MASCOTTY
  [R21]
  say nar "(The mascot waves. / Very, very slowly.)"
end

# ---- the joke grave (W16, the graveyard in Chapter 1) ----
script c1_grave
  sfx SFX_SQUEAK
  say nar "(A row of little graves. One / sign: Rest in pieces, / Mr. Huggins the 1st.)"
  [R8]
  say nar "(A small stone at the end, / in a child's hand:)"
  [MUTE 13]
  [HOLD 13]
  say nar "{d}FLUFFY THE CLOUD. / DRIFTED." [HEART]
  [R13]
  say nar "(Behind you, a pair of / wings stops flapping. / {..}Then starts again.)"
  setf SF_VISITED_GRAVE
end

script c1_grave_again
  say nar "(Fluffy the cloud. Drifted. / Someone has left a cupcake / on the stone.)"
end

# ---- region lines (the first time you walk in, Chapter 1) ----
script rg_wall
  say nar "(The Wall. Cold, rusty, very / tall. Something on the other / side giggles.)"
  [R8]
  say nar "(Nobody on this side looks / at it. You notice that.)"
  setf SF_WALL_HINT
end

script rg_vape
  say nar "(A shop under construction. / CLOUD NINE VAPOR - COMING / SOON!)"
  [R13]
  say nar "(Something purple flickers / inside. It flickers back / when you look.)"
end

script rg_gate
  say nar "(A huge iron gate, seven / padlocks and one very / stern sign: KEEP OUT.)"
  [R8]
  say nar "(Underneath, smaller: / FOREVER.)"
end

script rg_home_locked
  say nar "(Your new cottage! A pink / door. Locked. Nana has / the key.)"
end
