/* street.c - Home Row addressing and length rules (street.h; NEIGHBOURHOOD_ROADMAP M0). */
#include "doomo/doomo.h"
#include "doomo/street.h"

static int valid_slot(int slot) { return slot >= 0 && slot < DOOMO_MAX_RESIDENTS; }

int street_district(int slot) { return valid_slot(slot) ? slot / DOOMO_DISTRICT_SIZE : -1; }
int street_index(int slot)    { return valid_slot(slot) ? slot % DOOMO_DISTRICT_SIZE : -1; }
int street_chunk_of(int slot) { return valid_slot(slot) ? street_index(slot) / STREET_LOTS_PER_CHUNK : -1; }
int street_lot_of(int slot)   { return valid_slot(slot) ? street_index(slot) % STREET_LOTS_PER_CHUNK : -1; }
int street_side(int slot)     { return valid_slot(slot) ? (street_lot_of(slot) & 1) : -1; }
int street_block(int slot)    { return valid_slot(slot) ? street_index(slot) / 10 + 1 : -1; }
int street_room(int slot)     { return valid_slot(slot) ? street_index(slot) % 10 + 1 : -1; }

int street_slot(int district, int chunk, int lot)
{
    if (district < 0 || district >= STREET_DISTRICTS) return -1;
    if (chunk < 0 || chunk >= STREET_CHUNKS_MAX) return -1;
    if (lot < 0 || lot >= STREET_LOTS_PER_CHUNK) return -1;
    return district * DOOMO_DISTRICT_SIZE + chunk * STREET_LOTS_PER_CHUNK + lot;
}

int street_lots_for(int extent)
{
    int n;
    if (extent < 0) extent = 0;
    n = extent + STREET_FREE_LOTS;
    return n > DOOMO_DISTRICT_SIZE ? DOOMO_DISTRICT_SIZE : n;
}

int street_chunks_for(int extent)
{
    int n = (street_lots_for(extent) + STREET_LOTS_PER_CHUNK - 1) / STREET_LOTS_PER_CHUNK;
    return n < 1 ? 1 : n;
}

int street_extent(int district)
{
    int i, base;
    if (district < 0 || district >= STREET_DISTRICTS) return 0;
    base = district * DOOMO_DISTRICT_SIZE;
    for (i = DOOMO_DISTRICT_SIZE - 1; i >= 0; i--)
        if (doomo_town.res[base + i].status != RES_EMPTY) return i + 1;
    return 0;
}

int street_lots(int district)   { return street_lots_for(street_extent(district)); }
int street_chunks(int district) { return street_chunks_for(street_extent(district)); }

int street_free_lots(int district)
{
    int i, base, n = 0, lots;
    if (district < 0 || district >= STREET_DISTRICTS) return 0;
    base = district * DOOMO_DISTRICT_SIZE;
    lots = street_lots(district);
    for (i = 0; i < lots; i++)
        if (doomo_town.res[base + i].status == RES_EMPTY) n++;
    return n;
}

int street_is_full(int district) { return street_extent(district) == DOOMO_DISTRICT_SIZE && street_free_lots(district) == 0; }
