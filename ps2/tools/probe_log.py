"""Parse optional EE-console heartbeats emitted by the PS2 gameplay ELF.

A screenshot or living emulator process alone is not proof that the game
is ticking. This parser is conservative: zero matches mean unknown (not
passing), while two increasing tick counters prove at least EE code
progressed during the capture. No external packages required.
"""
import re

HEARTBEAT = re.compile(
    r"PS2_FRAME_HEARTBEAT tick=(\d+) gs_uploads=(\d+) indexed_fnv=([0-9a-fA-F]{8})"
)


def summarize(text):
    samples = [
        {"tick": int(t), "uploads": int(u), "frame_hash": h.lower()}
        for t, u, h in HEARTBEAT.findall(text)
    ]
    result = {
        "count": len(samples),
        "samples": samples[-12:],
        "status": "not_forwarded",
        "frame_hash_changed": False,
    }
    if not samples:
        return result
    if any(s["uploads"] > s["tick"] for s in samples):
        result["status"] = "invalid_upload_count"
        return result
    if len(samples) < 2:
        result["status"] = "one_sample_insufficient"
        return result
    prev, last = samples[0], samples[-1]
    if last["tick"] <= prev["tick"]:
        result["status"] = "no_tick_progress"
        return result
    if any(b["tick"] <= a["tick"] or b["uploads"] < a["uploads"]
           for a, b in zip(samples, samples[1:])):
        result["status"] = "non_monotonic"
        return result
    result["frame_hash_changed"] = len({s["frame_hash"] for s in samples}) > 1
    result["status"] = "ee_ticks_progressing"
    return result
