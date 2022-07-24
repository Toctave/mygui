#include "data_model.h"
#include "assert.h"
#include "memory.h"
#include "stretchy_buffer.h"

#include "plugin_sdk.h"

#include <string.h>

typedef struct mem_allocator_i mem_allocator_i;

typedef struct blob_t
{
    uint64_t size;
    void* data;
} blob_t;

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

struct database_o
{
    mem_allocator_i* alloc;
    /* array */ property_layout_t* properties;
    /* array */ object_type_definition_t* object_types;
    /* array */ object_slot_t* objects;
};

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

static database_o* create(mem_allocator_i* alloc)
{
    database_o* db = mem_alloc(alloc, sizeof(database_o));

    *db = (database_o){0};
    db->alloc = alloc;

    array_push(db->alloc, db->object_types, (object_type_definition_t){0});
    array_push(db->alloc, db->properties, (property_layout_t){0});
    array_push(db->alloc, db->objects, (object_slot_t){0});

    return db;
}

static void destroy(database_o* db)
{
    // TODO(octave) : check that all objects have been freed

    array_free(db->alloc, db->object_types);
    array_free(db->alloc, db->properties);
    array_free(db->alloc, db->objects);

    db->object_types = 0;
    db->properties = 0;
    db->objects = 0;

    mem_free(db->alloc, db, sizeof(database_o));
}

#define DO_SIZE_SWITCH_CASE(upper, lower, type)                                \
    case PTYPE_##upper:                                                        \
        return sizeof(type);

static uint32_t property_size(const property_definition_t* property)
{
    switch (property->type)
    {
        FOR_ALL_BASE_PROPERTY_TYPES(DO_SIZE_SWITCH_CASE)
    case PTYPE_BLOB:
        return sizeof(blob_t);
    case PTYPE_OBJECT:
        return sizeof(object_id_t);
    case PTYPE_REFERENCE:
        return sizeof(object_id_t);
    }
    ASSERT_MSG(false, "Unknown property type %u", property->type);
    return 0;
}

static object_type_t add_object_type(database_o* db,
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

    return (object_type_t){array_count(db->object_types) - 1};
}

static object_t* get_object(database_o* db, object_id_t id)
{
    ASSERT(id.index);
    ASSERT(id.info.slot < array_count(db->objects));

    object_t* object = &db->objects[id.info.slot].object;

    ASSERT(object->id.info.generation >= id.info.generation);
    if (object->id.info.generation > id.info.generation
        || !object->id.info.type.index)
    {
        return 0;
    }

    ASSERT(id.info.type.index == object->id.info.type.index);
    ASSERT(id.info.slot == object->id.info.slot);

    return object;
}

static const property_layout_t*
get_property_by_name(database_o* db, object_type_t type, const char* prop_name)
{
    ASSERT(type.index && type.index < array_count(db->object_types));
    const object_type_definition_t* type_def = &db->object_types[type.index];
    for (uint32_t i = 0; i < type_def->property_count; i++)
    {
        const property_layout_t* prop =
            &db->properties[type_def->first_property + i];

        if (!strncmp(prop->def.name, prop_name, sizeof(prop->def.name)))
        {
            return prop;
        }
    }

    return 0;
}

static void* get_property_ptr_full(database_o* db,
                                   object_id_t id,
                                   uint16_t property_type,
                                   object_type_t object_type,
                                   const char* name)
{
    const property_layout_t* prop =
        get_property_by_name(db, id.info.type, name);

    if (!prop //
        || prop->def.type != property_type
        || (object_type.index
            && prop->def.object_type.index != object_type.index))
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

static void* get_property_ptr(database_o* db,
                              object_id_t id,
                              uint32_t property_type,
                              const char* name)
{
    return get_property_ptr_full(db,
                                 id,
                                 property_type,
                                 (object_type_t){0},
                                 name);
}

#define DO_DEFINE_GETTER_SETTER(upper, lower, type)                            \
    static type get_##lower(database_o* db,                                    \
                            object_id_t object,                                \
                            const char* name)                                  \
    {                                                                          \
        type* ptr = get_property_ptr(db, object, PTYPE_##upper, name);         \
        ASSERT(ptr);                                                           \
        return *ptr;                                                           \
    }                                                                          \
    static void set_##lower(database_o* db,                                    \
                            object_id_t object,                                \
                            const char* name,                                  \
                            type value)                                        \
    {                                                                          \
        type* ptr = get_property_ptr(db, object, PTYPE_##upper, name);         \
        ASSERT(ptr);                                                           \
        *ptr = value;                                                          \
    }

FOR_ALL_BASE_PROPERTY_TYPES(DO_DEFINE_GETTER_SETTER)

static bool
reallocate_blob(database_o* db, object_id_t id, const char* name, uint64_t size)
{
    blob_t* ptr = get_property_ptr(db, id, PTYPE_BLOB, name);

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

static bool get_blob_data(database_o* db,
                          object_id_t id,
                          const char* name,
                          uint64_t offset,
                          uint64_t size,
                          void* data)
{
    blob_t* ptr = get_property_ptr(db, id, PTYPE_BLOB, name);

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

static bool set_blob_data(database_o* db,
                          object_id_t id,
                          const char* name,
                          uint64_t offset,
                          uint64_t size,
                          const void* data)
{
    blob_t* ptr = get_property_ptr(db, id, PTYPE_BLOB, name);

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

static object_id_t
get_sub_object(database_o* db, object_id_t id, const char* name)
{
    object_id_t* ptr = get_property_ptr(db, id, PTYPE_OBJECT, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
        return (object_id_t){0};
    }
    else
    {
        return *ptr;
    }
}

static object_id_t
get_reference(database_o* db, object_id_t id, const char* name)
{
    object_id_t* ptr = get_property_ptr(db, id, PTYPE_REFERENCE, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
        return (object_id_t){0};
    }
    else
    {
        return *ptr;
    }
}

static void set_reference(database_o* db,
                          object_id_t id,
                          const char* name,
                          object_id_t value)
{
    object_id_t* ptr =
        get_property_ptr_full(db, id, PTYPE_REFERENCE, value.info.type, name);

    if (!ptr)
    {
        // TODO(octave) : error handling.
    }
    else
    {
        *ptr = value;
    }
}

static void destroy_object(database_o* db, object_id_t id)
{
    object_t* object = get_object(db, id);
    if (!object)
    {
        // already deleted.
        return;
    }

    object_type_definition_t* type = &db->object_types[id.info.type.index];
    for (uint32_t i = 0; i < type->property_count; i++)
    {
        property_layout_t* prop = &db->properties[type->first_property + i];
        if (prop->def.type == PTYPE_BLOB)
        {
            blob_t* buf = (blob_t*)((uint8_t*)object->data + prop->offset);

            mem_free(db->alloc, buf->data, buf->size);
        }
        else if (prop->def.type == PTYPE_OBJECT)
        {
            object_id_t* sub_id =
                (object_id_t*)((uint8_t*)object->data + prop->offset);

            destroy_object(db, *sub_id);
        }
    }

    ASSERT(object->data);
    mem_free(db->alloc, object->data, type->bytes);

    object->data = 0;
    object->id.info.type = (object_type_t){0};

    db->objects[id.info.slot].next_free = db->objects[0].next_free;
    db->objects[0].next_free = id.info.slot;
}

static object_id_t create_object(database_o* db, object_type_t type)
{
    if (type.index == 0 || type.index >= array_count(db->object_types))
    {
        return (object_id_t){0};
    }

    uint32_t slot_index = db->objects[0].next_free;
    if (!slot_index)
    {
        array_push(db->alloc, db->objects, (object_slot_t){0});
        slot_index = array_count(db->objects) - 1;
    }

    object_t* object = &db->objects[slot_index].object;
    object->id.info.type = type;
    object->id.info.generation++;
    object->id.info.slot = slot_index;

    object->data = mem_alloc(db->alloc, db->object_types[type.index].bytes);
    memset(object->data, 0, db->object_types[type.index].bytes);

    object_type_definition_t* type_def = &db->object_types[type.index];
    for (uint32_t i = 0; i < type_def->property_count; i++)
    {
        property_layout_t* prop = &db->properties[type_def->first_property + i];

        if (prop->def.type == PTYPE_OBJECT)
        {
            object_id_t* sub_id =
                (object_id_t*)((uint8_t*)object->data + prop->offset);

            *sub_id = create_object(db, prop->def.object_type);
        }
    }

    return object->id;
}

#define DO_ASSIGN_GETTER_SETTER(upper, lower, type)                            \
    db->set_##lower = set_##lower;                                             \
    db->get_##lower = get_##lower;

static void load(void* api)
{
    database_api* db = api;

    FOR_ALL_BASE_PROPERTY_TYPES(DO_ASSIGN_GETTER_SETTER)

    db->create = create;
    db->destroy = destroy;
    db->add_object_type = add_object_type;
    db->create_object = create_object;
    db->destroy_object = destroy_object;
    db->get_sub_object = get_sub_object;
    db->get_reference = get_reference;
    db->set_reference = set_reference;
    db->reallocate_blob = reallocate_blob;
    db->get_blob_data = get_blob_data;
    db->set_blob_data = set_blob_data;
}

plugin_spec_t PLUGIN_SPEC = {
    .name = "database",
    .version = {0, 0, 2},
    .load = load,
    .api_size = sizeof(database_api),
};
