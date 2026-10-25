"""`show mix <ep>` (WP11): audio_events.json -> build/<ep>/audio/mix_web.wav + stems + QA.

    show mix ep101 [--range co03 | --upto co03] [--build DIR] [--lock FILE ...] [--p4 [PLOG]]
"""


def _run(a):
    from showtk import mix as M
    upto = a.upto
    if a.range:
        upto = a.range.split("..")[-1]
    res = M.mix(a.episode, build=a.build, upto=upto, locks=a.lock or (), out=a.out, p4=a.p4,
                merge_captions=not a.no_captions)
    ok = res["master"]["ok"] and res["aud_lic"]["ok"] and res.get("p4", {"ok": True}).get("ok", False)
    return 0 if ok else 1


def register(subparsers):
    p = subparsers.add_parser("mix", help="mix and master an episode's audio_events.json (web: -16 LUFS, -1 dBTP)")
    p.add_argument("episode")
    p.add_argument("--range", help="a..b: the mix runs from sample 0 to the end of shot b")
    p.add_argument("--upto", help="end the mix at the end of this shot")
    p.add_argument("--build", help="the compiler output dir (default show/build/<ep>)")
    p.add_argument("--out", help="output dir (default <build>/audio)")
    p.add_argument("--lock", action="append", help="extra voice.lock files (the dx_proof)")
    p.add_argument("--p4", nargs="?", const=True, help="cross-check P4 against plog.tsv (default build/<ep>/render)")
    p.add_argument("--no-captions", action="store_true", help="do not merge the SDH cues into captions.json")
    p.set_defaults(func=_run)
    return p
