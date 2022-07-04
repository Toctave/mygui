#include "data_model.h"
#include "assert.h"
#include "memory.h"
#include "stretchy_buffer.h"

#include <string.h>

typedef struct object_t
{
    object_id_t id;
    void* data;
} object_t;

union object_slot_t
{
    uint32_t next_free;
    object_t object;
};

void db_init(database_t* db, mem_allocator_i* alloc)
{
    *db = (database_t){0};
    db->alloc = alloc;

    array_push(db->alloc, db->object_types, (object_type_definition_t){0});
    array_push(db->alloc, db->properties, (property_layout_t){0});
    array_push(db->alloc, db->objects, (object_slot_t){0});
}

static uint32_t property_size(const property_definition_t* property)
{
    switch (property->type)
    {
    case PTYPE_BOOL:
        return 1;
    case PTYPE_INT64:
        return 8;
    case PTYPE_FLOAT64:
        return 8;
    case PTYPE_BUFFER:
        return sizeof(buffer_t);
    }
    ASSERT_MSG(false, "Unknown property type %u", property->type);
    return 0;
}

uint16_t add_object_type(database_t* db,
                         uint32_t property_count,
                         property_definition_t* properties)
{
    object_type_definition_t def;
    def.first_property = array_count(db->properties);
    def.property_count = property_count;

    // TODO(octave) : handle alignment
    uint32_t offset = 0;
    for (uint32_t i = 0; i < property_count; i++)
    {
        property_layout_t layout;
        layout.def = properties[i];
        layout.offset = offset;

        array_push(db->alloc, db->properties, layout);
        offset += property_size(&properties[i]);
    }
    def.bytes = offset;

    array_push(db->alloc, db->object_types, def);

    ASSERT(array_count(db->object_types) - 1 <= UINT16_MAX);

    return array_count(db->object_types) - 1;
}

object_t* get_object(database_t* db, object_id_t id)
{
    ASSERT(id.info.slot < array_count(db->objects));

    object_t* object = &db->objects[id.info.slot].object;

    ASSERT(object->id.info.generation >= id.info.generation);
    if (object->id.info.generation > id.info.generation
        || !object->id.info.type)
    {
        return 0;
    }

    ASSERT(id.info.type == object->id.info.type);
    ASSERT(id.info.slot == object->id.info.slot);

    return object;
}

static const property_layout_t*
get_property_by_name(database_t* db, uint32_t type_index, const char* prop_name)
{
    ASSERT(type_index < array_count(db->object_types));
    const object_type_definition_t* type = &db->object_types[type_index];
    for (uint32_t i = 0; i < type->property_count; i++)
    {
        const property_layout_t* prop =
            &db->properties[type->first_property + i];

        if (!strncmp(prop->def.name, prop_name, sizeof(prop->def.name)))
        {
            return prop;
        }
    }

    return 0;
}

void* get_property_ptr(database_t* db,
                       object_id_t id,
                       uint32_t property_type,
                       const char* name)
{
    const property_layout_t* prop =
        get_property_by_name(db, id.info.type, name);

    if (!prop || prop->def.type != property_type)
    {
        return 0;
    }

    object_t* object = get_object(db, id);
    if (object)
    {
        return (uint8_t*)object->data + prop->offset;
    }
    else
    {
        return 0;
    }
}

double get_float(database_t* db, object_id_t id, const char* name)
{
    double* ptr = get_property_ptr(db, id, PTYPE_FLOAT64, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
        return 0.;
    }
    else
    {
        return *ptr;
    }
}

void set_float(database_t* db, object_id_t id, const char* name, double value)
{
    double* ptr = get_property_ptr(db, id, PTYPE_FLOAT64, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
    }
    else
    {
        *ptr = value;
    }
}

int64_t get_int(database_t* db, object_id_t id, const char* name)
{
    int64_t* ptr = get_property_ptr(db, id, PTYPE_INT64, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
        return 0;
    }
    else
    {
        return *ptr;
    }
}

void set_int(database_t* db, object_id_t id, const char* name, int64_t value)
{
    int64_t* ptr = get_property_ptr(db, id, PTYPE_INT64, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
    }
    else
    {
        *ptr = value;
    }
}

bool reallocate_buffer(database_t* db,
                       object_id_t id,
                       const char* name,
                       uint64_t size)
{
    buffer_t* ptr = get_property_ptr(db, id, PTYPE_BUFFER, name);

    if (ptr)
    {
        // TODO(octave) : error check memcpy
        ptr->data = mem_realloc(db->alloc, ptr->data, ptr->size, size);
        ptr->size = size;
        return true;
    }
    else
    {
        return false;
    }
}

bool get_buffer_data(database_t* db,
                     object_id_t id,
                     const char* name,
                     uint64_t offset,
                     uint64_t size,
                     void* data)
{
    buffer_t* ptr = get_property_ptr(db, id, PTYPE_BUFFER, name);

    if (!ptr || offset + size >= ptr->size)
    {
        // TODO(octave) : error handling.
        return false;
    }
    else
    {
        // TODO(octave) : error check memcpy
        memcpy(data, (uint8_t*)ptr->data + offset, size);
        return true;
    }
}

bool set_buffer_data(database_t* db,
                     object_id_t id,
                     const char* name,
                     uint64_t offset,
                     uint64_t size,
                     const void* data)
{
    buffer_t* ptr = get_property_ptr(db, id, PTYPE_BUFFER, name);

    if (!ptr || offset + size >= ptr->size)
    {
        // TODO(octave) : error handling.
        return false;
    }
    else
    {
        // TODO(octave) : error check memcpy
        memcpy((uint8_t*)ptr->data + offset, data, size);
        return true;
    }
}

void delete_object(database_t* db, object_id_t id)
{
    object_t* object = get_object(db, id);
    if (!object)
    {
        // already deleted.
        return;
    }

    object_type_definition_t* type = &db->object_types[id.info.type];
    for (uint32_t i = 0; i < type->property_count; i++)
    {
        property_layout_t* prop = &db->properties[type->first_property + i];
        if (prop->def.type == PTYPE_BUFFER)
        {
            buffer_t* buf = (buffer_t*)((uint8_t*)object->data + prop->offset);

            mem_free(db->alloc, buf->data, buf->size);
        }
    }

    ASSERT(object->data);
    mem_free(db->alloc, object->data, type->bytes);

    object->data = 0;
    object->id.info.type = 0;

    db->objects[id.info.slot].next_free = db->objects[0].next_free;
    db->objects[0].next_free = id.info.slot;
}

object_id_t add_object(database_t* db, uint16_t type_index)
{
    ASSERT(type_index < array_count(db->object_types));

    uint32_t slot_index = db->objects[0].next_free;
    if (!slot_index)
    {
        array_push(db->alloc, db->objects, (object_slot_t){0});
        slot_index = array_count(db->objects) - 1;
    }

    object_t* object = &db->objects[slot_index].object;
    object->id.info.type = type_index;
    object->id.info.generation++;
    object->id.info.slot = slot_index;

    object->data = mem_alloc(db->alloc, db->object_types[type_index].bytes);
    memset(object->data, 0, db->object_types[type_index].bytes);

    return object->id;
}
