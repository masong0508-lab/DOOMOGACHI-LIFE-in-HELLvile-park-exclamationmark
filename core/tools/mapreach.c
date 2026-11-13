/* mapreach.c - the reachability search as a host tool (owner: levels, LV1).
 *
 *     mapreach FILE.mapx [...]
 *
 * Reads maps exported by `tools/mapc.py --export DIR` (plain numbers),
 * runs the search of map_bfs.h (the same code core/tests/test_maps.c
 * runs) and prints, per map:
 *
 *     map NAME W H
 *     start X Y            (or: start none)
 *     exit EXITS REACHED
 *     search STATES MASKS OVERFLOW KEYS
 *     stuck N [X Y BITS]   (the first stuck state)
 *     reach                followed by H rows of W characters, 1 = reachable
 *     end
 *
 * tools/levelcheck.py reads that. Exit status 2 on a file it cannot read.
 */
#include <stdio.h>
#include <stdlib.h>
#include "map_bfs.h"

static DoomoCell      cells[MAP_BFS_CELLS];
static DoomoThing     things[256];
static uint8_t        tflags2[256];
static DoomoZone      zones[DOOMO_ZONE_MAX];
static DoomoPath      paths[DOOMO_PATH_MAX];
static uint8_t        path_pts[DOOMO_PATH_MAX * DOOMO_PATH_POINTS * 2];
static DoomoSwapSet   swaps[DOOMO_SWAP_MAX];
static DoomoSwapCell  swap_cells[DOOMO_SWAP_CELLS_MAX];
static MapBfs         work;
static MapBfsResult   res;

static int num(FILE *f, int lo, int hi, int *bad)
{
    int v = 0;
    if (fscanf(f, "%d", &v) != 1 || v < lo || v > hi) { *bad = 1; return lo; }
    return v;
}

static int word(FILE *f, const char *want)
{
    char buf[32];
    return fscanf(f, "%31s", buf) == 1 && !strcmp(buf, want);
}

static int run_file(const char *path)
{
    FILE *f = fopen(path, "r");
    DoomoMapDef d;
    DoomoMapExt x;
    char name[64];
    int bad = 0, w, h, i, k, n, has_ext, sx = -1, sy = -1, npts = 0, ncells = 0;
    if (!f) return 0;
    memset(&d, 0, sizeof(d));
    memset(&x, 0, sizeof(x));
    if (!word(f, "doomomap") || num(f, 2, 2, &bad) != 2 || !word(f, "name") || fscanf(f, "%63s", name) != 1 ||
        !word(f, "size")) { fclose(f); return 0; }
    w = num(f, 1, DOOMO_MAP_MAX_W, &bad);
    h = num(f, 1, DOOMO_MAP_MAX_H, &bad);
    d.kind = (uint8_t)num(f, 0, 2, &bad);
    for (i = 0; i < w * h && !bad; i++)
        for (k = 0; k < 8; k++) ((uint8_t *)&cells[i])[k] = (uint8_t)num(f, 0, 255, &bad);
    if (bad || !word(f, "things")) { fclose(f); return 0; }
    n = num(f, 0, 255, &bad);
    for (i = 0; i < n && !bad; i++) {
        things[i].x = (uint8_t)num(f, 0, w - 1, &bad);
        things[i].y = (uint8_t)num(f, 0, h - 1, &bad);
        things[i].type = (uint8_t)num(f, 0, 255, &bad);
        things[i].angle = (uint8_t)num(f, 0, 255, &bad);
        things[i].arg = (uint8_t)num(f, 0, 255, &bad);
        things[i].flags = (uint8_t)num(f, 0, 255, &bad);
        tflags2[i] = (uint8_t)num(f, 0, 255, &bad);
        if (things[i].type == THING_PLAYER_START) { sx = things[i].x; sy = things[i].y; }
    }
    d.name = name;
    d.w = (uint8_t)w;
    d.h = (uint8_t)h;
    d.cells = cells;
    d.things = things;
    d.thing_count = (uint16_t)n;
    if (bad || !word(f, "ext")) { fclose(f); return 0; }
    has_ext = num(f, 0, 1, &bad);
    if (has_ext) {
        x.def = &d;
        x.tflags2 = tflags2;
        if (!word(f, "hdr")) bad = 1;
        x.flags = (uint8_t)num(f, 0, 255, &bad);
        x.fog = (uint8_t)num(f, 0, 255, &bad);
        x.view = (uint8_t)num(f, 0, 255, &bad);
        x.gravity = (uint8_t)num(f, 0, 255, &bad);
        x.note = (uint16_t)num(f, 0, 65535, &bad);
        if (!word(f, "zones")) bad = 1;
        x.zone_count = (uint8_t)num(f, 0, DOOMO_ZONE_MAX, &bad);
        for (i = 0; i < x.zone_count && !bad; i++) {
            zones[i].x0 = (uint8_t)num(f, 0, w - 1, &bad);
            zones[i].y0 = (uint8_t)num(f, 0, h - 1, &bad);
            zones[i].x1 = (uint8_t)num(f, 0, w - 1, &bad);
            zones[i].y1 = (uint8_t)num(f, 0, h - 1, &bad);
            zones[i].kind = (uint8_t)num(f, 0, 255, &bad);
            zones[i].arg = (uint8_t)num(f, 0, 255, &bad);
            zones[i].tag = (uint8_t)num(f, 0, 255, &bad);
        }
        if (!word(f, "paths")) bad = 1;
        x.path_count = (uint8_t)num(f, 0, DOOMO_PATH_MAX, &bad);
        for (i = 0; i < x.path_count && !bad; i++) {
            paths[i].id = (uint8_t)num(f, 0, 255, &bad);
            paths[i].kind = (uint8_t)num(f, 0, 255, &bad);
            paths[i].count = (uint8_t)num(f, 0, DOOMO_PATH_POINTS, &bad);
            paths[i].first = (uint16_t)npts;
            for (k = 0; k < paths[i].count && !bad; k++) {
                path_pts[npts * 2] = (uint8_t)num(f, 0, w - 1, &bad);
                path_pts[npts * 2 + 1] = (uint8_t)num(f, 0, h - 1, &bad);
                npts++;
            }
        }
        if (!word(f, "swaps")) bad = 1;
        x.swap_count = (uint8_t)num(f, 0, DOOMO_SWAP_MAX, &bad);
        for (i = 0; i < x.swap_count && !bad; i++) {
            int cnt;
            swaps[i].mode = (uint8_t)num(f, 0, MSWAP_COUNT - 1, &bad);
            swaps[i].event = (uint8_t)num(f, 0, 255, &bad);
            cnt = num(f, 0, DOOMO_SWAP_CELLS_MAX - ncells, &bad);
            swaps[i].first = (uint16_t)ncells;
            swaps[i].count = (uint16_t)cnt;
            for (k = 0; k < cnt && !bad; k++) {
                int j;
                swap_cells[ncells].cell = (uint16_t)num(f, 0, w * h - 1, &bad);
                for (j = 0; j < 8; j++) swap_cells[ncells].x[j] = (uint8_t)num(f, 0, 255, &bad);
                ncells++;
            }
        }
        x.zones = zones;
        x.paths = paths;
        x.path_pts = path_pts;
        x.swaps = swaps;
        x.swap_cells = swap_cells;
    }
    fclose(f);
    if (bad) return 0;
    printf("map %s %d %d\n", name, w, h);
    if (sx < 0) {
        printf("start none\nend\n");
        return 1;
    }
    map_bfs_run(&work, &d, has_ext ? &x : NULL, NULL, sx, sy, &res);
    printf("start %d %d\n", sx, sy);
    printf("exit %d %d\n", res.exits, res.exit_reached);
    printf("search %d %d %d %d\n", res.states, res.masks, res.overflow, res.keys);
    if (res.stuck) printf("stuck %d %d %d %u\n", res.stuck, res.stuck_cell % w, res.stuck_cell / w, (unsigned)res.stuck_bits);
    else printf("stuck 0\n");
    printf("reach\n");
    for (i = 0; i < h; i++) {
        for (k = 0; k < w; k++) putchar(res.reach[i * w + k] ? '1' : '0');
        putchar('\n');
    }
    printf("end\n");
    return 1;
}

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) {
        fprintf(stderr, "usage: mapreach FILE.mapx [...]\n");
        return 2;
    }
    for (i = 1; i < argc; i++)
        if (!run_file(argv[i])) {
            fprintf(stderr, "mapreach: cannot read %s\n", argv[i]);
            return 2;
        }
    return 0;
}
