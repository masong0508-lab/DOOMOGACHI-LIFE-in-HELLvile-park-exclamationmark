# ST1 - real analog stick support (PS1 DualShock + Dreamcast)

Owner ask (2026-11-08): "add actual dualshock/stick support so I don't have to move
the camera controls with wacky controlling". Goal: with an analog pad, every camera
and movement in the game works off the sticks with no setup and no ANALOG button.

## What exists today (checked 2026-11-08, tree d875604)
- ps1/src/pad.c reads PAD_ID_ANALOG / ANALOG_STICK sticks via the BIOS pad buffer, but
  never switches a DualShock into analog mode: the player must press ANALOG. No L3/R3.
- core/src/input.c: input_set_analog / input_analog / input_stick_mask; left stick is
  mirrored into D-pad bits for menus.
- Only core/src/combat.c uses the sticks, and only when Options > Controls = "Dual stick"
  (default is Modern: turning on R1/R2 - the "wacky" part). combat_start toasts
  "Dual stick needs an analog pad - press ANALOG".
- Dreamcast dc/src/pad.c + padmap.c pass the single stick through input_set_analog; the
  DC pad has one stick + analog L/R triggers, so "dual stick" does not fit it.
- Camera-on-buttons elsewhere: home_draw.c view rotate (L/R, 90-degree steps), creator.c
  preview turn (LEFT/L, RIGHT/R repeat), overworld.c (D-pad walking, 8-dir only),
  fish/aim scenes, room/house cameras. Audit every scene with a camera or free movement.

## Work
1. PS1 auto analog: on boot and on pad hot-plug, send the DualShock config sequence
   (enter config 0x43, set analog mode 0x44 with lock, exit) so the stick works without
   pressing ANALOG; keep digital pads and Dual Analog working; re-apply after a pad swap.
   Map L3/R3 (BTN_L3/BTN_R3 if free bits exist; else a documented choice). Optional:
   none of this may cost resident RAM beyond the 72 KB gate (put it -Os / cold).
2. Controls default "Auto": new default scheme (or equivalent) = Dual stick when an
   analog pad is present, Modern otherwise, switching live when the pad changes; the
   player's explicit pick still wins. Old saves keep their stored value unless it is the
   old default Modern, which migrates to Auto (note this in docs).
3. Combat/levels: PS1 dual stick as today (left move/strafe, right turn/look, ease on
   flicks, invert + look speed honoured). Dreamcast: stick X turns, stick Y walks,
   analog triggers strafe (pressure = speed), D-pad strafes/walks too; an Options row
   "Stick: turn / strafe" for DC. Right stick click (R3) = lock-on/center view where free.
4. Camera everywhere else, with an analog pad:
   - house/home: right stick (DC: triggers) rotates the view; smooth if the renderer
     allows it, otherwise a flick = one 90-degree step with repeat on hold;
   - creator try-on/preview: right stick spins the model smoothly, stick Y tilts/zooms
     if the preview supports it; tab switching stays on L1/R1;
   - overworld / island / any free-walking map: left stick walks with analog speed
     (slow creep at small deflection, run at full), any direction the map allows;
   - every other camera found in the audit (fishing cast aim, minigames with a cursor,
     3D level select, exorcism if it pans): sticks drive it where it reads naturally.
   D-pad/buttons keep working everywhere (GBA and digital pads untouched).
5. Clarity: on-screen prompts and control cards show stick glyphs when an analog pad is
   present ("L-stick Move", "R-stick Camera"); drop the "press ANALOG" toast (auto mode
   makes it moot; if the pad is digital-only, say "Analog sticks: use a DualShock").
6. Tests: core unit tests for the scheme resolution, dead zones/curves, the DC trigger
   strafe and each new stick consumer; PS1 harness and dcrun runs with injected stick
   values (extend the harnesses if they cannot inject analog yet) + screenshots of the
   house rotate, creator spin, overworld analog walk and combat turn.
7. Gates: core test, SAN, check-gen, xcheck, GBA build+tests unchanged, PS1 build with
   FMV linked >= 72 KB below stack top, every touched scene still 30 fps on PS1 (and DC
   where measurable); docs (docs/PS1.md controls, DREAMCAST.md, CONTROLS or README),
   ROADMAP. Land with the fast-land pattern (build -> review -> land), one package.
