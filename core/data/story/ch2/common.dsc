# ch2/common.dsc - Chapter 2 "Something in the Vapor", the chapter-common
# pack (owner: S2; SPEC sec.7 S2 content, story_final.md sec.1, 5, 6, 11).
# The HUD objectives (story_ch12.c), the cast's idle lines (story_talk.c),
# the wrongness rows W7-W15 (captions and scripts; the rows are
# core/src/story_ch12.c), the Ch2 authored possessors and their windows.
@pack ch2 kind=common chapter=2

# ---- the Ch2 authored possessors (exorcism.h DoomoPossessor rows 0-1; the
# table becomes strong once every row's owner has written its block:
# test_story.c holds these equal to exorcism.c's default rows) ----
@possessor row=0 name="TALLYWORN" host=steamy name_res=712 offer="TALLYWORN|DIRGELET|MOPSWORTH|HAZE" clue="a chalk stub|a tally wall|a bellows|a scrap: \"3,652\"" skin=EXS_STANDARD
@possessor row=1 name="WHEEZLE" host=sprinklepop name_res=707 offer="DIRGELET|MOPSWORTH|CLINKER|PLODDY" clue="purple glitter|a party hat|a grape vape|\"I'm fine!\"" skin=EXS_STANDARD flags=EXF_SCRIPTED_FAIL|EXF_FIXED_ODDS
# their windows: re-armed after a load while the story is between the scream
# and the rite (story.c story_rearm_possessors)
@armed steamy row=0 from=C2_SCREAM to=C2_EXORCISE unless=SF_STEAMY_SAVED|SF_STEAMY_LOST   # TALLYWORN
@armed sprinklepop row=1 from=C2_SCREAM to=C2_EXORCISE                                   # WHEEZLE

# ---- the HUD ----
string OBJ_C2_VAPOR "Check out the new shop by the Wall"
string OBJ_C2_BLOOM "See Dr. Bloom at the hospital"
string OBJ_C2_CLUES "Find 3 odd things for Dr. Bloom (@0/3)"
string OBJ_C2_REPORT "Take your clues to Dr. Bloom (hospital)"
string OBJ_C2_NIGHT "Go home to bed. (It's fine.)"
string OBJ_C2_NIGHT2 "Go home to bed. (Still fine.)"
string OBJ_C2_NIGHT3 "Go home to bed. (Fine-ish.)"
string OBJ_C2_SCREAM "Get a free duck from Steamy (plaza)"
string OBJ_C2_FUZZ "Fetch Father Fuzz from the church (W)"
string OBJ_C2_EXORCISE "Meet Father Fuzz by Steamy (plaza)"
string OBJ_C2_RIVET "See Rivet at her workshop (NE)"
string OBJ_C2_REST "Go home and sleep. Then: the cellar"
string OBJ_C2_CELLAR "Go down the hatch at Cloud Nine Vapor"

# ---- the cast's idle lines in Chapter 2 ----
string IDLE_MAYOR_2A "Tea, anyone? Everything's / fine. I've had nine cups / to make sure. Gobble."
string IDLE_MAYOR_2B "Everything is FINE! The / purple glow by the Wall is / a light show. Hee. Hee."
string IDLE_NANA_2A "Folks come in smelling of / grape, sweet pea. I don't / even BAKE with grape."
string IDLE_NANA_2B "@E chewed a table leg. / Teething! {..}It's just / teething. ...Mostly."
string IDLE_PIP_2A "Patrol rules: no purple / shop. Mr. Flops sniffed it. / It smells like lies."
string IDLE_FUZZ_2A "Holy water and hugs. Both / work on demons. Hugs also / work on me. Hint, hint."
string IDLE_RIVET_2A "Sparks are tiny fireworks / that bite. Don't flinch. / They can smell it."
string IDLE_BLOOM_2A "Three more purple tongues / today. Clinically: gross. / Personally: also gross."
string IDLE_VEX_2A "Back already, sweetie? The / first puff's free. The / second's a subscription."
string IDLE_LULLA_2A "Somebody left a vape pen / in the sandbox. At a / DAYCARE. I'm making a SIGN."
string IDLE_STEAMY_2A "Bath house is open! Steam / only. No vapor. Quackers / here bites vapor."
string IDLE_HAZE_2A "Don't buy the grape. / (waters a plant) / Don't buy any of it."
string IDLE_HATTIE_2A "My punchlines land late / now. Somebody laughs before / me. I hate a heckler."
string IDLE_GRAMPY_2A "My tomatoes taste of grape. / GRAPE, Toaster! I'm / writing to the council."

# ---- wrongness W7-W15 (captions appear, never pop) ----
string W7_CAP "(A neighbour says \"we\" / where they meant \"I\". / Nobody corrects them.)"
string W8_CAP "(Your shadow catches up / with you late. It looks / out of breath.)"
string W9_CAP "(The coffee tastes like / grape. The sign says: NEW! / GRAPE-FREE COFFEE.)"
string W10_CAP "(Your reflection in the / water blinks first.)"
string W11_CAP "(The church bell rings / backwards for one bar.)"
string W12_CAP "(A kid's drawing on the / fence: a purple cloud with / a crown. Gold star. NICE!)"
string W13_CAP "beep?"
string W14_CAP "(The cafe jukebox plays two / low, wrong notes. Then the / same two, louder.)"
string W15_CAP "(Through your window: a / chair you didn't buy. It's / nicer than your chairs.)"

script w7_we
  say ribbonetta HAPPY "We love the weather! / {..}I love it. Me. One / person. Normal amount."
end

script w10_wave
  [R5]
  say nar "(It waves. {.}You didn't. / It waves again, slower, / like you're a tourist.)"
end

script w11_bell
  say cantor SHOCK "Did the bell just un-ring? / Forty years I've rung it. / It has NEVER un-rung."
end

script w14_jukebox
  music SONG_OMINOUS
  [R26]
  sfx SFX_THUD
  say nana NEUTRAL "That's the jukebox, sweet / pea. One good thump and / it's back to polka."
  music SONG_HAHALAND_DAY
end

# ---- the chapter's NG+ subtitle (HaHaland Remembers) ----
string NG_CH2 "(You have seen this shop / before. Or you will. Your / loyalty card remembers.)"
