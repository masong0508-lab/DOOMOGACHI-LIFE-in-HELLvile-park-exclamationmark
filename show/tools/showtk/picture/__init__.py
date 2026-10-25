"""showtk.picture - the compiler side of the show's 2D picture (ENGINE lane 2:
WP5 strokes/tilemap/parallax/scale/matte/bake, WP7 text and cards, WP8 camera
and effects).

Each module turns staging statements into the binary records the player's
lane-2 modules read (show/include/show/show_*.h), and mirrors the player's
arithmetic so the compiler, `show timing` and the lints agree with the
picture frame for frame:

    fmt       the record layouts (struct packers; checked against the C headers)
    camera    cam keys -> ShowCamKey; the camera as a pure function of t
    strokes   STROKES planes -> ShowStrokeSet payloads; reveal curves
    tilemap   plates -> 49-tile atlases + map (ShowTilemap payloads)
    bake      gen:<call> -> indexed arrays, cached as PNG by input hash
    text      SUPERs, signs and cards -> ShowTextSet payloads; glyph schedules
    pxt       .pxt show fonts -> C tables and metrics
    fx        irises, fades, the look register, wipes and the Split
    lints     TIME-READ, TIME-FLASH, TIME-IRIS, TIME-WIPE, TIME-SPLIT, PIC-COVER
    stmts     the statement table: statement -> compiler function (the
              plug-in surface for showc's parse/plan/emit)
"""
