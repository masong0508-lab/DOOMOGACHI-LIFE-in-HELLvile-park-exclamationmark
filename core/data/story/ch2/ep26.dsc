# ch2/ep26.dsc - Ep 2.6 "Don't Make It Weird" (owner: S2; story_final.md
# sec.11). Legacy beat C2_RIVET: stage 0 Rivet (the weapon, the first
# weld: Flashback F1 plays after it, ch2/f1.dsc), 1 home to sleep (Dream
# D1, ch2/d1.dsc). SF_WEAPON_BRUTAL is read all game (sec.8.2).
@pack ep26 kind=episode chapter=2 minutes=25

# the Days Since Last Fire sign: 7, then "-" for 2 frames, then 0
beatscript b26_sign
  +0 sfx SFX_TINK
  +3 label L_PUNCH
L_PUNCH:
  +0 sfx SFX_THUD ; shake SHK_SLAM ; flash PAL_UI_RED 8 2
  +8 sfx SFX_SPLASH
  +16 sfx SFX_WHOOSH
  +21 label L_BUTTON
L_BUTTON:
  +0 sfx SFX_TIK
  +8 end
end

script e26_rivet
  say rivet NEUTRAL "So YOU'RE the one poking / demons with holy water. / Nice. I'm Rivet."
  say rivet "Yeah. Imp. Ran away from / Hellvile years ago."
  say nar "(You tell her you're glad / she's here.)"
  [R13]
  say rivet SHOCK "Don't make it weird."
  say nar "(You say she has a nice / workshop.)"
  [R8]
  say rivet SHOCK "Don't make it WEIRD."
  say nar "(You say thank you.)"
  [R5]
  [R3]
  say rivet ANGRY "DON'T. Make. It. Weird."
  [R13]
  say rivet NEUTRAL "Listen. I know that vapor. / Holy water won't cut it / if things go sideways."
  choice rivet "I weld. Cute or brutal, I / don't judge. What's your / style?" { "Cute! (Bubble Wand)" -> cute ; "Brutal! (Pump Shotgun)" -> brutal ; "Can you fix a lock?" -> lock }
lock:
  npc rivet FREEZE
  [MUTE 13]
  [HOLD 13]
  say rivet NEUTRAL "...No."
  npc rivet THAW
  choice rivet "Cute or brutal?" { "Cute! (Bubble Wand)" -> cute ; "Brutal! (Pump Shotgun)" -> brutal }
cute:
  native CB_C12 C12A_WEAPON 0
  say rivet HAPPY "A bubble wand?! HA! I love / it. Demons HATE being / adorable."
  jmp weld
brutal:
  native CB_C12 C12A_WEAPON 1
  say rivet HAPPY "Ohhh yes. A boomstick. / Hellsteel stock, pink grip. / Classy."
weld:
  befriend rivet 4
  say rivet "There's your bench. Weld / something. Mind the sparks!"
  push workshop
  push cutscene CUT_F1
  [MUTE 13]
  [HOLD 13]
  say nar "(Rivet's hand is shaking.)"
  [R8]
  say rivet SAD "{d}...Nothing. Weld fumes." [HEART]
  [R21]
  say nar "(Over the bench, a sign: / DAYS SINCE LAST FIRE: 7)"
  say nar "(Rivet's tail brushes a / spark.)"
  runbeat b26_sign
  say nar "(DAYS SINCE LAST FIRE: 0. / A bucket. Mask up. / Thumbs up.)"
  say rivet NEUTRAL "Back in Hellvile I welded / gates. One gate."
  [R13]
  say rivet NEUTRAL "I was ten."
  [R21]
  [R13]
  say rivet "Lord Vapula's cooking / something big. And your / Vex guy..."
  say rivet "...slipped into his cellar / with a glowing crate. Imps / know a portal crate."
  reward 0 furn=toaster_turret
  say rivet HAPPY "Here. A toaster turret. For / the house.{.} For nothing."
  [R13]
  setf SF_VEX_GONE
  say rivet "Get some sleep first. You / look like a Tuesday."
  native CB_C12 C12A_STEP 1
end

script e26_rivet_again
  say rivet NEUTRAL "Go. Sleep. Then the cellar. / Bring the boom. Don't make / it weird."
end

# home, then Dream D1 (the captions are stashed for the dream's floating
# letters; the scripted dream changes nothing, the morning is ours)
script e26_sleep
  [NIGHT]
  [MUTE 21]
  [HOLD 21]
  say nar "(You fall asleep the moment / you lie down.)"
  call d1_dream
  native CB_C12 C12A_SLEEP
  say nar "(You wake. The chair is / tucked into your bed again. / You are both in it.)"
  [R13]
  sfx SFX_BEEP
  say nar "beep."
  beat C2_CELLAR
end
