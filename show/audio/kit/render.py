"""Instrument registry (presets) and score -> stems rendering."""
from __future__ import annotations

import time

import numpy as np

from . import drums, fx, samplers, synths
from .core import SR

# name -> (class, default params). Tracks may use these keys as `inst`.
PRESETS = {
    # leads
    "supersaw": (synths.Supersaw, {}),
    "supersaw_lead": (synths.Supersaw, dict(vib_depth=14.0, detune=18.0, cutoff=6500.0, gain=0.2)),
    "supersaw_chords": (synths.Supersaw, dict(detune=26.0, cutoff=4200.0, env_amt=2500.0, a=0.01,
                                             r=0.4, gain=0.12)),
    "saw_lead": (synths.SawLead, {}),
    "square_lead": (synths.SawLead, dict(square=1.0, unison=2, detune=6.0, cutoff=3000.0, sub=0.0,
                                         gain=0.32)),
    "pluck": (synths.Pluck, {}),
    "fm_epiano": (synths.FMEPiano, {}),
    "celesta": (synths.Bell, dict(kind="celesta", t60=1.8)),
    "glock": (synths.Bell, dict(kind="glock", t60=2.6, gain=0.22)),
    "musicbox": (synths.Bell, dict(kind="musicbox", t60=2.0, gain=0.28)),
    "vibes": (synths.Bell, dict(kind="vibes", t60=3.0, tremolo=0.35)),
    "fm_bell": (synths.Bell, dict(kind="fm", t60=2.5, gain=0.2)),
    "tubular": (synths.Bell, dict(kind="tubular", t60=4.0, gain=0.18)),
    # chiptune (GBA / NES)
    "chip_p12": (synths.Chip, dict(wave="p12")),
    "chip_p25": (synths.Chip, dict(wave="p25")),
    "chip_p50": (synths.Chip, dict(wave="p50")),
    "chip_tri": (synths.Chip, dict(wave="tri", env=(15, 0, 0), gain=0.3)),
    "chip_noise": (synths.Chip, dict(wave="noise", env=(12, -1, 1))),
    "chip_arp": (synths.Chip, dict(wave="p12", arp=[0, 4, 7], arp_rate=50.0, env=(12, 0, 0))),
    # pads
    "warm_pad": (synths.WarmPad, {}),
    "string_pad": (synths.StringPad, {}),
    "brass": (synths.BrassStab, {}),
    "organ": (synths.Organ, {}),
    # basses
    "sub_bass": (synths.SubBass, {}),
    "bass808": (synths.Bass808, {}),
    "reese": (synths.Reese, {}),
    "synth_bass": (synths.SynthBass, {}),
    "pick_bass": (synths.PickBass, {}),
    "slap_bass": (synths.PickBass, dict(bright=0.5, t60=1.6, tone=2600.0)),
    # guitar
    "doom_guitar": (synths.DoomGuitar, {}),
    # samples
    "piano": (samplers.SalamanderPiano, {}),
    # orchestral seating: violins left, violas centre-right, celli/basses right
    "strings": (samplers.VSCO, dict(inst="violins", pan=-0.3, spread=-0.25)),
    "violins": (samplers.VSCO, dict(inst="violins", pan=-0.3, spread=-0.25)),
    "violas": (samplers.VSCO, dict(inst="violas", pan=0.15, spread=-0.15)),
    "celli": (samplers.VSCO, dict(inst="celli", pan=0.35, spread=-0.15)),
    "contrabass": (samplers.VSCO, dict(inst="basses", pan=0.45)),
    "pizz": (samplers.VSCO, dict(inst="violinsPizz", pan=-0.2, spread=-0.3)),
    "harp": (samplers.VSCO, dict(inst="harp", spread=0.5)),
    "glock_real": (samplers.VSCO, dict(inst="glockenspiel", spread=0.4)),
    "marimba": (samplers.VSCO, dict(inst="marimba", spread=0.6)),
    "horn": (samplers.VSCO, dict(inst="horn", pan=-0.15)),
    "trumpet": (samplers.VSCO, dict(inst="trumpet", pan=0.15)),
    "trombone": (samplers.VSCO, dict(inst="trombone", pan=0.3)),
    "flute": (samplers.VSCO, dict(inst="flute", pan=-0.1)),
    "timpani": (samplers.VSCO, dict(inst="timpani")),
    # drums
    "drums": (drums.DrumKit, dict(kit="pop")),
    "drums_pop": (drums.DrumKit, dict(kit="pop")),
    "drums_rock": (drums.DrumKit, dict(kit="rock")),
    "drums_808": (drums.DrumKit, dict(kit="808")),
    "drums_chip": (drums.DrumKit, dict(kit="chip")),
    "drums_acoustic": (drums.DrumKit, dict(kit="acoustic")),
    # fx
    "fx": (fx.FX, {}),
}


def make(key, **params):
    """Instantiate a preset by name, overriding parameters."""
    if not isinstance(key, str):
        return key
    if key.startswith("sf2:"):
        # 'sf2:generaluser:0:81' (font:bank:program)  or 'sf2:fluidr3:drums'
        parts = key.split(":")
        font = parts[1]
        if parts[2] == "drums":
            return samplers.SF2(font, 0, 0, drums=True, **params)
        return samplers.SF2(font, int(parts[3]), int(parts[2]), **params)
    cls, base = PRESETS[key]
    p = dict(base)
    p.update(params)
    return cls(**p)


def render_track(track, score, sr=SR):
    inst = make(track.inst, **(track.params or {})) if isinstance(track.inst, str) else track.inst
    return inst.render(track, score, sr)


def render_score(score, sr=SR, verbose=True, only=None):
    """Render every track -> {name: (n, 2) stem}."""
    stems = {}
    for name, tr in score.tracks.items():
        if tr.mute or not tr.notes or (only and name not in only):
            continue
        t0 = time.time()
        stems[name] = render_track(tr, score, sr)
        if verbose:
            pk = float(np.max(np.abs(stems[name]))) if len(stems[name]) else 0
            print(f"  rendered {name:<14} {len(tr.notes):4d} notes  peak {pk:6.3f}  {time.time() - t0:5.1f}s")
    return stems
