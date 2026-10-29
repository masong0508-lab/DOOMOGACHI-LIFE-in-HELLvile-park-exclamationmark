#!/usr/bin/env python3
"""langextract.py - list the English string literals of core/src files that
core/lang/es*.tsv do not cover yet, as translator worksheets.
    python3 tools/langextract.py OUT.tsv file.c [file.c ...]
Keys are written like es.tsv keys (\\n, \\\\, \\xHH)."""
import os, re, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
LIT = re.compile(r'(?:"(?:[^"\\\n]|\\.)*"\s*)+')
def unesc(s):
    out = bytearray(); i = 0
    while i < len(s):
        c = s[i]
        if c == "\\":
            n = s[i + 1]
            if n == "n": out.append(10); i += 2
            elif n == "t": out.append(9); i += 2
            elif n in "\\\"'": out.append(ord(n)); i += 2
            elif n == "x":
                j = i + 2
                while j < len(s) and j < i + 4 and s[j] in "0123456789abcdefABCDEF": j += 1
                out.append(int(s[i + 2:j], 16)); i = j
            elif n.isdigit():
                j = i + 1
                while j < len(s) and j < i + 4 and s[j] in "01234567": j += 1
                out.append(int(s[i + 1:j], 8)); i = j
            else: out.append(ord(n)); i += 2
        else: out.append(ord(c)); i += 1
    return bytes(out)
def esc(b):
    return "".join("\\n" if x == 10 else "\\\\" if x == 92 else chr(x) if 32 <= x < 127 else "\\x%02X" % x for x in b)
def have():
    h = set()
    d = os.path.join(ROOT, "core", "lang")
    for f in os.listdir(d):
        if f.startswith("es") and f.endswith(".tsv"):
            for l in open(os.path.join(d, f), encoding="utf-8"):
                if l.strip() and not l.startswith("#"): h.add(l.split("\t")[0])
    return h
def main(a):
    out, files = a[0], a[1:]
    got = have(); seen = set(); rows = []
    for f in files:
        src = subprocess.run(["gcc", "-E", "-P", "-I", os.path.join(ROOT, "core", "include"), os.path.join(ROOT, "core", "src", f)],
                             capture_output=True, text=True).stdout
        for m in LIT.finditer(src):
            parts = re.findall(r'"((?:[^"\\\n]|\\.)*)"', m.group(0))
            k = esc(b"".join(unesc(p) for p in parts))
            if not re.search(r"[A-Za-z]{2}", k) or k in got or k in seen: continue
            if re.fullmatch(r"[A-Za-z_./0-9-]+", k) and (" " not in k) and k.islower(): continue   # ids, file names
            seen.add(k); rows.append((k, f))
    with open(out, "w", encoding="utf-8") as o:
        for k, f in rows: o.write("%s\t\t# %s\n" % (k, f))
    print(len(rows), "strings")
main(sys.argv[1:])
