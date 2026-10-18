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
string OBJ_C2_VAPOR "A new shop by the Wall: Cloud Nine Vapor"
string OBJ_C2_BLOOM "Dr. Bloom wants to see you (hospital)"
string OBJ_C2_CLUES "Find clues for Dr. Bloom (@0/3)"
string OBJ_C2_REPORT "Tell Dr. Bloom what you found"
string OBJ_C2_NIGHT "Night. Go home. (It's fine.)"
string OBJ_C2_NIGHT2 "Go home to bed. (Night two.)"
string OBJ_C2_NIGHT3 "Go home to bed. (Night three.)"
string OBJ_C2_SCREAM "Steamy is handing out ducks in the plaza"
string OBJ_C2_FUZZ "Get Father Fuzz (church, W)"
string OBJ_C2_EXORCISE "Meet Father Fuzz in the plaza"
string OBJ_C2_RIVET "Rivet wants to see you (workshop, NE)"
string OBJ_C2_REST "Go home and get some sleep"
string OBJ_C2_CELLAR "The cellar hatch of Cloud Nine Vapor"

# ---- the cast's idle lines in Chapter 2 ----
string IDLE_MAYOR_2A "Has anyone seen my cocoa? / I need cocoa. Lots and lots / of cocoa."
string IDLE_MAYOR_2B "Everything is FINE. Look at / all this fine. So much / fine everywhere."
string IDLE_NANA_2A "Folks keep coming in / smelling of grape. I never / bake with grape, dearie."
string IDLE_NANA_2B "Ah. @E's teething. {..}That's / all it is. Teething."
string IDLE_PIP_2A "I'm not allowed near the / purple shop. Mr. Flops says / it smells like lies."
string IDLE_FUZZ_2A "Keep holy water close and / love closer. Both work on / demons, oddly."
string IDLE_RIVET_2A "Sparks are just tiny / fireworks that bite. Don't / be scared of 'em."
string IDLE_BLOOM_2A "Three more purple tongues / today. I don't like this, / @P."
string IDLE_VEX_2A "Back for more? The first / puff was free. The second / costs a little something."
string IDLE_LULLA_2A "One of the kids found a vape / pen at the playground. At a / DAYCARE."
string IDLE_STEAMY_2A "Bath house is open again! / Steam only. No vapor. / NEVER vapor."
string IDLE_HAZE_2A "(waters a plant) / (does not look up)"
string IDLE_HATTIE_2A "My punchlines keep landing / late. Like somebody else is / laughing first."
string IDLE_GRAMPY_2A "Tomatoes taste of grape this / year. Hm. Must be the / weather, Toaster."

# ---- wrongness W7-W15 (captions appear, never pop) ----
string W7_CAP "(Somebody says \"we\" where / they meant \"I\".)"
string W8_CAP "(Your shadow catches up with / you a moment late.)"
string W9_CAP "(The air smells like grape. / So does the coffee.)"
string W10_CAP "(Your reflection in the water / blinks.)"
string W11_CAP "(The church bell rings / backwards for one bar.)"
string W12_CAP "(A kid's drawing on the / fence: a purple cloud, / wearing a crown.)"
string W13_CAP "beep?"
string W14_CAP "(The cafe jukebox plays two / notes of something low and / wrong.)"
string W15_CAP "(Through your window: a / chair, where you didn't / put one.)"

script w7_we
  say ribbonetta HAPPY "We love the weather. / {..}I mean... I."
end

script w10_wave
  [R5]
  say nar "(It waves. {.}You didn't.)"
end

script w11_bell
  say cantor SHOCK "Did the bell just... / un-ring?"
end

script w14_jukebox
  music SONG_OMINOUS
  [R26]
  sfx SFX_THUD
  say nana NEUTRAL "(hits the jukebox) It does / that."
  music SONG_HAHALAND_DAY
end

# ---- the chapter's NG+ subtitle (HaHaland Remembers) ----
string NG_CH2 "(You have seen this shop / before. Or you will.)"
