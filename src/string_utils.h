#pragma once

#include <inttypes.h>

typedef struct string_slice_t
{
    const char* start;
    uint64_t size;
} string_slice_t;

void string_slice_print(string_slice_t slice);
bool get_line(const char* text, string_slice_t* line);
