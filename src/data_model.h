#pragma once

#include <inttypes.h>
#include <stdbool.h>

typedef struct mem_allocator_i mem_allocator_i;

typedef struct buffer_t
{
    uint64_t size;
    void* data;
} buffer_t;

typedef enum property_type_e
{
    PTYPE_NONE,
    PTYPE_BOOL,
    PTYPE_INT64,
    PTYPE_FLOAT64,
    PTYPE_BUFFER,
} property_type_e;

typedef struct property_definition_t
{
    char name[32];
    uint32_t type;
} property_definition_t;

typedef struct property_layout_t
{
    property_definition_t def;
    uint32_t offset;
} property_layout_t;

typedef struct object_type_definition_t
{
    uint32_t first_property;
    uint32_t property_count;
    uint32_t bytes;
} object_type_definition_t;

typedef union object_slot_t object_slot_t;

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

typedef struct database_t
{
    mem_allocator_i* alloc;
    /* array */ property_layout_t* properties;
    /* array */ object_type_definition_t* object_types;
    /* array */ object_slot_t* objects;
} database_t;

void db_init(database_t* db, mem_allocator_i* alloc);

uint16_t add_object_type(database_t* db,
                         uint32_t property_count,
                         property_definition_t* properties);
object_id_t add_object(database_t* db, uint16_t type_index);
void delete_object(database_t* db, object_id_t id);

double get_float(database_t* db, object_id_t object, const char* name);
void set_float(database_t* db,
               object_id_t object,
               const char* name,
               double value);
int64_t get_int(database_t* db, object_id_t id, const char* name);
void set_int(database_t* db, object_id_t id, const char* name, int64_t value);

bool get_buffer_data(database_t* db,
                     object_id_t id,
                     const char* name,
                     uint64_t offset,
                     uint64_t size,
                     void* data);
bool set_buffer_data(database_t* db,
                     object_id_t id,
                     const char* name,
                     uint64_t offset,
                     uint64_t size,
                     const void* data);
bool reallocate_buffer(database_t* db,
                       object_id_t id,
                       const char* name,
                       uint64_t size);
