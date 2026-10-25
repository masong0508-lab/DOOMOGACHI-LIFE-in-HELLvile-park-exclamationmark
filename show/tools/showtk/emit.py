"""emit.py - the compiler's outputs (spec 7.1 phase 8; formats in show/README.md
"Compiler outputs"): EP###.SHW, timeline.json, audio_events.json,
captions.json, plan.txt, lint.txt.
"""
import hashlib
import json
from pathlib import Path

from . import docunits, paths, textpack_bridge
from . import shw_format as F

TOOL_FILES = ("showc.py", "emit.py", "plan.py", "parse.py", "macros.py", "timeexpr.py", "stage.py", "looks.py",
              "textpack_bridge.py", "docunits.py", "shw_format.py")
AUDIO_KEYS = ("k", "smp", "src", "speaker", "follow", "from", "op", "bus", "gain_db", "pan", "note", "bend", "cc",
              "over_smp", "len_smp", "provisional")


def toolchain_hash():
    h = hashlib.sha256()
    for name in TOOL_FILES:
        p = paths.SHOWTK / name
        if p.exists():
            h.update(p.read_bytes())
    h.update(F.HEADER.read_bytes())
    return h.digest()[:8]


def _slug_strings(ep, s):
    b = ep.rundown.by_id[s.beat]
    act = next((a for a in ep.rundown.acts if a["id"] == b.act), {"title": ""})
    txt = [b.id, b.title or "", act["title"], docunits.tc(b.start)]
    return b"".join(t.encode("utf-8") + b"\0" for t in txt)


def chunk_for(ep, c):
    S = textpack_bridge.symbols()
    s = c.shot
    bop_shot, bop_end = S["BOP_SHOT"], S["BOP_END"]
    if getattr(c, "slug", False):
        hdr = {"set_kind": 0, "badge_est": 0, "set_name": F.NONE, "vram_plan": [0, 0, 0, 1]}
        return F.write_chunk({"hdr": hdr, "events": [{"at": s.len, "anchor": 0, "op": bop_end, "a": 0, "b": 0}],
                              "strings": _slug_strings(ep, s)}), []
    is_slug = getattr(c, "is_slug", False)
    args = []
    evs = []                     # (at, seq, record)
    seq = 0
    for f, op, a, b, w in c.ev_direct:
        evs.append((f, w.get("line", 0), seq, {"at": f, "anchor": 0, "op": op, "a": a, "b": b}))
        seq += 1
    ops = list(c.show_ops)
    for e in c.audio:
        ops.append((e["_f"] - s.start, "SOP_AUDIO", [e["k"]], e["_where"], 0))
    for f, name, words, w, _o in ops:
        idx = len(args)
        args.append(len(words))
        args += words
        if idx > 0x7FFF:
            raise ValueError("shot %s: the arg table is over 32 K words" % s.id)
        evs.append((f, w.get("line", 0), seq, {"at": f, "anchor": 0, "op": bop_shot,
                                               "a": F.SOP_FLAG | F.SOP[name], "b": idx}))
        seq += 1
    evs.sort(key=lambda e: (e[0], e[1], e[2]))
    events = [e[3] for e in evs] + [{"at": s.len, "anchor": 0, "op": bop_end, "a": 0, "b": 0}]
    tracks = []
    for (ai, kind), arr in c.tracks.items():
        runs = []
        for v in arr:
            if runs and runs[-1][0] == v and runs[-1][1] < 255:
                runs[-1][1] += 1
            else:
                runs.append([v, 1])
        tracks.append({"actor": ai, "kind": kind, "runs": [tuple(r) for r in runs]})
    strings = bytes(c.strings) if not is_slug else _slug_strings(ep, s)
    hdr = {"set_kind": 0 if is_slug else {"2d": 1, "view3d": 2, "overworld": 3, "room": 4}.get(c.set_kind, 1),
           "badge_est": 0, "set_name": ep.strtab.add(c.set_name) if c.set_name else F.NONE,
           "vram_plan": [len(c.texpages), min(len(c.looks), 255), len(c.crowds), 1 if c.supers else 0]}
    strip = lambda d, keys: {k: v for k, v in d.items() if k in keys}
    pk = {f for f, _t, _n in F.STRUCTS["ShwPlane"]}
    ak = {f for f, _t, _n in F.STRUCTS["ShwActor"]}
    prk = {f for f, _t, _n in F.STRUCTS["ShwProp"]}
    ck = {f for f, _t, _n in F.STRUCTS["ShwCrowd"]}
    sk = {f for f, _t, _n in F.STRUCTS["ShwStroke"]} | {"pts"}
    chunk = F.write_chunk({
        "hdr": hdr, "looks": c.looks, "planes": [strip(p, pk) for p in c.planes],
        "actors": [strip(a, ak) for a in c.actors], "props": [strip(p, prk) for p in c.props],
        "crowds": [strip(x, ck) for x in c.crowds], "camkeys": c.camkeys, "events": events, "args": args,
        "tracks": tracks, "strokes": [strip(x, sk) for x in c.strokes], "strings": strings,
        "texpages": c.texpages, "cluts": c.cluts})
    return chunk, events


def build(ep, res, compilers):
    """fill res.shw (bytes), res.timeline, res.audio, res.captions"""
    st = ep.strtab
    items = []
    shot_ix = {c.shot.id: i for i, c in enumerate(compilers)}
    for c in compilers:
        s = c.shot
        try:
            chunk, events = chunk_for(ep, c)
        except Exception as e:      # an encoding limit: report, keep going
            from .errors import Diag
            res.diags.append(Diag("EMIT", str(e), shot=s.id, **s.where()))
            continue
        c.chunk_bytes = len(chunk)
        c.ev_n = len(events)
        flags = 0
        if getattr(c, "slug", False) or getattr(c, "is_slug", False):
            flags |= F.C["SHW_SHOT_SLUG"]
        if hasattr(c, "flags"):
            flags |= c.flags
        base_shot, base_frame, rep_shot, rep_from = F.NONE, 0, F.NONE, 0
        for key in ("base", "replay"):
            v = s.kv.get(key)
            if v and "@" in v:
                sid, fr = v.split("@", 1)
                tgt = shot_ix.get(sid)
                if tgt is None:
                    from .errors import Diag
                    if sid not in ep.shot_by_id:
                        res.diags.append(Diag("COMP-ONECHANGE", "%s=%s: no shot %s" % (key, v, sid), shot=s.id,
                                              **s.where()))
                    continue
                n = int(fr.lstrip("+"))
                if key == "base":
                    base_shot, base_frame = tgt, n
                else:
                    rep_shot, rep_from = tgt, n
        look = 0
        if hasattr(c, "look_name"):
            look = {"pixel": 0, "crayon": 1, "sepia": 2, "lcd": 3, "hellify": 4}.get(c.look_name, 0)
        seed = (ep.rundown.seed * 0x9E3779B1 + int(s.kv.get("seed", s.index)) * 2654435761) & 0xFFFFFFFF
        items.append(({"start_frame": s.start, "len": s.len, "px": s.px, "look": look, "base_shot": base_shot,
                       "base_frame": base_frame, "replay_shot": rep_shot, "replay_from": rep_from, "flags": flags,
                       "name": st.add(s.id), "seed": seed}, chunk))
        c.seed = seed
    hdr = {"fps": docunits.FPS, "flags": 1 if res.slug else 0, "canvas_w": 320, "canvas_h": 180, "canvas_y": 30,
           "seed": ep.rundown.seed & 0xFFFFFFFF, "toolchain_hash": list(toolchain_hash()),
           "total_frames": sum(ix["len"] for ix, _c in items)}
    res.shw = F.write_episode(hdr, items, st)
    res.compilers = compilers
    res.timeline = timeline(ep, res, compilers)
    res.audio = {"format": "show-audio/1", "episode": ep.prj.ep, "rate": docunits.RATE, "spf": docunits.SPF,
                 "total_smp": ep.total * docunits.SPF,
                 "events": [{k: e[k] for k in AUDIO_KEYS if k in e} for e in ep.audio_events]}
    caps = []
    for c in compilers:
        caps += getattr(c, "captions", [])
    caps.sort(key=lambda x: (x["in"], x["out"]))
    res.captions = {"format": "show-captions/1", "episode": ep.prj.ep, "cues": caps}


def timeline(ep, res, compilers):
    from . import showc
    rd = ep.rundown
    by_id = {c.shot.id: c for c in compilers}
    shots = []
    scenes = {}
    tags = []
    lines = []
    strings = []
    for s in ep.order:
        c = by_id.get(s.id)
        rec = {"id": s.id, "index": s.index, "beat": s.beat, "scene": s.scene, "start": s.start, "len": s.len,
               "end": s.start + s.len, "tc_in": docunits.tc(s.start), "slug": s.slug,
               "compiled": c is not None, "file": paths.rel(s.file), "line": s.line}
        if c is not None and not s.slug:
            rec.update({
                "set": c.set_name, "set_kind": c.set_kind, "px": s.px, "look": c.look_name, "cut": c.cut,
                "matte": s.kv.get("matte", "open"), "gore": c.gore, "text": s.kv.get("text", "on"),
                "insert": "insert" in (s.stmt.words[1:] if s.stmt else []),
                "base": s.kv.get("base"), "replay": s.kv.get("replay"), "seed": getattr(c, "seed", 0),
                "cast": [{"id": a["id"], "who": a["who"], "slot": i, "kind": a["kind_name"], "age": a["age"],
                          "tags": a["tags"], "voice": a["voice"]} for i, a in enumerate(c.actors)],
                "crowds": [{"id": x["id"], "n": x["n"], "age": x["age_name"], "kind": x["kind_name"],
                            "species": x["species_name"], "tags": x["tags"]} for x in c.crowds],
                "lines": [l["id"] for l in c.lines_said],
                "strings": [x["id"] for x in getattr(c, "strings_shown", [])],
                "audio_k": [e["k"] for e in c.audio],
                "waivers": c.waivers, "asserts": getattr(c, "asserts_done", []), "notes": c.notes,
                "ad": c.ad, "provisional": c.provisional, "chunk_bytes": getattr(c, "chunk_bytes", 0),
                "badge": getattr(c, "badge", "PS1-LEGAL"),
                "events": sorted(c.events_tl, key=lambda e: (e["f"], e["line"]))})
            if getattr(c, "cam_drift", None):
                d = c.cam_drift
                rec["camera"] = {"drift_px_per_f": [float(d["vx"]), float(d["vy"])], "zero_shot_f": d["zero"],
                                 "zero_f": s.start + d["zero"]}
            tags += c.tags
            for l in c.lines_said:
                ln = ep.lib.lines.get(l["id"], {})
                spk = ln.get("speaker")
                ch = ep.cast.chars.get(spk, {}) if spk else {}
                lines.append({"id": l["id"], "speaker": spk, "text": ln.get("text"), "dir": ln.get("dir"),
                              "shot": s.id, "start": l["start"], "end": l["end"], "provisional": l["provisional"],
                              "tags": ln.get("tags", []), "to": ln.get("to"), "ref": ln.get("ref"),
                              "consent": ln.get("consent"), "voice": ch.get("voice"), "age": ch.get("age"),
                              "file": paths.rel(ln["where"]["file"]) if ln.get("where") else None,
                              "line": ln.get("where", {}).get("line")})
                if ln.get("tags"):
                    tags.append({"on": "line", "id": l["id"], "shot": s.id, "tags": ln["tags"], "to": ln.get("to"),
                                 "ref": ln.get("ref"), "consent": ln.get("consent"), "start": l["start"],
                                 "end": l["end"], "file": paths.rel(ln["where"]["file"]),
                                 "line": ln["where"]["line"]})
            for x in getattr(c, "strings_shown", []):
                strings.append({k: x[k] for k in ("id", "text", "shot", "start", "end", "style")})
                if x.get("tags"):
                    tags.append({"on": "string", "id": x["id"], "shot": s.id, "tags": x["tags"], "to": None,
                                 "ref": None, "consent": None, "start": x["start"], "end": x["end"],
                                 "file": None, "line": None})
            # scene membership and who is in it
            sc = scenes.setdefault(s.scene, {"id": s.scene, "shots": [], "start": s.start, "end": s.start + s.len,
                                             "children": False, "child_sources": [], "gore": "CUTE"})
            sc["shots"].append(s.id)
            sc["end"] = s.start + s.len
            if c.gore == "BRUTAL":
                sc["gore"] = "BRUTAL"
            for a in c.actors:
                if a["age"] in ("child", "teen") or "KID" in a["tags"]:
                    sc["children"] = True
                    sc["child_sources"].append("actor %s.%s (%s, age=%s)" % (s.id, a["id"], a["who"], a["age"]))
            for x in c.crowds:
                if x["age_name"] in ("child", "teen") or "KID" in x["tags"]:
                    sc["children"] = True
                    sc["child_sources"].append("crowd %s.%s (age=%s)" % (s.id, x["id"], x["age_name"]))
            for e in c.audio:
                src = e.get("src", "")
                if src.startswith("walla:"):
                    bed = src[6:]
                    for lid, ln in ep.lib.lines.items():
                        spk = ln["speaker"]
                        if spk.startswith("walla:"):
                            v = ep.lib.voices.get(spk[6:], {})
                            if v.get("age") in ("child", "teen") or "kid" in spk:
                                if "walla %s" % bed not in " ".join(sc["child_sources"]):
                                    sc["children"] = True
                                    sc["child_sources"].append("walla %s (voice %s, age=%s)" % (
                                        bed, spk[6:], v.get("age") or "unset"))
                                break
                if src.startswith("line:"):
                    ln = ep.lib.lines.get(src[5:], {})
                    ch = ep.cast.chars.get(ln.get("speaker"), {})
                    if ch.get("age") in ("child", "teen"):
                        sc["children"] = True
                        sc["child_sources"].append("line %s (%s, age=%s)" % (src[5:], ln.get("speaker"), ch["age"]))
        shots.append(rec)
    for t in tags:
        sh = next((x for x in ep.order if x.id == t["shot"]), None)
        t["scene"] = sh.scene if sh else None
    acts = []
    for a in rd.acts:
        bs = [rd.by_id[b] for b in a["beats"]]
        acts.append({"id": a["id"], "title": a["title"], "start": bs[0].start if bs else 0,
                     "len": sum(b.len for b in bs), "beats": a["beats"]})
    beats = [{"id": b.id, "act": b.act, "title": b.title, "start": b.start, "len": b.len,
              "tc_in": docunits.tc(b.start), "elastic": int(b.elastic) if b.elastic not in (None, "") else None,
              "protected": b.protected, "shots": b.shots, "slug_frames": getattr(b, "slug_frames", 0),
              "file": paths.rel(b.stmt.file), "line": b.stmt.line} for b in rd.beats]
    diags = [{"rule": d.rule, "severity": d.severity, "text": d.format()} for d in res.diags]
    return {"format": "show-timeline/1", "episode": ep.prj.ep, "id": rd.id, "title": rd.title,
            "fps": docunits.FPS, "rate": docunits.RATE, "spf": docunits.SPF, "total_frames": ep.total,
            "slug": res.slug, "inputs_hash": showc.inputs_hash(ep.prj.files),
            "toolchain_hash": toolchain_hash().hex(), "acts": acts, "beats": beats,
            "scenes": list(scenes.values()), "shots": shots, "lines": lines, "strings": strings, "tags": tags,
            "diagnostics": diags}


def write(res, out_dir, name=None):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    ep = res.timeline["episode"]
    name = name or ("SLUG.SHW" if res.slug else "%s.SHW" % ep.upper())
    written = []

    def put(fn, data, binary=False):
        p = out_dir / fn
        tmp = p.with_suffix(p.suffix + ".tmp")
        if binary:
            tmp.write_bytes(data)
        else:
            tmp.write_text(data)
        tmp.replace(p)
        written.append(p)

    put(name, res.shw, True)
    put("timeline.json", json.dumps(res.timeline, indent=1) + "\n")
    put("audio_events.json", json.dumps(res.audio, indent=1) + "\n")
    put("captions.json", json.dumps(res.captions, indent=1) + "\n")
    put("plan.txt", res.plan)
    put("lint.txt", "".join(d.format() + "\n" for d in res.diags))
    return written
