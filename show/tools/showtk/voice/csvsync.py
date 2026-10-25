"""`show voice sync-csv <ep>`: bring <ep>/lines.shw in line with the published
docs/show/<ep>/lines.csv (line_id, scene, character, text, direction, target_seconds).

The CSV is the script's word: a line it has and lines.shw lacks is appended (speaker, text,
dir=); a line whose text or speaker changed is rewritten keeping its other keys (take=lib:...,
mode=, say=, asr=). Lines only in lines.shw are kept (and reported). Run with --check to only
report.
"""
from __future__ import annotations

import csv
import os
import re

from . import env, shwio


def _csv(ep):
    p = os.path.join(env.REPO, "docs", "show", ep, "lines.csv")
    with open(p, encoding="utf-8") as fh:
        return [r for r in csv.DictReader(fh) if r.get("line_id")]


def _q(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def sync(ep, check=False, log=print):
    d = env.ep_dir(ep)
    dst = os.path.join(d, "lines.shw")
    with open(dst, encoding="utf-8") as fh:
        src = fh.read().splitlines()
    rows = _csv(ep)
    want = {r["line_id"]: r for r in rows}
    have = {}
    for i, ln in enumerate(src):
        s = ln.strip()
        if s.startswith("line "):
            toks = shwio.tokenize(s)
            have[toks[1]] = i
    changed = added = 0
    for lid, r in want.items():
        spk, text = r["character"].strip(), r["text"].strip()
        if lid in have:
            i = have[lid]
            toks = shwio.tokenize(src[i].strip())
            old_spk = toks[2]
            old_text = next(t[1] for t in toks if isinstance(t, tuple) and t[0] == "str")
            if old_spk == spk and old_text == text:
                continue
            changed += 1
            log(f"  ~ {lid}: {old_spk} {old_text!r} -> {spk} {text!r}")
            ln = src[i]
            m = re.match(r'^(\s*line\s+\S+\s+)(\S+)(\s+)("(?:[^"\\]|\\.)*")', ln)
            src[i] = m.group(1) + spk + m.group(3) + _q(text) + ln[m.end():]
        else:
            added += 1
            dr = (r.get("direction") or "").strip()
            extra = f" dir={_q(dr)}" if dr else ""
            if dr.lower().startswith("possessed"):
                extra += " mode=possessed"
            log(f"  + {lid}: {spk} {text!r}")
            src.append(f"line {lid:18s} {spk} {_q(text)}{extra}")
    only = [k for k in have if k not in want]
    log(f"sync {ep}: {added} added, {changed} changed, {len(only)} only in lines.shw {only}")
    if not check and (added or changed):
        with open(dst, "w", encoding="utf-8") as fh:
            fh.write("\n".join(src) + "\n")
    return added, changed, only
