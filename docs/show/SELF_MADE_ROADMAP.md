# DOOMOGACHI LIFE in HELLvile Park!: The New-Year Roadmap

**Four finished episodes by New Year's Eve, made by Claude on the game's own PS1 engine, directed by you in about an hour a week.**

| | |
|---|---|
| Version | **2.0, 2026-10-25.** Week 1 starts **Monday 2026-10-26**. The last day is **Thursday 2026-12-31** |
| Your milestone (your words) | "By the beginning of the new year, I will have some episodes." And: "I do want to make this show without thinking I'll be spending weeks and weeks when you can do it." |
| The target | **101 "Mingle Night" (the pilot), 102 "Open Late", 103 "Three Neighbours" and 104 "Fluffy Cupcakes"**, each about 22 minutes, released online. **Committed: 101-103. Target: 101-104. Floor if everything goes wrong: 101 + 102** (§6) |
| Release | Personal and online only: YouTube (marked not made for kids, age-restricted 18+) and anywhere else you like. No network pitching, no studio deliverables (§7) |
| Who does what | **Claude does the heavy lifting**: writing, staging, rendering, voices, music, mixing, editing and packaging. **You direct and approve**: a handful of decisions a week, measured in minutes (§1) |
| Engine | `SHOW_ENGINE_SPEC.md` v1.1 (same folder). Milestones M0-M4 and work packages WP0-WP17 there match the dates here |
| Content | The docs in `docs/show/` stay the creative source. Claude reads them and never edits them. Another workflow is re-aiming them to TV-MA, so Claude re-reads them at the start of every episode |
| Replaces | v1.0 of this file (one pilot by June 2027, 15-20 hours a week of your time). Its schedule, the broadcast deliverables and the 30-day request list are retired. Appendix A lists what was cut and when it can come back |

---

## 1. The deal: what you do, what Claude does

**You** make decisions that only you can make, and you watch the episodes. **Claude** does everything else, and comes to you only at the review points in §4.

| You (minutes) | Claude (everything else) |
|---|---|
| Approve each episode's one-page outline | Writes the outline, the beat sheet, the script with line ids, the dialogue and every on-screen word, all inside the canon and the four hard limits |
| Pick voices once from a numbered audition list (optional) | Casts every role from the Kokoro voices, renders and locks every line, writes mouth tracks |
| Watch each episode once and give notes in plain words ("the drip is too fast in the bandstand bit") | Stages every shot as `.shw` text, renders every frame on the PS1 build, scores, mixes, captions, fixes notes, re-renders |
| Tick the standards checklist (about 5 minutes, §5) | Runs every lint, writes the standards report with pictures of every flagged scene |
| Upload each finished episode to YouTube (about 10 minutes, §7) | Packages the upload: MP4, captions, title, description, chapters, thumbnail, credits |

**Your time, week by week, is in the "You" column of §4.** It adds up to about **8-10 hours over the whole ten weeks**, most of it watching episodes. Everything you do not decide has a default (§8), so nothing waits for you.

**Optional, never required:** recording your own voice for a role (a phone in a quiet closet is fine), drawing hand art, reading the full scripts. If you do any of these, the episode gets better, not later.

---

## 2. What makes four episodes in ten weeks possible

- **Every frame is drawn by the game's own engine.** The busts, figures, chibis, expressions, the overworld maps, the 2.5D house, the first-person 3D, the camera effects and the fonts already exist and already run on the PS1. The show adds a player that scripts them. Measured today: a stand-alone show executable linked from the game's own code boots in the emulator and draws the portrait engine's images, with **no change to the game** (spec §15.1).
- **Acting is text.** A performance is a few lines Claude writes: `expr`, `say`, `blink`, `freeze`, `pop`, `walk`. A note becomes a one-line diff, not a redraw.
- **Rendering is fast.** An episode's picture takes about **4-6 minutes** in the emulator, its HD encode about **15-20 minutes**, its voices about **20 minutes** and its score a few minutes (spec §15). Rendering is never the bottleneck.
- **The episodes after the pilot reuse it.** 102-104 are "Medium" episodes (shape A) on sets, looks, voices, gags and music cues the pilot builds. Each one needs no new engine feature.
- **Scope is cut hard.** Anything that does not put a finished episode on YouTube by New Year waits for 2027 (Appendix A). That includes the broadcast versions, 5.1 sound, audio description, ProRes, the wide canvas, the disc edition and the native debug build.

**The honest limit is Claude's working time, not the computer's.** The plan needs about **80 "Claude-days"** of focused work in **67 calendar days** (engine about 36, the pilot's content about 12, each later episode about 8-10). That means Claude works most days, in long sessions or parallel workflows, with a peak in week 2. If your plan's usage limits cut sessions short, the checkpoints in §6 drop scope in a fixed order instead of letting dates slide. Heavy daily use may push you toward a higher usage tier for these ten weeks; check current plan prices.

---

## 3. Milestones

| Milestone | Gate date | What exists at the gate | Spec |
|---|---|---|---|
| **M0 First light** | Sun **Nov 1** | The tools are vaulted. `show.exe` builds without touching the game. The pilot's 22:00 placeholder animatic is rendered by the PS1 build | §17 M0 |
| **M1 First scenes** | Sun **Nov 8** | Cold open shots 1-3 as an MP4 with music, the kids' walla and captions, plus a **dialogue proof**: the Mayor and Nana talking, with lip sync, in a two-shot and close-ups | §17 M1 |
| **M2 Cold open** | Sun **Nov 15** | The full 1:40 cold open, voiced and scored. The pilot's script is locked | §17 M2 |
| **M3 Pilot released** | Sat **Dec 5** | 101 "Mingle Night" finished, checked and uploaded | §17 M3 |
| **M4 The season loop** | **Dec 12 / Dec 19 / Dec 31** | 102 by Dec 12, 103 by Dec 19, 104 by Dec 31, one week each on the per-episode loop (§5) | §17 M4 |

---

## 4. Week by week (Mon Oct 26 to Thu Dec 31)

Each row says what Claude builds and renders that week, the few things you decide, and roughly how many minutes they take. "Review" means Claude sends you a file and you reply with notes or "fine".

| Week | Dates | Claude builds and renders | You decide or watch | Your time | Exit |
|---|---|---|---|---|---|
| **W1** | Oct 26-Nov 1 | **M0.** Day 1: copies the docs into the repo and **vaults the tools** (the emulator, SDK, voices and audio kit exist only in this container today). Then: the zero-touch `show.exe` build and the game-unchanged gate (WP1), the player skeleton and slug cards (WP2), frame capture (WP3), the compiler's rundown pass (WP4 v0), the pilot's 22:00 slug animatic rendered on the PS1, speed and RAM numbers, the 8 MB hatch test | Say yes to the private vault repo. Glance at 2 minutes of the slug animatic and the canvas sample (320x180 at 1080p). Optional: listen to the voice audition list and pick by number | 15-30 min | **M0** |
| **W2** | Nov 2-8 | **M1, the peak week.** The full compiler (WP4), 2D stages with crayon strokes, grain and parallax (WP5), busts, figures and chibis with expression, mouth and blink tracks (WP6), SUPERs (WP7), irises and fades (WP8), TTS voices with mouth tracks (WP9), MINGLE and the SFX (WP10), the mix (WP11), delivery (WP12), contact sheets and timing text (WP13), the hard-limit lints (WP17). Renders co01-co03 and the dialogue proof. In parallel, **drafts the pilot's full script** from the beat sheet | Watch the 13-second M1 clip and the 20-second dialogue proof. Pick the crayon weight and the Mayor's look from numbered options (or accept the defaults) | 15-20 min | **M1** |
| **W3** | Nov 9-15 | **M2.** Cold open shots 4-21: the bandstand set (tilemap), the Mayor's ladder, roller and balloon poses, the fountain insert, the banner bloom, `base=`/`replay=` shots, the Wall, fireworks with safe luminance ramps, the plaza crowd, Mascotty, the heart iris, the hum. **Locks the pilot script** and renders a TTS table read | Read the pilot outline (1 page) or listen to the table read at 1.5x. Approve the script ("lock it") or name what to change. Watch the 1:40 cold open | 30-45 min | **M2** |
| **W4** | Nov 16-22 | **M3, part 1.** New set kinds on the game's existing data: the overworld wides on the game's own maps, the Pink Door Cottage on the house renderer (WP15). The main title on the theme's TV size, the eyecatch. Stages, voices and scores **Act One** | Watch Act One (about 5 min) and give notes | 15 min | Act One preview |
| **W5** | Nov 23-29 | **M3, part 2.** First-person runs on the game's existing 3D maps with the HUD and the filter-to-truth switch. Stages **Acts Two and Three**. Fixes the Act One notes | Watch Acts Two and Three (about 9 min). Thanksgiving week: notes can wait until the weekend | 15-20 min | Acts 2-3 preview |
| **W6** | Nov 30-Dec 6 | **M3, part 3.** **Act Four, the tag and the credits**, the full mix, captions, the standards report and the release package. Uploads nothing itself; hands you the files. In parallel: **102's outline, script and slug animatic** (WP16 templates) | **Watch the whole pilot** (22 min) and give notes. Tick the standards checklist. Approve 102's outline. **Upload 101** (Sat Dec 5) | 60-75 min | **M3: 101 released** |
| **W7** | Dec 7-13 | **102 "Open Late"** on the per-episode loop (§5): Grumbleguts' debut, the series' first TV-MA (L, S, V) episode, so the consent and kids lints carry the most weight here. Also drafts 103's outline | Watch 102, notes, checklist, upload (Sat Dec 12). Approve 103's outline | 45-60 min | **102 released** |
| **W8** | Dec 14-20 | **103 "Three Neighbours"** on the loop. Drafts 104's outline | Watch 103, notes, checklist, upload (Sat Dec 19). Approve 104's outline | 45-60 min | **103 released** |
| **W9** | Dec 21-27 | **104 "Fluffy Cupcakes"** (the Arc 1 finale) on the loop. Holiday week: Claude holds every file for whenever you are free | If you can: watch the 104 cut and give notes | 0-30 min | 104 cut |
| **W10** | Dec 28-31 | Finishes 104 with your notes. Builds the "season so far" playlist package (titles, descriptions, an optional 60-second trailer rendered on the engine). Writes the 2027 plan | Watch, tick, **upload 104** (by Thu Dec 31) | 30-45 min | **104 released** |

**Total of your time: about 8-10 hours over ten weeks.** Watching the four episodes is about 1.5 hours of it.

---

## 5. The per-episode loop (one week each, from W7)

| Day | Claude | You |
|---|---|---|
| **Mon** | Re-reads `docs/show/` for the episode (they may have changed). Writes the one-page outline, the beat sheet, the rundown and the full script with stable line ids, every line tagged for standards (§5.1). Renders the TTS table read and the slug animatic on the PS1 | Approve the outline (5 min). The script itself is optional reading |
| **Tue-Thu** | Stages the four acts from the series libraries (sets, looks, acting macros, crowds, music cues), renders, lints and fixes its own problems first. Sends an Act One preview on Wednesday | Optional: watch the Wednesday preview (5 min) |
| **Fri** | Assembles the full cut with score and mix. Runs every lint and writes the standards report | **Watch the cut (22 min) and send notes** (5 min) |
| **Sat** | Applies the notes, re-renders the changed shots (seconds each), re-mixes, delivers, and prepares the upload package | **Tick the checklist (5 min) and upload (10 min)** |
| **Sun** | Starts the next outline. Purges caches (disk is tight, §9) | — |

### 5.1 The standards checklist (the hard limits, every episode)

The four hard limits in `docs/show/SERIES_OVERVIEW.md` §2 bind every frame, line, title, thumbnail and promo. They are enforced twice: by **lints that block delivery** (spec §13.5) and by **your tick-list** (spec §13.7), because a lint cannot judge a joke.

| Hard limit | What the lints enforce | What you check in the standards report |
|---|---|---|
| **1. Nothing explicit.** No sexual acts on screen, no genitals, ever | `STD-EXPLICIT`: no shot, line or asset may carry `explicit`; sexual content may only be `implied`, `offscreen` or `innuendo`; anatomy-flagged art is refused; the sticker gag at most once a season | Every frame range tagged `implied` or `innuendo`, as a contact sheet |
| **2. Adults, and consent.** Everything sexual or suggestive is between adults and consensual; a no ends it; silence is never a yes; consent is never the punchline | `STD-CONSENT`: every `advance` names its adult target; any touch needs a prior spoken or scripted `yes` line from the target in the same scene; after a `no`, the next line from the advancer is a `stepback` and nothing suggestive toward that character follows in the scene | Each advance in the episode, with its yes or no line, in a two-column list |
| **3. Children are never in it.** No child or teen in a sexual or suggestive scene or line; no graphic gore with a child in frame (the filter stays on) | `STD-AGE`: every cast member and crowd has an `age`; `STD-KIDS` works per **scene** (one continuous time and place), not per shot: a scene with any KID or TEEN character, crowd member or voice carries no innuendo, suggestive line, `advance`, truth insert or BRUTAL gore; no suggestive line refers to a child; Grumbleguts' rear stays out of frame with a child in the scene | Every scene with a child in it, with the reason it is clean |
| **4. No real-world hate.** No slurs, no real hate symbols, no joke aimed at a real person, group or faith | `STD-HATE`: a word list for slurs and a list of real names Claude must not target | The episode's jokes about groups or beliefs, if any |

Plus the release items: the TV-MA card at the head of the episode, the YouTube settings in §7, captions present, the flash and loudness checks green, and the credits for the CC BY samples. **`show deliver --release` refuses to package an episode until your signed checklist matches the exact render** (spec WP17).

---

## 6. Checkpoints: what gets cut if the dates slip

The plan protects the episode count, then the dates, then the polish, in that order. Claude reports each checkpoint to you in two lines.

| Checkpoint | If this is true | Then (in this order) |
|---|---|---|
| **CP1: Wed Nov 11** | M1 is not done | The cold open's storybook drops to flat crayon plates (no stroke reveal or boil), and the child's-hand font becomes the game font. **104 moves to January** |
| **CP2: Wed Nov 18** | The cold open is not done | Shots 4-11 ship at board level (a still per beat with camera moves). The pilot's first-person runs are cut to one |
| **CP3: Wed Dec 9** | 101 is not released | **103 and 104 move to January.** 102 is still made in W8-W9 |
| **CP4: Sun Dec 20** | 103 is not released | 103 finishes in W9-W10, and 104 is not started |
| **Any week** | Your notes average more than three rounds per scene | Claude builds an acting macro for the recurring note before staging more shots |
| **Any week** | Claude's sessions are being cut short by usage limits | Claude stops engine polish first, then the optional trailer, then reduces each episode's new sets (re-using more) |

**The floor: 101 and 102 released by Dec 31.** That is "some episodes" in the worst case.

---

## 7. Releasing online

**Where.** YouTube is the default. Vimeo, your own site or itch.io work the same way with the same files.

**How the files reach YouTube.** This container can reach GitHub, PyPI and npm, not your YouTube account. Claude puts each finished episode in a private place you can download from (a release on the vault repo, or the session itself) and gives you a ready-to-paste upload sheet. You upload it in YouTube Studio in about 10 minutes.

**The upload sheet Claude prepares** (WP17): the 1080p30 H.264 MP4 at −16 LUFS; English captions (`.srt` and `.vtt`); title; description with the credits and licence lines; chapters (eyecatches); tags; a thumbnail rendered on the engine; and the settings below.

**Settings to choose on every upload:**
- **Audience: "No, it's not made for kids."** This is required by law (COPPA) and by YouTube for adult content. The show looks like a cute cartoon, so YouTube's classifier may guess wrong: check the setting after upload.
- **Age restriction: "Yes, restrict my video to viewers over 18."** The docs require it (`SERIES_OVERVIEW.md`, Online). Age-restricted videos get limited or no ads, which is fine for a personal release.
- **Keep thumbnails, titles and descriptions plainly adult.** YouTube removes, and can strike, cartoon-styled videos that look made for children but contain adult themes ("misleading family content" under its child-safety policy). No kid-appeal thumbnails, no kids' keywords, and the TV-MA card stays in the first seconds.
- **Altered or synthetic content.** YouTube asks for a label when realistic content could be mistaken for real people or events. An animated show with stock synthetic voices is generally not that case, but the description says plainly "Voices: synthetic (Kokoro TTS) and [your name]". Check YouTube Studio's current wording at upload.
- **Credits** in the description and the end credits: "Salamander Grand Piano V3 by Alexander Holm, CC BY 3.0" and "DWSD / freesound.org, CC BY" if their samples are in the episode (the mix log says so). Everything else in the audio kit is CC0, MIT, ISC or free-use (spec §10.8).
- **Content ID.** Royalty-free and CC0 samples are sometimes claimed by mistake on YouTube. Claude keeps the licence and provenance log for every sound, so you can dispute a false claim with one paste.

---

## 8. Decisions only you can make (each has a default, so nothing waits)

| Decision | Default if you say nothing | When |
|---|---|---|
| The vault repo's name and privacy | `doomo-vault`, private | W1 Mon |
| Canvas | 320x180 inside the PS1's real 320x240 mode. The wider 384x216 option is deferred to 2027 | W1 (a 1-minute look) |
| Voices | The casting table in spec §10.3, all TTS | W1-W2, changeable any time per line |
| Look picks: crayon weight, the Mayor's hair and paint, the kids' look | Claude's option 1 of 3 | W2-W3 |
| Your own voice for any role | None (all TTS) | Any time; a take swaps in by line id |
| Each episode's outline | Claude's outline as drafted from the docs | Monday of its week |
| Notes on each cut | "Fine" | Friday or Saturday of its week |
| The standards checklist | **No default: an episode cannot be released without your tick** | Saturday of its week |
| Release day | Saturday of each episode's week | Any time |

---

## 9. Costs and disk

| Item | Needed? | Cost |
|---|---|---|
| Claude Code (this interface) | Yes | Your existing plan. The ten-week peak may need a higher usage tier; check current prices. This is the only real cost |
| Compute (this container) | Yes | $0 |
| The vault (a private GitHub repo) | Yes | $0 |
| Software: PCSX-Redux (GPL), PSn00bSDK (MPL-2.0), Kokoro-82M (Apache-2.0), Whisper (MIT), ffmpeg, Python libraries | Yes | $0. None of them places terms on the rendered episodes |
| Audio samples (CC0, CC BY 3.0, MIT, ISC, free-use) | Yes | $0, with the two CC BY credits in §7 |
| A microphone | Optional | $0 (a phone) to about $50-150 (rough guess) |
| External drive, real PS1, legal, insurance | **Not needed** for a personal online release | $0 |
| **Total required** | | **$0 beyond your Claude plan** |

**Disk.** The container had as little as 4.9 GB free today (6.8 GB earlier in the month; other sessions share the disk, so it moves by gigabytes). One episode's working set is about 2-2.5 GB (lossless picture masters about 0.6 GB, audio stems about 1-1.9 GB, caches). So Claude keeps **one episode in progress at a time**: after each release it moves that episode's masters and stems into the vault (or deletes them, because every frame and sample re-renders from the commit), and keeps only the final MP4 and captions. `show doctor` warns below 2 GB free.

---

## 10. Requests you can paste

Each week starts with one paste. After that, Claude works through the week, sends you files at the review points, and asks only the questions in §8. Your replies can be one line.

### W1, Mon Oct 26: keep everything safe, then first light

```
Read docs/show/SELF_MADE_ROADMAP.md and docs/show/SHOW_ENGINE_SPEC.md. The show lives in show/ on
the working branch (never edit core/, gba/, ps1/ or the creative docs in docs/show/).
Then do WP0 first, today: vault the emulator, BIOS, PSn00bSDK, the Kokoro and Whisper models and the
audio kit (spec §10.7, including the gh-authentication finding), and prove AT0.1-AT0.3.
Then work through the rest of W1 in the roadmap (M0). Send me the slug animatic sample and the
canvas sample when they exist, and tell me the M0 test results at the end of the week.
```

### W2, Mon Nov 2: M1

```
Start W2 of SELF_MADE_ROADMAP.md: M1 exactly as spec §17 M1 describes (cold open shots 1-3 plus the
Mayor-and-Nana dialogue proof). Draft the pilot script in parallel. Send me the M1 MP4 and the
dialogue proof with three numbered look options each for the crayon and the Mayor. Report CP1 on
Wednesday.
```

### W3, Mon Nov 9: the cold open and the pilot script

```
Start W3: M2 (the full cold open) and lock the pilot script. Send me the one-page outline and a
1.5x TTS table read first, then the cold open when it is done.
```

### W4-W6: the pilot

```
Start W<n> of SELF_MADE_ROADMAP.md. Send me each act preview as it is ready.
```

### Each episode, W7 onward

```
Start episode <102|103|104> on the per-episode loop (roadmap §5). Send me the outline today.
```

### Notes (any time)

```
Notes on <episode/act>: <your words, as many or as few as you like>.
```

### Release (each Saturday)

```
Standards checklist for <episode>: I've read the report. <ticked / here is what to change>.
Make the upload package.
```

---

## 11. Risks

| Risk | Sign | Response |
|---|---|---|
| The container is reset before the vault exists | A new session has no emulator | WP0 on Day 1, before anything else. The emulator can be rebuilt from its local source, the SDK reinstalled, the voices re-downloaded from npm; the vault just makes that minutes instead of a day |
| Claude's working time runs short | Sessions end mid-task, the week slips | The checkpoints in §6, in their fixed order. Front-load the engine in long sessions; reviews and notes are short |
| The pilot takes longer than three weeks | CP3 | 103 and 104 move to January; 102 still ships |
| The look reads as "stiff" | Your notes mention it twice | Acting macros from the gag bible, more close-ups at `px=2`, flipbooks for hero beats (spec R4) |
| YouTube removes or misclassifies an episode | A notice in YouTube Studio | The §7 settings on every upload; the TV-MA card; adult thumbnails and titles. Appeal with the standards report |
| A false Content ID claim | A claim notice | Dispute with the provenance log |
| The docs change under the plan (TV-MA re-aim) | A changed `docs/show/` file | Claude re-reads them at each episode's start; the hard-limit lints read the docs' cast list |
| The game workflows change shared code | The show build breaks | The show compiles copies of the two portrait files from a patch and never edits game files, so a game change can break only the show build, loudly, never the game (spec §4.3) |
| Disk fills | `show doctor` warns | One episode in progress at a time; masters to the vault after release (§9) |
| Review fatigue | Notes get shorter | That is expected and fine: every decision has a default |

---

## Appendix A. What was cut for New Year, and when it can come back

| Cut from 2026 | Why it waits | Earliest return |
|---|---|---|
| Broadcast versions: 29.97 conform, the 22:00;00 elastic trim, breaks, `r128`/`a85`/`dlg27` loudness | Nobody broadcasts a personal release; YouTube takes 30p at −14 to −16 LUFS | When a buyer asks |
| 5.1 mix, M&E, stems as deliverables | Stereo is what YouTube plays to almost everyone | 2027 |
| Audio description (AD) and TTML/SCC captions | YouTube captions take SRT/VTT; AD needs its own script and voice per episode | 2027 (AD is the first thing to add back) |
| ProRes masters, UHD (4K) web masters | No buyer; the FFV1 lossless master is the archive. UHD is a 66-minute encode per episode, optional on any quiet night | Any time (UHD), on request (ProRes) |
| 2x11, textless, 4:3 and 9:16 versions | Not needed to release | 2027 (9:16 Shorts first, for promotion) |
| The wide 384x216 canvas | It needs guarded changes in three PS1 game files | 2027, if you want it after seeing four episodes |
| The disc edition (real-time playback on a PS1) | Off the critical path | After the season |
| `runner-show` (the native debug build), the P2 draw-call trace, GP0 attribution | Debugging luxuries; P1 state hashes and frame hashes catch regressions | When a bug needs them |
| Eye-direction (gaze) control | The engine draws about 20 eye styles each with its own pupil, so gaze is a bigger change than one hunk (spec §4.3) | 2027 |
| The crowd atlas with recolour CLUTs | Up to about 48 distinct looks re-used across a crowd covers the pilot's plaza | 2027 |
| Git LFS for takes and hand art | The vault holds them; LFS adds a dependency for every clone of the game repo | When takes outgrow the vault |
| Pitching, legal, E&O insurance, a company | Not needed for a personal release | If you ever pitch |

---

## Appendix B. Critique fixes (2026-10-25)

This roadmap and spec v1.1 were reviewed against the real code, with measurements in `showengine/critique_nye/`. What changed in this file:

1. **Re-aimed at your New-Year milestone.** v1.0 delivered one pilot in June 2027 with 15-20 hours a week of your time. v2.0 delivers 101-103 (committed) and 104 (target) by Dec 31 with about an hour a week, by moving the heavy lifting to Claude, cutting every non-release deliverable (Appendix A) and running episodes 102-104 on a one-week loop.
2. **M1 was not achievable in one week as specified** (the spec's own estimates for its work packages add up to about 27 Claude-days). It is re-scoped (spec §17 M1): a lighter crayon register, the game font until M2, simpler foley, no native debug build, and a **dialogue proof** (two characters talking) replacing the single-word lip-sync test, because dialogue is most of every episode.
3. **The vault step as written would fail here.** `gh` is not logged in in this container (`gh auth status`: "The token in GH_TOKEN is invalid"), so release-asset upload through `gh` does not work. WP0 now has a fallback order (spec §10.7).
4. **No LFS and no repo-wide hook by default.** A committed SessionStart hook would make every game session in this repo download ~1 GB of models; it is now opt-in and only checks for the emulator (spec §4.1).
5. **The game is never edited.** v1.0 put guarded hunks in two core files and the PS1 build files, which other workflows are actively changing (the repo has live `.claude/worktrees`). The show now builds stand-alone, from copies of the two portrait files patched at build time. Measured: a show-style executable links from the game's own objects with nothing in `core/`, `gba/` or `ps1/` changed, and boots in PCSX-Redux (spec §15.1).
6. **The hard limits are encoded per scene, with ages and consent, in both lints and a signed checklist** (§5.1, spec §13.5 and §13.7). v1.0's single `STD-KIDS` rule worked per shot and had no consent, explicitness, age or hate-speech rule.
7. **Release realities added:** the container cannot reach your YouTube account; the not-made-for-kids and 18+ settings; the misleading-family-content risk for a cute-looking adult cartoon; the CC BY credits; Content ID disputes (§7).
8. **Disk is the tightest resource.** As little as 4.9 GB free today against a 2-2.5 GB working set per episode, so one episode is in progress at a time (§9).
