#pragma once

#include "base_types.h"

typedef struct database_o database_o;
typedef struct mem_allocator_i mem_allocator_i;

typedef enum property_type_e
{
    PTYPE_NONE,
    PTYPE_BOOL,
    PTYPE_INTEGER,
    PTYPE_FLOATING,
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

typedef struct database_api
{
    database_o* (*create)(mem_allocator_i* alloc);
    void (*destroy)(database_o* db);

    uint16_t (*add_object_type)(database_o* db,
                                uint32_t property_count,
                                property_definition_t* properties);
    object_id_t (*add_object)(database_o* db, uint16_t type_index);
    void (*delete_object)(database_o* db, object_id_t id);

    double (*get_float)(database_o* db, object_id_t object, const char* name);
    void (*set_float)(database_o* db,
                      object_id_t object,
                      const char* name,
                      double value);
    int64_t (*get_int)(database_o* db, object_id_t id, const char* name);
    void (*set_int)(database_o* db,
                    object_id_t id,
                    const char* name,
                    int64_t value);

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
