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
    /* Preserve legacy centered audio until an explicit music pan arrives. */
    s->tone[0].pan=s->tone[1].pan=s->tone[2].pan=s->tone[3].pan=1;
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
/* Q16 ratios for integer semitone offsets -24..+24.
 * Never invoke libm/pow, allocate, or resample a bank at frame time. */
static const uint32_t pitch_q16[49] = {
    16384,17358,18390,19484,20643,21870,23170,24548,26008,27554,
    29193,30929,32768,34716,36781,38968,41285,43740,46341,49097,
    52016,55109,58386,61858,65536,69433,73562,77936,82570,87480,
    92682,98193,104032,110218,116772,123715,131072,138866,147123,
    155872,165140,174960,185364,196386,208064,220436,233544,
    247431,262144
};
void ps2_synth_tone_pan(Ps2Synth *s,int ch,int pan)
{
    if(!s || ch<0 || ch>=PS2_SOUND_TONES)return;
    s->tone[ch].pan=(uint8_t)(pan<=0?0:pan>=2?2:1);
}
void ps2_synth_sample_ex(Ps2Synth *s,const int8_t *pcm,uint32_t n,
                         uint32_t rate,uint8_t volume,int8_t pan,
                         uint8_t priority,int8_t semitones)
{
    int i,slot=-1,first,lowest;
    Ps2Sample *v;
    uint64_t adjusted;
    if(!s||!pcm||!n||!rate||!volume)return;
    if(volume>15u)volume=15u;
    if(pan < -64)pan=-64;
    if(pan > 64)pan=64;
    if(semitones < -24)semitones=-24;
    if(semitones > 24)semitones=24;
    first=s->steal%PS2_SOUND_SAMPLES;
    lowest=first;
    /* Rotate tie order so simultaneous sounds share the three voices,
     * but never replace a more important sound with a low-priority one. */
    for(i=0;i<PS2_SOUND_SAMPLES;i++){
        int at=(first+i)%PS2_SOUND_SAMPLES;
        if(!s->sample[at].pcm){slot=at;break;}
        if(s->sample[at].priority<s->sample[lowest].priority)lowest=at;
    }
    if(slot<0){
        if(priority<s->sample[lowest].priority)return;
        slot=lowest;
    }
    s->steal=(uint8_t)(slot+1);
    v=&s->sample[slot];
    adjusted=((uint64_t)rate*pitch_q16[semitones+24]+32768u)>>16;
    if(adjusted==0)adjusted=1;
    v->step=(uint32_t)((adjusted<<16)/PS2_SOUND_RATE);
    if(!v->step)v->step=1;
    v->pcm=pcm;v->length=n;v->pos=0;
    v->volume=volume;v->pan=pan;v->priority=priority;
}
void ps2_synth_sample(Ps2Synth *s,const int8_t *pcm,uint32_t n,
                      uint32_t rate,uint8_t volume)
{
    ps2_synth_sample_ex(s,pcm,n,rate,volume,0,0,0);
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
        int left=0,right=0;
        for(j=0;j<PS2_SOUND_TONES;j++){
            Ps2Tone *t=&s->tone[j];
            int v=tone_value(t);
            if(t->pan!=2)left+=v;
            if(t->pan!=0)right+=v;
        }
        for(j=0;j<PS2_SOUND_SAMPLES;j++){
            Ps2Sample *v=&s->sample[j];
            uint32_t at;
            int sample,lg,rg;
            if(!v->pcm)continue;
            at=v->pos>>16;
            if(at>=v->length){v->pcm=0;continue;}
            sample=(int)v->pcm[at]*(int)v->volume*11;
            lg=v->pan>0?64-(int)v->pan:64;
            rg=v->pan<0?64+(int)v->pan:64;
            left+=sample*lg/64;
            right+=sample*rg/64;
            v->pos+=v->step;
            if((v->pos>>16)>=v->length)v->pcm=0;
        }
        /* Saturate channels separately; no additional PCM framebuffer
         * and no extra hardware voices. */
        stereo[f*2]=(int16_t)clip16(left);
        stereo[f*2+1]=(int16_t)clip16(right);
    }
}
