# DOOMOGACHI production kit

A Python production kit for the "Doomtastic Times" theme revisions (the animated-show
opening and the alternate game theme). It covers everything from the score to the
master: a score format with MIDI I/O, 52 instrument presets (synths, samplers,
drums, FX, scratches), a mixer with sends, buses, sidechain and automation, a
streaming master (-14 LUFS, <= -1 dBTP) and a QA suite that measures loudness, tuning,
clashes, masking, clicks and loop seams.

Everything renders at **48 kHz** (the broadcast/animation rate) in float64. Every
render is deterministic: randomness is seeded per note. Heavy work is capped at
**2 threads** (`kit.limit_threads(2)` runs when the package is imported).

```
kit/            the package       (core, dsp, score, theory, instrument, synths, samplers,
                                   drums, fx, reverb, mixer, master, qa, render)
assets/         licensed samples + SoundFonts (with each source's licence file)
demo/           showcase.py, groove16.py, compare_sources.py and their outputs
tests/          smoke.py (every preset), audition.py / contact.py (spectrogram sheets)
.cache/         sample_tuning.json (auto-measured sample intonation)
```

## Quick start

```python
import sys; sys.path.insert(0, ".../theme/kit")
import kit
from kit import qa, theory

s = kit.Score(bpm=150, key="Ab", mode="major", bars=8, title="sketch")
s.section("chorus", 0, 8)
kick = s.track("kick", inst=kit.make("drums_pop", kick_tune=51.9), role="drums", drum=True)
kick.pattern("x...x...x...x...", 36, bars=8)
lead = s.track("lead", inst="supersaw_lead", role="lead", program=81)
lead.seq("C5:.5 C5:.5 Eb5:.5 C5:.5 Ab5:1 Eb5:1", start=0)        # HOOK1 in Ab
keys = s.track("keys", inst="piano", role="harmony")
keys.chord(0, 4, theory.voice("Ab", center=67, n=4), v=0.75)
s.humanize(timing_ms=5, vel=0.05); s.swing(0.08, grid=0.25, tracks=["kick"])
s.export_midi("out/midi")                                          # per-track + combined

stems = kit.render_score(s)
mx = kit.Mixer(s)
mx.triggers("kick", [n.t for n in kick.notes])                     # sidechain key
mx.reverb("plate", kind="plate", rt60=1.5, hp=300, lp=9000)
mx.channel("kick", stems["kick"], bus="drums", eq=[("hp", 28), ("peak", 56, 3, 1.1)])
mx.channel("lead", stems["lead"], bus="lead", sends={"plate": -12})
mx.channel("keys", stems["keys"], bus="music",
           sidechain=dict(source="kick", depth=3, release=120))
y, master_report = kit.master(mx.render(), target_lufs=-14.0, tp_max=-1.0)
kit.export(y, "out/sketch")                                        # .wav 24-bit, .flac, .mp3 320k
print(qa.format_report(qa.report(y, stems=mx.stems, score=s, ref="jpop",
                                 png="out/sketch.png", path_wav="out/sketch.wav")))
```

Run the proofs with `python3 demo/showcase.py`, `python3 demo/groove16.py` (add
`--remix` to reuse cached stems while you adjust the mix) and
`python3 demo/compare_sources.py`. Each one takes 1-3 minutes on 2 threads.

---

## 1. Sound sources and licences

I searched PyPI and the npm registry (the only reachable hosts), downloaded the
candidates, read each licence and analysed the audio. The table lists everything the
kit ships. Each folder under `assets/` keeps its original licence or credit file.

| Source (registry package) | What the kit uses | Licence / obligations |
|---|---|---|
| **Sonic Pi sample set**, npm `supersonic-scsynth-samples@0.91.0` (`assets/sonicpi`, 207 FLAC) | acoustic kit (heavy kick, snare, hats, toms, crash/splash/ride), electronic hits, **vinyl_scratch / vinyl_hiss / vinyl_rewind / backspin**, `misc_cineboom`, swooshes, choir | **CC0 1.0**. Freesound CC0 uploads plus Arovane's donated CC0 loops; see `PROVENANCE.md`. No attribution needed. |
| **VSCO-2 Community Edition** (Versilian Studios), npm `tiny-orchestra@0.2.0` (`assets/vsco2`) | violins, violas, celli, basses, pizzicato, harp, glockenspiel, marimba, horn, trumpet, trombone, tuba, flute, oboe, clarinet, bassoon, timpani, cymbals | **CC0 1.0**. Attribution is optional but courteous ("Versilian Studios VSCO-2 CE"). |
| **Salamander Grand Piano V3** (Alexander Holm), npm `@audio-samples/piano-mp3-velocity{3,6,8,11,14,16}@1.0.5` (`assets/salamander`, 6 velocity layers, every minor third) | grand piano | **CC BY 3.0**. **Credit required:** "Salamander Grand Piano V3 by Alexander Holm, CC BY 3.0". The npm wrapper code is MIT. |
| **Roland TR-808 set** by Michael Fischer (1994), npm `@fluid-music/tr-808@0.0.2` (`assets/tr808`) | 808 kick (25 tone/decay settings), snare, clap, hats, cymbals, cowbell, toms, congas, rimshot, claves, maracas | Freely distributed with no licensing restrictions ("ABSOLUTELY FREE", `TR808.TXT`). The package is ISC. |
| **teropa drumkit**, npm `@teropa/drumkit@1.1.0` (`assets/teropa`) | crash and ride layers, a deep-house snare layer | Code MIT. Samples come from freesound: DWSD "Deep House Drum Kit" (**CC BY**, credit "DWSD / freesound.org"), Stomachache "Analog Cymbal" (CC0), Karman Lyne 808 toms (CC0). Credit DWSD in the release notes. |
| **GeneralUser GS 1.471** (S. Christian Collins), npm `generaluser@1.47.1` | GM fallback (`sf2:generaluser:<bank>:<prog>`) | GeneralUser GS License v2.0: free for music production, including commercial use. |
| **MuseScore General Lite** and **FluidR3 Mono GM** (SF3), npm `@librescore/sf3@0.8.0` | GM fallbacks for the comparison | **MIT**. Keep the copyright files shipped next to them. |
| TimGM6mb.sf2 (inside `pretty_midi`) | comparison baseline only, never in mixes | GPL-2.0 |
| Reverb, cabinet and room impulse responses | **synthesised by the kit** (`kit/reverb.py`) | the kit's own: no third-party IRs |

**Rejected candidates and why:**

* `@fluid-music/tr-909`: the Rob Roy terms say "do not change samples… not for profit".
* `@bundling-server-example/drum-samples`: these are MusicRadar SampleRadar samples. The package is mislabelled MIT, and SampleRadar does not allow redistribution.
* `webaudiofont`: GPL-3 with mixed sources.
* `web-music-score-samples`: MusyngKite under CC BY-SA, and it duplicates sources we already have.
* `soundfont-for-samplers`: FluidR3 MP3s, also duplicates.
* `musical-instrument`: Tone.js example audio with unclear provenance.
* `pepperscore-sounds`: the "Perfect Drums" licence is unclear.
* `@supersynth/*`: needs a JavaScript runtime.

The tools are numpy, scipy, numba, mido, soundfile, pyloudnorm, tinysoundfont,
matplotlib and ffmpeg. pedalboard (GPL-3) is optional: the Mixer accepts any
pedalboard plugin in a `chain`, but the kit itself never imports it. None of these
tools places obligations on the rendered audio.

### Head-to-head: same phrases through every source (`demo/compare_sources.{wav,md,json}`)

Each cell shows three numbers: the bandwidth holding 99.9 % of the energy, the energy
above 10 kHz, and the L/R correlation (1.00 = mono). Every clip is loudness-matched to
-20 LUFS.

| phrase | timgm6mb | generaluser | musescore | fluidr3 | kit |
|---|---|---|---|---|---|
| piano | 2.8k / -71 dB / +1.00 | 3.6k / -68 dB / +0.99 | 0.9k / -80 dB / +0.59 | 6.5k / -50 dB / +1.00 | 3.7k / -69 dB / **+0.28** (Salamander, stereo, 6 layers) |
| electric piano | 1.3k / -68 / +1.00 | 1.3k / -67 / +1.00 | 1.6k / -65 / +1.00 | 1.6k / -65 / +1.00 | **13.6k / -28** / +0.99 (FM tine) |
| celesta | 8.7k / -31 / +1.00 | 5.8k / -38 / +1.00 | 6.0k / -52 / +1.00 | 5.6k / -50 / +1.00 | 8.9k / -40 / +1.00 |
| glockenspiel | 16.0k / -12 / +1.00 | 11.7k / -18 / +1.00 | 14.7k / -7 / +1.00 | 14.7k / -7 / +1.00 | 13.2k / -17 / +0.99 (VSCO) |
| strings | 7.7k / -34 / -0.06 | 5.0k / -45 / +0.94 | 4.8k / -46 / +0.10 | 5.0k / -49 / +1.00 | 6.5k / -42 / +1.00 (VSCO; width comes from seating and the hall) |
| synth brass | 6.3k / -49 / +1.00 | 11.0k / -28 / +1.00 | 3.1k / -53 / +0.91 | 7.7k / -34 / +1.00 | 7.3k / -41 / **+0.65** |
| saw lead | 7.0k / -46 / +1.00 | 18.6k / -19 / +0.97 | 4.7k / -52 / +1.00 | 9.0k / -34 / +1.00 | 7.9k / -39 / +0.91 |
| square lead | 7.9k / -38 / +1.00 | 7.8k / -39 / +1.00 | 2.6k / -66 / +0.98 | 7.0k / -41 / +0.55 | 7.6k / -41 / +0.85 |
| finger bass | 2.8k / -61 / +1.00 | 2.1k / -61 / +1.00 | 1.0k / -62 / +1.00 | 1.0k / -62 / +1.00 | 2.7k / -68 / +1.00 |
| slap bass | 6.9k / -45 / +1.00 | 5.5k / -53 / +1.00 | 6.5k / -37 / +1.00 | 6.6k / -36 / +1.00 | 7.9k / -37 / +1.00 |
| synth bass | 3.8k / -57 / +1.00 | 2.9k / -62 / +0.99 | 0.6k / -66 / +0.98 | 3.6k / -56 / +1.00 | 1.6k / -94 / +1.00 (sub-heavy by design; raise `cutoff`/`env_amt` for bite) |
| distortion guitar | 4.7k / -62 / +1.00 | 7.2k / -32 / +1.00 | 4.9k / -44 / +1.00 | 4.9k / -44 / +1.00 | 6.8k / -50 / **+0.20** (double-tracked) |
| drums | 13.3k / -23 / +1.00 | 11.1k / -27 / +1.00 | 19.9k / -18 / +1.00 | 20.3k / -11 / +1.00 | 14.7k / -19 / +1.00 (panned per piece in a real mix) |

**Verdict.** TimGM6mb is band-limited (piano 2.8 kHz), mono and single-layer, so it is
for comparison only. GeneralUser GS is the best GM font here and makes a good fallback
or sketch layer. MuseScore Lite and FluidR3 Mono are mono with very dark pianos and
leads. The kit's own instruments win for this project on several counts:

* the piano is stereo with 6 layers;
* the guitar is a double-tracked amp sim, with power chords and palm mutes as articulations;
* the slap bass has a real slap/pop/ghost technique;
* the FM EP has the bright DX tine;
* the drums are layered synthesis plus CC0 samples;
* everything is band-limited synthesis with sample-accurate glides, bends and filter envelopes.

---

## 2. Instruments (`kit.make(name, **params)`, list: `kit.PRESETS`)

Every preset takes overrides, for example `kit.make("supersaw_lead", detune=18, cutoff=7000)`.
You can also pass an instrument object as `Track.inst`.

| Family | Presets | Engine / notes |
|---|---|---|
| Leads | `supersaw`, `supersaw_lead`, `supersaw_chords`, `saw_lead`, `square_lead`, `pluck` | JP-8000 7-voice polyBLEP supersaw (non-linear detune spread, stereo, key-tracked SVF, filter envelope, delayed vibrato). `SawLead` is a mono legato voice with 3-voice unison, sub, transistor-ladder filter, glides (`Note.glide`, `slide`) and pitch bends (`Note.bend`). |
| Keys | `piano`, `fm_epiano`, `organ` | Salamander multisample sampler (cubic interpolation, velocity layers, damping). DX-style FM EP (1:14 tine + 1:1 body, alias-guarded). Tonewheel organ with drawbars, key click and rotary speaker. |
| Mallets / bells | `celesta`, `glock`, `musicbox`, `vibes`, `fm_bell`, `tubular`, `glock_real`, `marimba`, `harp` | Additive inharmonic-partial models (bar ratios 1 : 2.756 : 5.404 …) with a mallet transient, plus VSCO samples. |
| Chiptune (GBA/NES) | `chip_p12`, `chip_p25`, `chip_p50`, `chip_tri`, `chip_noise`, `chip_arp` | PolyBLEP pulse at 12.5/25/50/75 %, 4-bit stepped triangle, 15-bit and 7-bit LFSR noise, a Game Boy volume envelope (4-bit steps, de-clicked), frame-rate arpeggios (`meta={'arp':[0,4,7]}`). |
| Pads / ensembles | `warm_pad`, `string_pad`, `strings`/`violins`, `violas`, `celli`, `contrabass`, `pizz`, `brass`, `horn`, `trumpet`, `trombone`, `flute`, `timpani` | The analogue pad uses saws plus PWM through a ladder filter. The Solina-style string machine has a triple-BBD ensemble. VSCO samples are **auto-tuned** (each sample's pitch is measured and corrected, cached) and seated orchestrally. The brass stab has a "bwah" filter sweep and a pitch scoop. |
| Basses | `pick_bass`, `slap_bass`, `synth_bass`, `sub_bass`, `bass808`, `reese` | The Karplus-Strong electric bass starts from a plucked-string triangle displacement, so it has a strong fundamental. Articulations: `slap`, `pop`, `ghost`, `mute`. The mono synth basses support glides. The 808 has a pitch drop and saturation. All basses keep the low end mono. |
| Guitar | `doom_guitar` | Drop-tuned Karplus-Strong strings. Power chords are automatic (`single` turns them off), with `mute` (palm mute) and `open`. The amp is a TS-style tight boost into two 4x-oversampled clip stages and a 4x12 cab EQ, with a DI-keyed gate. Two takes are double-tracked and panned L/R. |
| Drums | `drums_pop`, `drums_rock`, `drums_808`, `drums_chip`, `drums_acoustic` | GM keys (36 kick, 38 snare, 39 clap, 42/44/46 hats, toms 41-50, 49/57 crash, 51 ride, 54 tamb, 56 cowbell, 70 shaker). Each piece is synth + sample layers with round robin, velocity-to-brightness, hat choke, flams, per-piece `pans`, and `kick_tune` to tune the kick sub to the key. |
| FX (`fx`, notes carry `meta['fx']`) | `riser`, `downlifter`, `impact`, `revcymbal`, `whoosh`, `vinyl`, `scratch`, `hey`/`ho`/`hai`/`oi`/`yeah`/`ahh`, `sample` | Risers and reverse cymbals end on the note end. The turntable scratch is integrated from hand motion (`baby`, `chirp`, `transformer`, `flare`, `tear`, `scribble`, `stab`): level and tone follow stylus speed, the crossfader cuts in 1.2 ms, and groove noise and crackle sit under it. Any audio can be the record (`src=`). Gang shouts are formant-synthesised (see limits). |
| SoundFont | `"sf2:<font>:<bank>:<prog>"`, `"sf2:<font>:drums"` | Fonts: `generaluser`, `musescore`, `fluidr3`, `timgm6mb`. |

## 3. Score engine (`kit.score`, `kit.theory`)

* `Score(bpm, key, mode, bars, beats_per_bar=4, title, tail_beats)`. Use `.track(name, inst=, role=, drum=, program=, params=)`, `.section(name, bar, bars)`, `.chord_at(beat, dur, "Dbmaj7")` (read by the clash detector), `.humanize(timing_ms, vel, seed, tracks)`, `.swing(amount, grid=.25)`, `.export_midi(dir)` (one SMF per track plus a combined file with tempo, metre, key and section markers; bends become pitch-wheel events) and `.b2s`/`.b2n` (beats to seconds or samples).
* `Track`:
  * `.add(t, d, p, v, art, glide, bend, **meta)`
  * `.seq("C5:.5 Eb5:.5! [C4,Eb4,G4]:2 r:1 G4:1~")`: `!` accent, `~` slide, `^` staccato, `[..]` chord
  * `.chord(t, d, pitches, strum=)`
  * `.pattern("x..gX.o.f...", note, bars=)` (x hit, X accent, g ghost, o open, f flam)
  * `.transpose`, `.shift`, `.copy_range`
  * `chromatic_ok=True` whitelists a track's out-of-key notes
* `Note(t, d, p, v, art, glide, bend=[(beat_offset, semitones)], meta)`. Times are in **beats**.
* `import_midi(path)` reads any SMF; it was tested on the user's stems (PPQ 4096).
* `theory`: `voice("Fm7", center=64, n=4)`, `bass_note`, `scale_pcs`, `degree`, `in_scale`, `transpose_text`.

## 4. Mixer (`kit.Mixer`)

`mx.channel(name, stem, bus=, gain_db=, eq=[...], comp={...}, sat=dB, chain=[pedalboard plugins | fn(x, sr)], post_eq=, pan=, width=, mono_below=Hz, sends={'plate': -12}, sidechain={...}, automation={...}, autopan={...})`

* **EQ** specs: `("hp", f, order)`, `("lp", f, order)`, `("peak", f, dB, Q)`, `("lowshelf"|"highshelf", f, dB)`, `("notch", f, Q)`. These are RBJ biquads and Butterworth filters.
* **Compressor** (`kit.compress`): stereo-linked, soft knee, peak or RMS detector, sidechain input, detector high-pass (`sc_hp`), wet/dry mix.
* **Sidechain:** `mx.triggers("kick", beats)` then `sidechain=dict(source="kick", depth=6, attack=3, hold=10, release=170)`. This gives Kickstart-style ducking that is sample-exact from the score. Use `{key: audio}` to duck from an envelope follower instead.
* **Returns:**
  * `mx.reverb(name, kind=plate|hall|room|chamber|ambience|spring, rt60, predelay_ms, hp, lp, width, duck=)` uses synthetic IRs with per-octave RT60, early reflections and decorrelated stereo.
  * `mx.delay(name, beats=.75, beats_r=.5, feedback, hp, lp, cross, duck=, verb=)` is a ping-pong delay filtered inside the loop.
* **Buses:** `mx.bus(name, eq, comp, parallel=dict(thresh, ratio, mix_db), sat, width, mono_below, automation, sidechain)`. `parallel` is New York compression.
* **Automation** lanes on channels and buses: `{"gain_db": [(beat, dB)…], "lpf": [(beat, Hz)…], "hpf": …, "send:plate": …}`. Frequency lanes are interpolated in log space.
* **Stereo tools:** `width` (M/S), `mono_below` (phase-coherent, zero-phase split), `autopan` (tempo-synced), `haas`.
* `mx.render()` returns the pre-master. `mx.stems` holds the processed channels and returns, and `mx.bus_out` the buses.

## 5. Master (`kit.master`, `kit.export`)

The chain is mono-bass (120 Hz), then tonal EQ, glue compressor (SC-HP 90 Hz, about
1.5-2 dB GR), a 4x-oversampled soft clipper, and a look-ahead **true-peak limiter**
(4x-oversampled detection, smooth ramps). `master()` iterates the input gain until
integrated loudness is within 0.05 LU of `target_lufs` and true peak is at or below
`tp_max`. It reports glue GR, limiter GR (max and p99), the gain and the ceiling.

`export(y, base)` writes three files:

* `.wav` (24-bit with TPDF dither)
* `.flac` (24-bit)
* `.mp3` (LAME 320k)

The MP3 is decoded again and **true-peak re-checked**; it is trimmed if its peak is
over -1 dBTP. You can override any stage, for example
`master(x, chain=dict(eq=[…], glue=dict(thresh=-20), limit=dict(release_ms=120)))`.
For loud CD or broadcast masters (-9 LUFS), raise the clipper threshold
(`clip=dict(threshold_db=-1.5, ceiling_db=-0.4)`) so the limiter takes more of the work.
At -14 LUFS the clipper touches 0.05 % of samples; at -9 LUFS with the default chain
it would touch 3.7 %.

## 6. QA (`kit.qa`)

| Tool | What it does |
|---|---|
| `loudness_stats` | LUFS-I, short-term max, true peak (4x), sample peak, RMS, crest, PLR, clipped samples, clip runs |
| `ebur128_ffmpeg(path)` | ffmpeg `ebur128=peak=true`: I, **LRA**, TP. A second, independent meter. |
| `band_correlation` | L/R correlation and side/mid ratio for 20-150 / 150-500 / 500-2k / 2k-8k / 8k-20k Hz |
| `spectral_balance(x, ref="pop"\|"jpop")` | 1/3-octave long-term spectrum compared with a modern-pop reference curve (±4 dB tolerance) and a J-pop/anisong band-mix variant (less 25-45 Hz sub, +1/+1.5 dB presence/air). **These curves are approximations built from published averages and production practice, not measurements of specific records.** |
| `masking_report(stems)` | Stem pairs that compete in the same ERB-like band at a similar level at the same time, ranked |
| `tuning_symbolic(score)` | Every pitched note is in the score's key and mode (per-track whitelist available) |
| `tuning_audio(inst, pitches)` / `tuning_preset(name)` | Renders isolated notes and takes the energy-weighted median frame pitch against 12-TET at A=440. `dry=True` turns off detune and vibrato so it checks the oscillator or sample tuning itself. |
| `global_tuning(mix)` | Tuning offset of a whole mix, in cents (circular mean of spectral peaks) |
| `clash_report(score)` | Bass notes that sit a semitone (b9 high, maj7 low unless the chord is maj7) or a tritone (medium unless m7b5/dim) against sounding harmony on every beat |
| `click_detect` | Isolated broadband spikes (truncated voices, discontinuities). Spikes that recur periodically or are followed by ringing are ignored. Sharp percussive attacks (marimba, GB noise, transformer cuts) can still be flagged, so listen to the reported times. |
| `loop_seam(loop, reference=(continuous, idx))` | Compares the wrap's step size with the music's own, and the band-energy envelope across the wrap with the same transition in a continuous render. It catches dry starts and cut tails. Use it with `make_loopable` or a render-twice loop. |
| `stem_levels` / `format_levels` | Per-stem active LUFS and band energies, for gain staging |
| `spectrogram_png` | Log-frequency spectrogram with section markers, waveform with short-term LUFS, 1/3-octave curve against the reference, per-band correlation |
| `report` / `format_report` | All of the above in one call |

---

## 7. Proof: demo renders and their QA

Both demos were mastered by the kit and are **in Ab major at 150 BPM**.

### `demo/showcase.wav` (+ `.flac`, `.mp3`, `showcase_spectrogram.png`, `showcase_qa.{txt,json}`)

The showcase runs through 54 segments: every preset (4 drum kits, 6 basses, 3 keys, 9
mallets/harp, 4 leads/plucks, 6 chip voices, 7 pads/strings, 5 orchestral, the DOOM
guitar) followed by the FX (riser into impact, downlifter, reverse cymbal into crash, 7
scratch patterns, vinyl, gang shouts). Each segment is mixed through an
instrument-type strip with shared plate, hall and delay. Segments are loudness-matched
to -20 LUFS and the whole reel is mastered.

| Metric | Value |
|---|---|
| Duration | 142.4 s |
| Integrated loudness | **-14.02 LUFS** (ffmpeg: -14.0) |
| True peak | **-1.30 dBTP** (MP3 after encode: -1.19 dBTP) |
| LRA | 2.4 LU |
| Short-term max | -12.2 LUFS |
| PLR | 12.7 dB |
| Crest | 16.5 dB |
| Clipped samples | **0** |
| Limiter GR | max 1.4 dB, p99 0.3 dB |
| Glue GR (p95) | 0.5 dB |
| Stereo correlation | full +0.75; 20-150 Hz +0.99 (mono-safe bass); 150 Hz-20 kHz +0.60…+0.69 |
| Tuning, symbolic | 461 notes, **0 out of Ab major** (the verifier caught one Cb in an early draft) |
| Global tuning | -0.5 cents vs A440 |
| Audio tuning, dry (max \|cents\| per preset) | all synths ≤ 1.2; Salamander piano 2.8 (its natural stretch tuning); VSCO strings 5.2 / violas 2.7 / **celli 7.4** (sectional vibrato, after auto-tune); pizz 3.1; brass 2.4-3.1; all else < 1 |
| Click detector | 10 flags, all deliberate transients: GB noise kit, marimba mallets, transformer-scratch cuts |

The spectral balance of a solo-instrument reel is not meant to match a pop mix (sub
-12 dB), so it is reported for completeness only.

### `demo/groove16.wav` (+ `.flac`, `.mp3`, `groove16_spectrogram.png`, `groove16_qa.{txt,json}`, `out/groove16/{midi,stems,groove16_chorus_loop.wav}`)

The groove is 16 bars of J-pop:

* **Intro (bars 1-2):** a vinyl-filtered start, with bus LPF automation opening into bar 3.
* **Verse:** royal-road progression (Dbmaj7 Eb Cm7 Fm7), FM EP comping, celesta topline, finger bass with octave pops and ghosts. The **BEEP** motif sounds on a 12.5 % GB pulse.
* **Bar 8:** Hellvile peeks in with a power-chord stab, riser, reverse cymbal, a "Hey!" and a scratch.
* **Chorus:** the game's **HOOK** (HOOK1/2/3/7 from `trailer/music.c`, moved from C to Ab) on a supersaw lead doubled by a mono saw an octave below. Under it: slap bass, four-on-the-floor kick, clap, open hats, supersaw chords pumping from the kick, piano 8ths, VSCO strings, glock, GBA arps, palm-muted DOOM guitar, gang shouts and scratches. It ends on an Ab button with an impact.

| Metric | Value |
|---|---|
| Duration | 28.8 s |
| Integrated loudness | **-14.01 LUFS** (ffmpeg: -14.0) |
| True peak | **-1.30 dBTP** (MP3: -1.23 dBTP) |
| LRA | 3.7 LU |
| Short-term max | -12.6 LUFS |
| PLR | 12.7 dB |
| Clipped samples | **0** |
| Glue GR (p95) | 1.7 dB |
| Limiter GR | max 0.4 dB |
| Spectral balance vs J-pop ref (within ±4 dB) | sub -1.9, low +0.4, low-mid -0.4, mid 0.0, presence +1.4, air +1.5 dB |
| Spectral balance vs pop ref | sub -4.7, presence +2.4, air +3.0 dB (brighter, less sub: typical of anisong) |
| Stereo correlation | full +0.85; 20-150 Hz +1.00; 500-2k +0.48 |
| Tuning, symbolic | 1182 notes, **0 out of Ab major** |
| Global tuning | -1.9 cents |
| Bass/harmony clashes | **0 high, 0 medium, 0 low** over 64 beats |
| Clicks | **0** |
| Masking (informational) | kick~slap at 40-80 Hz 25 % (handled by 5 dB kick ducking plus a 52 Hz bass dip); string/supersaw/slap overlap at 350-700 Hz (all carved) |
| Chorus loop (render twice, keep pass 2) | band-envelope difference vs. the continuous transition **0.69 dB**, start tail deficit **0.00 dB**: **seamless**. The control, a naive cut of pass 1, measures 1.39 dB with a 1.66 dB tail deficit and fails, as it should. |

---

## 8. Production recipes for the theme

* **Key and tuning.** Write in the cue's key and set `Score(key=…)`. Run `qa.tuning_symbolic` and `qa.clash_report` before rendering, then `qa.global_tuning` on the master. Tune the kick sub to the tonic (`kick_tune=51.9` is Ab1).
* **The user's demo, run through the same tools.** `import_midi` on `sequence.mid` (bass) and `pad0.mid` + `pad1.mid` (pads) shows why it sounds "out of tune". The bass sits around E/Gb/Db/B while the pads sit in Ab/Bb/C/F. `clash_report` finds **16 high (b9) and 13 tritone clashes on strong beats in 116 beats**, plus 21 maj7 rubs. Re-harmonise (or re-key) the bass and pads together before any sound design.
* **J-pop mix template:** see `demo/groove16.py → mix()`.
  * Kick: HP 28, +3 dB at 56, -4 dB at 330, +2.5 dB at 3.8 kHz.
  * Bass: compressed 4:1, saturated, mono < 150 Hz, -2 dB at 52 Hz, ducked 5 dB from the kick.
  * Chord pads and supersaws: HP 250-280, -2.5 dB at 500-600, width 1.3, 6 dB pump on the chorus only.
  * Lead: presence +2 dB at 2.6 kHz; plate -12 dB, dotted-8th ping-pong delay -10 dB (ducked).
  * Drum bus: glue 2.2:1 plus parallel 8:1 at -10 dB.
* **Loudness.** Streaming masters target -14 LUFS / -1 dBTP. The MP3 is re-checked automatically.
* **Game loops.** Render the section twice, keep pass 2 and verify it with `qa.loop_seam(…, reference=…)`. Both GBA and PS1 loop players then get a seamless wrap.
* **Leitmotifs** (SPEC §14): HOOK `E E G E C' G`, RIFF `E E G E E Bb`, BEEP `c e g r c'` and so on. Use `theory.transpose_text` to move the canonical lines into any key.

## 9. Known limits and next steps

* **Gang shouts are synthetic** (formant voices; 12 voices, saturation and room). They work as a layer under a mix. For an exposed "Hey!", record 4-8 real voices. Use only these vocables: hey, ho, hai (はい), oi, yeah, ahh.
* No sung vocals and no lyrics. The kit stops at instruments.
* VSCO samples are 96 kbps mono MP3, so they are a section layer, not a solo string. The Salamander layers are 108 kbps MP3, which is transparent at mix level.
* The click detector and masking report are heuristics, so check the flagged times by ear. The spectral reference curves are approximations.
* Possible additions: real recorded shouts and claps, a convolution cab from a measured IR, a multiband master option, more VSCO articulations (staccato, tremolo).
