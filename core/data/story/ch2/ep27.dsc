# ch2/ep27.dsc - Ep 2.7 "The Cellar" (owner: S2; story_final.md sec.11,
# sec.3). Hell Breach 1, the note, and the eruption in three parts: the
# birds stop (overworld), "The Gate Cracks" (Grumbleguts' first scene: the
# CUTV_GATE_CRACKS vignette, grumbleguts_gfx), CUT_ERUPTION. Ch2 ends at
# home. Legacy beat C2_CELLAR; then C3_ERUPTION (S3).
@pack ep27 kind=episode chapter=2 minutes=27

string VEX_PA1 "VEX: Customer satisfaction is our TOP priority!"
string VEX_PA2 "VEX: ...why are you shooting the merch."

# the birds stop: daylight, birds; one stops; all of them stop; the music
# tape-stops and the camera creeps toward the Wall
beatscript b27_birds
  +0 sfx SFX_SPARKLE
  +21 sfx SFX_TIK
  +29 label L_PUNCH
L_PUNCH:
  +0 npc mayor RIPPLE 12 ; tapestop 8 ; nobutton
  +8 mute 64
  +13 cam CREEP 10
  +64 cam FOLLOW
  +77 end
end

# CUTV_GATE_CRACKS (25 s): Vapula looms over the sealed gate; below, the
# Gatekeeper, mid-meal on a mountain of Brimstone Bagels, rear to camera.
# Shots: 0 the meal, 1 the forklift, 2 the gut, 3 the belt, 4 the
# belly-flop, 5 the squeeze, 6 the crack. Cartoon-modest (SF sec.3).
beatscript bv_gate
  +0 music SONG_HELL_BREACH ; shot 0 ; caption "VAPULA: Gatekeeper.{..} / The gate."
  +26 waita S1 12 120
S1:
  +0 shot 1 ; caption "GRUMBLEGUTS: (mouth full) / Darlin', I only had a / light snack--"
  +5 sfx SFX_BEEP
  +13 sfx SFX_THUD ; shake SHK_TAP
  +26 waita S2 12 120
S2:
  +0 shot 2 ; sfx SFX_SQUEAK ; caption "GURGLE: gurgle. / GRUMBLEGUTS: He says hello."
  +26 waita S3 12 120
S3:
  +0 shot 3 ; caption "(He stands. He sucks the / gut in.)"
  +13 hold POP 13
POP:
  +0 sfx SFX_SCRATCH ; shake SHK_TAP ; caption "(It springs back out.)"
  +26 waita S4 12 120
S4:
  +0 shot 4 ; sfx SFX_CREAK ; caption "(A dramatic sigh. The belt / comes off.) Don't be like / that. I'll be gentle."
  +21 sfx SFX_CREAK
  +26 waita S5 12 150
S5:
  +0 shot 5 ; sfx SFX_WHOOSH ; caption "(The belly-flop.)"
  +3 hitstop 3
  +5 label L_PUNCH
L_PUNCH:
  +0 shake SHK_ROAR ; flash PAL_UI_RED 10 4 ; sfx SFX_EXPLODE ; nobutton
  +13 caption "(The gate cracks.)"
  +26 waita S6 12 120
S6:
  +0 shot 6 ; caption "(He squeezes through rear / first. He is stuck.)"
  +13 hold BOING 13
BOING:
  +0 sfx SFX_BOING ; shake SHK_STAMP ; caption "BOING."
  +2 sfx SFX_SCRATCH
  +26 waita S7 12 120
S7:
  +0 shot 7 ; caption "GRUMBLEGUTS: That was the / appetiser, sugar. Open / late."
  +26 waita S8 12 150
S8:
  +0 caption "VAPULA: {d}The feast is on / the OTHER side."
  +26 waita END 12 150
END:
  +0 end
end

script e27_hatch
  say haze NEUTRAL "(looks up, for the first / time) It's unlocked. It / was always unlocked."
  choice nar "(The hatch. Purple light. It / smells of grape and / sulfur.)" { "Go down! (Hell Breach)" -> down ; "Not yet" -> no }
no:
  end
down:
  push combat MAPID_VAPE_CELLAR captions=VEX_PA1,VEX_PA2
  jlt v0 1 lost
  reward 100 ITEM_SCRAP 3
  say nar "(You smash the portal crate. / The vape wraith pops like a / soap bubble.)"
  [R13]
  say nar "(Vex is gone. Only a note, in / curly purple writing:)"
  say vex HAPPY "Too late, Hillvile. The big / cloud is already rising. / Kisses! - V"
  [R13]
  [R8]
  say nar "(Smaller, underneath: / get your pet indoors)"
  [R21]
  say nar "(Outside. Daylight. Birds.)"
  [R21]
  runbeat b27_birds
  [SILENCE]
  [HOLD 26]
  push cutscene CUTV_GATE_CRACKS
  push cutscene CUT_ERUPTION
  shake SHK_POW
  sfx SFX_DOOR
  say nar "(You sprint home. The door / slams behind you.)"
  say nar "(@E hides under the bed. / Red light in the window.)"
  [R13]
  sfx SFX_BEEP
  say nar "beep."
  native CB_C12 C12A_EP_DONE BEAT_EP2_7
  beat C3_ERUPTION
  end
lost:
  say nar "(You stumble back up the / stairs, coughing purple. / Catch your breath.)"
end

script e27_hatch_locked
  say nar "(A heavy hatch. A padlock / shaped like a little skull. / Locked. For now.)"
end
