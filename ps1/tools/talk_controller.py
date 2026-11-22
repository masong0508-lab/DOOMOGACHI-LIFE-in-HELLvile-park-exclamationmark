#!/usr/bin/env python3
"""Keep only the controller's exact-key vocabulary in the PS1 PARSE bank.

The PS1 has no typed input. Its three menu tables are the only callers of
the keyboard lexicon, and each only needs an exact lookup. Keep their
concept IDs, flags, and tags verbatim, and leave all other banks intact.
"""
from pathlib import Path
import re
import struct
import sys
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "core" / "tools"))
import talkc


def menu_keys(source):
    def table(name):
        match = re.search(r"\b" + name + r"\s*\[\s*\]\s*=\s*\{(.*?)\};", source, re.S)
        if match is None:
            raise ValueError(f"cannot find Talk menu table {name}")
        return match[1]

    words = re.findall(r'"([^"]+)"', table("stock_key") + table("everyday"))
    words += re.findall(r'\{\s*"([^"]+)"', table("outing"))
    return {word.encode("latin-1") for word in words}


def compact_parse(data, keys):
    header = list(struct.unpack_from("<16I", data))
    magic, count, first, tagcount, tagfirst = header[:5]
    strings_start = header[10]
    words = bytearray()
    strings = bytearray()
    tags = bytearray(16)
    tag_ids = {0: 0}
    for i in range(count):
        start = first + 12 * i
        offset, concept, flags, tag, length_words, length = struct.unpack_from("<IHHHBB", data, start)
        key = data[strings_start + offset:strings_start + offset + length]
        if key not in keys:
            continue
        if tag not in tag_ids:
            if tag >= tagcount:
                raise ValueError("invalid Talk lexicon tag")
            tag_ids[tag] = len(tags) // 16
            tags += data[tagfirst + 16 * tag:tagfirst + 16 * (tag + 1)]
        words += struct.pack("<IHHHBB", len(strings), concept, flags, tag_ids[tag], length_words, length)
        strings += key + b"\0"

    if not words:
        raise ValueError("Talk controller lexicon is empty")
    tags_at = 64 + len(words)
    strings_at = tags_at + len(tags)
    shape = (magic, len(words) // 12, 64, len(tags) // 16, tags_at,
             0, strings_at, 0, 0, strings_at, strings_at, len(strings),
             header[12], header[13], 0, 0)
    return struct.pack("<16I", *shape) + words + tags + strings


def controller_bank(image, keys):
    _, _, lang, dictionary, count, concepts, _ = struct.unpack_from("<IHHIHHI", image)
    names = {index: name for name, index in talkc.TB.items()}
    banks = []
    for i in range(count):
        index, kind, flags, start, packed, raw, _ = struct.unpack_from(
            "<HBBIIII", image, talkc.HDR + talkc.DIRENT * i)
        name = names[index]
        data = image[start:start + packed]
        if name == "PARSE":
            data = compact_parse(data, keys)
            raw = len(data)
            flags &= ~talkc.TBF_DC_ONLY
        banks.append((name, kind, flags, data, raw))
    # Keep the controller, safety and grammar together at the front. The
    # PS1 copies a bounded prefix to SPU RAM at boot to avoid scattered
    # seeks for a first reply. Banks beyond that prefix still stream.
    warm = {name: i for i, name in enumerate(("CATS", "PARSE", "BEATS", "SAFETY", "GRAMMAR", "CATNAMES"))}
    banks.sort(key=lambda bank: warm.get(bank[0], len(warm)))
    return talkc.pack_bin(banks, lang, concepts, SimpleNamespace(hash=dictionary))


if __name__ == "__main__":
    src, scene, output = map(Path, sys.argv[1:])
    image = src.read_bytes()
    packed = controller_bank(image, menu_keys(scene.read_text(encoding="utf-8")))
    output.write_bytes(packed)
    print(f"talk_controller: {len(image)} -> {len(packed)} B")
