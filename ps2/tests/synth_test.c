/* Host tests for the exact portable PCM mixer linked into the PS2 ELF.
 * No audsrv / console headers. Audio must be predictable at 30Hz even
 * when GS frames are presented twice per logic tick. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "synth.h"

static int checks,errors;
#define CHECK(c) do {checks++;if(!(c)){errors++;printf("FAIL %d: %s\n",__LINE__,#c);}}while(0)
static uint32_t checksum(const int16_t *p,unsigned n)
{
    uint32_t h=2166136261u;
    unsigned i;
    for(i=0;i<n;i++){h^=(uint16_t)p[i];h*=16777619u;}
    return h;
}
int main(void)
{
    Ps2Synth a,b;
    static int16_t pcm[PS2_SOUND_FRAMES*2+8],reference[PS2_SOUND_FRAMES*2];
    static const int8_t sfx[7]={0,127,-128,64,-40,24,0};
    unsigned i,j;
    CHECK(PS2_SOUND_RATE/30u==PS2_SOUND_FRAMES);
    ps2_synth_reset(&a);
    memset(pcm,0x57,sizeof pcm);
    ps2_synth_render(&a,pcm,PS2_SOUND_FRAMES);
    for(i=0;i<PS2_SOUND_FRAMES*2;i++)CHECK(pcm[i]==0);
    for(i=PS2_SOUND_FRAMES*2;i<PS2_SOUND_FRAMES*2+8;i++)CHECK(pcm[i]==0x5757);
    /* Deterministic exact sample stream for each of the six waveforms;
     * no random host time or GPU presentation enters the mixer. */
    for(j=0;j<6;j++){
        ps2_synth_reset(&a);
        ps2_synth_tone(&a,0,440,12,(uint8_t)j);
        ps2_synth_render(&a,pcm,PS2_SOUND_FRAMES);
        memcpy(reference,pcm,sizeof reference);
        ps2_synth_reset(&b);
        ps2_synth_tone(&b,0,440,12,(uint8_t)j);
        ps2_synth_render(&b,pcm,PS2_SOUND_FRAMES);
        CHECK(memcmp(reference,pcm,sizeof reference)==0);
        CHECK(checksum(reference,PS2_SOUND_FRAMES*2)!=checksum(reference,24));
        for(i=0;i<PS2_SOUND_FRAMES;i++)CHECK(pcm[i*2]==pcm[i*2+1]);
    }
    /* A seven-sample one-shot ends during the tick. The next tick
     * contains zeroes (no stuck sounds, no reread past the SFX length). */
    ps2_synth_reset(&a);
    ps2_synth_sample(&a,sfx,7,PS2_SOUND_RATE,15);
    ps2_synth_render(&a,pcm,PS2_SOUND_FRAMES);
    CHECK(pcm[2]==127*15*11);
    CHECK(pcm[4]==-128*15*11);
    CHECK(a.sample[0].pcm==NULL);
    ps2_synth_render(&a,pcm,PS2_SOUND_FRAMES);
    for(i=0;i<PS2_SOUND_FRAMES*2;i++)CHECK(pcm[i]==0);
    /* invalid channel/index, zero volume, no sound, no out-of-bounds */
    ps2_synth_reset(&a);
    ps2_synth_tone(&a,-1,440,15,2);
    ps2_synth_tone(&a,9,440,15,2);
    ps2_synth_sample(&a,NULL,7,22050,15);
    ps2_synth_sample(&a,sfx,7,0,15);
    ps2_synth_render(&a,pcm,PS2_SOUND_FRAMES);
    for(i=0;i<PS2_SOUND_FRAMES*2;i++)CHECK(pcm[i]==0);
    /* Stereo tone pan should route without changing the oscillator rate. */
    ps2_synth_reset(&a);
    ps2_synth_tone(&a,0,440,12,2);
    ps2_synth_tone_pan(&a,0,0);
    ps2_synth_render(&a,pcm,64);
    CHECK(pcm[0]!=0 && pcm[1]==0);
    ps2_synth_tone_pan(&a,0,2);
    ps2_synth_render(&a,pcm,64);
    CHECK(pcm[0]==0 && pcm[1]!=0);

    /* PCM pan must not duplicate the right channel into the left. */
    ps2_synth_reset(&a);
    ps2_synth_sample_ex(&a,sfx,7,PS2_SOUND_RATE,15,-64,4,0);
    ps2_synth_render(&a,pcm,7);
    CHECK(pcm[2]==127*15*11 && pcm[3]==0);
    ps2_synth_reset(&a);
    ps2_synth_sample_ex(&a,sfx,7,PS2_SOUND_RATE,15,64,4,0);
    ps2_synth_render(&a,pcm,7);
    CHECK(pcm[2]==0 && pcm[3]==127*15*11);

    /* One octave upward consumes every other source PCM sample. */
    ps2_synth_reset(&a);
    ps2_synth_sample_ex(&a,sfx,7,PS2_SOUND_RATE,15,0,4,12);
    CHECK(a.sample[0].step==131072u);
    ps2_synth_render(&a,pcm,3);
    CHECK(pcm[2]==-128*15*11);
    CHECK(pcm[4]==-40*15*11);

    /* A low-priority tick cannot interrupt three important SFX;
     * a higher-priority request can reclaim one existing voice. */
    ps2_synth_reset(&a);
    for(i=0;i<PS2_SOUND_SAMPLES;i++)
        ps2_synth_sample_ex(&a,sfx,7,PS2_SOUND_RATE,15,0,10,0);
    ps2_synth_sample_ex(&a,sfx,7,PS2_SOUND_RATE,15,0,1,0);
    CHECK(a.sample[0].priority==10);
    CHECK(a.sample[1].priority==10);
    CHECK(a.sample[2].priority==10);
    ps2_synth_sample_ex(&a,sfx,7,PS2_SOUND_RATE,15,0,20,0);
    CHECK(a.sample[0].priority==20);
    CHECK(sizeof(Ps2Synth)<256u);
    printf("PS2 SPU2 PCM mixer: %d host checks, %d failed\n",checks,errors);
    return errors!=0;
}
