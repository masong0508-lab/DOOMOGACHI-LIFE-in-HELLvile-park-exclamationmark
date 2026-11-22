/* PS2-3 real SPU2 stream using PS2SDK audsrv IRX, without allocating
 * a per-frame sound buffer. The game generates exactly 735 stereo s16
 * frames for each 30 Hz logic tick, preserving game/music tempo.
 * If no IRX is available we fail safely and log the missing asset.
 */
#include <stdint.h>
#include <stdio.h>
#include <loadfile.h>
#include <audsrv.h>
#include "doomo/audio.h"
#include "doomo/platform.h"
#include "synth.h"

static Ps2Synth synth;
static int16_t pcm[PS2_SOUND_FRAMES*2] __attribute__((aligned(64)));
static int running;

int ps2_audio_init(void)
{
    struct audsrv_fmt_t format;
    int mod;
    ps2_synth_reset(&synth);
    running=0;
    /* A real PS2 has LIBSD in ROM. BIOS-free emulators need the
     * compatible PS2SDK FreeSD driver before audsrv can drive SPU2. */
    if(SifLoadModule("rom0:LIBSD",0,NULL)<0 &&
       SifLoadModule("cdrom0:\\FREESD.IRX;1",0,NULL)<0)
        (void)SifLoadModule("host:freesd.irx",0,NULL);
    /* A homebrew ELF can load the IRX from the PCSX2 host filesystem.
     * In the packaged disc it lives at the root as AUDSRV.IRX. */
    mod=SifLoadModule("cdrom0:\\AUDSRV.IRX;1",0,NULL);
    if(mod<0)mod=SifLoadModule("host:audsrv.irx",0,NULL);
    if(mod<0){
        printf("PS2 audio: audsrv.irx missing (disc or host); muted\n");
        return 0;
    }
    if(audsrv_init()!=0){
        printf("PS2 audio: audsrv RPC init failed\n");
        return 0;
    }
    format.bits=16;
    format.freq=PS2_SOUND_RATE;
    format.channels=2;
    if(audsrv_set_format(&format)!=0){
        printf("PS2 audio: unsupported 22050 Hz stereo format\n");
        return 0;
    }
    (void)audsrv_set_volume(MAX_VOLUME);
    running=1;
    printf("PS2 audio: SPU2 stream 22050 Hz stereo, 30 Hz tick\n");
    return 1;
}
void ps2_audio_frame(void)
{
    if(!running)return;
    /* Advance the synth on every logic tick, even when SPU2 is full:
     * dropping one block must not delay a whole section of the score. */
    ps2_synth_render(&synth,pcm,PS2_SOUND_FRAMES);
    /* Never block the GS/logic scheduler for a late IOP consumer. */
    if(audsrv_available()<(int)sizeof pcm)return;
    (void)audsrv_play_audio((const char *)pcm,(int)sizeof pcm);
}
void plat_tone(int ch,uint16_t hz,uint8_t vol,uint8_t wave)
{
    ps2_synth_tone(&synth,ch,hz,vol,wave);
}
void plat_tone_pan(int ch,int pan)
{
    ps2_synth_tone_pan(&synth,ch,pan);
}
void plat_sample_ex(int sfx,uint8_t vol,int8_t pan,uint8_t priority,int8_t semis)
{
    uint32_t n=0,rate=0;
    const int8_t *p=audio_sfx_pcm(sfx,&n,&rate);
    if(p)ps2_synth_sample_ex(&synth,p,n,rate,vol,pan,priority,semis);
}
void plat_sample(int sfx,uint8_t vol)
{
    plat_sample_ex(sfx,vol,0,0,0);
}
