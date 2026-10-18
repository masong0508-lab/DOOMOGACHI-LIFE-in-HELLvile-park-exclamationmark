# ch2/f1.dsc - Flashback F1 "The Fountain Gift" (owner: S2; story_final.md
# sec.11, sec.1). 40 seconds in a sepia palette lerp, triggered by the
# first weld in Ep 2.6. CUT_F1, drawn by core/src/cutscene_ch12.c: shot 0
# the forge, 1 the fountain (two laughing faces), 2 the bow, 3 now.
@pack f1 kind=episode chapter=2 minutes=1

beatscript bv_f1
  +0 pal PAL_SEPIA 16 13 ; music SONG_MINGLE ; shot 0 ; caption "(Twelve years ago. A forge / in Hellvile. A little imp / holds the torch.)"
  +13 sfx SFX_WELD
  +21 sfx SFX_SPARKLE
  +26 waita S1 12 160
S1:
  +0 shot 1 ; caption "ANVIL: Two faces, laughing. / Someone laughs here, it / bubbles up over THERE."
  +8 sfx SFX_WELD
  +21 sfx SFX_BUBBLE
  +26 waita S2 12 180
S2:
  +0 shot 2 ; sfx SFX_SPARKLE ; caption "ANVIL: A bow on top. There. / For the Mingle."
  +26 waita S3 12 150
S3:
  +0 label L_PUNCH
L_PUNCH:
  +0 pal PAL_SEPIA 0 0 ; shot 3 ; sfx SFX_THUD ; mute 26 ; caption "(Now.)" ; nobutton
  +26 waita END 12 60
END:
  +0 end
end
