#pragma once
#include <stddef.h>
#include <stdint.h>
constexpr uint32_t MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2;
extern size_t fakeFreeHeap, fakeLargestBlock;
inline size_t heap_caps_get_free_size(uint32_t) { return fakeFreeHeap; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return fakeLargestBlock; }
