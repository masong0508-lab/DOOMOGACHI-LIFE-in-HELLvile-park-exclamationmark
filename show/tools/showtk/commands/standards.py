"""show standards <ep> and show signoff <ep> "<reply>" (WP17, sec.13.7).

    show standards ep101                  build/ep101/standards.txt + .html (stills of flagged scenes);
                                          exit 1 when an STD-* error blocks --release
    show signoff ep101 "all 8 fine"       show/ep101/standards_signoff.toml, bound to the newest
                                          delivery's render hash ("ok except 7: ..." leaves 7 open)
"""
import argparse


def run_standards(a):
    from showtk.sidecars import load_timeline
    from showtk.standards import report
    txt, htm, diags = report.build(load_timeline(a.ep), a.masters)
    print(open(txt).read())
    print("%s\n%s" % (txt, htm))
    return 1 if any(d.is_error for d in diags) else 0


def run_signoff(a):
    from showtk.standards import signoff
    p, res = signoff.write(a.ep, a.reply)
    open_ = [n for n, (ok, _) in res.items() if not ok]
    print("%s: %s" % (p, "all 8 checks ticked" if not open_ else "open: %s" % open_))
    return 0


def register(subparsers):
    p = subparsers.add_parser("standards", help="the standards report (hard limits) with stills",
                              description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("ep")
    p.add_argument("--masters")
    p.set_defaults(func=run_standards)
    q = subparsers.add_parser("signoff", help="record the creator's checklist reply, bound to the render")
    q.add_argument("ep")
    q.add_argument("reply")
    q.set_defaults(func=run_signoff)
