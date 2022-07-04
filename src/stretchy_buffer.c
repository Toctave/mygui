#include "stretchy_buffer.h"

#include "memory.h"

array_header_t* array_header(void* ptr)
{
    return (array_header_t*)((char*)(ptr) - sizeof(array_header_t));
}

void* array_grow(mem_allocator_i* alloc, void* ptr, uint32_t element_size)
{
    if (!ptr)
    {
        uint32_t initial_capacity = 16;
        void* bytes =
            mem_alloc(alloc,
                      sizeof(array_header_t) + element_size * initial_capacity);

        void* result = ((uint8_t*)bytes + sizeof(array_header_t));

        array_header_t* header = bytes;
        header->capacity = initial_capacity;
        header->count = 0;

        return result;
    }
    else
    {
        array_header_t* old_header = array_header(ptr);
        uint32_t old_capacity = old_header->capacity;
        uint32_t old_count = old_header->count;

        uint32_t new_capacity = old_capacity * 2;
        array_header_t* new_header =
            mem_realloc(alloc,
                        old_header,
                        sizeof(array_header_t) + element_size * old_capacity,
                        sizeof(array_header_t) + element_size * new_capacity);

        new_header->count = old_count;
        new_header->capacity = new_capacity;

        return new_header + 1;
    }
}
