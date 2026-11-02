# ch2/ep22.dsc - Ep 2.2 "Grape" (owner: S2; story_final.md sec.11). A
# playable investigation that replaces the old "ask three people" counter.
# Legacy beat C2_WEIRD, stages 0-2 (story_ch12.c): 0 Dr. Bloom, 1 the
# Clues board (find 3 of 5: STRY.clues bits 0-4, C12A_CLUE), 2 report.
# Ep 2.3 (stages 3-5) is ch2/ep23.dsc.
@pack ep22 kind=episode chapter=2 minutes=30

script e22_bloom
  say bloom NEUTRAL "@P. Sit. Strip to the / waist. {.}Kidding. I'm a / ghoul, not a pervert."
  say bloom "Five patients this week. / Every one says EVERYTHING / tastes like grape."
  [R8]
  say bloom NEUTRAL "Let me be technical: / that is some weird shit."
  [R13]
  say bloom "Find me three odd things. / Weird tastes, weird words, / weird shadows. Go."
  sfx SFX_PICKUP
  say nar "NEW! The quest log has a / CLUES board. Find 3 clues. / Detective hat: paid DLC."
  native CB_C12 C12A_STEP 1
end

# after each clue: three is enough for the doctor
script e22_counted
  native CB_C12 C12A_CLUES
  jlt v2 3 more
  [R13]
  say nar "Three clues! That's a / pattern. Take them to / Dr. Bloom at the hospital."
  native CB_C12 C12A_STEP 2
  end
more:
  say nar "CLUE FOUND! That's @2 of 3 / for Dr. Bloom."
end

# 1: grape tongue - Sandwichy's blind taste test
script e22_taste
  say sandwichy HAPPY "Blind taste test! Three / spoons, three flavours. / Eyes shut. No peeking!"
  say sandwichy HAPPY "Mouth open, please. {.}It's / health code. Not a line. / I get asked."
  choice nar "Spoon one. It tastes like...?" { "Strawberry?" ; "Mint?" ; "Lemon?" }
  say sandwichy "Grape."
  [R8]
  choice nar "Spoon two. It tastes like...?" { "Cheese?" ; "Pickle?" ; "Soup?" }
  say sandwichy "Grape."
  [R8]
  choice nar "Spoon three. It's clearly water. It tastes like...?" { "...Water?" }
  [R5]
  say sandwichy NEUTRAL "{.}Grape."
  [R13]
  say sandwichy SHOCK "Wait. I didn't PUT grape / in any of them. {..}That's / a clue! Write that down!"
  native CB_C12 C12A_CLUE 0
  call e22_counted
end

# 2: "we" - catch a customer mid-slip (a timing check)
script e22_we
  say ribbonetta HAPPY "Isn't it a lovely day? We / adore it. We..."
  say nar "Listen for the slip! Press / A the moment she says \"we\"."
  push timing pattern=three skin=thump
  jlt v0 1 missed
  sfx SFX_TINK
  say ribbonetta SHOCK "We-- I. I mean I! Why did / I say \"we\"? There's only / one of me. I checked."
  native CB_C12 C12A_CLUE 1
  call e22_counted
  end
missed:
  say ribbonetta HAPPY "We love the weather. / We love it so much."
  say nar "Missed it. She said it / smoother that time. Try / again."
end

# 3: lagging shadows - photograph one
script e22_shadow
  say bonejangles HAPPY "Clack! Take my picture? / My shadow's been running / late. It's off the beat!"
  verb photo bonejangles then=e22_shadow_done
end

script e22_shadow_done
  jlt v1 1 nope
  say bonejangles SHOCK "Look! In the photo my / shadow's still mid-step. / It missed the downbeat!"
  native CB_C12 C12A_CLUE 2
  call e22_counted
  end
nope:
  say nar "Just sky. Some fan will / freeze-frame it and find / a face anyway. Again!"
end

# 4: the bells ring backwards (the church, at night)
script e22_bells
  native CB_C12 C12A_NIGHT
  jge v2 1 night
  say cantor HAPPY "The bells? Lovely bells. / Come back after dark. They / do a little... thing."
  end
night:
  sfx SFX_BELL
  [R5]
  sfx SFX_BELL
  say nar "(The bell rings. Then it / un-rings, backwards, like / it wants the note back.)"
  say cantor SHOCK "Did it just UN-ring? / Bells can't do that. Tell / the doctor! It's a clue!"
  [R13]
  native CB_C12 C12A_CLUE 3
  call e22_counted
end

# 5: the daycare drawings
script e22_drawings
  say lulla NEUTRAL "Look at today's drawings. / Every kid drew the same / thing. Every. Single. Kid."
  [R13]
  say nar "(A crowned purple cloud in / every picture. In one, it / is holding a little hand.)"
  [R21]
  say lulla NEUTRAL "They say it's their new / friend. {.}That's a clue. / And a parent meeting."
  native CB_C12 C12A_CLUE 4
  call e22_counted
end

# not a clue: Hattie's set, a beat late
script e22_hattie
  say hattie HAPPY "Why did the ghost cross / the road?"
  [R8]
  [R8]
  sfx SFX_HAPPY
  say nar "(A laugh comes from the / Wall. Before the punchline. / Hattie hasn't said it yet.)"
  say hattie NEUTRAL "Thank you. You're too kind. / {..}Whoever you are."
end

script e22_report
  say bloom NEUTRAL "Show me what you've got. / Slowly. I'm writing."
  say bloom "Grape. \"We.\" Late shadows. / It's not a bug. It's in / the air."
  say bloom "Let me take your pulse."
  sfx SFX_TIK
  say nar "(Her hand is VERY cold. / Like a fridge with a / medical degree.)"
  [R8]
  say bloom SHOCK "Sorry. Ghoul."
  [R8]
  say bloom HAPPY "...I mean: COOL hands. / My last date called them / 'a bold choice.'"
  [MUTE 13]
  [HOLD 13]
  say bloom SAD "I've seen this before.{..} / {d}Somewhere else.{punch}" [HEART]
  [HOLD 13]
  say bloom NEUTRAL "Go home, @P. Get some / sleep. I'll keep looking."
  native CB_C12 C12A_STEP 3
end

script e22_bloom_wait
  say bloom NEUTRAL "Three odd things, @P. / Tastes, words, shadows, / bells, drawings. Go!"
end
