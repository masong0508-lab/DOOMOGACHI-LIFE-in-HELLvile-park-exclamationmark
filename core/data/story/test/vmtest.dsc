# vmtest.dsc - fixtures for core/tests/test_story_vm.c and test_textpack.c
# (owner: S2). A tiny pack that exercises every VM op the engine runs
# itself and the box lint (it is an episode-kind pack, so its lines must
# fit 3 x 28 at 240x160). Nothing in the game starts these scripts.
@pack vmtest kind=episode chapter=0 minutes=0

string VMT_HELLO "Hello, @P! You have @1 hats."
string VMT_PAGES "One page.//Two pages, / with a break."
string VMT_CAP_A "Boss caption one"
string VMT_CAP_B "Boss caption two"

script vmt_flags
  clrf SF_SPARE_253
  jif SF_SPARE_253 no
  setf SF_SPARE_253
  jifn SF_SPARE_253 no
  setv v2 7
  addv v2 -2
  jlt v2 5 no
  jge v2 5 big
no:
  setv v3 1
  end
big:
  setv v3 42
end

script vmt_say
  say mayor HAPPY "Hi there, @P." [HEART]
  say nar "Two lines, one box. {d}"
  setv v1 9
end

script vmt_choice
  choice nar "Left or right?" { "Left" -> left ; "Right" -> right }
left:
  setv v4 10
  end
right:
  setv v4 20
end

script vmt_choice_plain
  choice me "Plain?" { "Yes" ; "No" ; "Maybe" }
  setv v4 30
end

script vmt_call
  call vmt_sub
  setv v6 2
end

script vmt_sub
  setv v5 1
  ret
end

script vmt_wait
  wait 5
  hold 3
  setv v7 1
end

script vmt_push
  push timing 0x0301
  setv v8 1
end

script vmt_later
  later 2 vmt_sub
  rng v9 7
end

script vmt_ext
  native 63 5         # 63: test_story_vm.c observes it as a VM_EXT
  beat C1_KEY
  give ITEM_FLOWERS 2
end

beatscript vmt_beat
  +0 shot 0 ; sfx SFX_BEEP
  +8 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_TIK ; flash PAL_WHITE 6 2
  +13 label L_BUTTON
  +40 end
end

# S2: the caption stash (C5), the verbs and the timing push
script vmt_stash
  stash 2 "A stashed line"
  push combat 1 captions=VMT_CAP_A,VMT_CAP_B
  setv v8 2
end

script vmt_verb
  verb find cell=10,12 then=vmt_sub
  verb escort pip cell=40,21 then=vmt_sub
  verb deliver nana fragile then=vmt_sub
  setv v10 1
end

script vmt_timing
  push timing pattern=rimshot skin=laugh drift
  setv v11 1
end

# test_story_lint.c's frame accounting: 10 + 5 frames, 5 of them muted
script vmt_silence
  wait 10
  mute 5
  hold 5
end
