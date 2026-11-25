#!/usr/bin/env python3
"""Reproducible English Talk Atlas expansion.

The TSV recipes contain independently authored observations, imagined
activities, and genuine questions, grouped by exact existing topic category.
Pairs are formed only within that category. These are composed variants, not
independently handwritten complete sentences. Keep the expanded .tlk files
in source control so every port's existing language-bank build picks them up.
No extra concept IDs, save fields, or runtime asset format are required.
"""
import argparse
import re
from pathlib import Path

from talkc_safe import hate_key, load_hate

DATA = Path(__file__).resolve().parents[1] / "data" / "talk"
GRAMMAR = DATA / "grammar" / "en"
LIMIT = 66
GROUPS = {
    "01_people_places": ("people", "places"),
    "02_food_music": ("food", "music"),
    "03_hobbies_creatures_house": ("hobbies", "creatures", "house"),
    "04_lore_life_work": ("lore", "hellvile", "feelings", "work"),
    "05_time_school_stuff": ("time", "school", "stuff"),
}
# player act, legal response act, content lane, selection weight, optional
# effective personality guard. All these are ordinary topic conversations.
WRAPPERS = (
    ("ask_about", "answer", "insight", 12, ""),
    ("ask_about", "tell", "possibility", 10, "easygoing"),
    ("ask_opinion", "answer", "insight", 14, ""),
    ("tell", "ask_about", "possibility", 14, ""),
    ("tell", "ask_about", "insight", 8, "serious"),
    ("change_topic", "answer", "insight", 12, ""),
    ("change_topic", "tell", "possibility", 12, "quirky"),
    ("continue", "tell", "possibility", 14, ""),
    ("agree", "tell", "insight", 10, ""),
    ("answer", "ask_about", "possibility", 14, ""),
    ("gossip", "ask_about", "insight", 6, ""),
)

def read_profiles():
    categories = {}
    for line in (DATA / "categories.tsv").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            c = line.split("\t")
            categories[c[0]] = c
    profiles = {}
    for path in sorted((DATA / "atlas").glob("*.tsv")):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line or line.startswith("#"):
                continue
            key, obs, possibilities, questions = line.split("\t")
            c = categories[key]
            if key in profiles or "." not in key or c[3:5] != ["clean", "any"] or "talk" not in c[1].split(","):
                raise ValueError(f"{path}:{number}: invalid Atlas category {key}")
            lists = tuple(s.split("|") for s in (obs, possibilities, questions))
            for values in lists:
                if len(values) != 12 or len(set(values)) != 12:
                    raise ValueError(f"{path}:{number}: expected 12 distinct clauses per column")
                if any(not v or v != v.strip() or not v.isascii() or any(ch in v for ch in '"{}[]') for v in values):
                    raise ValueError(f"{path}:{number}: malformed clause")
            profiles[key] = lists
    return categories, profiles

def read_followups(profiles):
    followups = {}
    for number, line in enumerate((DATA / "atlas_followups.tsv").read_text(encoding="ascii").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        key, prompts = line.split("\t")
        values = prompts.split("|")
        if key not in profiles or key in followups or not 10 <= len(values) <= 14 or len(set(values)) != len(values):
            raise ValueError(f"atlas_followups.tsv:{number}: invalid category or question count")
        if any(not v or v != v.strip() or any(ch in v for ch in '"{}[]?') or v in profiles[key][2] for v in values):
            raise ValueError(f"atlas_followups.tsv:{number}: invalid or repeated question")
        followups[key] = values
    if set(followups) != set(profiles):
        raise ValueError("followups must cover every existing Atlas category")
    return followups

def check_literal_vocabulary(profiles, followups):
    """Fail on the same exact squeezed-word matches as talkc_gen.
    
    Check authored clauses before expansion, rather than discovering a
    rejected literal after thousands of generated alternatives. The full
    grammar compiler still runs its other register and safety checks.
    """
    def die(where, message):
        raise ValueError(f"{where}: {message}")

    salt, exact, _ = load_hate(DATA / "safety" / "hate.hash", die, str)
    blocked = set(exact)
    checked = {}
    for key, columns in profiles.items():
        for column in (*columns, followups[key]):
            for phrase in column:
                for word in re.findall(r"[A-Za-z0-9'][A-Za-z0-9'-]*", phrase):
                    if word not in checked:
                        checked[word] = hate_key(salt, word) not in blocked
                    if not checked[word]:
                        raise ValueError(f"{key}: literal rejected by Talk vocabulary lint: {phrase!r}")

def build():
    categories, profiles = read_profiles()
    followups = read_followups(profiles)
    check_literal_vocabulary(profiles, followups)
    distinct = set()
    alternatives = 0
    result = {}
    for group, roots in GROUPS.items():
        lines = [
            "# GENERATED by core/tools/talk_atlas.py --write; edit the recipes in data/talk/atlas/.",
            "# Safe category-specific conversations. The compiler checks every expanded line.",
            "# The leaf sampler reads the shared guard, then one uniformly selected alternative.",
            "",
        ]
        for root in roots:
            keys = sorted(k for k in profiles if k.split(".")[0] == root)
            if not keys:
                raise ValueError(f"missing root {root}")
            missing = sorted(k for k in categories if (k == root or k.startswith(root + ".")) and k not in keys)
            guard = "&".join(["topic", "root=" + root, "!x.grief"] + ["!category=" + k for k in missing])
            if len(guard.split("&")) > 14:
                raise ValueError(f"too many guards for {root}")
            for index, (act, reply, lane, weight, personality) in enumerate(WRAPPERS):
                g = guard + ("&" + personality if personality else "")
                lines += [
                    f"@rule core.{act}.atlas.{root}.v{index} guard: {g} act: {reply}",
                    f'  "{{atlas.{root}.{lane}}}" w={weight}',
                    "",
                ]
            for key in keys:
                leaf = key.split(".")[1]
                obs, possibilities, questions = profiles[key]
                for lane, statements in (("insight", obs), ("possibility", possibilities)):
                    lines.append(f"@rule atlas.{root}.{lane}.{leaf} guard: category={key}")
                    seen = set()
                    for s in statements:
                        for q in questions:
                            template = f"{{x:cap}}? {s}. {q}?"
                            if len(template) > LIMIT or template in seen:
                                continue
                            seen.add(template)
                            distinct.add(template)
                            lines.append(f'  "{template}"')
                    if not 16 <= len(seen) <= 255:
                        raise ValueError(f"{key}/{lane}: {len(seen)} alternatives outside batch bounds")
                    alternatives += len(seen)
                    lines.append("")
        result[GRAMMAR / f"zz_atlas_{group}.tlk"] = "\n".join(lines) + "\n"
    # Keep the original leaves and their wrappers intact. Additional guarded
    # leaves join the same nested symbols; the sampler weights each batch by
    # eligible row count without scanning individual alternatives.
    original = set(distinct)
    added = set()
    for group, roots in GROUPS.items():
        lines = [
            "# GENERATED by core/tools/talk_atlas.py --write; edit atlas_followups.tsv.",
            "# Category-local follow-up questions; separate bounded leaves preserve existing wrappers.",
            "",
        ]
        for root in roots:
            for key in sorted(k for k in profiles if k.split(".")[0] == root):
                leaf = key.split(".")[1]
                obs, possibilities, _ = profiles[key]
                for lane, statements in (("insight", obs), ("possibility", possibilities)):
                    lines.append(f"@rule atlas.{root}.{lane}.{leaf}.followup guard: category={key}")
                    seen = set()
                    for s in statements:
                        for q in followups[key]:
                            template = f"{{x:cap}}? {s}. {q}?"
                            if len(template) > LIMIT or template in seen or template in original:
                                continue
                            seen.add(template)
                            added.add(template)
                            lines.append(f'  "{template}"')
                    if not 16 <= len(seen) <= 255:
                        raise ValueError(f"{key}/{lane}/followup: {len(seen)} alternatives outside batch bounds")
                    alternatives += len(seen)
                    lines.append("")
        result[GRAMMAR / f"zz_atlas_more_{group}.tlk"] = "\n".join(lines) + "\n"
    if sum(len(t.encode("ascii")) for t in added) < 1_000_000:
        raise ValueError("Atlas follow-up expansion fell below 1 MB of new unique template text")
    distinct.update(added)
    size = sum(len(t.encode("ascii")) for t in distinct)
    if size < 1_000_000:
        raise ValueError(f"Atlas content fell below 1 MB: {size} unique template bytes")
    return result, len(profiles), alternatives, len(distinct), size

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    files, profiles, alternatives, distinct, size = build()
    for path, content in files.items():
        if args.write:
            path.write_text(content, encoding="ascii", newline="\n")
        elif args.check and (not path.exists() or path.read_bytes() != content.encode("ascii")):
            parser.error(f"{path}: stale Atlas grammar; run talk_atlas.py --write")
    print(f"Talk Atlas: {profiles} categories, {alternatives} contextual alternatives, "
          f"{distinct} distinct composed templates, {size} unique uncompressed text bytes")

if __name__ == "__main__":
    main()
