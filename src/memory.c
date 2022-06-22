#include "memory.h"

#include "assert.h"
#include "platform.h"

void mem_stack_create(mem_stack_allocator_t* alloc, uint64_t size)
{
    alloc->size = size;
    alloc->used = 0;
    alloc->base = platform_virtual_alloc(size);
}

void mem_stack_destroy(mem_stack_allocator_t* alloc)
{
    platform_virtual_free(alloc->base, alloc->size);
    alloc->base = 0;
    alloc->size = 0;
    alloc->used = 0;
}

uint64_t mem_stack_top(mem_stack_allocator_t* alloc) { return alloc->used; }

void mem_stack_reset(mem_stack_allocator_t* alloc, uint64_t top)
{
    ASSERT(top <= alloc->used);
    alloc->used = top;
}

void* mem_stack_push(mem_stack_allocator_t* alloc, uint64_t size)
{
    ASSERT(alloc->used + size < alloc->size);
    void* result = (uint8_t*)alloc->base + alloc->used;

    alloc->used += size;

    return result;
}

