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

    int64_t* x = &get_plug_value(&graph, f, 0)->integer;
    int64_t* y = &get_plug_value(&graph, f, 1)->integer;
    int64_t* z = &get_plug_value(&graph, g, 1)->integer;

    *x = 43;
    *y = 25;
    *z = 4;

    build_schedule(&mem->std, &graph);
    evaluate_schedule(&graph);

    log_debug("result = %ld", get_plug_value(&graph, g, 2)->integer);
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

    uint32_t line_height = ui->get_line_height();
    uint32_t margin = ui->get_margin();

    *x = is_input(graph, node_index, plug)
             ? node->box.min[0]
             : node->box.min[0] + node->box.extent[0];
    *y = node->box.min[1]                // top
         + (line_height + margin)        // title
         + (line_height + margin) * plug // plug
         + line_height / 2               // line middle
        ;
}

static void graph_ui(oui_api* ui, node_graph_t* graph)
{
    typedef struct plug_info_t
    {
        uint32_t node;
        uint32_t plug;
    } plug_info_t;

    for (uint32_t node_index = 1; node_index < array_count(graph->nodes);
         node_index++)
    {
        node_t* node = &graph->nodes[node_index];
        node_type_definition_t* type = &graph->node_types[node->type];

        uint32_t line_height = ui->get_line_height();
        uint32_t ui_margin = ui->get_margin();
        node->box.extent[1] =
            (line_height + ui_margin) * (type->plug_count + 1);

        ui->begin_draw_region(node->box.min[0],
                              node->box.min[1],
                              node->box.extent[0],
                              node->box.extent[1]);

        ui->push_id(node_index);
        /* ui->begin_node(type->name); */

        ui->draw_quad(node->box, *ui->get_color(UI_COLOR_BACKGROUND));
        ui->draw_quad((quad_i32_t){{node->box.min[0], node->box.min[1]},
                                   {node->box.extent[0], line_height}},
                      *ui->get_color(UI_COLOR_SECONDARY));

        char title[512];
        snprintf(title, sizeof(title), "%s (%u)", type->name, node_index);

        ui->text(title);

        uint32_t plugs_margin = 5;
        ui->begin_draw_region(ui->get_cursor_x() + plugs_margin,
                              ui->get_cursor_y(),
                              node->box.extent[0] - 2 * plugs_margin,
                              node->box.extent[1] - 2 * ui_margin);

        for (uint32_t plug = 0; plug < type->plug_count; plug++)
        {
            ui->push_id(plug);

            ui->text(type->plugs[plug].name);
            ui->same_line();

            int32_t plug_x, plug_y;
            get_plug_pos(ui, graph, node_index, plug, &plug_x, &plug_y);
            quad_i32_t sq = square(plug_x, plug_y, 12);

            quad_i32_t hover_sq = quad_i32_grown(sq, 4);
            ui->hold_rect(hover_sq.min[0],
                          hover_sq.min[1],
                          hover_sq.extent[0],
                          hover_sq.extent[1]);

            plug_info_t src_plug = {node_index, plug};
            int32_t src_plug_x = plug_x, src_plug_y = plug_y;

            bool is_connected_input = is_input(graph, node_index, plug)
                                      && node->plugs[plug].connected_node;

            if (is_connected_input)
            {
                // if already connected, draw connection, and use the
                // connected plug as source when dragging
                src_plug = (plug_info_t){node->plugs[plug].connected_node,
                                         node->plugs[plug].connected_plug};
                get_plug_pos(ui,
                             graph,
                             src_plug.node,
                             src_plug.plug,
                             &src_plug_x,
                             &src_plug_y);

                ui->draw_line(src_plug_x,
                              src_plug_y,
                              plug_x,
                              plug_y,
                              4,
                              *ui->get_color(UI_COLOR_MAIN));
            }

            if (ui->drag_and_drop_source(&src_plug, sizeof(src_plug)))
            {
                if (is_connected_input)
                {
                    disconnect_node(graph, node_index, plug);
                }
            }

            if (ui->drag_and_drop_target(&src_plug, sizeof(src_plug)))
            {
                if (can_connect_nodes(graph,
                                      src_plug.node,
                                      src_plug.plug,
                                      node_index,
                                      plug))
                {
                    connect_nodes(graph,
                                  src_plug.node,
                                  src_plug.plug,
                                  node_index,
                                  plug);
                }
            }

            ui->draw_quad(sq, *ui->get_color(UI_COLOR_BACKGROUND));
            ui->draw_quad(quad_i32_grown(sq, -2),
                          *ui->get_color(UI_COLOR_MAIN));

            node_plug_value_t* val = get_plug_value(graph, node_index, plug);

            switch (type->plugs[plug].type)
            {
            case PLUG_FLOAT:
            {
                float u = val->floating;
                ui->slider(type->plugs[plug].name, &u, -INFINITY, INFINITY);
                ui->same_line();

                val->floating = u;
            }
            break;
            case PLUG_INTEGER:
            {
                float u = val->integer;
                ui->slider(type->plugs[plug].name, &u, -INFINITY, INFINITY);
                ui->same_line();

                val->integer = u;
            }
            break;
            }

            ui->new_line();

            ui->pop_id();
        }

        uint32_t resize_margin = 10;

        int32_t dx, dy;
        ui->push_string_id("move");
        if (ui->drag_rect(node->box.min[0],
                          node->box.min[1],
                          node->box.extent[0] - resize_margin,
                          node->box.extent[1],
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
                          node->box.extent[1],
                          &dx,
                          &dy))
        {
            node->box.extent[0] += dx;
        }
        ui->pop_id();

        ui->end_draw_region();

        ui->pop_id();
        ui->end_draw_region();
    }

    plug_info_t dragging_plug;
    if (ui->get_drag_and_drop_payload(&dragging_plug, sizeof(dragging_plug)))
    {
        int32_t dragging_x, dragging_y;
        get_plug_pos(ui,
                     graph,
                     dragging_plug.node,
                     dragging_plug.plug,
                     &dragging_x,
                     &dragging_y);

        ui->draw_line(dragging_x,
                      dragging_y,
                      ui->get_mouse().x,
                      ui->get_mouse().y,
                      4,
                      *ui->get_color(UI_COLOR_MAIN));
    }
}

DefineNodeEvaluator(node_get_time)
{
    (void)inputs;

    static uint64_t t0 = 0;
    if (t0 == 0)
    {
        t0 = platform_get_nanoseconds();
    }

    outputs[0].floating = 1e-9 * (platform_get_nanoseconds() - t0);
}

DefineNodeEvaluator(node_sin) { outputs[0].floating = sin(inputs[0].floating); }

DefineNodeEvaluator(node_multiply)
{
    outputs[0].floating = inputs[0].floating * inputs[1].floating;
}

DefineNodeEvaluator(node_add)
{
    outputs[0].floating = inputs[0].floating + inputs[1].floating;
}

renderer_i* renderer;

DefineNodeEvaluator(node_draw_quad)
{
    renderer->draw_quad(
        renderer,
        (quad_i32_t){{inputs[0].floating, inputs[1].floating},
                     {inputs[2].floating, inputs[3].floating}},
        color_rgb(inputs[4].floating, inputs[5].floating, inputs[6].floating));
    (void)outputs;
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

    renderer = render_api->create(mem, permanent_alloc, tmp_alloc);

    platform_input_info_t input = {0};

    ui->init(&mem->std, renderer);

    node_graph_t graph;
    node_graph_init(&mem->std, &graph);

    add_node_type(&mem->std,
                  &graph,
                  (node_type_definition_t){
                      .name = "add integers",
                      .input_count = 2,
                      .plug_count = 3,
                      .plugs = {{.name = "a", .type = PLUG_INTEGER},
                                {.name = "b", .type = PLUG_INTEGER},
                                {.name = "result", .type = PLUG_INTEGER}},

                      .evaluate = add_integer,
                  });

    add_node_type(&mem->std,
                  &graph,
                  (node_type_definition_t){
                      .name = "get time",
                      .input_count = 0,
                      .plug_count = 1,
                      .plugs = {{.name = "time", .type = PLUG_FLOAT}},

                      .evaluate = node_get_time,
                  });

    add_node_type(&mem->std,
                  &graph,
                  (node_type_definition_t){
                      .name = "sin",
                      .input_count = 1,
                      .plug_count = 2,
                      .plugs = {{.name = "x", .type = PLUG_FLOAT},
                                {.name = "result", .type = PLUG_FLOAT}},
                      .evaluate = node_sin,
                  });

    add_node_type(&mem->std,
                  &graph,
                  (node_type_definition_t){
                      .name = "multiply",
                      .input_count = 2,
                      .plug_count = 3,
                      .plugs =
                          {
                              {.name = "a", .type = PLUG_FLOAT},
                              {.name = "b", .type = PLUG_FLOAT},
                              {.name = "result", .type = PLUG_FLOAT},
                          },
                      .evaluate = node_multiply,
                  });

    add_node_type(&mem->std,
                  &graph,
                  (node_type_definition_t){
                      .name = "add",
                      .input_count = 2,
                      .plug_count = 3,
                      .plugs =
                          {
                              {.name = "a", .type = PLUG_FLOAT},
                              {.name = "b", .type = PLUG_FLOAT},
                              {.name = "result", .type = PLUG_FLOAT},
                          },
                      .evaluate = node_add,
                  });

    add_node_type(&mem->std,
                  &graph,
                  (node_type_definition_t){
                      .name = "draw rectangle",
                      .input_count = 7,
                      .plug_count = 7,
                      .plugs =
                          {
                              {.name = "x", .type = PLUG_FLOAT},
                              {.name = "y", .type = PLUG_FLOAT},
                              {.name = "width", .type = PLUG_FLOAT},
                              {.name = "height", .type = PLUG_FLOAT},
                              {.name = "r", .type = PLUG_FLOAT},
                              {.name = "g", .type = PLUG_FLOAT},
                              {.name = "b", .type = PLUG_FLOAT},
                          },
                      .evaluate = node_draw_quad,
                  });

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

        platform_handle_input_events(&input);

        ui->begin_frame(&input);

        graph_ui(ui, &graph);

        if (array_count(graph.nodes) > 1)
        {
            build_schedule(&mem->std, &graph);
            evaluate_schedule(&graph);
        }

        for (uint32_t type_index = 1;
             type_index < array_count(graph.node_types);
             type_index++)
        {
            char* label = tprintf(mem,
                                  tmp_alloc,
                                  "%s",
                                  graph.node_types[type_index].name);
            if (ui->button(label))
            {
                uint32_t idx = add_node(&mem->std, &graph, type_index);
                graph.nodes[idx].box = (quad_i32_t){{300, 10}, {200, 200}};
            }
        }

        ui->end_frame();

        renderer->do_draw(renderer, input.width, input.height);
        platform_swap_buffers();

        log_flush();
    }

    log_terminate();

    return 0;
}
