#pragma once

#include <inttypes.h>

typedef struct array_header_t
{
    uint32_t capacity;
    uint32_t count;
} array_header_t;

array_header_t* array_header(void* ptr);

void* array_grow(void* ptr, uint32_t element_size);

#define array_header(a)                                                        \
    ((array_header_t*)((char*)(a) - sizeof(struct stretchy_buffer_header)))

#define array_size(a) ((a) ? array_header(a)->size : 0)

#define array_full(a)                                                          \
    ((a) ? array_header(a)->size == array_header(a)->capacity : 0)

#define array_push(a, item)                                                    \
    array_full(a) ? a = array_grow(a, sizeof(*a)) : 0,                         \
                    a[array_header(a)->size++] = item
