"""show vault pack|verify|fetch - the vault (spec 10.7; showtk/vault.py has the details)."""
from .. import errors, paths


def _pack(args):
    from .. import vault
    only = tuple(x.strip() for x in args.only.split(",")) if args.only else ("emu", "sdk")
    vault.pack(args.out, only)
    return 0


def _verify(args):
    from .. import vault
    diags, ok = vault.verify(args.dir, check_models=not args.no_models,
                             check_archives=not args.no_archives)
    for line in ok:
        print("vault: OK  " + line)
    n = errors.report(diags)
    print("vault: %s (%d ok, %d error(s), %d warning(s))"
          % ("FAIL" if n else "PASS", len(ok), n, len(diags) - n))
    return 1 if n else 0


def _fetch(args):
    from .. import vault
    only = tuple(x.strip() for x in args.only.split(",")) if args.only else None
    vault.fetch(args.dir, args.root, args.force, only)
    return 0


def register(sub):
    p = sub.add_parser("vault", help="pack, verify or restore the emulator, BIOS and SDK (spec 10.7)")
    vs = p.add_subparsers(dest="vault_cmd", metavar="<pack|verify|fetch>")
    a = vs.add_parser("pack", help="archives (xz, split at 95 MiB) -> DIR; pins -> show/vault.lock")
    a.add_argument("--out", default=str(paths.VAULT_DIR), help="default %(default)s")
    a.add_argument("--only", help="emu,sdk (default both)")
    a.set_defaults(func=_pack)
    a = vs.add_parser("verify", help="AT0.3: installed tools, models and archives against show/vault.lock")
    a.add_argument("--dir", default=str(paths.VAULT_DIR), help="where the archive parts are")
    a.add_argument("--no-models", action="store_true")
    a.add_argument("--no-archives", action="store_true")
    a.set_defaults(func=_verify)
    a = vs.add_parser("fetch", help="restore from a directory of archive parts")
    a.add_argument("dir", help="a directory holding the parts named in show/vault.lock")
    a.add_argument("--root", default="/", help="install under this root (default /)")
    a.add_argument("--only", help="emu,sdk (default both)")
    a.add_argument("--force", action="store_true", help="replace files whose content differs")
    a.set_defaults(func=_fetch)
    p.set_defaults(func=lambda args: (p.print_help(), 2)[1])
