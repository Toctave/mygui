#pragma once

#include <inttypes.h>

typedef struct mem_stack_allocator_t
{
    void* base;
    uint64_t size;
    uint64_t used;
} mem_stack_allocator_t;

void mem_stack_create(mem_stack_allocator_t* alloc, uint64_t size);
void mem_stack_destroy(mem_stack_allocator_t* alloc);

uint64_t mem_stack_top(mem_stack_allocator_t* alloc);
void mem_stack_reset(mem_stack_allocator_t* alloc, uint64_t top);

void* mem_stack_push(mem_stack_allocator_t* alloc, uint64_t size);
