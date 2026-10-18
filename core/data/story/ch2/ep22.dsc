# ch2/ep22.dsc - Ep 2.2 "Grape" (owner: S2; story_final.md sec.11). A
# playable investigation that replaces the old "ask three people" counter.
# Legacy beat C2_WEIRD, stages 0-2 (story_ch12.c): 0 Dr. Bloom, 1 the
# Clues board (find 3 of 5: STRY.clues bits 0-4, C12A_CLUE), 2 report.
# Ep 2.3 (stages 3-5) is ch2/ep23.dsc.
@pack ep22 kind=episode chapter=2 minutes=30

script e22_bloom
  say bloom NEUTRAL "@P. Good. Sit. My hands / are cold, it's a doctor / thing."
  say bloom "Five patients this week. All / say everything tastes like / grape."
  [R8]
  say bloom NEUTRAL "I'm a doctor, so: that's / weird."
  [R13]
  say bloom "Help me? Look for anything / odd. Tastes. Words. Shadows. / Write it all down."
  sfx SFX_PICKUP
  say nar "(The quest log has a Clues / board now. Find three.)"
  native CB_C12 C12A_STEP 1
end

# after each clue: three is enough for the doctor
script e22_counted
  native CB_C12 C12A_CLUES
  jlt v2 3 more
  [R13]
  say nar "(That's three. Dr. Bloom / should hear this.)"
  native CB_C12 C12A_STEP 2
  end
more:
  say nar "(A clue for Dr. Bloom: @2 / of 3.)"
end

# 1: grape tongue - Sandwichy's blind taste test
script e22_taste
  say sandwichy HAPPY "Blind taste test! Three / spoons. Tell me what they / are. Close your eyes!"
  choice nar "(Spoon one.)" { "Strawberry?" ; "Mint?" ; "Lemon?" }
  say sandwichy "Grape."
  [R8]
  choice nar "(Spoon two.)" { "Cheese?" ; "Pickle?" ; "Soup?" }
  say sandwichy "Grape."
  [R8]
  choice nar "(Spoon three. It's water.)" { "...Water?" }
  [R5]
  say sandwichy NEUTRAL "{.}Grape."
  [R13]
  say sandwichy SHOCK "Wait. I didn't put grape in / ANY of them."
  native CB_C12 C12A_CLUE 0
  call e22_counted
end

# 2: "we" - catch a customer mid-slip (a timing check)
script e22_we
  say ribbonetta HAPPY "Isn't it a lovely day? We / adore it. We..."
  say nar "(Listen for the slip. Press / A right on it.)"
  push timing pattern=three skin=thump
  jlt v0 1 missed
  sfx SFX_TINK
  say ribbonetta SHOCK "We-- I. I mean I. Why did I / say we?"
  native CB_C12 C12A_CLUE 1
  call e22_counted
  end
missed:
  say ribbonetta HAPPY "We love the weather. / We love it so much."
  say nar "(Smoother this time. You / missed it. Try again.)"
end

# 3: lagging shadows - photograph one
script e22_shadow
  say bonejangles HAPPY "Clack! Take my picture? My / shadow's been running a / little late. Clack."
  verb photo bonejangles then=e22_shadow_done
end

script e22_shadow_done
  jlt v1 1 nope
  say nar "(In the photo, his shadow / is still mid-step. He / isn't.)"
  native CB_C12 C12A_CLUE 2
  call e22_counted
  end
nope:
  say nar "(Just sky. Try again.)"
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
  say nar "(The bell rings. Then it / un-rings, backwards, for / one bar.)"
  say cantor SHOCK "Did the bell just un-ring?"
  [R13]
  native CB_C12 C12A_CLUE 3
  call e22_counted
end

# 5: the daycare drawings
script e22_drawings
  say lulla NEUTRAL "Look. They drew again. / Every single one."
  [R13]
  say nar "(A crowned purple cloud in / every picture. In one, it / is holding a little hand.)"
  [R21]
  say lulla NEUTRAL "They all drew the same / friend."
  native CB_C12 C12A_CLUE 4
  call e22_counted
end

# not a clue: Hattie's set, a beat late
script e22_hattie
  say hattie HAPPY "Why did the ghost cross / the road?"
  [R8]
  [R8]
  sfx SFX_HAPPY
  say nar "(The laugh comes from the / Wall. Late. Like a giggle / in fog.)"
  say hattie NEUTRAL "Thank you. You're too kind. / {..}Whoever you are."
end

script e22_report
  say bloom NEUTRAL "Show me."
  say bloom "Grape. \"We.\" Late shadows. / It's not a bug. It's in / the air."
  say bloom "Let me take your pulse."
  sfx SFX_TIK
  say nar "(A very cold hand.)"
  [R8]
  say bloom SHOCK "Sorry. Ghoul."
  [R8]
  say bloom HAPPY "...I mean: COOL hands. It's / a doctor thing."
  [MUTE 13]
  [HOLD 13]
  say bloom SAD "I've seen this before.{..} / {d}Somewhere else.{punch}" [HEART]
  [HOLD 13]
  say bloom NEUTRAL "Go home, @P. Get some / sleep. I'll keep looking."
  native CB_C12 C12A_STEP 3
end

script e22_bloom_wait
  say bloom NEUTRAL "Anything odd. Tastes, / words, shadows, bells, / drawings. Three will do."
end
