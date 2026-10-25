"""`show voice <ep>`: render or ingest every line's take, align it, make its mouth track,
check it with Whisper and lock it (spec §10.2).

A line is rebuilt only when what made it changed (`inputs`: text, voice recipe, take
choice and file hash, model, lexicon, code version), so swapping one line from TTS to a
recording rewrites exactly that line's row (AT9.3).
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
import time

import numpy as np

from . import asr, env, libtakes, lock, mouth, shwio, takes, world

CODE_VERSION = "wp9.3"


def _h(obj):
    return hashlib.sha256(json.dumps(obj, sort_keys=True, ensure_ascii=False).encode()).hexdigest()[:16]


def load_ep(ep):
    d = env.ep_dir(ep)
    lines, beds = shwio.read_lines(os.path.join(d, "lines.shw"))
    cfg = shwio.read_takes(os.path.join(d, "takes.toml"))
    vo = takes.vo_files(d)
    return d, lines, beds, cfg, vo


def plan_line(line, voices, cast, cfg, vo, rows):
    """-> dict with everything that decides this line's take (and its inputs hash)."""
    vid, voice = takes.voice_for(line, voices, cast)
    kind, ref, retime = takes.choose(line, cfg, vo)
    params = takes.tts_params(line, vid, voice)
    p = {"line": line.id, "voice": vid, "kind": kind, "retime": retime, "text": line.text,
         "say": params["say"], "code": CODE_VERSION}
    if kind == "tts":
        p["tts"] = params
        p["model"] = env.model_id()
        p["phon"] = takes.g2p()(params["say"]).phon
    elif kind in ("rec", "scratch"):
        p["file"] = env.rel(ref)
        p["file_sha256"] = env.file_sha256(ref)
        p["fx"] = line.attrs.get("fx", "")
    elif kind == "lib":
        p["lib"] = ref
        p["lib_sha"] = libtakes.lookup(ref).get("sha256", "missing")
    if "sync" in line.attrs:
        tgt = rows.get(line.attrs["sync"], {})
        p["sync"] = {"line": line.attrs["sync"], "target_sha": tgt.get("sha256", "")}
    for k in ("mouth",):
        if k in line.attrs:
            p[k] = line.attrs[k]
    p["inputs"] = _h(p)
    return p, params


def make_take(line, plan, params, rows, ep):
    kind = plan["kind"]
    if kind == "tts":
        tk = takes.render_tts(line, params)
        if "sync" in plan and plan["sync"]["line"] in rows:
            tk = _sync_to(tk, rows[plan["sync"]["line"]])
    elif kind in ("rec", "scratch"):
        tk = takes.ingest(env.unrel(plan["file"]), line, params, kind=kind,
                          denoise=line.attrs.get("fx") != "dry")
    elif kind == "lib":
        tk = libtakes.take(plan["lib"], line)
    else:
        raise ValueError(kind)
    if tk is None:
        return None
    tk.path, tk.sha = takes.store(tk.y, plan["voice"], ep, line.id, kind)
    return tk


def _sync_to(tk, target_row):
    """Lay a twin's words on the target line's words (same text, same rhythm)."""
    tw = target_row.get("words") or []
    if not tw or len(tw) != len(tk.words):
        return tk
    on = env.PREROLL
    src = [0]
    dst = [0]
    for (_, s0, s1), (_, d0, d1) in zip(tk.words, tw):
        if s0 > src[-1] and d0 > dst[-1]:
            src.append(s0 + on)
            dst.append(d0 + on)
        if s1 > src[-1] and d1 > dst[-1]:
            src.append(s1 + on)
            dst.append(d1 + on)
    tail = env.SR // 20
    src.append(src[-1] + tail)
    dst.append(dst[-1] + tail)
    out_len = int(dst[-1] + takes.TAIL)
    y = world.warp(tk.y, env.SR, src, dst, out_len)
    sm, dm = np.asarray(src, float), np.asarray(dst, float)

    def remap(t):
        return int(np.interp(t + on, sm, dm) - on)

    tk.y = y
    tk.words = [(w, remap(a), remap(b)) for w, a, b in tk.words]
    tk.phones = [(c, remap(a), remap(b)) for c, a, b in tk.phones]
    tk.y, tk.phones, tk.words, _ = takes.finish(tk.y, [(c, a + on, b + on) for c, a, b in tk.phones],
                                                [(w, a + on, b + on) for w, a, b in tk.words])
    tk.src += f",sync={target_row.get('sha256', '')[:12]}"
    return tk


def row_for(line, plan, tk, do_asr=True):
    y = tk.y
    on = env.PREROLL
    _, end = takes.onset_end(y)
    dur = max(1, end - on)
    if "mouth" in line.attrs and line.attrs["mouth"] not in ("auto", "none"):
        m, mf = line.attrs["mouth"], -1
    elif line.attrs.get("mouth") == "none" or line.walla:
        m, mf = "", -1
    else:
        m, mf = mouth.track(tk.phones, y[on:])
    row = {"speaker": line.speaker, "voice": plan["voice"], "text": line.text, "kind": tk.kind,
           "src": tk.src, "inputs": plan["inputs"], "take": env.rel(tk.path), "sha256": tk.sha,
           "onset": on, "len": int(len(y)), "dur": int(dur), "frames": int(np.ceil(dur / env.SPF)),
           "mouth_from": mf, "mouth": m,
           "words": [[w, int(a), int(b)] for w, a, b in tk.words],
           "phones": [[c, int(a), int(b)] for c, a, b in tk.phones if c not in mouth.SILENT],
           "align": tk.align, "lufs": round(takes.lufs(y), 1)}
    if tk.kind == "tts" or tk.params.get("synthetic"):
        row["synthetic"] = True
    if tk.kind == "tts":
        row["tts"] = {k: tk.params[k] for k in ("kokoro", "speed", "shape", "say", "phon") if k in tk.params}
        if tk.params.get("oov"):
            row["oov"] = tk.params["oov"]
    if "sync" in line.attrs:
        late = line.attrs.get("late", "0")
        row["sync"] = {"line": line.attrs["sync"], "late": _late_smp(late)}
    if line.attrs.get("asr") == "skip" or not do_asr:
        row["asr"] = "skip" if line.attrs.get("asr") == "skip" else "pending"
    else:
        row["asr"] = asr.check(y, plan["say"])
    return row


def _late_smp(v):
    v = str(v)
    if v in env.R:
        return env.R[v] * env.SPF
    if v.endswith("smp"):
        return int(v[:-3])
    if v.endswith("f"):
        return int(v[:-1]) * env.SPF
    return int(float(v) * env.SPF)


def build(ep, only=None, force=False, do_asr=True, log=print):
    d, lines, beds, cfg, vo = load_ep(ep)
    voices, cast = takes.load_cast()
    lk = lock.read(d)
    rows = lk.get("lines", {})
    order = list(lines)
    # sync twins after their targets
    todo = [i for i in order if "sync" not in lines[i].attrs] + [i for i in order if "sync" in lines[i].attrs]
    if only:
        todo = [i for i in todo if i in only]
    t0 = time.time()
    n_new = 0
    for lid in todo:
        line = lines[lid]
        plan, params = plan_line(line, voices, cast, cfg, vo, rows)
        old = rows.get(lid)
        if old and not force and old.get("inputs") == plan["inputs"] and _have(old):
            if do_asr and old.get("asr") == "pending" and line.attrs.get("asr") != "skip":
                old["asr"] = asr.check(takes.load(env.unrel(old["take"])), plan["say"])
            continue
        tk = make_take(line, plan, params, rows, os.path.basename(d))
        if tk is None:
            rows[lid] = {"speaker": line.speaker, "voice": plan["voice"], "text": line.text, "kind": "pending",
                         "src": f"lib:{plan.get('lib', '')}", "inputs": plan["inputs"],
                         "status": libtakes.pending_reason(plan.get("lib", ""))}
            log(f"  {lid:20s} pending ({rows[lid]['status']})")
            continue
        rows[lid] = row_for(line, plan, tk, do_asr)
        n_new += 1
        r = rows[lid]
        a = r["asr"] if isinstance(r["asr"], str) else f"wer {r['asr']['wer']:.2f} heard {r['asr']['heard']!r}"
        log(f"  {lid:20s} {r['kind']:4s} {r['frames']:3d} f  {r['mouth'][:60]:60s} {a}")
    # drop rows whose line is gone
    for lid in list(rows):
        if lid not in lines:
            del rows[lid]
    lk.update({"episode": os.path.basename(d), "sr": env.SR, "spf": env.SPF, "preroll": env.PREROLL,
               "model": env.model_id(), "lines": rows})
    p = lock.write(d, lk, order)
    log(f"{env.rel(p)}: {len(rows)} lines, {n_new} (re)built in {time.time() - t0:.1f} s")
    return lk


def _have(row):
    t = row.get("take")
    if not t:
        return row.get("kind") == "pending" and False
    p = env.unrel(t)
    return os.path.exists(p)


def verify(ep, rerender=True, log=print):
    """AT9.1: every row's take is in the store with its hash; TTS rows re-render to the
    same samples (the lock reproduces)."""
    d, lines, beds, cfg, vo = load_ep(ep)
    lk = lock.read(d)
    bad = 0
    for lid, r in lk["lines"].items():
        if r.get("kind") == "pending":
            log(f"  {lid:20s} PENDING {r.get('status', '')}")
            continue
        p = env.unrel(r["take"])
        if not os.path.exists(p):
            log(f"  {lid:20s} MISSING {r['take']}")
            bad += 1
            continue
        sha = takes.pcm_sha(takes.load(p))
        ok = sha == r["sha256"]
        msg = "store ok" if ok else f"STORE HASH MISMATCH {sha[:12]} != {r['sha256'][:12]}"
        if ok and rerender and r.get("kind") == "tts":
            line = lines[lid]
            voices, cast = takes.load_cast()
            plan, params = plan_line(line, voices, cast, cfg, vo, lk["lines"])
            tk = make_take(line, plan, params, lk["lines"], os.path.basename(d) + "_verify")
            ok = tk.sha == r["sha256"]
            msg += ", re-render identical" if ok else f", RE-RENDER DIFFERS {tk.sha[:12]}"
            try:
                os.remove(tk.path)
            except OSError:
                pass
        bad += not ok
        log(f"  {lid:20s} {msg}")
    log(f"verify {ep}: {len(lk['lines']) - bad}/{len(lk['lines'])} ok")
    return bad == 0


if __name__ == "__main__":
    build(sys.argv[1])
