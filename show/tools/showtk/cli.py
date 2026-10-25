"""cli.py - `show <verb> ...` (spec 14).

The command table is not written here. Every file
show/tools/showtk/commands/<name>.py that defines

    def register(subparsers):
        p = subparsers.add_parser("my-verb", help="one line")
        p.add_argument(...)
        p.set_defaults(func=run)          # run(args) -> exit code (int)

is imported at start-up and adds its own subcommand(s), so the workflows
that build the show add commands without touching a shared file. Files
starting with "_" are skipped. A command module that fails to import is
reported (with the file) and the other commands still work; `show doctor`
lists such failures too.

Commands return an int exit code, or raise showtk.errors.ShowError, which
is printed in the spec 13.4 format with exit code 1.
"""
import argparse
import importlib
import pkgutil
import sys
import traceback

from . import errors, paths

PROG = "show"


def load_commands(subparsers):
    """import every showtk.commands.<name> and call its register();
    returns [(module name, error text)] for the ones that failed"""
    from . import commands
    failed = []
    for info in sorted(pkgutil.iter_modules(commands.__path__), key=lambda m: m.name):
        if info.name.startswith("_"):
            continue
        name = "showtk.commands." + info.name
        try:
            mod = importlib.import_module(name)
            reg = getattr(mod, "register", None)
            if not callable(reg):
                failed.append((info.name, "no register(subparsers) function"))
                continue
            reg(subparsers)
        except Exception as e:      # one broken command must not take down the CLI
            failed.append((info.name, "%s: %s" % (type(e).__name__, e)))
    return failed


def build_parser():
    ap = argparse.ArgumentParser(
        prog=PROG,
        description="DOOMOGACHI LIFE in HELLvile Park! - the show's tools (show/README.md).",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--debug", action="store_true", help="print Python tracebacks")
    sub = ap.add_subparsers(dest="command", metavar="<command>")
    failed = load_commands(sub)
    return ap, failed


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    ap, failed = build_parser()
    for name, why in failed:
        print("show: warning: command module %s/%s.py not loaded (%s)"
              % (paths.rel(paths.COMMANDS), name, why), file=sys.stderr)
    args = ap.parse_args(argv)
    if not getattr(args, "func", None):
        ap.print_help()
        return 2
    try:
        rc = args.func(args)
    except errors.ShowError as e:
        errors.report(e.diags)
        return 1
    except KeyboardInterrupt:
        print("show: interrupted", file=sys.stderr)
        return 130
    except Exception as e:
        if args.debug:
            traceback.print_exc()
        print(errors.Diag("SHOW-INTERNAL", "%s failed: %s: %s (rerun with --debug for the traceback)"
                          % (args.command, type(e).__name__, e)).format(), file=sys.stderr)
        return 1
    return int(rc or 0)
