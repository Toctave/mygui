#include "base_types.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "color.h"
#include "data_model.h"
#include "evaluation_graph.h"
#include "hash.h"
#include "logging.h"
#include "memory.h"
#include "platform.h"
#include "plugin_manager.h"
#include "renderer.h"
#include "stretchy_buffer.h"
#include "ui.h"
#include "util.h"

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

static void test_db(mem_api* mem, database_api* db)
{
    database_o* mydb = db->create(&mem->vm);

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

void add_integer(const node_plug_value_t* inputs, node_plug_value_t* outputs)
{
    outputs[0].integer = inputs[0].integer + inputs[1].integer;
}

static void test_eval_graph(mem_api* mem)
{
    node_graph_t graph;
    node_graph_init(&mem->std, &graph);

    node_type_definition_t node_add = {
        .name = "add",
        .input_count = 2,
        .plug_count = 3,
        .plugs = {{.name = "a", .type = PLUG_INTEGER},
                  {.name = "b", .type = PLUG_INTEGER},
                  {.name = "result", .type = PLUG_INTEGER}},
        .evaluate = add_integer,
    };

    uint32_t add_type = add_node_type(&mem->std, &graph, node_add);

    uint32_t f = add_node(&mem->std, &graph, add_type);
    uint32_t g = add_node(&mem->std, &graph, add_type);

    connect_nodes(&graph, f, 2, g, 0);

    int64_t* x = &get_input_value(&graph, f, 0)->integer;
    int64_t* y = &get_input_value(&graph, f, 1)->integer;
    int64_t* z = &get_input_value(&graph, g, 1)->integer;

    *x = 43;
    *y = 25;
    *z = 4;
    log_debug("result = %ld", stupid_evaluate(&graph, g, 2).integer);
}

static quad_i32_t square(int32_t x, int32_t y, int32_t width)
{
    ASSERT(!(width % 2));

    return (quad_i32_t){{x - width / 2, y - width / 2}, {width, width}};
}

static void get_plug_pos(oui_api* ui,
                         node_graph_t* graph,
                         uint32_t node_index,
                         uint32_t plug,
                         int32_t* x,
                         int32_t* y)
{
    node_t* node = &graph->nodes[node_index];

    *x = is_input(graph, node_index, plug)
             ? node->box.min[0]
             : node->box.min[0] + node->box.extent[0];
    *y = node->box.min[1] + ui->get_line_height() * (1.5 + plug);
}

static void graph_ui(oui_api* ui, node_graph_t* graph)
{
    for (uint32_t node_index = 1; node_index < array_count(graph->nodes);
         node_index++)
    {
        node_t* node = &graph->nodes[node_index];
        node_type_definition_t* type = &graph->node_types[node->type];

        ui->push_id(node_index);
        /* ui->begin_node(type->name); */

        uint32_t resize_margin = 10;
        int32_t dx, dy;
        uint32_t line_height = ui->get_line_height();

        ui->draw_quad(node->box, color_rgb(0x00, 0x00, 0x00));
        ui->draw_quad((quad_i32_t){{node->box.min[0], node->box.min[1]},
                                   {node->box.extent[0], line_height}},
                      color_rgb(0xFF, 0x40, 0x40));
        ui->draw_text(type->name, node->box.min[0], node->box.min[1]);

        int32_t x = node->box.min[0];
        int32_t y = node->box.min[1] + line_height;

        typedef struct plug_info_t
        {
            uint32_t node;
            uint32_t plug;
        } plug_info_t;

        for (uint32_t plug = 0; plug < type->plug_count; plug++)
        {
            ui->push_id(plug);

            char plug_txt[MAX_PLUG_NAME + 256];
            snprintf(plug_txt,
                     sizeof(plug_txt),
                     "%s : %ld",
                     type->plugs[plug].name,
                     node->plugs[plug].value.integer);

            ui->draw_text(plug_txt, x + 4, y);
            y += line_height;

            int32_t plug_x, plug_y;
            get_plug_pos(ui, graph, node_index, plug, &plug_x, &plug_y);
            quad_i32_t sq = square(plug_x, plug_y, 12);

            quad_i32_t hover_sq = quad_i32_grown(sq, 4);
            ui->hold_rect(hover_sq.min[0],
                          hover_sq.min[1],
                          hover_sq.extent[0],
                          hover_sq.extent[1]);

            plug_info_t this_plug = {node_index, plug};
            if (ui->drag_and_drop_source(&this_plug, sizeof(this_plug)))
            {
                ui->draw_line(plug_x,
                              plug_y,
                              ui->get_mouse().x,
                              ui->get_mouse().y,
                              4,
                              color_gray(0x80));
            }

            plug_info_t source_plug;
            if (ui->drag_and_drop_target(&source_plug, sizeof(source_plug)))
            {
                if (can_connect_nodes(graph,
                                      source_plug.node,
                                      source_plug.plug,
                                      node_index,
                                      plug))
                {
                    connect_nodes(graph,
                                  source_plug.node,
                                  source_plug.plug,
                                  node_index,
                                  plug);
                }
            }

            ui->draw_quad(sq, color_rgb(0x00, 0x00, 0x40));
            ui->draw_quad(quad_i32_grown(sq, -2), color_rgb(0x40, 0x40, 0xff));

            if (is_input(graph, node_index, plug)
                && node->plugs[plug].input_node)
            {
                int32_t src_plug_x, src_plug_y;
                get_plug_pos(ui,
                             graph,
                             node->plugs[plug].input_node,
                             node->plugs[plug].input_plug,
                             &src_plug_x,
                             &src_plug_y);
                ui->draw_line(src_plug_x,
                              src_plug_y,
                              plug_x,
                              plug_y,
                              4,
                              color_gray(0x80));
            }

            ui->pop_id();
        }

        ui->push_string_id("move");
        if (ui->drag_rect(node->box.min[0],
                          node->box.min[1],
                          node->box.extent[0] - resize_margin,
                          node->box.extent[1] - resize_margin,
                          &dx,
                          &dy))
        {
            node->box.min[0] += dx;
            node->box.min[1] += dy;
        }
        ui->pop_id();

        ui->push_string_id("resize_x");
        if (ui->drag_rect(node->box.min[0] + node->box.extent[0]
                              - resize_margin,
                          node->box.min[1],
                          resize_margin,
                          node->box.extent[1] - resize_margin,
                          &dx,
                          &dy))
        {
            node->box.extent[0] += dx;
        }
        ui->pop_id();

        ui->push_string_id("resize_y");
        if (ui->drag_rect(node->box.min[0],
                          node->box.min[1] + node->box.extent[1]
                              - resize_margin,
                          node->box.extent[0] - resize_margin,
                          resize_margin,
                          &dx,
                          &dy))
        {
            node->box.extent[1] += dy;
        }
        ui->pop_id();

        ui->push_string_id("resize_x_y");
        if (ui->drag_rect(
                node->box.min[0] + node->box.extent[0] - resize_margin,
                node->box.min[1] + node->box.extent[1] - resize_margin,
                resize_margin,
                resize_margin,
                &dx,
                &dy))
        {
            node->box.extent[0] += dx;
            node->box.extent[1] += dy;
        }
        ui->pop_id();

        /* ui->end_node(); */
        ui->pop_id();
    }
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
    mem_api* mem = load_plugin("memory", (version_t){1, 0, 0});
    ASSERT(mem);
    database_api* db = load_plugin("database", (version_t){0, 0, 1});
    ASSERT(db);
    renderer_api* render_api = load_plugin("renderer", (version_t){0, 0, 1});
    ASSERT(render_api);

    oui_api* ui = load_plugin("ui", (version_t){0, 0, 1});
    ASSERT(ui);

    log_init(&mem->vm);

    test_hash();
    test_db(mem, db);
    test_eval_graph(mem);

    void* tmp_stack_buf = mem_alloc(&mem->vm, Gibi(1024));
    mem_stack_allocator_o* tmp_alloc =
        mem->stack_create(tmp_stack_buf, Gibi(1024));

    void* permanent_stack_buf = mem_alloc(&mem->vm, Gibi(1024));
    mem_stack_allocator_o* permanent_alloc =
        mem->stack_create(permanent_stack_buf, Gibi(1024));

    renderer_i* renderer = render_api->create(mem, permanent_alloc, tmp_alloc);

    platform_input_info_t input = {0};

    ui->init(&mem->std, renderer);

    float freq = 1.0f;

    uint64_t t0 = platform_get_nanoseconds();

    node_graph_t graph;
    {
        node_graph_init(&mem->std, &graph);

        node_type_definition_t node_add = {
            .name = "add",
            .input_count = 2,
            .plug_count = 3,
            .plugs = {{.name = "a", .type = PLUG_INTEGER},
                      {.name = "b", .type = PLUG_INTEGER},
                      {.name = "result", .type = PLUG_INTEGER}},

            .evaluate = add_integer,
        };

        uint32_t add_type = add_node_type(&mem->std, &graph, node_add);
        add_node(&mem->std, &graph, add_type);
        add_node(&mem->std, &graph, add_type);
        add_node(&mem->std, &graph, add_type);
    }

    for (uint32_t i = 1; i < array_count(graph.nodes); i++)
    {
        uint32_t width = 200;
        uint32_t height = 300;
        uint32_t margin = 10;
        graph.nodes[i].box =
            (quad_i32_t){{i * (margin * 2 + width) + margin, margin},
                         {width, height}};
    }

    // main loop
    while (!input.should_exit)
    {
        platform_file_event_t file_event;
        while (platform_poll_file_event(&file_event))
        {
        }

        mem->stack_revert(tmp_alloc, 0);
        uint64_t now = platform_get_nanoseconds();

        float t = (now - t0) * 1e-9f;
        float y = sinf(t);

        platform_handle_input_events(&input);

        ui->begin_frame(&input);

        graph.nodes[1].plugs[0].value.integer++;
        graph_ui(ui, &graph);

        stupid_evaluate(&graph, array_count(graph.nodes) - 1, 2);

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

        if (ui->checkbox("Thingy dingy", &b))
        {
            log_debug("The thingy is %s", b ? "on" : "off");
        }

        y = sinf(freq * t);
        ui->slider("freq", &freq, 0.0f, 10.0f);
        ui->slider("sin(freq * t)", &y, -1.0f, 1.0f);

        ui->end_frame();

        renderer->do_draw(renderer, input.width, input.height);
        platform_swap_buffers();

        log_flush();
    }

    log_terminate();

    return 0;
}
