/* SPU2-independent software renderer: no allocation, one 735-frame tick.
 * Channel waves mirror core/platform.h WAVE_ order (0..5).
 * Pitch phase is Q0.32; sample positions use Q16.16 for compact voices.
 */
#include <string.h>
#include "synth.h"

static int clip16(int v)
{
    return v < -32768 ? -32768 : v > 32767 ? 32767 : v;
}
void ps2_synth_reset(Ps2Synth *s)
{
    if(!s)return;
    memset(s,0,sizeof *s);
    s->tone[0].noise=0xACE1u;
    s->tone[1].noise=0x7B21u;
    s->tone[2].noise=0x9E37u;
    s->tone[3].noise=0x16A5u;
}
void ps2_synth_tone(Ps2Synth *s,int ch,uint16_t hz,uint8_t vol,uint8_t wave)
{
    Ps2Tone *t;
    if(!s||ch<0||ch>=PS2_SOUND_TONES)return;
    t=&s->tone[ch];
    if(!hz||!vol){t->volume=0;t->hz=0;return;}
    if(hz>10000u)hz=10000u; /* Nyquist margin at 22050 */
    if(vol>15u)vol=15u;
    if(t->hz!=hz)
        t->step=(uint32_t)(((uint64_t)hz<<32)/PS2_SOUND_RATE);
    t->hz=hz;t->volume=vol;
    t->wave=(uint8_t)(wave<6u?wave:2u);
}
void ps2_synth_sample(Ps2Synth *s,const int8_t *pcm,uint32_t n,uint32_t rate,uint8_t volume)
{
    int i,slot=-1;
    Ps2Sample *v;
    if(!s||!pcm||!n||!rate||!volume)return;
    if(volume>15u)volume=15u;
    /* Preserve playing samples; steal in rotating order only under load. */
    for(i=0;i<PS2_SOUND_SAMPLES;i++){
        if(!s->sample[i].pcm){slot=i;break;}
    }
    if(slot<0){slot=s->steal%PS2_SOUND_SAMPLES;s->steal++;}
    v=&s->sample[slot];
    v->pcm=pcm;v->length=n;v->pos=0;
    v->step=(uint32_t)(((uint64_t)rate<<16)/PS2_SOUND_RATE);
    if(!v->step)v->step=1;
    v->volume=volume;
}
static int tone_value(Ps2Tone *t)
{
    uint32_t p=t->phase, next;
    int value;
    if(!t->volume)return 0;
    next=p+t->step;
    if(t->wave==5 && next<p) {
        uint32_t n=t->noise ? t->noise : 0xACE1u;
        n=(n>>1)^((n&1u)?0xB400u:0u);
        t->noise=n;
    }
    t->phase=next;
    switch(t->wave){
    case 0: value=p<0x20000000u?8192:-8192;break;
    case 1: value=p<0x40000000u?8192:-8192;break;
    case 2: value=p<0x80000000u?8192:-8192;break;
    case 3: {
        int32_t triangle=(int32_t)(p>>16);
        value=p<0x80000000u ? (triangle/2)-8192 :
                                8192-(triangle-32768)/2;
        break;
    }
    case 4: value=(int)(p>>18)-8192;break;
    default: value=(t->noise&1u)?8192:-8192;break;
    }
    return value*t->volume/15;
}
void ps2_synth_render(Ps2Synth *s,int16_t *stereo,unsigned frames)
{
    unsigned f;
    int j;
    if(!s||!stereo)return;
    for(f=0;f<frames;f++){
        int mix=0;
        for(j=0;j<PS2_SOUND_TONES;j++)mix+=tone_value(&s->tone[j]);
        for(j=0;j<PS2_SOUND_SAMPLES;j++){
            Ps2Sample *v=&s->sample[j];
            uint32_t at;
            if(!v->pcm)continue;
            at=v->pos>>16;
            if(at>=v->length){v->pcm=0;continue;}
            mix+=(int)v->pcm[at]*(int)v->volume*11;
            v->pos+=v->step;
            if((v->pos>>16)>=v->length)v->pcm=0;
        }
        /* A shared mono mix reaches both speakers at the same time;
         * no speculative per-frame spatial state or extra ring buffer. */
        stereo[f*2]=(int16_t)clip16(mix);
        stereo[f*2+1]=stereo[f*2];
    }
}
