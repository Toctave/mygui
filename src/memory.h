#pragma once

#include <inttypes.h>

#define MAX_ALIGNMENT 8

// TODO(octave) : should alignment be part of the API ?
typedef struct mem_allocator_api
{
    struct mem_allocator_impl* impl;
    void* (*realloc)(struct mem_allocator_impl* alloc,
                     void* ptr,
                     uint64_t size);
} mem_allocator_api;

typedef struct mem_stack_allocator_t
{
    void* base;
    uint64_t size;
    uint64_t used;
} mem_stack_allocator_t;

void* mem_realloc(mem_allocator_api* alloc, void* ptr, uint64_t size);
void* mem_alloc(mem_allocator_api* alloc, uint64_t size);
void mem_free(mem_allocator_api* alloc, void* ptr);

mem_stack_allocator_t* mem_stack_create(void* buffer, uint64_t size);
void mem_stack_destroy(mem_stack_allocator_t* alloc);

uint64_t mem_stack_top(mem_stack_allocator_t* alloc);
void mem_stack_reset(mem_stack_allocator_t* alloc, uint64_t top);

void* mem_stack_push(mem_stack_allocator_t* alloc, uint64_t size);
