"""showtk.standards.signoff - the creator's checklist, bound to the render hash (sec.13.7, AT17.2).

    show signoff ep101 "all 8 fine"            # or "ok except 7: thumbnail too cute"
writes show/<ep>/standards_signoff.toml with the eight checks, the creator's reply,
and the render_hash of the newest delivery manifest.  check(ep, render_hash) says
"signed", "stale" (signed for another render: any later change re-opens it),
"open" (a check not ticked) or "unsigned".
"""
import datetime
import json
import re
import tomllib

from ..sidecars import ep_dir

CHECKS = [
    (1, "Nothing explicit is visible anywhere; sex is only implied, off-screen or innuendo", "1"),
    (2, "Every sexual or suggestive moment is between adults, and each one asked and got a yes first", "2"),
    (3, "Every no ended the advance on the spot; no silence was treated as a yes; consent is never the joke", "2"),
    (4, "No child or teen is in a scene with innuendo, a suggestive line, or graphic gore; no suggestive line is about a child", "3"),
    (5, "No joke targets a real person, group or faith; no slurs or real hate symbols", "4"),
    (6, "Gore rules: the filter is on with children; gore lands on demons and monsters only; Sunny is never shown hurt; the pet is never harmed", "docs 2"),
    (7, "Release: the TV-MA card is at the head; the title, thumbnail and description are plainly adult; YouTube set to not made for kids and 18+", "release"),
    (8, "Credits: the CC BY credit lines are in the description and end credits; voices are credited as synthetic where they are", "release"),
]


def path(ep):
    return ep_dir(ep) / "standards_signoff.toml"


def parse_reply(reply):
    """-> {n: (ok, note)}: 'all fine', 'ok except 7: ...', '1-6 ok, 7 no (...)'."""
    r = reply.lower()
    res = {n: (False, "") for n, _, _ in CHECKS}
    if re.search(r"\b(all|every|everything|1\s*-\s*8|yes|ok|fine|approved?|signed?)\b", r):
        res = {n: (True, "") for n, _, _ in CHECKS}
    for m in re.finditer(r"(?:except|not|no on|fail(?:s|ed)?|but)\s*#?(\d)(?:\s*[:(-]\s*([^,;)]*))?", r):
        res[int(m.group(1))] = (False, (m.group(2) or "").strip())
    for m in re.finditer(r"\b#?(\d)\s*(?:is\s*)?(?:no|not ok|fails?|x)\b(?:\s*[:(]\s*([^,;)]*))?", r):
        res[int(m.group(1))] = (False, (m.group(2) or "").strip())
    return res


def latest_manifest(ep):
    mans = sorted((ep_dir(ep) / "deliveries").glob("*/manifest.json"), key=lambda p: p.stat().st_mtime)
    return json.loads(mans[-1].read_text()) if mans else None


def write(ep, reply, render_hash=None, version=None, when=None):
    if render_hash is None:
        m = latest_manifest(ep)
        if not m:
            raise RuntimeError("%s: no delivery manifest to sign (run `show deliver %s web` first)" % (ep, ep))
        render_hash, version = m["render_hash"], m["version"]
    res = parse_reply(reply)
    when = when or datetime.datetime.now().strftime("%Y-%m-%dT%H:%M")

    def q(s):
        return '"%s"' % str(s).replace("\\", "\\\\").replace('"', '\\"')
    rows = ["# %s standards sign-off (SHOW_ENGINE_SPEC.md sec.13.7), written by `show signoff`" % ep,
            "# from the creator's reply. Bound to the render hash: any later change re-opens it.",
            "episode = %s" % q(ep), "version = %s" % q(version or ""), "render_hash = %s" % q(render_hash),
            "signed = %s" % q(when), "reply = %s" % q(reply), ""]
    for n, text, limit in CHECKS:
        ok, note = res[n]
        rows += ["[check.%d]" % n, "text = %s" % q(text), "hard_limit = %s" % q(limit),
                 "ok = %s" % ("true" if ok else "false"), "note = %s" % q(note), ""]
    p = path(ep)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text("\n".join(rows))
    return p, res


def check(ep, render_hash):
    """-> (state, [reasons])"""
    p = path(ep)
    if not p.exists():
        return "unsigned", ["%s does not exist: run `show standards %s`, then `show signoff %s \"<reply>\"`"
                            % (p, ep, ep)]
    d = tomllib.loads(p.read_text())
    if d.get("render_hash") != render_hash:
        return "stale", ["the sign-off is for render %s, the current render is %s: re-check and re-sign"
                         % (str(d.get("render_hash"))[:12], render_hash[:12])]
    open_ = [n for n, _, _ in CHECKS if not d.get("check", {}).get(str(n), {}).get("ok")]
    if open_:
        return "open", ["checklist item(s) %s not ticked" % ", ".join(map(str, open_))]
    return "signed", []
