#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "bitmap.h"

int bitmap_get(void* bm, int ii) {
    assert(ii >= 0);
    int groupIdx = ii / sizeof(uint8_t);
    int bitIdx = ii % sizeof(uint8_t);
    uint8_t mask = 1 << bitIdx;

    return (((uint8_t*)bm)[groupIdx] & mask) >> bitIdx;
}

void bitmap_put(void* bm, int ii, int vv) {
    assert(vv == 1 || vv == 0);
    assert(ii >= 0);
    int groupIdx = ii / sizeof(uint8_t);
    int bitIdx = ii % sizeof(uint8_t);
    uint8_t mask = 1 << bitIdx;

    if (vv == 0) {
        mask = ~mask;
        ((uint8_t*)bm)[groupIdx] &= mask;
    } else {
        ((uint8_t*)bm)[groupIdx] |= mask;
    }
}

void bitmap_print(void* bm, int size) {
    assert(size % sizeof(uint8_t) == 0);
    uint8_t* map = (uint8_t*)bm;
    for (uint8_t* ii = map; ii < map + size / sizeof(uint8_t); ii++) {
        printf("%d\n", *map);
    }
}