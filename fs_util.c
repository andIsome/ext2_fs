#include "fs_util.h"


uint8_t set_bits8(uint8_t n){
    return n>=8 ? ~(uint8_t)0 : ((uint8_t)1 << n) - 1;
}

uint16_t set_bits16(uint8_t n){
    return n>=16 ? ~(uint16_t)0 : ((uint16_t)1 << n) - 1;
}

uint32_t set_bits32(uint8_t n){
    return n>=32 ? ~(uint32_t)0 : ((uint32_t)1 << n) - 1;
}

uint64_t set_bits64(uint8_t n){
    return n>=64 ? ~(uint64_t)0 : ((uint64_t)1 << n) - 1;
}




uint32_t bitmap_alloc64(void* _bitmap, const uint32_t BITMAP_SIZE_IN_BYTES){

    const uint32_t ELEMENTS_PER_BLOCK = BITMAP_SIZE_IN_BYTES/sizeof(uint64_t);
    uint64_t* bitmap = (uint64_t*)_bitmap;

    uint32_t c = 0;
    // Find first element which doesn't have all bits set
    while(bitmap[c] == ~((uint64_t)0)){
        // Completely full
        if(c >= ELEMENTS_PER_BLOCK) return ~((uint32_t)0);

        c++; // :D
    }

    uint64_t bitmap_qw = bitmap[c];

    // Get the index of the first bit set to 0
    uint32_t nth_bit = __builtin_ctzll(~bitmap_qw);

    // Set the bit
    bitmap[c] |= 1 << nth_bit;

    return c * 8 * sizeof(uint64_t) + nth_bit;
}

uint32_t bitmap_alloc32(void* _bitmap, const uint32_t BITMAP_SIZE_IN_BYTES){

    const uint32_t ELEMENTS_PER_BLOCK = BITMAP_SIZE_IN_BYTES/sizeof(uint32_t);
    uint32_t* bitmap = (uint32_t*)_bitmap;

    uint32_t c = 0;
    // Find first element which doesn't have all bits set
    while(bitmap[c] == ~((uint32_t)0)){
        // Completely full
        if(c >= ELEMENTS_PER_BLOCK) return ~((uint32_t)0);

        c++;
    }

    uint32_t bitmap_qw = bitmap[c];

    // Get the index of the first bit set to 0
    uint32_t nth_bit = __builtin_ctz(~bitmap_qw);

    // Set the bit
    bitmap[c] |= 1 << nth_bit;

    return c * 8 * sizeof(uint32_t) + nth_bit;
}

int bitmap_free(void* _bitmap, uint32_t index, const uint32_t BITMAP_SIZE_IN_BYTES){
    const uint32_t BITS_PER_BLOCK = BITMAP_SIZE_IN_BYTES * 8;

    if(index >= BITS_PER_BLOCK)
        return -1;

    uint32_t* bitmap = (uint32_t*)_bitmap;

    uint32_t byte_num = index / 32;
    uint32_t bit_num =  index % 32;

    uint32_t mask = (1 << bit_num);

    int ret = !(bitmap[byte_num] & mask);

    bitmap[byte_num] &= ~mask;

    return ret;
}

int bitmap_get(void* _bitmap, uint32_t index, const uint32_t BITMAP_SIZE_IN_BYTES){
    const uint32_t BITS_PER_BLOCK = BITMAP_SIZE_IN_BYTES * 8;

    if(index >= BITS_PER_BLOCK)
        return -1;

    uint32_t* bitmap = (uint32_t*)_bitmap;

    uint32_t byte_num = index / 32;
    uint32_t bit_num  = index % 32;


    uint32_t mask = (1 << bit_num);

    return bitmap[byte_num] & mask ? 1 : 0;
}




uint32_t pad_multiple_of(uint32_t val_to_pad, uint32_t multiple_of){
    if(val_to_pad % multiple_of != 0){
        return val_to_pad + (multiple_of - val_to_pad % multiple_of);
    }
    return val_to_pad;
}