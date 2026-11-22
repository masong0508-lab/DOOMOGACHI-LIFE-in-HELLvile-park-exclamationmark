# PS2 Deluxe — PCSX2 validation walkthrough (PS2-5)

This is a **real-runtime** gate, not a successful-ELF-link or emulator-
process-alive gate. A passing GitHub build is required first. The
`doomogachi_ps2_game.iso` artifact contains `SYSTEM.CNF`, the shared
game ELF, the generated `TALK_EN.BIN`, and the audsrv and FreeSD
modules from PS2SDK. Play! smoke tests cover title and Story controller
input using that ISO. Local Play! tests have also exercised Talk, SPU2
output, and Free Play save recovery. The PCSX2 and console acceptance
checks below track the remaining release validation.

## Launch prerequisites

- Use the current official PCSX2 release, configured with a **BIOS
  dumped from your own PS2**. No BIOS is bundled or uploaded by CI.
- Use a fresh, formatted PS2 memory card image in port 1. Leave a
  second test card unplugged to exercise the missing-card path.
- Start the ISO: `pcsx2-qt -batch -- ps2/build/doomogachi_ps2_game.iso`.
- Optional Linux startup probe: `sh ps2/tools/pcsx2_boot_smoke.sh
  ps2/build/doomogachi_ps2_game.iso`. Its exit status never proves
  rendering, audio, controller control or save reliability.
- Capture a native-resolution frame (320x240 logical output, scaled to
  640x448 NTSC interlaced) plus an FPS/timing capture for each scenario.

## Acceptance walkthrough (not performed by the build)

1. **Boot** — Disc boots directly into the real title, not the PS2-0
   hardware diagnostic. Cross/Start progress into Free Play; the game
   does not crash or display an infinite loading frame.
2. **Clock & controller** — At 60 GS fields per second, simulation
   advances at **30 logic ticks**, not 60; hold / release left stick,
   D-pad and diagonals; disconnect/reconnect DualShock 2; no movement
   persists after disconnection.
3. **Roster & portraits** — Scroll rapidly through more than 50 citizens;
   selected portrait and current name match. Switching district/search
   must not flash a previous citizen's face; record worst-case latency.
4. **3D maps and rooms** — Enter the city map, turn through four views,
   enter a furnished house, orbit and move. Walls and roofs occlude
   correctly, bodies stand on floors, nothing phases through geometry.
5. **Talk** — From a living resident's house choose Talk. A greeting,
   quick reply, `Ask about their day`, an available concern and a
   history row must work. If TALK_EN.BIN is absent the action is hidden
   rather than presenting phantom dialogue.
6. **SPU2** — Confirm cursor/select SFX, music, procedural speech tones
   and multiple overlapping sounds; no persistent note after leaving a
   scene. Record silence, crackle or lost audio during long card writes.
7. **Save/reload** — Set an unmistakable resident/player change, save
   Free Play to card, exit emulator completely, relaunch ISO, reload,
   verify same state. Repeat in Story slot. Missing or unformatted card
   must show failure without pretending to save.
8. **Card recovery** — Back up the card file first. In a *copy* of the
   test card, corrupt only the newer journal file and verify the previous
   valid save loads. Pull the card mid-write using emulator controls,
   restore it and verify the surviving bank; never alter someone's real
   save without a backup.
9. **Stress** — Play at least 20 minutes, switching scenes, Talk, saves,
   portraits, 3D view and sound. Record average/worst frame time, resident
   count and memory; compare with the previous 32-bit GS texture build.

## Release criteria

Keep PR #6 draft until the shared-game CI job, host regression tests,
ISO asset manifest, and a documented PCSX2 run pass. Hardware
verification still requires a homebrew-enabled PS2. The PCSX2 run needs
an emulator/BIOS; this repository does not include either or claim the
above manual playthrough was completed.

Known constraints: no GS-native 3D polygons yet; the shared reference
renderers still rasterize into one software indexed surface. PSG/PCM
audio uses a simple three-effect mixer and SPU2 22050 Hz stream, not
the Dreamcast full stereo mixer. The new 16-bit texture needs visual
colour/alpha tests on an actual GS/PCSX2 render target.
