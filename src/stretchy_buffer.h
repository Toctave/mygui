#pragma once

#include <inttypes.h>

typedef struct array_header_t
{
    uint32_t capacity;
    uint32_t count;
} array_header_t;

array_header_t* array_header(void* ptr);

typedef struct mem_allocator_i mem_allocator_i;

void* array_grow(mem_allocator_i* alloc, void* ptr, uint32_t element_size);

#define array_count(a) ((a) ? array_header(a)->count : 0)

#define array_full(a)                                                          \
    ((a) ? array_header(a)->count == array_header(a)->capacity : 1)

#define array_push(alloc, a, item)                                             \
    array_full(a) ? a = array_grow(alloc, a, sizeof(*a)) : 0,                  \
                    a[array_header(a)->count++] = item
