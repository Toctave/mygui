#include "memory.h"

#include "assert.h"
#include "platform.h"

static void* align_up(void* ptr, uint64_t alignment)
{
    uint64_t addr = (uint64_t)ptr;
    uint64_t aligned = alignment * ((addr + alignment - 1) / alignment);

    return (void*)aligned;
}

static void* shift_up(void* ptr, uint64_t bytes)
{
    uint64_t addr = (uint64_t)ptr;

    return (void*)(addr + bytes);
}

static void* shift_align_up(void* ptr, uint64_t bytes, uint64_t alignment)
{
    return align_up(shift_up(ptr, bytes), alignment);
}

void* mem_realloc(mem_allocator_api* alloc, void* ptr, uint64_t size)
{
    ASSERT(alloc->realloc);
    return alloc->realloc(alloc->impl, ptr, size);
}

void* mem_alloc(mem_allocator_api* alloc, uint64_t size)
{
    return mem_realloc(alloc, 0, size);
}

void mem_free(mem_allocator_api* alloc, void* ptr)
{
    void* result = mem_realloc(alloc, ptr, 0);
    (void)result;
    ASSERT(!result);
}

mem_stack_allocator_t* mem_stack_create(void* buffer, uint64_t size)
{
    mem_stack_allocator_t* stack = buffer;

    stack->base =
        shift_align_up(buffer, sizeof(mem_stack_allocator_t), MAX_ALIGNMENT);
    stack->size = (uint8_t*)buffer + size - (uint8_t*)stack->base;
    stack->used = 0;

    return stack;
}

void mem_stack_destroy(mem_stack_allocator_t* stack)
{
    stack->base = 0;
    stack->size = 0;
    stack->used = 0;
}

uint64_t mem_stack_get_cursor(mem_stack_allocator_t* alloc)
{
    return alloc->used;
}

void mem_stack_revert(mem_stack_allocator_t* alloc, uint64_t cursor)
{
    ASSERT(cursor <= alloc->used);
    alloc->used = cursor;
}

void* mem_stack_push(mem_stack_allocator_t* alloc, uint64_t size)
{
    ASSERT(alloc->used + size < alloc->size);
    void* result = (uint8_t*)alloc->base + alloc->used;

    alloc->used += size;

    return result;
}

