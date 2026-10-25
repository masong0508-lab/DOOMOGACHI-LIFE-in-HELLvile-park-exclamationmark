"""voice.lock: generated, committed (spec §3, §5.1, §10.2 step 5).

JSON, one object per line id in lines.shw order. Every time is in samples at 48 kHz.

  {"format": "show-voice-lock/1", "episode": "ep101", "sr": 48000, "spf": 1600,
   "preroll": 1440, "model": "<kokoro model id>",
   "lines": {
     "co15_perfect": {
       "speaker": "mayor", "voice": "mayor", "text": "Perfect.",
       "kind": "tts",                       # tts | rec | scratch | lib | pending
       "src": "tts:kokoro@<model sha>:am_puck:0.6+am_michael:0.4,speed=1,pitch=+2st",
       "inputs": "<sha16 of everything that made the take>",
       "take": "store:takes/mayor/ep101/co15_perfect_tts_<sha12>.flac",
       "sha256": "<sha256 of the take's 24-bit samples>",
       "onset": 1440,          # sample of the first sound inside the take file (= preroll)
       "len": 30000,           # take length in samples
       "dur": 26400,           # first sound to last sound
       "frames": 17,           # ceil(dur / 1600)
       "words":  [["Perfect.", 0, 25000]],           # relative to the onset
       "phones": [["p", 0, 2400], ["ɜ", 2400, ...]],  # relative to the onset, marks folded in
       "mouth": "XMMEEECCEEXX", "mouth_from": -1,     # char i is on frame say + mouth_from + i
       "align": "tts", "asr": {"heard": "Perfect.", "wer": 0.0, "char": 1.0},
       "lufs": -23.0, "tts": {...the recipe, so `show voice verify` can re-render it...},
       "sync": {"line": "a2_01_bone_01", "late": 12800}     # only on synced twins
     }, ...}}

Placement contract for the mixer and the compiler: `say <line>` on frame F puts the take's
sample `onset` on episode sample 1600*F (so the take file starts 30 ms earlier);
@<line>.start = F, .end = F + frames, .w<k> = F + words[k-1][1] // 1600 (likewise .p<k>).
"""
from __future__ import annotations

import json
import os

from . import env

FORMAT = "show-voice-lock/1"


def path(ep_dir):
    return os.path.join(ep_dir, "voice.lock")


def read(ep_dir):
    p = path(ep_dir)
    if not os.path.exists(p):
        return {"format": FORMAT, "lines": {}}
    with open(p, encoding="utf-8") as fh:
        d = json.load(fh)
    if d.get("format") != FORMAT:
        raise ValueError(f"{env.rel(p)}: unknown format {d.get('format')!r}")
    return d


def _dump_row(row):
    """One row, readable in a diff: scalars first, long arrays on their own lines."""
    parts = []
    order = ["speaker", "voice", "text", "kind", "src", "inputs", "take", "sha256", "onset", "len", "dur",
             "frames", "mouth_from", "mouth", "words", "phones", "align", "asr", "lufs", "sync", "tts", "status"]
    keys = [k for k in order if k in row] + sorted(k for k in row if k not in order)
    for k in keys:
        v = row[k]
        parts.append(f'      {json.dumps(k)}: {json.dumps(v, ensure_ascii=False, separators=(", ", ": "))}')
    return "{\n" + ",\n".join(parts) + "\n    }"


def write(ep_dir, lock, order=None):
    lines = lock.get("lines", {})
    ids = [i for i in (order or []) if i in lines] + sorted(i for i in lines if i not in (order or []))
    head = {k: v for k, v in lock.items() if k != "lines"}
    head["format"] = FORMAT
    out = ["{"]
    for k in ("format", "episode", "sr", "spf", "preroll", "model"):
        if k in head:
            out.append(f"  {json.dumps(k)}: {json.dumps(head[k])},")
    for k in sorted(set(head) - {"format", "episode", "sr", "spf", "preroll", "model"}):
        out.append(f"  {json.dumps(k)}: {json.dumps(head[k], ensure_ascii=False)},")
    out.append('  "lines": {')
    rows = [f"    {json.dumps(i)}: {_dump_row(lines[i])}" for i in ids]
    out.append(",\n".join(rows))
    out.append("  }")
    out.append("}")
    txt = "\n".join(out) + "\n"
    json.loads(txt)  # never write a lock that does not parse
    p = path(ep_dir)
    tmp = p + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        fh.write(txt)
    os.replace(tmp, p)
    return p


def tts_lines(lock):
    """AT9.4 / `--require human-voice`: every line still voiced by TTS."""
    return [(i, r.get("speaker"), r.get("text"), r.get("kind")) for i, r in lock.get("lines", {}).items()
            if r.get("synthetic") or r.get("kind") == "tts" or str(r.get("src", "")).startswith("tts:")]
