#pragma once

#include "base_types.h"

typedef struct array_header_t
{
    uint32_t capacity;
    uint32_t count;
} array_header_t;

array_header_t* array_header(void* ptr);

typedef struct mem_allocator_i mem_allocator_i;

void* array_reserve_(mem_allocator_i* alloc,
                     void* ptr,
                     uint32_t element_size,
                     uint32_t required);

void array_free_(mem_allocator_i* alloc, void* ptr, uint32_t element_size);

#define array_free(alloc, a) array_free_(alloc, a, sizeof(*a))

#define array_count(a) ((a) ? array_header(a)->count : 0)

#define array_reserve(alloc, a, capacity)                                      \
    (a = array_reserve_(alloc, a, sizeof(*a), capacity))

#define array_push(alloc, a, item)                                             \
    do                                                                         \
    {                                                                          \
        array_reserve(alloc, a, array_count(a) + 1);                           \
        a[array_header(a)->count++] = item;                                    \
    } while (0)

#define array_safe_get(a, index) (index < array_count(a) ? &a[index] : 0)
