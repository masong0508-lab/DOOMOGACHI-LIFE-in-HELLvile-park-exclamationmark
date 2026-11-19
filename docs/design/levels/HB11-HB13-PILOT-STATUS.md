# Hell Breach 11-13 pilot status

This pilot promotes three existing planned map packages into the native project registry:

- **HB11 — The Glum Pipes** (`core/data/maps/glum_pipes.txt`): 30 x 56 pipeworks map, three rising-sludge/tide sections and relief-valve use events, with the Swallowed Scream encounter group.
- **HB12 — The Wall** (`core/data/maps/the_wall.txt`): 64 x 20 long two-face wall map, face zones and the `faceb` toggle swap set, with the Foreman encounter group.
- **HB13 — Boneyard Buffet & Bandstand** (`core/data/maps/boneyard_buffet.txt`): 40 x 36 family-restaurant map, showtime lighting zones, breaker and spotlight events, with the Gravy Train Gary encounter group.

The maps are registered in `map.h`, `maps.c`, `levels_mapx.c`, and `levels2.c`; their generated map C and level text are refreshed. The existing level plan remains the design source. The Department of Contracts remains in `core/data/maps_pending/`: its stamp-and-queue mechanic still needs its own runtime implementation, so it was not promoted in this pilot.

## Undercroft lighting pass

`core/data/maps/the_undercroft.txt` now uses ambient 5 (was 2), view distance 14 (was 12), and brighter low-light cell values. It stays a dark level, retains its content note and candle/lantern mechanics, but the entrance and stair sightlines are easier to read.

## Validation

- `python3 tools/mapc.py --check`: passed for all active maps.
- `make -C core BUILD=build-render levelcheck`: 17 maps, 0 failed (map size/budget and reachability checks).
- `make -C core BUILD=build-render runner`: native runner compiled with `-Wall -Wextra -Werror`.
- Native reference-renderer captures: HB11, HB12, HB13 and brighter HB9, 320 x 240.

## Not claimed as complete

These are native reference-renderer / shared-core map integrations. This does not claim a PS1 disc/ISO, PS1 emulator capture, 30 fps on hardware, full playthrough certification, bespoke boss art/AI, or completed PS1/Dreamcast streaming. The Department of Contracts, Fright Night, and remaining planned levels remain pending. The three promoted maps use their planned encounter groups but still need bespoke boss presentation and full level-by-level mechanic/playthrough QA.
