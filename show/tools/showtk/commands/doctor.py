"""show doctor - check the machine (showtk/doctor.py)."""


def _run(args):
    from .. import doctor
    return doctor.run(models=args.models, boot=args.boot)


def register(sub):
    p = sub.add_parser("doctor", help="check the vault hashes, tools, disk space and (--boot) the emulator")
    p.add_argument("--models", action="store_true", help="also hash the TTS/ASR models (~1.2 GB)")
    p.add_argument("--boot", action="store_true", help="also boot show.exe to the test chart")
    p.set_defaults(func=_run)
