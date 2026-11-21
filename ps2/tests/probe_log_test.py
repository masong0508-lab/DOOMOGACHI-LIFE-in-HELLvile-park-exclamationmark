"""Host-only tests for optional Play! EE heartbeat extraction."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from probe_log import summarize


def hb(tick, uploads, frame="deadbeef"):
    return f"PS2_FRAME_HEARTBEAT tick={tick} gs_uploads={uploads} indexed_fnv={frame}\n"


def main():
    count = 0

    def check(expr):
        nonlocal count
        count += 1
        if not expr:
            raise AssertionError(f"EE heartbeat test {count} failed")

    empty = summarize("Play! window started\n")
    check(empty["count"] == 0)
    check(empty["status"] == "not_forwarded")
    one = summarize(hb(1, 1))
    check(one["status"] == "one_sample_insufficient")
    good = summarize(hb(1, 1) + hb(121, 3) + hb(241, 4, "baadf00d"))
    check(good["status"] == "ee_ticks_progressing")
    check(good["count"] == 3)
    check(good["frame_hash_changed"])
    check(good["samples"][-1]["uploads"] == 4)
    still = summarize(hb(1, 1) + hb(1, 1))
    check(still["status"] == "no_tick_progress")
    bad_upload = summarize(hb(5, 6))
    check(bad_upload["status"] == "invalid_upload_count")
    reset = summarize(hb(50, 30) + hb(55, 2))
    check(reset["status"] == "non_monotonic")
    unknown = summarize("PS2_FRAME_HEARTBEAT tick=oops gs_uploads=1 indexed_fnv=...\n")
    check(unknown["status"] == "not_forwarded")
    many = summarize("".join(hb(t, t // 2) for t in range(1, 21)))
    check(many["count"] == 20)
    check(len(many["samples"]) == 12)
    check(many["status"] == "ee_ticks_progressing")
    print(f"PS2 emulator heartbeat parser: {count} checks passed")


if __name__ == "__main__":
    main()
