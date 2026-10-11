/* irq.iwram.c - interrupt dispatch and the DirectSound mixer (ARM, IWRAM;
 * owner: gba). crt0.s gba_irq_master acknowledges the interrupt and calls
 * gba_irq_dispatch() with IRQs disabled.
 *
 * Mixer: timer 0 runs DirectSound A at 10512 Hz = exactly 176 samples per
 * frame. DMA1 streams a 2 x 176 byte buffer into FIFO A; every VBlank it
 * is restarted at the buffer start every other frame, and the half that
 * plays next frame is mixed here from up to MIX_VOICES voices.
 */
#include "gba_plat.h"
#include "audio_int.h"

void gba_irq_dispatch(uint32_t flags);

MixVoice mix_voice[MIX_VOICES];
static int8_t mixbuf[2 * MIX_SAMPLES] __attribute__((aligned(4)));
static int16_t acc[MIX_SAMPLES];
static int active_half;
static int mixer_on;

static void mix_into(int8_t *out)
{
    int v, i, any = 0;
    for (v = 0; v < MIX_VOICES; v++) {
        MixVoice *m = &mix_voice[v];
        const int8_t *pcm;
        uint32_t pos, step, len;
        int vol;
        if (!m->active) continue;
        pcm = m->pcm;
        pos = m->pos;
        step = m->step;
        len = m->len;
        vol = m->vol;
        if (!any) {
            for (i = 0; i < MIX_SAMPLES; i++) {
                if (pos >= len) { acc[i] = 0; continue; }
                acc[i] = (int16_t)(pcm[pos >> 12] * vol);
                pos += step;
            }
            any = 1;
        } else {
            for (i = 0; i < MIX_SAMPLES && pos < len; i++) {
                acc[i] = (int16_t)(acc[i] + pcm[pos >> 12] * vol);
                pos += step;
            }
        }
        m->pos = pos;
        if (pos >= len) m->active = 0;
    }
    if (!any) {
        uint32_t *o = (uint32_t *)out;
        for (i = 0; i < MIX_SAMPLES / 4; i++) o[i] = 0;
        return;
    }
    for (i = 0; i < MIX_SAMPLES; i++) {
        int s = acc[i] >> 4;
        out[i] = (int8_t)(s > 127 ? 127 : (s < -128 ? -128 : s));
    }
}

static void audio_vblank_mix(void)
{
    if (!mixer_on) return;
    if (active_half == 1) {
        /* the second half just finished: restart the stream at the start */
        REG_DMA1CNT = 0;
        REG_DMA1SAD = (uint32_t)mixbuf;
        REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
        active_half = 0;
    } else {
        active_half = 1;
    }
    mix_into(mixbuf + (active_half ? 0 : MIX_SAMPLES));
}

void gba_irq_dispatch(uint32_t flags)
{
    if (flags & IRQ_VBLANK) {
        gba_vblanks++;
        audio_vblank_mix();
    }
}

void mixer_init(void)
{
    int i;
    for (i = 0; i < MIX_VOICES; i++) mix_voice[i].active = 0;
    for (i = 0; i < 2 * MIX_SAMPLES; i++) mixbuf[i] = 0;
    REG_TM0CNT = 0;
    REG_TM0D = (uint16_t)(65536u - 1596u);          /* 16777216 / 1596 = 10512 Hz */
    REG_DMA1CNT = 0;
    REG_DMA1SAD = (uint32_t)mixbuf;
    REG_DMA1DAD = (uint32_t)&REG_FIFO_A;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
    active_half = 0;
    mixer_on = 1;
    REG_TM0CNT = TM_ENABLE;
}

void gba_irq_init(void)
{
    REG_IME = 0;
    REG_DISPSTAT = DSTAT_VBL_IRQ;
    REG_IE = IRQ_VBLANK;
    REG_IF = 0xFFFF;
    BIOS_IF = 0;
    REG_IME = 1;
}
