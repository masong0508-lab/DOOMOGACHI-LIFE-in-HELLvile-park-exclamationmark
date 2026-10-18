# ch2/ep21.dsc - Ep 2.1 "Grand Opening" (owner: S2; story_final.md sec.11).
# Legacy beat C2_VAPOR. "A few days pass" is played: three mornings, a
# smash cut each; then a shop has appeared by the Wall. Vex's pitch is a
# performed routine; the choice (decline / one puff / "Where are you
# from?") is remembered (SF_VAPE_SAMPLE, STRY.branch).
@pack ep21 kind=episode chapter=2 minutes=21

# three mornings, a smash cut at 21 frames each
beatscript b21_mornings
  +0 sfx SFX_THUD ; flash PAL_WHITE 12 3 ; shake SHK_TAP
  +21 sfx SFX_THUD ; flash PAL_WHITE 12 3 ; shake SHK_TAP
  +42 sfx SFX_THUD ; flash PAL_WHITE 12 3 ; shake SHK_TAP
  +55 end
end

script e21_mornings
  [DAY 5]
  say nar "(A few days pass.)"
  runbeat b21_mornings
  [DAY 7]
  say nar "(Three mornings. Each one, / @E's nubs are a tiny bit / longer.)"
  say nar "(Each one, the purple glow / at the Wall is a little / closer.)"
  [R13]
  say nar "(And this morning there is a / new shop by the Wall. It was / not there yesterday.)"
  [R13]
  sfx SFX_BLIP
  say nana NEUTRAL "(a text) Saw @E at your / window. Little nubs! / Ah. {..}He's teething."
  jifn SF_NGPLUS done
  say nar "(You have seen this shop / before. Or you will.)"
done:
end

script e21_ribbon
  say ribbonetta HAPPY "Grand opening! I cut ribbons / professionally. Stand back!"
  sfx SFX_TINK
  say nar "(Snip.)"
  [R8]
  sfx SFX_SPARKLE
  say ribbonetta HAPPY "Achoo!"
  [R13]
  say nar "(The sneeze is purple.)"
  [R8]
  [R21]
  say nar "(In the window, framed: a / crowned purple cloud. / EMPLOYEE OF THE MONTH.)"
  [R8]
  say nar "(Its eyes follow you.)"
  [R13]
end

script e21_sprinkle
  say sprinklepop HAPPY "First in line! Grape? I LOVE / grape. I love everything!"
  sfx SFX_SPARKLE
  say nar "(She sneezes. Glitter. / The doctors say it's fine.)"
  [R13]
end

script e21_haze
  say haze NEUTRAL "(waters a plant, does not / look up)"
end

# the free sample: the inhale (a 10-frame stretch, a 5-frame hold), the
# FWOOMP, two red eyes in the fog, and you are fine
beatscript b21_puff
  +0 sfx SFX_WHOOSH ; zoom 280 10
  +13 label L_PUNCH
L_PUNCH:
  +0 shake SHK_FWOOMP ; flash PAL_UI_PURPLE 16 22 ; sfx SFX_POSSESS
  +13 sfx SFX_TIK
  +21 caption "{d}Everything is fine."
  +24 label L_BUTTON
L_BUTTON:
  +0 sfx SFX_POP2
  +13 end
end

script e21_vex
  npc vex HOP
  say vex HAPPY "WELCOME to Cloud Nine! Where / every breath is a hug from / the inside!"
  [R3]
  npc vex HOP
  say vex HAPPY "Grape! Cherry! Sunset / Serenity! Mystery Mauve!"
  [R8]
  say vex NEUTRAL "{d}...Mystery Mauve is grape."
  npc vex HOP
  say vex HAPPY "First puff's free. Second / puff's free. All the puffs / are free."
  [R13]
  [R5]
  say vex HAPPY "That's the business model."
  npc vex STILL
  [R8]
  say haze NEUTRAL "(not looking up) The plants / hate it too."
  npc vex THAW
  choice vex "So! One little puff?" { "No thanks." -> no ; "...Just one puff." -> puff ; "Where are you from?" -> where }
no:
  npc vex STILL
  [MUTE 21]
  [HOLD 21]
  say vex NEUTRAL "...Suit yourself."
  [R13]
  npc vex THAW
  npc haze EMOTE EMO_HEART
  say nar "(Behind the counter, the / clerk gives you a tiny / thumbs-up.)"
  jmp out
puff:
  setf SF_VAPE_SAMPLE
  give ITEM_VAPE_PEN
  native CB_C12 C12A_PUFF
  runbeat b21_puff
  say vex HAPPY "There it is. The Cloud Nine / feeling. We'll be seeing / you, @P."
  say nar "(Somewhere, the fountain / giggles. A little louder / than before.)"
  jmp out
where:
  npc vex FREEZE
  [MUTE 13]
  [HOLD 13]
  say vex NEUTRAL "...WAY out of town."
  npc vex THAW
  [R21]
  sfx SFX_TIK
  say nar "(In the window, the crowned / cloud in the frame blinks.)"
out:
  befriend vex 1
  say nar "(People leave the shop / humming. All the same / note.)"
  beat C2_WEIRD
end

script e21_vex_closed
  say nar "(BACK IN 5 MINUTES - VEX. / The sign has said that for / an hour.)"
end
