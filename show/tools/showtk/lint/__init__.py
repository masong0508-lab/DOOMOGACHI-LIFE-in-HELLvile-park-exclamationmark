"""showtk.lint - lint checks beyond the compiler's own (spec 13.5). `show lint <ep>` runs every
module named in CHECKS; each exposes lint(ep, shots=None) -> [Diag]. One line per check."""
CHECKS = [
    "anim_density",     # ANIM-*: the creator's quality bar (no slideshows)
]
