#pragma once

#include <stdint.h>

/**
 * Sets the lower n bits of an 8-bit unsigned integer to 1 and returns the result.
 * @param n The number of bits to set to 1. Must be between 0 and 8 (inclusive).
 * @return The resulting 8-bit unsigned integer with the first n bits set to 1.
 * */
uint8_t set_bits8(uint8_t n);
/**
 * Sets the lower n bits of a 16-bit unsigned integer to 1 and returns the result.
 * @param n The number of bits to set to 1. Must be between 0 and 16 (inclusive).
 * @return The resulting 16-bit unsigned integer with the first n bits set to 1.
 * */
uint16_t set_bits16(uint8_t n);
/**
 * Sets the lower n bits of a 32-bit unsigned integer to 1 and returns the result.
 * @param n The number of bits to set to 1. Must be between 0 and 32 (inclusive).
 * @return The resulting 32-bit unsigned integer with the first n bits set to 1.
 * */
uint32_t set_bits32(uint8_t n);
/**
 * Sets the lower n bits of a 64-bit unsigned integer to 1 and returns the result.
 * @param n The number of bits to set to 1. Must be between 0 and 64 (inclusive).
 * @return The resulting 64-bit unsigned integer with the first n bits set to 1.
 * */
uint64_t set_bits64(uint8_t n);





/**
 * Allocates the first available bit in a bitmap.
 * @param _bitmap Pointer to the bitmap.
 * @param BITMAP_SIZE_IN_BYTES The size of the bitmap in bytes.
 * @return The index of the allocated bit, or MAX_UINT32 if no bits are available.
 * */
uint32_t bitmap_alloc64(void* _bitmap, uint32_t BITMAP_SIZE_IN_BYTES);

/**
 * Allocates the first available bit in a bitmap.
 * @param _bitmap Pointer to the bitmap.
 * @param BITMAP_SIZE_IN_BYTES The size of the bitmap in bytes.
 * @return The index of the allocated bit, or MAX_UINT32 if no bits are available.
 * */
uint32_t bitmap_alloc32(void* _bitmap, uint32_t BITMAP_SIZE_IN_BYTES);

/**
 * Frees a bit in a bitmap.
 * @param _bitmap Pointer to the bitmap.
 * @param index The index of the bit to free.
 * @param BITMAP_SIZE_IN_BYTES The size of the bitmap in bytes.
 * @return 0 if the bit was previously set, 1 if it was already free, or -1 if the index is out of range.
 * */
int bitmap_free(void* _bitmap, uint32_t index, uint32_t BITMAP_SIZE_IN_BYTES);

/**
 * Gets the value of a bit in a bitmap.
 * @param _bitmap Pointer to the bitmap.
 * @param index The index of the bit to get.
 * @param BITMAP_SIZE_IN_BYTES The size of the bitmap in bytes.
 * @return 1 if the bit is set, 0 if it is not set, or -1 if the index is out of range.
 * */
int bitmap_get(void* _bitmap, uint32_t index, uint32_t BITMAP_SIZE_IN_BYTES);




uint32_t pad_multiple_of(uint32_t val_to_pad, uint32_t multiple_of);