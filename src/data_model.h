#pragma once

#include <inttypes.h>
#include <stdbool.h>

typedef enum property_type_e
{
    PTYPE_NONE,
    PTYPE_BOOL,
    PTYPE_INT64,
    PTYPE_FLOAT64,
    PTYPE_BUFFER,
} property_type_e;

typedef union property_value_t
{
    bool boolean;
    int64_t integer;
    double floating;
    struct
    {
        uint64_t size;
        void* data;
    } buffer;
} property_value_t;

