#pragma once

#include <inttypes.h>

typedef struct mem_stack_allocator_o mem_stack_allocator_o;

typedef struct mem_allocator_i
{
    struct mem_allocator_impl* impl;
    void* (*realloc)(struct mem_allocator_impl* impl,
                     void* ptr,
                     uint64_t old_size,
                     uint64_t new_size,
                     const char* filename,
                     uint32_t line_number);
} mem_allocator_i;

typedef struct mem_api
{
    mem_allocator_i* std; // C standard lib allocator
    mem_allocator_i* vm;  // Virtual Memory allocator

    mem_stack_allocator_o* (*stack_create)(void* buffer, uint64_t size);
    void (*stack_destroy)(mem_stack_allocator_o* stack);

    uint64_t (*stack_get_cursor)(mem_stack_allocator_o* stack);
    void (*stack_revert)(mem_stack_allocator_o* stack, uint64_t cursor);

    void* (*stack_push)(mem_stack_allocator_o* alloc, uint64_t size);
} mem_api;

#define mem_alloc(a, size)                                                     \
    ((a)->realloc((a)->impl, 0, 0, size, __FILE__, __LINE__))
#define mem_free(a, ptr, size)                                                 \
    ((a)->realloc((a)->impl, ptr, size, 0, __FILE__, __LINE__))
#define mem_realloc(a, ptr, old_size, new_size)                                \
    ((a)->realloc((a)->impl, ptr, old_size, new_size, __FILE__, __LINE__))

