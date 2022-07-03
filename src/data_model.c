#include "data_model.h"
#include "memory.h"

#if 0

typedef enum property_type_e
{
    PTYPE_NONE,
    PTYPE_BOOL,
    PTYPE_INT64,
    PTYPE_FLOAT64,
} property_type_e;

typedef struct property_definition_t
{
    property_type_e type;
    const char* name;
} property_definition_t;

typedef struct object_t object_t;

typedef struct database_t
{
} database_t;

uint32_t add_object_type(database_t* db,
                         uint32_t property_count,
                         property_definition_t* properties)
{
}

object_t* create_object(uint32_t type) {}

bool get_bool(object_t* obj, const char* property_name) {}
void set_bool(object_t* obj, const char* property_name, bool value) {}

int64_t get_int(object_t* obj, const char* property_name) {}
void set_int(object_t* obj, const char* property_name, int64_t value) {}

double get_float(object_t* obj, const char* property_name) {}
void set_float(object_t* obj, const char* property_name, double value) {}

#endif
