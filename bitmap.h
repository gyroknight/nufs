// based on cs3650 starter code

#ifndef BITMAP_H
#define BITMAP_H

/**
 * @brief Gets the value of a bit in a bitmap.
 * 
 * @param bm the beginning of the bitmap
 * @param ii the index of the bit
 * @return int the value of the bit, either 1 or 0
 */
int bitmap_get(void* bm, int ii);

/**
 * @brief Sets the value of a bit in a bitmap.
 * 
 * @param bm the beginning of the bitmap
 * @param ii the index of the bit
 * @param vv the value to set it to
 */
void bitmap_put(void* bm, int ii, int vv);

/**
 * @brief Prints a bitmap.
 * 
 * @param bm the beginning of the bitmap
 * @param size the size of the bitmap
 */
void bitmap_print(void* bm, int size);

#endif
