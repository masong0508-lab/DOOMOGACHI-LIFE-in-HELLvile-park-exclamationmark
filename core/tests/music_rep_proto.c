/* music_rep_proto.c - T3's reference stream: the build-time composer
 * (tools/musgen, the design study's prototype, bit-identical: its
 * fingerprint is checked below) played into the CF-38 metric (owner: MU6;
 * docs/design/music/SPEC.md CF-38 rule 6(e)). Host test code.
 *
 * The prototype's own bar signature hashes `vel >> 2` and the instrument
 * into every event (CF-37); here every event is re-described velocity-free:
 * the drum steps the section's DNA leaves optional, the per-bar dice of
 * the open hats and the ghost snares are not core steps; the lead's echo
 * is a surface op. */
#include <stdlib.h>
#include "../../tools/musgen/mg.h"
#include "../../tools/musgen/mg_gen.c"
#include "../../tools/musgen/mg_themes.c"
#include "music_rep.h"

static const MgTheme *const themes[4] = { &mg_theme_hook, &mg_theme_riff, &mg_theme_home, &mg_theme_title };
static const char *const tnames[4] = { "hook (HAHALAND)", "riff (INVASION)", "home (HOME)", "title (TITLE)" };

const char *rep_proto_name(int theme) { return tnames[theme & 3]; }

/* the design study's driver scripts (proto/host_main.c drive()) */
static void drive(MgPiece *p, int script, uint32_t bar, uint32_t *rng)
{
    if (script == RS_STEADY) { mg_set(p, 3, p->th->dial); return; }
    if (script == RS_WALK) {
        if ((bar & 7) == 0) {
            int i = p->intensity;
            *rng ^= *rng << 13; *rng ^= *rng >> 17; *rng ^= *rng << 5;
            i += (int)(*rng % 3) - 1;
            if (i < 1) i = 1;
            if (i > 5) i = 5;
            mg_set(p, i, p->th->dial);
        }
        return;
    }
    {
        static const int8_t dials[4] = { -1, 2, 4, -1 };
        int d = dials[(bar >> 6) & 3];
        mg_set(p, (bar >> 6) & 1 ? 4 : 3, d < 0 ? p->th->dial : d);
    }
}

/* is_anchor(): mg_gen.c's own (A, A2, PEAK state the theme) */

/* drum lanes of the DNA (mg_gen.c do_drums) */
static int drum_lane(int dr)
{
    static const uint8_t lane_dr[6] = { GDR_KICK, GDR_SNARE, GDR_HAT, GDR_OPEN, GDR_CLAP, GDR_TOM };
    int l;
    for (l = 0; l < 6; l++) if (lane_dr[l] == dr) return l;
    return -1;
}

static void bar_to_rep(RepRun *r, const MgPiece *p, const MgBar *b)
{
    const MgSection *s = &p->cur;
    const MgStyle *sty = &mg_styles[s->dial];
    RepMeta m;
    uint16_t core[6];
    int k, l, bar = b->bar_in_sec, steps = p->th->steps, fill = (s->fills >> (bar & 7)) & 1;
    memset(&m, 0, sizeof(m));
    m.dial = s->dial;
    m.key = (uint8_t)((b->key % 12) | (s->mode << 4));
    m.bpm = p->th->bpm;
    if (b->head_stated) m.flags |= RBF_HEAD;
    if (is_anchor(s->type)) m.flags |= RBF_ANCHOR | (bar < 4 ? RBF_ANCHOR4 : 0);
    if (bar == s->bars - 1) {               /* host_main.c's cadence check */
        int rate = sty->chord_rate;
        MgChord last = s->chord[s->bars * rate - 1];
        int deg = GCH_DEG(last);
        m.flags |= RBF_CAD;
        if (deg == 0 || deg == 4 || deg == 5 || deg == 6 || GCH_FN(last) == GFN_D) m.flags |= RBF_CAD_OK;
    }
    /* the steps every bar of the section plays (do_drums: base lane minus
     * the optional steps; the energy thinning; at energy >= 4 both hat
     * patterns hold 0x4444) */
    for (l = 0; l < 6; l++) core[l] = (uint16_t)(s->lane[l] & ~s->opt[l]);
    if (s->energy <= 2) { core[2] &= 0x1111; core[3] = 0; core[5] = 0; }
    if (s->energy >= 4) core[2] |= 0x4444;
    rep_begin_bar(r, &m);
    for (k = 0; k < b->n; k++) {
        const MgEv *e = &b->ev[k];
        RepNote n;
        memset(&n, 0, sizeof(n));
        n.at = e->at;
        n.len = e->dur;
        n.pitch = e->note;
        n.vel = e->vel;
        n.role = e->role;
        switch (e->role) {
        case GR_LEAD: n.cls = RC_LEAD; break;
        case GR_COUNTER: case GR_DESCANT: n.cls = RC_COUNTER; break;
        case GR_HARM: case GR_PAD: n.cls = RC_HARM; break;
        case GR_BASS: n.cls = RC_BASS; break;
        case GR_ECHO: n.cls = RC_LEAD; n.flags |= RNF_SURF; break;
        default: n.cls = RC_KIT; break;
        }
        if ((e->role == GR_HARM || e->role == GR_PAD) && ((e->inst & 0xF0) == 0x20 || (e->inst & 0xF0) == 0x30)) {
            static const uint8_t q_of[GCQ_COUNT] = { 0, 1, 2, 3, 5, 12, 6, 7, 8, 9, 10, 13 };  /* GCQ -> MCH */
            n.flags |= RNF_CHORD;
            n.q = q_of[(e->inst & 15) < GCQ_COUNT ? (e->inst & 15) : 0];
        }
        if (e->role == GR_DRUMS) {
            int step = e->at / 12;
            n.flags |= RNF_DRUM;
            if (e->note == GDR_CRASH) n.flags |= RNF_CORE;
            else if (fill && step >= steps - 4) n.flags |= RNF_CORE;           /* the fill */
            else if ((l = drum_lane(e->note)) >= 0 && ((core[l] >> step) & 1) && e->vel > 16) n.flags |= RNF_CORE;
        }
        rep_note(r, &n);
    }
    rep_end_bar(r);
}

void rep_proto_run(RepRun *r, int theme, uint32_t seed, int minutes, int script)
{
    static MgPiece p;
    static MgBar b;
    const MgTheme *th = themes[theme & 3];
    int beats = th->steps / 4, n = th->bpm / beats * minutes, i;
    uint32_t rng = seed | 1;
    rep_init(r, beats, th->bpm, RK_HEAD | RK_CAD);
    mg_start(&p, th, seed);
    for (i = 0; i < n; i++) {
        drive(&p, script, (uint32_t)i, &rng);
        mg_next_bar(&p, &b);
        bar_to_rep(r, &p, &b);
    }
}

int rep_proto_old_lrr(int theme, uint32_t seed, int minutes, int script)
{
    static MgPiece p;
    static MgBar b;
    const MgTheme *th = themes[theme & 3];
    int beats = th->steps / 4, n = th->bpm / beats * minutes, i, lrr;
    uint32_t rng = seed | 1, *sig = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)n);
    if (!sig) return -1;
    mg_start(&p, th, seed);
    for (i = 0; i < n; i++) {
        drive(&p, script, (uint32_t)i, &rng);
        mg_next_bar(&p, &b);
        sig[i] = b.sig;                       /* hashes vel >> 2 and inst (CF-37) */
    }
    lrr = rep_lrr(sig, n, NULL, NULL);
    free(sig);
    return lrr;
}

/* the composer is the design study's, bit for bit (MU1T acceptance) */
uint32_t rep_proto_fingerprint(int theme)
{
    static MgPiece p;
    static MgBar b;
    const MgTheme *th = themes[theme & 3];
    int n = (th->steps == 16 ? th->bpm / 4 : th->bpm / 3) * 10, i, k;
    uint32_t h = 2166136261u;
    mg_start(&p, th, 0x1234u + 7u);
    for (i = 0; i < n; i++) {
        mg_set(&p, 3, th->dial);
        mg_next_bar(&p, &b);
        for (k = 0; k < b.n; k++) {
            const MgEv *e = &b.ev[k];
            h = (h ^ (e->at | (uint32_t)e->dur << 16)) * 16777619u;
            h = (h ^ ((uint32_t)e->role | (uint32_t)e->note << 8 | (uint32_t)e->vel << 16 | (uint32_t)e->inst << 24)) * 16777619u;
        }
    }
    return h;
}
