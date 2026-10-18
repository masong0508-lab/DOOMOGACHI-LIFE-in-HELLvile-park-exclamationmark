# ch3/int3.dsc - Interlude III "The Empty Room": the dark night (owner: S3;
# story_final.md sec.12). No music: the pack is exempt from the silence budget
# (test_story_lint.c). The house is EMPTY (remove-only), the Mayor's
# confession, then the seven knocks, each a visitor who places one gift, the
# music sneaking back one instrument at a time, then the Mayor: "Now."
@pack int3 kind=episode chapter=3 minutes=8

string OI3_HOME "Go home."
string OI3_KNOCK "Someone is knocking (@0/7)"

script si3_home
  [SILENCE]
  native CB_C34 C34A_HOME
  home SET_PHASE HP_EMPTY
  sfx SFX_DOOR
  [HOLD 26]
  say nar "(Home. Half the furniture / burned. Half went to the / barricades. The rest fell.)"
  [R13]
  say nar "(The pet bed is empty. Wisp / is quiet in a corner, very / small.)" [HEART]
  sfx SFX_TIK
  [R26]
  sfx SFX_TIK
  say nar "(The only sound is Tock's / clock on the wall.)"
  choice nar "(You could tidy up. Only / tidy. Take things away.)" { "Look around" -> look ; "Sit on the floor" -> sit }
look:
  home REMOVE_ONLY 1
  push home -1
  home REMOVE_ONLY 0
sit:
  cam CAM_CREEP 10
  [HOLD 26]
  say nar "(You sit on the floor.)"
  sfx SFX_TIK
  [R26]
  cam CAM_RESET
  call si3_mayor
  call si3_k1
  call si3_k2
  call si3_k3
  call si3_k4
  call si3_k5
  call si3_k6
  call si3_k7
  call si3_now
  native CB_C34 C34A_DONE
end

script si3_mayor
  sfx SFX_CREAK
  say nar "(The Mayor didn't go home. / He sits on the picnic crate.)"
  [R21]
  pal PAL_CRAYON 8
  [R2]
  pal PAL_CRAYON 0
  say mayor SAD "It was mine. The breath. / My fear. Your little @E / ate my fear." [HEART]
  [R13]
  say mayor SAD "I never asked where the sad / went. {..}I know where it went / now."
  [HOLD 26]
  pal PAL_CRAYON 8
  [R2]
  pal PAL_CRAYON 0
  say mayor SAD "I had a cloud once."
  [HOLD 26]
  say mayor NEUTRAL "He followed me to school."
  [R13]
  say mayor HAPPY "He rained on bullies. Only / bullies. He was very / precise."
  [R21]
  pal PAL_CRAYON 8
  [R2]
  pal PAL_CRAYON 0
  say mayor SAD "I put a stone up. I was / nine. I never told anyone / what it was for."
  jifn SF_VISITED_GRAVE nostone
  [R8]
  say mayor SAD "...You saw the stone."
nostone:
  [HOLD 26]
  say mayor SAD "Go get yours back, @P. / {d}Don't put up a stone." [HEART]
  [HOLD 26]
  say mayor NEUTRAL "I'll come with you. Over / there.{..} Tomorrow."
  sfx SFX_DOOR
  [R13]
  say nar "(He goes. The door clicks. / The clock ticks.)"
end

# ---- the seven knocks: each one a separate visitor, R26 or more apart ----
script si3_k1
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say nana NEUTRAL "Light's the first thing. / Then soup."
  sfx SFX_THUD
  reward 0 furn=honey_lamp
  say nar "(She sets down a lamp. It / lands soft. She doesn't / leave.)" [HEART]
  native CB_C34 C34A_KNOCK 0
  music SONG_HAHALAND_NIGHT
  duck 40 32000
end

script si3_k2
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say pip NEUTRAL "He's good at being held. / You can borrow him."
  [R8]
  say pip SAD "Not forever. {..}Mostly / forever."
  reward 0 furn=mr_flops_plush
  native CB_C34 C34A_KNOCK 1
  duck 70 32000
end

script si3_k3
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say rivet NEUTRAL "I reset the sign. One day / since the last fire. Don't / make it weird."
  reward 0 furn=days_since_fire
  [R13]
  sfx SFX_THUD
  say nar "(She makes it weird. A hug / like a vice.)"
  native CB_C34 C34A_KNOCK 2
  duck 100 32000
end

script si3_k4
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  jif SF_STEAMY_LOST ghost
  say steamy HAPPY "A duck. For your bath. / Ducks make everything / warmer. That's science."
  jmp gift
ghost:
  say steamy SAD "(Steamy's ghost, with a duck that is also a ghost.) It still floats. Mostly."
gift:
  reward 0 furn=rubber_duck_guest
  native CB_C34 C34A_KNOCK 3
  duck 130 32000
end

script si3_k5
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say hattie NEUTRAL "I didn't bring anything."
  [R8]
  say hattie HAPPY "I'm the bit."
  native CB_C34 C34A_KNOCK 4
  duck 160 32000
end

script si3_k6
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say bloom NEUTRAL "Cold hands. Warm blanket. / That's the deal."
  reward 0 furn=hot_water_bottle
  native CB_C34 C34A_KNOCK 5
  duck 200 32000
end

script si3_k7
  [HOLD 26]
  sfx SFX_THUD
  [R13]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say nar "(Mascotty. She has never / been inside anything. She / stands in the doorway.)"
  [HOLD 21]
  say nar "(She puts down a single / Hellvile sunflower seed. / She leaves.)" [HEART]
  reward 0 furn=sunflower
  native CB_C34 C34A_KNOCK 6
  duck 240 32000
  [R13]
  push home -1
end

script si3_now
  [HOLD 26]
  sfx SFX_THUD
  [R26]
  sfx SFX_THUD
  choice nar "*knock knock*" { "Open the door" }
  say nar "(The Mayor. He doesn't / come in.)"
  say mayor SAD "Tomorrow, I said."
  [R26]
  say mayor ANGRY "{big}No.{/} Now."
  [R8]
  say mayor SHOCK "{d}...Hee. Hee. Oh no. Now."
  native CB_C34 C34A_KNOCK 7
  home SET_PHASE HP_OPEN
  iris heart 40
  [R21]
  iris 0 0
end
