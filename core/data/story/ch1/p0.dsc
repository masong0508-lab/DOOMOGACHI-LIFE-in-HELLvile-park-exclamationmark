# ch1/p0.dsc - PROLOGUE P0 "The Egg Crossing" (owner: S2; story_final.md
# sec.9). A crayon cutscene before the first day: plant the Mingle, the
# cloud on a string, Nana's boats and the egg, so Ep 4.6 can land. No
# portraits; captions in a child's hand. CUT_P0, drawn by
# core/src/cutscene_ch12.c (shot k = the panel; labels carry the beats).
@pack p0 kind=episode chapter=1 minutes=2

beatscript p0_egg
  +0 music SONG_MINGLE ; pal PAL_CRAYON 16 0 ; shot 0 ; iris_in 0 0
  +2 caption "Every year, the park had / a potluck."
  +8 sfx SFX_SPARKLE
  +13 waita S1 12 150
S1:
  +0 shot 1 ; caption "Everybody came."
  +3 sfx SFX_HAPPY
  +8 label RUN
  +21 sfx SFX_BOING
  +26 waita S2 12 150
S2:
  +0 shot 2 ; caption "Fireworks! The cloud looked / up. It loved fireworks."
  +5 sfx SFX_EXPLODE
  +13 sfx SFX_SPARKLE
  +21 sfx SFX_EXPLODE
  +26 label SLIP
SLIP:
  +8 label L_PUNCH ; sfx SFX_PLUCK
  +21 label L_BUTTON
L_BUTTON:
  +0 hold LET 13 ; caption "The string slipped. / Nobody noticed."
LET:
  +0 waita S3 12 120
S3:
  +0 shot 3 ; caption "Not everybody went home." ; mute 29
  +8 sfx SFX_WHOOSH
  +29 waita S4 12 120
S4:
  +0 shot 4 ; sfx SFX_THUD ; shake SHK_RUMBLE ; caption "{d}Nobody talks about why."
  +13 sfx SFX_CREAK
  +26 waita S5 12 150
S5:
  +0 shot 5 ; caption "Except Nana. Nana sends / eggs." ; sfx SFX_SPLASH
  +26 label BOAT
BOAT:
  +0 sfx SFX_SPLASH ; caption "A boat comes back the other / way: burnt cookies."
  +21 sfx SFX_EAT
  +26 sfx SFX_LOVE
  +34 waita S6 12 150
S6:
  +0 shot 6 ; iris_out 0 85 ; sfx SFX_SHWIP
  +21 sfx SFX_EGG_CRACK
  +34 label TITLE
TITLE:
  +0 shot 7 ; sfx SFX_FANFARE ; caption "DOOMOGACHI LIFE / in HELLvile Park!"
  +13 waita END 12 120
END:
  +0 pal PAL_CRAYON 0 8
  +8 end
end
