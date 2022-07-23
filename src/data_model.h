#pragma once

#include "base_types.h"

typedef struct database_o database_o;
typedef struct mem_allocator_i mem_allocator_i;

#define FOR_ALL_BASE_PROPERTY_TYPES(X)                                         \
    X(BOOL, bool, bool)                                                        \
    X(INT8, int8, int8_t)                                                      \
    X(INT16, int16, int16_t)                                                   \
    X(INT32, int32, int32_t)                                                   \
    X(INT64, int64, int64_t)                                                   \
    X(UINT8, uint8, uint8_t)                                                   \
    X(UINT16, uint16, uint16_t)                                                \
    X(UINT32, uint32, uint32_t)                                                \
    X(UINT64, uint64, uint64_t)                                                \
    X(FLOAT32, float32, float)                                                 \
    X(FLOAT64, float64, double)

#define DO_BASE_TYPE_ENUM(upper, lower, type) PTYPE_##upper,

typedef enum property_type_e
{
    PTYPE_NONE,

    // clang-format off
    FOR_ALL_BASE_PROPERTY_TYPES(DO_BASE_TYPE_ENUM)
    // clang-format on

    PTYPE_BUFFER,
    PTYPE_OBJECT,
    PTYPE_REFERENCE,
} property_type_e;

typedef struct property_definition_t
{
    char name[32];
    uint16_t type;
    uint16_t object_type;
} property_definition_t;

typedef union object_id_t
{
    uint64_t index;
    struct
    {
        // order is important here : type == 0 means that the slot
        // is free

        // TODO(octave) : investigate bit share of each field. Are 64K
        // generations enough ? Create a test case.
        uint32_t slot;
        uint16_t type;
        uint16_t generation;
    } info;
} object_id_t;

#define DO_DECLARE_GETTER_SETTER(upper, lower, type)                           \
    type (                                                                     \
        *get_##lower)(database_o * db, object_id_t object, const char* name);  \
    void (*set_##lower)(database_o * db,                                       \
                        object_id_t object,                                    \
                        const char* name,                                      \
                        type value);

typedef struct database_api
{
    database_o* (*create)(mem_allocator_i* alloc);
    void (*destroy)(database_o* db);

    uint16_t (*add_object_type)(database_o* db,
                                uint32_t property_count,
                                property_definition_t* properties);
    object_id_t (*create_object)(database_o* db, uint16_t type_index);
    void (*destroy_object)(database_o* db, object_id_t id);

    FOR_ALL_BASE_PROPERTY_TYPES(DO_DECLARE_GETTER_SETTER)

    object_id_t (*get_sub_object)(database_o* db,
                                  object_id_t id,
                                  const char* name);

    void (*set_reference)(database_o* db,
                          object_id_t id,
                          const char* name,
                          object_id_t value);
    object_id_t (*get_reference)(database_o* db,
                                 object_id_t id,
                                 const char* name);

    bool (*reallocate_buffer)(database_o* db,
                              object_id_t id,
                              const char* name,
                              uint64_t size);
    bool (*get_buffer_data)(database_o* db,
                            object_id_t id,
                            const char* name,
                            uint64_t offset,
                            uint64_t size,
                            void* data);
    bool (*set_buffer_data)(database_o* db,
                            object_id_t id,
                            const char* name,
                            uint64_t offset,
                            uint64_t size,
                            const void* data);
} database_api;
