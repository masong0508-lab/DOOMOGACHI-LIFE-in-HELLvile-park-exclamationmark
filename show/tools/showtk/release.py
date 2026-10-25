"""showtk.release - the YouTube upload package (WP17, SHOW_ENGINE_SPEC.md sec.13.7, AT17.3).

`show deliver <ep> web --release` calls release_gate() (every STD-* lint green, the
compiler's lint report free of errors, the creator's sign-off matching this render),
then build_package(), which writes build/<ep>/release/<version>/:

    <ep>_web_hd.mp4  .srt  .vtt         the delivery (hard-linked or copied)
    title.txt  description.txt          plainly adult; credits, the voices line, chapters
    chapters.txt  tags.txt              YouTube's "00:00 Title" lines; adult keywords only
    thumbnail.png                       1280x720 = a master frame x4 (rendered on the engine)
    upload_sheet.md                     every YouTube setting: not made for kids, 18+, ...
    standards.txt  standards_signoff.toml  manifest.json  package.json (the item list)
Release metadata comes from timeline.json's "release" block when the compiler gives
one (title, logline, tags, thumb=<shot>+<frame>), else from the defaults below.
"""
import json
import os
import shutil
import tomllib
from pathlib import Path

import numpy as np

from .sidecars import SHOW, build_dir, load_audio_events, tc

SERIES = "DOOMOGACHI LIFE in HELLvile Park!"
RATING = "TV-MA"
DEFAULT_TAGS = ["adult animation", "adult cartoon", "TV-MA", "comedy horror", "pixel art",
                "PS1", "PlayStation 1 engine", "retro game animation", "dark comedy", "DOOMOGACHI"]
KID_KEYWORDS = {"kids", "kid", "children", "child", "family", "toddler", "nursery", "baby",
                "cocomelon", "for kids", "cartoon for kids", "preschool"}
ITEMS = ["mp4", "srt", "vtt", "title", "description", "chapters", "tags", "thumbnail",
         "upload_sheet", "age_restriction", "standards", "signoff", "manifest"]


def release_gate(ep, manifest, timeline):
    """[] when the episode may be packaged, else the reasons (AT17.2)."""
    from .standards import lints, signoff
    from .review.status import read_lints
    out = []
    std = [d for d in lints.run(timeline) if d.is_error]
    out += ["standards: " + d.format() for d in std]
    rep = read_lints(ep)
    if rep is None:
        out.append("no compiler lint report (build/%s/lint.txt or lint.json): run `show lint %s`" % (ep, ep))
    elif rep[0]:
        out.append("%d lint error(s) in %s: every sec.13.5 lint must be green" % (rep[0], rep[4]))
    state, why = signoff.check(ep, manifest["render_hash"])
    if state != "signed":
        out += ["sign-off %s: %s" % (state, w) for w in why]
    return out


def credits(ep):
    """Credit lines for every sample source in the mix (licences.toml, longest prefix)."""
    lic = tomllib.loads((SHOW / "lib" / "licences" / "licences.toml").read_text())
    rows = [(m, v) for v in lic.values() if isinstance(v, dict) for m in v.get("match", [])]
    srcs = set()
    for e in load_audio_events(ep):
        s = str(e.get("src", ""))
        srcs.add(s)
        for k in ("samples", "sources"):
            srcs |= set(e.get(k, []) or [])
    for p in (build_dir(ep) / "audio").glob("*sources*.json"):
        try:
            srcs |= set(json.loads(p.read_text()))
        except ValueError:
            pass
    out, refused = [], []
    for s in sorted(srcs):
        best = max((r for r in rows if s.startswith(r[0])), key=lambda r: len(r[0]), default=None)
        if best:
            if best[1].get("credit") and best[1]["credit"] not in out:
                out.append(best[1]["credit"])
            if best[1].get("in_mix") is False:
                refused.append(s)
    return out, refused


def _thumb(tl, manifest):
    """(shot, frame) for the thumbnail: release.thumb, else the middle of the longest shot
    whose scene has no child in it (the thumbnail must be plainly adult)."""
    from .standards.lints import kid_scenes
    kids = kid_scenes(tl)
    t = tl.release.get("thumb")
    if t:
        sid, _, n = str(t).partition("+")
        s = tl.shot(sid)
        if s.scene in kids:
            raise RuntimeError("thumbnail %s is in scene %s, which has a child in it" % (t, s.scene))
        return s, s.start + int(n or 0)
    f0, f1 = manifest["frames"]
    cands = [s for s in tl.shots_in(f0, f1) if s.scene not in kids]
    if not cands:
        raise RuntimeError("no shot in the delivery is free of children: set release.thumb")
    s = max(cands, key=lambda s: (s.len, -s.start))
    return s, max(f0, min(f1 - 1, s.start + s.len // 2))


def build_package(ep, manifest, timeline, out=None, masters_dir=None):
    tl = timeline
    paths = manifest["_paths"]
    out = Path(out) if out else build_dir(ep) / "release" / manifest["version"]
    out.mkdir(parents=True, exist_ok=True)
    files = {}
    for k in ("mp4", "srt", "vtt"):
        src = Path(paths[k])
        dst = out / src.name
        if dst.exists():
            dst.unlink()
        try:
            os.link(src, dst)
        except OSError:
            shutil.copy2(src, dst)
        files[k] = dst.name
    rel = tl.release
    epnum = ep.replace("ep", "")
    title = rel.get("title") or "%s | Ep %s \"%s\" (%s adult animation)" % (SERIES, epnum, tl.title, RATING)
    (out / "title.txt").write_text(title + "\n")
    files["title"] = "title.txt"
    chap = ["%s %s" % (tc(a)[3:8] if a < 108000 else tc(a)[:8], t) for a, _, t in
            [(c["start"], c["end"], c["title"]) for c in manifest["chapters"]]]
    (out / "chapters.txt").write_text("\n".join(chap) + "\n")
    files["chapters"] = "chapters.txt"
    cred, refused = credits(ep)
    if refused:
        raise RuntimeError("AUD-LIC: sources that may never be in a mix: %s" % ", ".join(refused))
    tts = bool(manifest.get("tts_lines"))
    voices = rel.get("voices") or ("Voices: synthetic (Kokoro TTS)" if tts else "Voices: the creator")
    if tts and "synthetic" not in voices.lower():
        voices += " (some voices synthetic: Kokoro TTS)"
    logline = rel.get("logline") or ("An adult animated comedy-horror, made entirely in a PlayStation 1 "
                                     "game engine. Rated %s: graphic cartoon violence on demons and monsters. "
                                     "Not for children." % RATING)
    desc = [logline, "", "Rated %s. Adults only (18+). Not made for kids." % RATING, "",
            "Chapters:"] + chap + ["", voices] + cred + [
        "Every frame drawn by the show's own PS1 engine (PCSX-Redux capture), 320x180 scaled x6.", ""]
    (out / "description.txt").write_text("\n".join(desc))
    files["description"] = "description.txt"
    tags = [t for t in (rel.get("tags") or DEFAULT_TAGS) if t.lower() not in KID_KEYWORDS]
    (out / "tags.txt").write_text(", ".join(tags) + "\n")
    files["tags"] = "tags.txt"
    # thumbnail: one master frame, 320x180 x4 = 1280x720, nearest neighbour
    from .review.pictures import Pictures, save_png
    s, f = _thumb(tl, manifest)
    pic = Pictures(tl, masters_dir or manifest.get("_masters")).frames([f])[f]
    save_png(np.repeat(np.repeat(pic, 4, 0), 4, 1), out / "thumbnail.png")
    files["thumbnail"] = "thumbnail.png"
    sheet = ["# Upload sheet: %s %s \"%s\"" % (SERIES, ep, tl.title), "",
             "File: `%s` (%d frames, %s, 1920x1080 30p H.264, AAC 48 kHz)" % (
                 files["mp4"], manifest["n_frames"], tc(manifest["n_frames"])),
             "Render hash: `%s` (the signed checklist is bound to it)" % manifest["render_hash"], "",
             "## YouTube Studio settings (every upload)", "",
             "- [ ] Title: paste `title.txt`; Description: paste `description.txt` (credits and the voices line included)",
             "- [ ] Thumbnail: upload `thumbnail.png` (an adult scene; never a child-appeal image)",
             "- [ ] **Audience: \"No, it's not made for kids\"** (COPPA; check it again after upload: the classifier may guess wrong)",
             "- [ ] **Age restriction: \"Yes, restrict my video to viewers over 18\"**",
             "- [ ] Altered or synthetic content: answer per YouTube's current wording; the description says the voices are synthetic",
             "- [ ] Subtitles: upload `%s` (English) or `%s`" % (files["srt"], files["vtt"]),
             "- [ ] Chapters: they come from the description's timestamps (first at 00:00)",
             "- [ ] Tags: paste `tags.txt` (no kids' keywords)",
             "- [ ] Category: Film & Animation; Licence: Standard YouTube; Comments: your choice",
             "- [ ] The TV-MA card is in the first seconds of the video",
             "- [ ] Content ID: if a CC0/royalty-free sample is claimed, dispute with the licence log (show/lib/licences/)",
             "", "## Age restriction note", "",
             "This episode is %s adult animation. It must be marked not made for kids and age-restricted to 18+ "
             "on every platform (SELF_MADE_ROADMAP.md sec.7; SERIES_OVERVIEW.md sec.2)." % RATING, ""]
    (out / "upload_sheet.md").write_text("\n".join(sheet))
    files["upload_sheet"] = "upload_sheet.md"
    files["age_restriction"] = "upload_sheet.md#age-restriction-note"
    from .standards import report, signoff
    txt, _, _ = report.build(tl, masters_dir or manifest.get("_masters"), out_dir=build_dir(ep))
    shutil.copy2(txt, out / "standards.txt")
    files["standards"] = "standards.txt"
    shutil.copy2(signoff.path(ep), out / "standards_signoff.toml")
    files["signoff"] = "standards_signoff.toml"
    clean = {k: v for k, v in manifest.items() if not k.startswith("_")}
    (out / "manifest.json").write_text(json.dumps(clean, indent=1, sort_keys=True))
    files["manifest"] = "manifest.json"
    (out / "package.json").write_text(json.dumps({"episode": ep, "render_hash": manifest["render_hash"],
                                                  "items": files, "credits": cred,
                                                  "thumbnail_frame": f}, indent=1))
    missing = [i for i in ITEMS if i not in files]
    if missing:
        raise RuntimeError("release package incomplete: %s" % missing)
    return out
