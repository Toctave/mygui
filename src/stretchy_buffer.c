#include "stretchy_buffer.h"

#include "memory.h"

array_header_t* array_header(void* ptr)
{
    return (array_header_t*)((char*)(ptr) - sizeof(array_header_t));
}

void array_free_(mem_allocator_i* alloc, void* ptr, uint32_t element_size)
{
    array_header_t* header = array_header(ptr);
    mem_free(alloc,
             header,
             sizeof(array_header_t) + element_size * header->capacity);
}

void* array_reserve_(mem_allocator_i* alloc,
                     void* ptr,
                     uint32_t element_size,
                     uint32_t required)
{
    if (!required)
    {
        return;
    }

    if (!ptr)
    {
        uint32_t min_capacity = 16;
        if (required < min_capacity)
        {
            required = min_capacity;
        }

        void* bytes =
            mem_alloc(alloc, sizeof(array_header_t) + element_size * required);

        void* result = ((uint8_t*)bytes + sizeof(array_header_t));

        array_header_t* header = bytes;
        header->capacity = required;
        header->count = 0;

        return result;
    }
    else
    {
        array_header_t* old_header = array_header(ptr);
        uint32_t old_capacity = old_header->capacity;
        ASSERT(old_capacity);

        uint32_t old_count = old_header->count;

        if (required <= old_capacity)
        {
            return ptr;
        }
        else
        {
            uint32_t new_capacity = old_capacity;
            while (new_capacity < required)
            {
                new_capacity = (new_capacity * 3) / 2;
            }
            array_header_t* new_header = mem_realloc(
                alloc,
                old_header,
                sizeof(array_header_t) + element_size * old_capacity,
                sizeof(array_header_t) + element_size * new_capacity);

            new_header->count = old_count;
            new_header->capacity = new_capacity;

            return new_header + 1;
        }
    }
}
