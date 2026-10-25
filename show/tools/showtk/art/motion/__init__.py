"""showtk.art.motion - the ART package's motion layer (owner: ART).

  ambient.py   the exact integer math of every `amb` kind (sets' ambient layers)
  render.py    a set + its ambient layers + walking actors at frame t (previews)
  actors.py    cast flipbooks on a set (walk / run / idle / pops) for previews
  budget.py    the VRAM page budget of every set with its ambient layers
  preview.py   animated previews (MP4 / GIF x3) and frame strips

    cd show/tools && python3 -m showtk.art.motion.preview [SET[:VARIANT] ...] [--out DIR]
    cd show/tools && python3 -m showtk.art.motion.budget
"""
