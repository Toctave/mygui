#include "memory.h"

#include "assert.h"
#include "platform.h"
#include "plugin_sdk.h"

#include <stdlib.h>

static void* load();

plugin_spec_t PLUGIN_SPEC = {
    .name = "memory",
    .version = {1, 0, 0},
    .load = load,
    .unload = 0,
};

#define MAX_ALIGNMENT 8

static mem_api mem;
static mem_allocator_i std;
static mem_allocator_i vm;

static void* std_realloc(struct mem_allocator_impl* impl,
                         void* ptr,
                         uint64_t old_size,
                         uint64_t new_size,
                         const char* filename,
                         uint32_t line_number)
{
    (void)old_size;
    (void)filename;
    (void)line_number;
    ASSERT(!impl);
    return realloc(ptr, new_size);
}

static void* vm_realloc(struct mem_allocator_impl* impl,
                        void* ptr,
                        uint64_t old_size,
                        uint64_t new_size,
                        const char* filename,
                        uint32_t line_number)
{
    ASSERT(!impl);
    (void)filename;
    (void)line_number;

    if (ptr)
    {
        platform_virtual_free(ptr, old_size);
    }

    if (new_size)
    {
        ptr = platform_virtual_alloc(new_size);
    }
    else
    {
        ptr = 0;
    }

    return ptr;
}

struct mem_stack_allocator_o
{
    void* base;
    uint64_t size;
    uint64_t used;
};

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

static mem_stack_allocator_o* stack_create(void* buffer, uint64_t size)
{
    mem_stack_allocator_o* stack = buffer;

    stack->base =
        shift_align_up(buffer, sizeof(mem_stack_allocator_o), MAX_ALIGNMENT);
    stack->size = (uint8_t*)buffer + size - (uint8_t*)stack->base;
    stack->used = 0;

    return stack;
}

static void stack_destroy(mem_stack_allocator_o* stack)
{
    stack->base = 0;
    stack->size = 0;
    stack->used = 0;
}

static uint64_t stack_get_cursor(mem_stack_allocator_o* alloc)
{
    return alloc->used;
}

static void stack_revert(mem_stack_allocator_o* alloc, uint64_t cursor)
{
    ASSERT(cursor <= alloc->used);
    alloc->used = cursor;
}

static void* stack_push(mem_stack_allocator_o* alloc, uint64_t size)
{
    ASSERT(alloc->used + size < alloc->size);
    void* result = (uint8_t*)alloc->base + alloc->used;

    alloc->used += size;

    return result;
}

static void* load()
{
    std.realloc = std_realloc;
    vm.realloc = vm_realloc;

    mem.std = &std;
    mem.vm = &vm;

    mem.stack_create = stack_create;
    mem.stack_destroy = stack_destroy;
    mem.stack_get_cursor = stack_get_cursor;
    mem.stack_revert = stack_revert;
    mem.stack_push = stack_push;

    return &mem;
}
