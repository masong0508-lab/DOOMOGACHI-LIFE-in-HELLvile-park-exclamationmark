/* items.h - everything you can buy, find, gift, eat or weld.
 * Item ids are stored in saves (inventory counts). Append only. */
#ifndef DOOMO_ITEMS_H
#define DOOMO_ITEMS_H

#include "types.h"

enum {
    /* food (feeding residents / the pet) */
    ITEM_NONE = 0,
    ITEM_CUPCAKE, ITEM_ONIGIRI, ITEM_SOUL_SOUP, ITEM_BRIMSTONE_BAGEL,
    ITEM_CANDY_EYEBALLS, ITEM_PUDDING, ITEM_HELL_PEPPERS, ITEM_ANGEL_CAKE,
    ITEM_MEAT_SLAB, ITEM_SALAD, ITEM_MILK, ITEM_COFFEE,
    /* gifts & fun */
    ITEM_PLUSHIE, ITEM_FLOWERS, ITEM_LOVE_LETTER, ITEM_VIDEO_GAME,
    ITEM_SKULL_TRINKET, ITEM_PERFUME, ITEM_BOARD_GAME, ITEM_RING,
    /* care */
    ITEM_SOAP, ITEM_MEDICINE, ITEM_ENERGY_DRINK, ITEM_CHILL_PILL,
    /* exorcism tools */
    ITEM_HOLY_WATER, ITEM_SAGE_BUNDLE, ITEM_PLUSH_CROSS, ITEM_ROSARY,
    ITEM_BLESSED_SALT, ITEM_HYMN_BOOK,
    /* welding materials */
    ITEM_SCRAP, ITEM_GEARS, ITEM_DEMON_HORN, ITEM_GLITTER, ITEM_HELLSTEEL,
    ITEM_PLUSH_STUFFING, ITEM_WELD_ROD, ITEM_BLUEPRINT,
    /* story / key items */
    ITEM_HOUSE_KEY, ITEM_DOOMOGACHI_EGG, ITEM_LOST_BUNNY, ITEM_VAPE_PEN,
    ITEM_CELLAR_KEY, ITEM_GATE_SIGIL,
    /* (fishcook, additive) Brimstone Bites ingredients + dishes; rows in
     * core/src/items_ext.c. The first four line up with doomo_food_names
     * 44..47 (item - ITEM_CUPCAKE), so likes/hates work for them. */
    ITEM_BLOOD_ORANGE, ITEM_PICKLES, ITEM_SPROUTS, ITEM_BONE_BROTH,
    ITEM_CLOUD_FLOUR, ITEM_PIXIE_SUGAR, ITEM_SPECKLED_EGG, ITEM_CHARRED,
    ITEM_FISH_RAMEN, ITEM_TEMPURA_ROLL, ITEM_TAKOYAKI, ITEM_PANCAKES,
    ITEM_LAVA_CAKE, ITEM_CURRY, ITEM_PICKLE_PIZZA, ITEM_GRAVE_CAKE,
    ITEM_EYEBALL_JELLY, ITEM_SOCK_STEW,
    ITEM_COUNT
};
DOOMO_STATIC_ASSERT(ITEM_COUNT <= 64, items_fit_inventory);

enum { ICAT_FOOD, ICAT_GIFT, ICAT_CARE, ICAT_EXORCISM, ICAT_MATERIAL,
       ICAT_KEY };

typedef struct DoomoItemInfo {
    const char *name;
    const char *desc;     /* one short line                         */
    uint16_t price;       /* H$; 0 = not sold                        */
    uint8_t  category;    /* ICAT_*                                  */
    int8_t   hunger, happy, hygiene, energy, health, sanity, corruption;
                          /* stat deltas when used on a resident     */
    uint8_t  holy;        /* exorcism power 0..100                   */
    uint8_t  icon;        /* ICON_* glyph from font.h                */
} DoomoItemInfo;

extern const DoomoItemInfo doomo_items[ITEM_COUNT];

#endif
