#include "base_types.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "data_model.h"
#include "hash.h"
#include "logging.h"
#include "memory.h"
#include "platform.h"
#include "plugin_manager.h"
#include "renderer.h"
#include "ui.h"
#include "util.h"

static mem_api* mem;
static database_api* db;
static oui_api* ui;

static void test_hash()
{
    const uint32_t buckets = 32;
    uint64_t keys[buckets];
    uint64_t values[buckets];

    memset(keys, 0, sizeof(keys));

    hash_t h;
    h.bucket_count = buckets;
    h.keys = keys;
    h.values = values;

    ASSERT(!hash_find(&h, 1, 0));

    hash_insert(&h, 1, 1);
    hash_insert(&h, 33, 32);
    hash_insert(&h, 1234567, 1234567);
    hash_insert(&h, 33, 33);
    hash_insert(&h, 65, 65);
    ASSERT(hash_find(&h, 1, 0) == 1);
    ASSERT(hash_find(&h, 33, 0) == 33);
    ASSERT(hash_find(&h, 1234567, 0) == 1234567);
    ASSERT(hash_find(&h, 65, 0) == 65);

    hash_remove(&h, 1);
    ASSERT(!hash_find(&h, 1, 0));
    ASSERT(hash_find(&h, 33, 0) == 33);
    ASSERT(hash_find(&h, 1234567, 0) == 1234567);
    ASSERT(hash_find(&h, 65, 0) == 65);

    hash_remove(&h, 1234567);
    ASSERT(!hash_find(&h, 1, 0));
    ASSERT(hash_find(&h, 33, 0) == 33);
    ASSERT(!hash_find(&h, 1234567, 0));
    ASSERT(hash_find(&h, 65, 0) == 65);

    log_flush();
}

typedef struct blob_t
{
    int u;
    char w[43];
    float x;
} blob_t;

static void test_db()
{
    database_o* mydb = db->create(mem->vm);

    property_definition_t props[] = {
        {.name = "active", .type = PTYPE_BOOL},
        {.name = "x", .type = PTYPE_FLOATING},
        {.name = "y", .type = PTYPE_FLOATING},
        {.name = "blob", .type = PTYPE_BUFFER},
    };
    uint16_t typ = db->add_object_type(mydb, STATIC_ARRAY_COUNT(props), props);

    property_definition_t nprops[] = {
        {.name = "subobject", .type = PTYPE_OBJECT, .object_type = typ},
        {.name = "reference", .type = PTYPE_REFERENCE, .object_type = typ},
    };
    uint16_t ntyp =
        db->add_object_type(mydb, STATIC_ARRAY_COUNT(nprops), nprops);

    log_debug("typ = %u", typ);

    object_id_t nobj = db->add_object(mydb, ntyp);
    object_id_t obj = db->add_object(mydb, typ);
    blob_t my_blob = {-12, "123", 3.14f};

    db->reallocate_buffer(mydb, obj, "blob", sizeof(blob_t));
    db->set_buffer_data(mydb, obj, "blob", 0, sizeof(blob_t), &my_blob);

    object_id_t obj2 = db->get_sub_object(mydb, nobj, "subobject");
    db->reallocate_buffer(mydb, obj2, "blob", sizeof(blob_t));
    db->set_buffer_data(mydb, obj2, "blob", 0, sizeof(blob_t), &my_blob);

    db->set_float(mydb, obj, "x", 3.0);
    for (uint32_t i = 0; i < 100; i++)
    {
        object_id_t id = db->add_object(mydb, typ);

        log_debug("ID : %u %u %u\n",
                  id.info.type,
                  id.info.generation,
                  id.info.slot);
    }
    log_debug("obj.x = %g", db->get_float(mydb, obj, "x"));
    db->get_buffer_data(mydb, obj, "blob", 0, sizeof(blob_t), &my_blob);
    log_debug("obj.blob = { .u = %d, .w = \"%s\", .x = %f }",
              my_blob.u,
              my_blob.w,
              my_blob.x);

    db->destroy(mydb);
}

int main(int argc, const char** argv)
{
    ASSERT(sizeof(void*) == sizeof(uint64_t));
    (void)argc;

    uint32_t window_width = 640;
    uint32_t window_height = 480;
    if (!platform_init(argv[0], argv[0], window_width, window_height))
    {
        return 1;
    }

    plugin_manager_init();
    mem = load_plugin("memory", (version_t){1, 0, 0});
    ASSERT(mem);
    db = load_plugin("database", (version_t){0, 0, 1});
    ASSERT(db);
    renderer_api* render_api = load_plugin("renderer", (version_t){0, 0, 1});
    ASSERT(render_api);

    ui = load_plugin("ui", (version_t){0, 0, 1});
    ASSERT(ui);

    log_init(mem->vm);

    test_hash();
    test_db();

    void* tmp_stack_buf = mem_alloc(mem->vm, Gibi(1024));
    mem_stack_allocator_o* tmp_alloc =
        mem->stack_create(tmp_stack_buf, Gibi(1024));

    void* permanent_stack_buf = mem_alloc(mem->vm, Gibi(1024));
    mem_stack_allocator_o* permanent_alloc =
        mem->stack_create(permanent_stack_buf, Gibi(1024));

    renderer_i* renderer = render_api->create(mem, permanent_alloc, tmp_alloc);

    platform_input_info_t input = {0};

    ui->init(mem, tmp_alloc, renderer, &input);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float freq = 1.0f;

    uint64_t t0 = platform_get_nanoseconds();

    quad_i32_t node = {{10, 10}, {200, 200}};
    quad_i32_t node2 = {{10, 10}, {200, 200}};

    // main loop
    while (!input.should_exit)
    {
        mem->stack_revert(tmp_alloc, 0);
        uint64_t now = platform_get_nanoseconds();

        float t = (now - t0) * 1e-9f;
        float y = sinf(t);

        platform_handle_input_events(&input);

        ui->begin();

        if (ui->button("Do the thingy"))
        {
            log_debug("Did the thingy");
        }
        if (ui->button("Undo the thingy"))
        {
            log_debug("Undid the thingy");
        }

        static bool b;
        if (ui->checkbox("Thingy on", &b))
        {
            log_debug("The thingy is %s", b ? "on" : "off");
        }

        ui->begin_node("my node", &node);
        ui->plug("The value");
        ui->plug("The other value");
        ui->end_node();

        ui->begin_node("my other node", &node2);
        ui->plug("amazing value");
        ui->end_node();

        y = sinf(freq * t);
        ui->slider("freq", &freq, 0.0f, 10.0f);
        ui->slider("sin(freq * t)", &y, -1.0f, 1.0f);

        ui->end();

        renderer->do_draw(renderer, input.width, input.height);
        platform_swap_buffers();

        log_flush();
    }

    log_terminate();

    return 0;
}
