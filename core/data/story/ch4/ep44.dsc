# ch4/ep44.dsc - Chapter 4: Ep 4.4 "Wisp's Room" (the house thread pays off;
# the second player line) and dream D2 "The Ninth Cloud" (owner: S3;
# story_final.md sec.13). Decorate-for runs in SCENE_HOME (HSM_DECORATE_FOR,
# layout 1 = core/data/home/layouts_s3.txt). D2 is SCENE_DREAM's scripted
# mode (DREAM_SCRIPTED_D2); its letters are stashed (SPEC C5).
@pack ep44 kind=episode chapter=4 minutes=32

string O44_PITS "Follow Wisp to Hell Pits 13 (N)"
string O44_SLEEP "Sleep in your new flat (Hell Pits)"

string D2_L0 "Dear Nox, I'm sorry. I'm / sorry. I'm"
string D2_L1 "I miss the bone-flour / bread. - N."
string D2_L2 "I should have held on / longer. - F."
string D2_WAKE "(Morning. The jar on your / shelf is very quiet.)"

script s44_room
  sfx SFX_DOOR
  say nar "(Hell Pits, apartment 13. / Graveley's building. A tiny / imp body asleep in a bed.)"
  [MUTE 26]
  [HOLD 21]
  say nar "(Exactly the bed that turned up in your house. The one you never placed.)" [HEART]
  [R13]
  say nar "(Wisp's parents sit beside it. Two years in. Wisp's mum is humming.)"
  sfx SFX_PLUCK
  say nar "(The lullaby Steamy hummed / wrong. This is the right / way.)"
  [R13]
  say nar "WISP'S DAD: We cleared the / room out. To make it less / sad. {.}It didn't work."
  [R5]
  say ID_WISP SAD "it's all wrong. the chair / faced the wall. the rug was / turned. i can SHOW you."
  push home 0x4004
  home DECORATE_FOR 1
end

script s44_wake
  [MUTE 40]
  say nar "(The room is right. The / thinking chair faces the / wall. The rug is turned.)"
  [HOLD 21]
  say nar "(Wisp lies down in their own / body.)" [HEART]
  [HOLD 21]
  sfx SFX_TIK
  say nar "(Fingers twitch.)"
  [R13]
  say nar "WISP'S MUM: ...Wisp?"
  [R21]
  say ID_WISP SLEEPY "...i went somewhere. there / was a nice house. with a / chair." [HEART]
  [HOLD 26]
  setf SF_WISP_AWAKE
  music SONG_HELLVILE
end

script s44_ask
  say nar "(The parents go to fetch / soup. Wisp sits up, and / looks at you.)"
  player_line ID_WISP "Did they talk to me? While / I was out?" { "Your mum hummed." ; "Your name, every night." ; "They kept your chair." }
  native CB_C34 C34A_WISP
  [R13]
  say ID_WISP HAPPY "...okay. okay. {..}i'm going / to remember that."
end

script s44_penpals
  sfx SFX_SPLASH
  say nar "(Wisp spots the paper boat / in your bag. The crayon / house. From the beach.)"
  [R5]
  say ID_WISP SHOCK "That's MY house. You / KEPT it?"
  [R13]
  say nar "(Pip has come through the gate with Nana. The two kids look at each other.)"
  [R5]
  say pip NEUTRAL "He's good at scary. Mr. / Flops. One night."
  [R5]
  say pip SAD "I'll be fine. My eyes might / do a rain."
  [R5]
  say ID_WISP HAPPY "can Mr. Flops stay? he's / good at scary."
  [R21]
  say nar "(Mr. Flops's stitched heart / and the candle in Wisp's / drawing: the same pink.)"
  reward 0 furn=wisp_drawing_1
  [R13]
  sfx SFX_WELD
  say nar "(The flat next door is yours. And back home,//Rivet welds a Hellvile Annex onto the cottage.)"
  [R8]
  say nar "(The seam is a visible, ugly bead. She is very proud of it.)"
  reward 0 furn=pentagram_rug
  say nar "(Charlotte sends a housewarming gift://a pentagram rug. Cursed. Plush.)"
  insert 4 4
  [R8]
  say nar "(Your lamp has grown horns.)"
  [R8]
  say nar "KEVIN: ...it's fine."
end

# ---- D2 "The Ninth Cloud": night in the Hell Pits flat ----
script s44_d2
  [NIGHT]
  say nar "(Your first night in the flat. The bed is hard and cosy. You sleep.)"
  stash 0 D2_L0
  stash 1 D2_L1
  stash 2 D2_L2
  stash 3 D2_WAKE
  push dream 0x4001
  setf SF_VAPULA_PLUSH
  [DAY 0]
end
