/* fp.h - first-person host (ported from the teaser's s10 raycaster, made
 * reusable): a grid map given by callbacks, textured walls/floor/ceiling with
 * depth fog, projection so any wd_* drawer can be placed as a sprite, the
 * first-person shotgun, the 32-px HUD and lens blood. View is 240x128. */
#ifndef FP_H
#define FP_H
#include <stdint.h>
#define FP_VH 128

typedef struct FpMap {
    char (*cell)(int x, int y);                     /* '.' = open, anything else = wall */
    uint8_t (*wall)(char c, int mx, int my, float u, float v);   /* v 0 floor .. 1 ceiling */
    uint8_t (*floor)(float wx, float wy);
    uint8_t (*ceil)(float wx, float wy);
    uint8_t fog;                                     /* colour things fade into */
    float fog_k;                                     /* fog per unit of depth (0..1) */
} FpMap;

void fp_camera(float x, float y, float yaw, float bob);
void fp_render(const FpMap *m);
/* project a world point at height z (0 floor .. 1 ceiling): screen x, y and
 * pixels per world unit; returns 0 behind the camera or hidden by a wall */
int fp_project(float wx, float wy, float z, float *sx, float *sy, float *scale);
/* point the wd_* camera so local (0,0) lands at the projected point, one
 * local unit = `unit` world units; returns 0 if not visible */
int fp_sprite(float wx, float wy, float z, float unit);
void fp_shotgun(int fire_age, int pump_age, float raise);
typedef struct FpHud { int hearts, shells, pet_look, pet_mood, brutal, sticker_age, sticker_torn_age, horn; const char *weapon, *plate, *mode; } FpHud;
/* the welded saw, first person: idle purr or full scream */
void fp_saw(int f, float rev, float raise, float dip);
void fp_hud(const FpHud *h, int f);
void fp_lens_blood(int age, uint32_t seed);
#endif
