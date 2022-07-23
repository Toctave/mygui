#include "util.h"

#include "memory.h"
#include <stdio.h>

char* vtprintf(mem_api* mem,
               mem_stack_allocator_o* stack,
               const char* fmt,
               va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    int needed = vsnprintf(0, 0, fmt, args) + 1;

    char* result = mem->stack_push(stack, needed);

    vsnprintf(result, needed, fmt, args_copy);

    return result;
}

char* tprintf(mem_api* mem, mem_stack_allocator_o* stack, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char* result = vtprintf(mem, stack, fmt, args);

    va_end(args);

    return result;
}

quad_i32_t quad_i32_grown(quad_i32_t q, int32_t offset)
{
    return (quad_i32_t){
        .min = {q.min[0] - offset, q.min[1] - offset},
        .extent = {q.extent[0] + 2 * offset, q.extent[1] + 2 * offset}};
}

