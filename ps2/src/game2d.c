/* PS2-1: first playable shared-core 2D framebuffer.
 * The core's authoritative 256-colour palette, 5x8 font, portraits and
 * user interface draw here at 320x240. A single 512x256 RGBA GS texture
 * is refreshed at 30Hz; NTSC fields display the most recent image at
 * 60Hz. The placeholder 3D and overworld adapters are deliberately
 * visibly incomplete until the proper GS 3D renderer is ported.
 * No new per-resident portrait cache, heap allocation or save fields.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <gsKit.h>
#include "game2d.h"
#include "doomo/doomo.h"
#include "doomo/font.h"
#include "doomo/palette.h"

enum { W=320, H=240, TEX_W=512, TEX_H=256 };
static uint8_t fb[W*H];
static uint32_t upload[TEX_W*TEX_H] __attribute__((aligned(64)));
static uint32_t colors[256];
static GSTEXTURE picture;
static uint32_t frame_ticks;
static uint8_t drawing;

int plat_screen_w(void){return W;}
int plat_screen_h(void){return H;}
void plat_begin_frame(void){ drawing=1; }
void plat_end_frame(void){ drawing=0; frame_ticks++; }
void plat_clear(uint8_t c){memset(fb,c,sizeof fb);}
static void pix(int x,int y,uint8_t c)
{
    if((unsigned)x<(unsigned)W && (unsigned)y<(unsigned)H) fb[y*W+x]=c;
}
void plat_rect(int x,int y,int w,int h,uint8_t c)
{
    int row;
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(w>W-x) w=W-x;
    if(h>H-y) h=H-y;
    if(w<=0 || h<=0) return;
    for(row=0;row<h;row++)memset(fb+(y+row)*W+x,c,(size_t)w);
}
void plat_line(int x0,int y0,int x1,int y1,uint8_t c)
{
    int dx=x1>=x0?x1-x0:x0-x1;
    int dy=y1>=y0?y1-y0:y0-y1;
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
    int i;
    for(i=0;i<8192;i++){
        int twice;
        pix(x0,y0,c);
        if(x0==x1 && y0==y1)break;
        twice=err*2;
        if(twice>-dy){err-=dy;x0+=sx;}
        if(twice<dx){err+=dx;y0+=sy;}
    }
}
void plat_shade_rect(int x,int y,int w,int h,int level)
{
    int row,col;
    const uint8_t *map;
    if(level<0)level=0;
    if(level>=DOOMO_LIGHT_LEVELS)level=DOOMO_LIGHT_LEVELS-1;
    map=doomo_colormap[level];
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(w>W-x)w=W-x;if(h>H-y)h=H-y;
    if(w<=0||h<=0)return;
    for(row=0;row<h;row++){
        uint8_t *p=fb+(y+row)*W+x;
        for(col=0;col<w;col++)p[col]=map[p[col]];
    }
}
void plat_text(int x,int y,const char *s,uint8_t c)
{
    int bx=x;
    if(!s)return;
    while(*s){
        unsigned ch=(unsigned char)*s++;
        int col,row;
        if(ch=='\n'){x=bx;y+=FONT_LINE;continue;}
        if(ch<FONT_FIRST||ch>FONT_LAST)ch='?';
        for(col=0;col<5;col++){
            uint8_t bits=doomo_font[ch-FONT_FIRST][col];
            for(row=0;row<8;row++)if(bits&(1u<<row))pix(x+col,y+row,c);
        }
        x+=FONT_ADVANCE;
    }
}
void plat_image(int x,int y,const DoomoImage *img,int scale,int flags,uint8_t color)
{
    int iy,ix,dx,dy;
    if(!img||!img->pix||!img->w||!img->h)return;
    if(scale<1)scale=1;if(scale>4)scale=4;
    if(flags&IMG_SHADOW){
        for(iy=0;iy<img->h;iy++)for(ix=0;ix<img->w;ix++){
            int sx=(flags&IMG_FLIPX)?img->w-1-ix:ix;
            if(img->pix[iy*img->w+sx])
                plat_rect(x+ix*scale+scale,y+iy*scale+scale,scale,scale,PAL_GRAY_D);
        }
    }
    for(iy=0;iy<img->h;iy++)for(ix=0;ix<img->w;ix++){
        int sx=(flags&IMG_FLIPX)?img->w-1-ix:ix;
        uint8_t c=img->pix[iy*img->w+sx];
        if(!c)continue;
        if(flags&IMG_SILHOUETTE)c=color;
        if(scale==1)pix(x+ix,y+iy,c);
        else for(dy=0;dy<scale;dy++)for(dx=0;dx<scale;dx++)
            pix(x+ix*scale+dx,y+iy*scale+dy,c);
    }
}
void plat_checker(int x,int y,int w,int h,uint8_t c)
{
    int i,j;
    if(w<=0||h<=0)return;
    for(j=0;j<h;j++)for(i=0;i<w;i++)if(((x+i+y+j)&1)==0)pix(x+i,y+j,c);
}

/* No frame-time allocation. Palette BGR555 -> GS RGBA32 once at boot;
 * the 512x256 DMA surface reserves 512KiB and 512KiB of GS eDRAM. */
int ps2_game2d_init(GSGLOBAL *gs)
{
    unsigned i;
    memset(&picture,0,sizeof picture);
    picture.Width=TEX_W;picture.Height=TEX_H;
    picture.PSM=GS_PSM_CT32;
    picture.Filter=GS_FILTER_NEAREST;
    picture.Mem=upload;
    picture.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(TEX_W,TEX_H,picture.PSM),
                                  GSKIT_ALLOC_USERBUFFER);
    if(picture.Vram==GSKIT_ALLOC_ERROR)return 0;
    for(i=0;i<256;i++){
        uint16_t v=doomo_palette[i];
        uint32_t r=(v&31u)*255u/31u;
        uint32_t g=((v>>5)&31u)*255u/31u;
        uint32_t b=((v>>10)&31u)*255u/31u;
        colors[i]=r|(g<<8)|(b<<16)|0x80000000u;
    }
    gsKit_set_clamp(gs,GS_CMODE_CLAMP);
    return 1;
}
void ps2_game2d_present(GSGLOBAL *gs)
{
    unsigned y,x;
    u64 tint=GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0);
    for(y=0;y<H;y++){
        uint32_t *dst=upload+y*TEX_W;
        const uint8_t *src=fb+y*W;
        for(x=0;x<W;x++)dst[x]=colors[src[x]];
        for(;x<TEX_W;x++)dst[x]=0;
    }
    for(;y<TEX_H;y++)memset(upload+y*TEX_W,0,TEX_W*sizeof(uint32_t));
    gsKit_texture_upload(gs,&picture);
    gsKit_clear(gs,GS_SETREG_RGBAQ(12,9,20,0x80,0));
    gsKit_prim_sprite_texture(gs,&picture,22.0f,0.0f,0.0f,0.0f,
                              618.0f,448.0f,(float)W,(float)H,1,tint);
}

/* Deliberate bring-up limitations: the core still advances the *real*
 * map and player state, but 3D/world geometry needs a proper GS backend.
 * The reference room renderer in core/src/room_ref.c is linked and draws
 * through plat_rect/line/image with correct game positions. */
void plat_view3d(const DoomoView3D *v)
{
    int i;
    if(!v)return;
    plat_rect(v->vx,v->vy,v->vw,v->vh/2,PAL_RAMP(RAMP_SKY,5));
    plat_rect(v->vx,v->vy+v->vh/2,v->vw,v->vh-v->vh/2,PAL_RAMP(RAMP_STONE,4));
    for(i=0;i<v->vw;i+=16)
        plat_line(v->vx+i,v->vy+v->vh/2,v->vx+v->vw/2+(i-v->vw/2)*2,
                  v->vy+v->vh,PAL_RAMP(RAMP_STONE,3));
    plat_text(v->vx+5,v->vy+5,"PS2 3D ADAPTER IN PROGRESS",PAL_WHITE);
}
void plat_overworld(const DoomoOverworldView *v)
{
    int x,y;
    if(!v)return;
    plat_rect(v->vx,v->vy,v->vw,v->vh,PAL_RAMP(RAMP_GRASS,5));
    for(y=v->vy;y<v->vy+v->vh;y+=24)
        for(x=v->vx;x<v->vx+v->vw;x+=24)
            plat_rect(x,y,23,23,((x+y)&24)?PAL_RAMP(RAMP_GRASS,4):PAL_RAMP(RAMP_GRASS,6));
    plat_text(v->vx+5,v->vy+5,"PS2 WORLD ADAPTER IN PROGRESS",PAL_BLACK);
}
void plat_tone(int ch,uint16_t hz,uint8_t vol,uint8_t wave)
{(void)ch;(void)hz;(void)vol;(void)wave;}
void plat_sample(int sfx,uint8_t vol){(void)sfx;(void)vol;}
/* No PS2 memory-card IOP transport yet. Report failure safely, never
 * pretend a save was persisted to EE RAM. */
int plat_save_write(int slot,const void *data,uint32_t size)
{(void)slot;(void)data;(void)size;return 0;}
int plat_save_read(int slot,void *data,uint32_t cap,uint32_t *size)
{(void)slot;(void)data;(void)cap;if(size)*size=0;return 0;}
int plat_save_erase(int slot){(void)slot;return 0;}
uint32_t plat_entropy(void){return 0xCE11A17u;}
uint32_t plat_ticks(void){return frame_ticks;}
void plat_log(const char *s){if(s)printf("[PS2 core] %s\n",s);}
const char *plat_name(void){return "PS2";}
