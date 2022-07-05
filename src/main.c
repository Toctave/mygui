#include "base_types.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <math.h>
#include <stdarg.h>

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
#include "util.h"

static mem_api* mem;
static database_api* db;

color_t color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    color_t result;
    result.rgba[0] = r;
    result.rgba[1] = g;
    result.rgba[2] = b;
    result.rgba[3] = a;

    return result;
}

color_t color_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return color_rgba(r, g, b, 0xFF);
}

color_t color_gray(uint8_t w) { return color_rgb(w, w, w); }

typedef struct ui_node_t
{
    quad_i32_t bbox;
} ui_node_t;

struct
{
    mem_stack_allocator_o* tmp_stack;
    platform_input_info_t* input;
    renderer_i* renderer;

    int32_t cursor_x;
    int32_t cursor_y;

    uint64_t active_id;
    uint64_t hovered_id;
    uint64_t dropped_id;

    uint64_t id_stack[1024];
    uint32_t id_stack_height;

    int32_t draw_region_stack[1024][2];
    uint32_t draw_region_stack_height;

    struct
    {
        color_t main;
        color_t secondary;
        color_t background;
        color_t text;
        color_t text_shadow;
        color_t active_overlay;
        color_t hover_overlay;
    } colors;

    uint32_t padding;
    uint32_t margin;
    uint32_t line_height;

    ui_node_t* current_node;
    const char* current_node_name;
} ui;

bool mouse_inside_region(int32_t x, int32_t y, int32_t width, int32_t height)
{
    return ui.input->mouse_x >= x && ui.input->mouse_x < x + width //
           && ui.input->mouse_y >= y && ui.input->mouse_y < y + height;
}

static void ui_push_id(uint64_t id)
{
    ASSERT(ui.id_stack_height < ARRAY_COUNT(ui.id_stack));
    uint64_t hashed =
        ui.id_stack_height
            ? hash_combine(ui.id_stack[ui.id_stack_height - 1], id)
            : id;
    ui.id_stack[ui.id_stack_height++] = hashed;
}

static void ui_push_string_id(const char* txt) { ui_push_id(hash_string(txt)); }

static void ui_pop_id()
{
    ASSERT(ui.id_stack_height);
    ui.id_stack_height--;
}

static uint64_t ui_current_id()
{
    ASSERT(ui.id_stack_height);

    return ui.id_stack[ui.id_stack_height - 1];
}

static void ui_begin_draw_region(int32_t x, int32_t y)
{
    ASSERT(ui.draw_region_stack_height < ARRAY_COUNT(ui.draw_region_stack));

    ui.draw_region_stack[ui.draw_region_stack_height][0] = ui.cursor_x;
    ui.draw_region_stack[ui.draw_region_stack_height][1] = ui.cursor_y;
    ui.draw_region_stack_height++;

    ui.cursor_x = x + ui.margin;
    ui.cursor_y = y + ui.margin;
}

static void ui_end_draw_region()
{
    ASSERT(ui.draw_region_stack_height);
    ui.draw_region_stack_height--;

    int32_t(*cursor)[2] = &ui.draw_region_stack[ui.draw_region_stack_height];

    ui.cursor_x = (*cursor)[0];
    ui.cursor_y = (*cursor)[1];
}

float clamped_float(float t, float min, float max)
{
    if (t < min)
    {
        return min;
    }
    else if (t > max)
    {
        return max;
    }
    else
    {
        return t;
    }
}

bool ui_handle_hover(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    bool is_inside = mouse_inside_region(x, y, w, h);

    if (!ui.hovered_id && is_inside)
    {
        ui.hovered_id = ui_current_id();
        return true;
    }
    else if (ui.hovered_id == ui_current_id() && !is_inside)
    {
        ui.hovered_id = 0;
        return false;
    }

    return ui.hovered_id == ui_current_id();
}

bool ui_handle_hold_and_release(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    bool hovered = ui_handle_hover(x, y, w, h);

    if (hovered && (ui.input->mouse_pressed & MOUSE_BUTTON_LEFT)
        && !ui.active_id)
    {
        ui.active_id = ui_current_id();
        return false;
    }
    else if (ui.active_id == ui_current_id()
             && (ui.input->mouse_released & MOUSE_BUTTON_LEFT))
    {
        ui.active_id = 0;
        return true;
    }
    else
    {
        return false;
    }
}

bool ui_handle_drag(int32_t x,
                    int32_t y,
                    uint32_t w,
                    uint32_t h,
                    int32_t* dx,
                    int32_t* dy)
{
    ui_handle_hold_and_release(x, y, w, h);

    if (ui.active_id == ui_current_id())
    {
        *dx = ui.input->mouse_dx;
        *dy = ui.input->mouse_dy;

        return *dx || *dy;
    }

    return false;
}

bool ui_handle_drag_x(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t* dx)
{
    int32_t dy;
    return ui_handle_drag(x, y, w, h, dx, &dy) && *dx;
}

bool ui_handle_drag_y(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t* dy)
{
    int32_t dx;
    return ui_handle_drag(x, y, w, h, &dx, dy) && *dy;
}

float lerp(float t, float min, float max) { return min + t * (max - min); }

float unlerp(float t, float min, float max) { return (t - min) / (max - min); }

void ui_newline() { ui.cursor_y += ui.line_height + ui.margin; }

void ui_draw_hover_and_active_overlay(int32_t x,
                                      int32_t y,
                                      uint32_t width,
                                      uint32_t height)
{
    if (ui.active_id == ui_current_id())
    {
        ui.renderer->draw_quad(ui.renderer,
                               (quad_i32_t){{x, y}, {width, height}},
                               ui.colors.active_overlay);
    }
    else if (ui.hovered_id == ui_current_id())
    {
        ui.renderer->draw_quad(ui.renderer,
                               (quad_i32_t){{x, y}, {width, height}},
                               ui.colors.hover_overlay);
    }
}

void ui_draw_text(const char* txt, int32_t x, int32_t y)
{
    ui.renderer->draw_shadowed_text(
        ui.renderer,
        txt,
        x + ui.padding,
        y + ui.padding - 3, // TODO(octave) : why 3 ? font ascent ?
        ui.colors.text,
        ui.colors.text_shadow);
}

bool ui_slider(const char* txt, float* value, float min, float max)
{
    ui_push_string_id(txt);

    uint32_t box_width = 200;
    uint32_t box_height = ui.line_height;
    int32_t box_x = ui.cursor_x;
    int32_t box_y = ui.cursor_y;

    float range = max - min;
    bool finiteRange = range < INFINITY;
    float unitsToPixels = finiteRange ? box_width / range : 100.0f;

    bool changed = false;

    int32_t dx;
    if (ui_handle_drag_x(box_x, box_y, box_width, box_height, &dx))
    {
        *value += (float)dx / unitsToPixels;
        *value = clamped_float(*value, min, max);
    }

    ui.renderer->draw_quad(
        ui.renderer,
        (quad_i32_t){{box_x, box_y}, {box_width, box_height}},
        ui.colors.secondary);
    if (finiteRange)
    {
        int32_t slider_position =
            unitsToPixels * (clamped_float(*value, min, max) - min);

        ui.renderer->draw_quad(
            ui.renderer,
            (quad_i32_t){{box_x, box_y}, {slider_position, box_height}},
            ui.colors.main);
    }

    char* value_txt = tprintf(mem, ui.tmp_stack, "%.3f", *value);

    ui_draw_text(value_txt, box_x, box_y);

    ui_draw_hover_and_active_overlay(box_x, box_y, box_width, box_height);

    ui_pop_id();

    ui_newline();

    return changed;
}

bool ui_button(const char* txt)
{
    ui_push_string_id(txt);

    int32_t width =
        ui.renderer->get_text_width(ui.renderer, txt) + 2 * ui.padding;
    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool result = ui_handle_hold_and_release(x, y, width, height);

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    ui.renderer->draw_quad(ui.renderer, pos_quad, ui.colors.main);

    ui_draw_text(txt, x, y);

    ui_draw_hover_and_active_overlay(x, y, width, height);

    ui_pop_id();

    ui_newline();

    return result;
}

quad_i32_t quad_i32_grown(quad_i32_t q, int32_t offset)
{
    return (quad_i32_t){
        .min = {q.min[0] + offset, q.min[1] + offset},
        .extent = {q.extent[0] - 2 * offset, q.extent[1] - 2 * offset}};
}

bool ui_checkbox(const char* txt, bool* value)
{
    ui_push_string_id(txt);

    /* int32_t width = */
    /* ui.renderer->font.bbox.extent[0] * strlen(txt) + 2 * ui.padding; */
    int32_t width = ui.line_height;
    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool clicked = ui_handle_hold_and_release(x, y, width, height);
    if (clicked)
    {
        *value = !*value;
    }

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    ui.renderer->draw_quad(ui.renderer, pos_quad, ui.colors.secondary);

    if (*value)
    {
        ui.renderer->draw_quad(ui.renderer,
                               quad_i32_grown(pos_quad, width / 4),
                               ui.colors.main);
    }

    /* ui_draw_text(txt, x, y); */

    ui_draw_hover_and_active_overlay(x, y, width, height);

    ui_pop_id();

    ui_newline();

    return clicked;
}

void ui_init(mem_stack_allocator_o* tmp_stack,
             renderer_i* renderer,
             platform_input_info_t* input)
{
    ui.tmp_stack = tmp_stack;
    ui.renderer = renderer;
    ui.input = input;

    ui.colors.main = color_gray(0x60);
    ui.colors.secondary = color_gray(0x30);
    ui.colors.background = color_gray(0x10);

    ui.colors.text = color_gray(0xFF);
    ui.colors.text_shadow = color_gray(0x00);

    ui.colors.hover_overlay = color_rgba(0xFF, 0xFF, 0xFF, 0x40);
    ui.colors.active_overlay = color_rgba(0xFF, 0xFF, 0xFF, 0x20);

    ui.padding = 6;
    ui.line_height = ui.renderer->get_font_height(ui.renderer) + 2 * ui.padding;
    ui.margin = 4;

    ui_push_id(1);
}

void ui_begin_node(const char* name, ui_node_t* node)
{
    ASSERT(!ui.current_node);
    ASSERT(!ui.current_node_name);

    ui.current_node = node;
    ui.current_node_name = name;

    ui_push_string_id(name);

    int32_t x = node->bbox.min[0];
    int32_t y = node->bbox.min[1];
    uint32_t width = node->bbox.extent[0];
    /* uint32_t height = node->bbox.extent.height; */

    ui.renderer->draw_quad(ui.renderer, node->bbox, ui.colors.background);
    ui.renderer->draw_quad(ui.renderer,
                           (quad_i32_t){{x, y}, {width, ui.line_height}},
                           ui.colors.main);

    ui_draw_text(ui.current_node_name, x, y);

    ui_begin_draw_region(x, y + ui.line_height);
}

void ui_end_node()
{
    ASSERT(ui.current_node);

    ui_node_t* node = ui.current_node;

    int32_t x = node->bbox.min[0];
    int32_t y = node->bbox.min[1];
    uint32_t width = node->bbox.extent[0];
    uint32_t height = node->bbox.extent[1];

    int32_t dx, dy;
    if (ui_handle_drag(x, y, width, height, &dx, &dy))
    {
        uint32_t border_width = 5;
        int32_t xprev = ui.input->mouse_x - ui.input->mouse_dx;
        int32_t yprev = ui.input->mouse_y - ui.input->mouse_dy;

        bool on_horizontal_border = x + width - xprev <= border_width;
        bool on_vertical_border = y + height - yprev <= border_width;

        if (on_horizontal_border || on_vertical_border)
        {
            if (on_horizontal_border)
            {
                node->bbox.extent[0] += dx;
            }
            if (on_vertical_border)
            {
                node->bbox.extent[1] += dy;
            }
        }
        else
        {
            node->bbox.min[0] += dx;
            node->bbox.min[1] += dy;
        }
    }

    ui_end_draw_region();

    ui_pop_id();
    ui.current_node = 0;
    ui.current_node_name = 0;
}

void ui_plug(const char* name)
{
    ui_push_string_id(name);
    ASSERT(ui.current_node);

    uint32_t draw_width = 10;
    uint32_t hover_width = 30;
    int32_t draw_x = ui.cursor_x - draw_width;
    int32_t draw_y = ui.cursor_y + ui.line_height / 2 - draw_width / 2;

    int32_t hover_x = ui.cursor_x - draw_width / 2 - hover_width / 2;
    int32_t hover_y = ui.cursor_y + ui.line_height / 2 - hover_width / 2;

    ui.renderer->draw_quad(
        ui.renderer,
        (quad_i32_t){{draw_x, draw_y}, {draw_width, draw_width}},
        color_rgb(0xFF, 0x00, 0x00));

    ui_draw_text(name, ui.cursor_x, ui.cursor_y);
    ui_newline();

    ui_handle_hold_and_release(hover_x, hover_y, hover_width, hover_width);

    if (ui.active_id == ui_current_id())
    {
        int32_t cx = draw_x + draw_width / 2;
        int32_t cy = draw_y + draw_width / 2;
        ui.renderer->draw_line(ui.renderer,
                               cx,
                               cy,
                               ui.input->mouse_x,
                               ui.input->mouse_y,
                               4.0f,
                               color_rgb(0x00, 0x00, 0xff));
    }
    else if (ui.hovered_id == ui_current_id() && ui.dropped_id)
    {
        log_debug("Tried to connect %lxu to %lxu",
                  ui.dropped_id,
                  ui.hovered_id);
    }

    ui_pop_id();
}

void ui_begin()
{
    ui_begin_draw_region(0, 0);
    ui.hovered_id = 0;
    ui.dropped_id = 0;
    if (ui.input->mouse_released & MOUSE_BUTTON_LEFT)
    {
        ui.dropped_id = ui.active_id;
    }
}

void ui_end()
{
    ui_end_draw_region();
    ASSERT(ui.draw_region_stack_height == 0);

    ASSERT(ui.id_stack_height == 1);
}

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
    uint16_t typ = db->add_object_type(mydb, ARRAY_COUNT(props), props);

    property_definition_t nprops[] = {
        {.name = "subobject", .type = PTYPE_OBJECT, .object_type = typ},
        {.name = "reference", .type = PTYPE_REFERENCE, .object_type = typ},
    };
    uint16_t ntyp = db->add_object_type(mydb, ARRAY_COUNT(nprops), nprops);

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

    log_init(mem->vm);

    test_hash();
    test_db();

    void* tmp_stack_buf = mem_alloc(mem->vm, Gibi(1024));
    mem_stack_allocator_o* tmp_alloc =
        mem->stack_create(tmp_stack_buf, Gibi(1024));

    ASSERT(sizeof(void*) == sizeof(uint64_t));
    void* permanent_stack_buf = mem_alloc(mem->vm, Gibi(1024));
    mem_stack_allocator_o* permanent_alloc =
        mem->stack_create(permanent_stack_buf, Gibi(1024));

    renderer_i* renderer = render_api->create(mem, permanent_alloc, tmp_alloc);

    platform_input_info_t input = {0};

    ui_init(tmp_alloc, renderer, &input);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float freq = 1.0f;

    uint64_t t0 = platform_get_nanoseconds();

    ui_node_t node = {{{10, 10}, {200, 200}}};
    ui_node_t node2 = {{{10, 10}, {200, 200}}};

    // main loop
    while (!input.should_exit)
    {
        mem->stack_revert(tmp_alloc, 0);
        /* log_debug("active = %lu, hovered = %lu", ui.active_id, ui.hovered_id); */
        uint64_t now = platform_get_nanoseconds();

        float t = (now - t0) * 1e-9f;
        float y = sinf(t);

        platform_handle_input_events(&input);

        ui_begin();

        if (ui_button("Do the thingy"))
        {
            log_debug("Did the thingy");
        }
        if (ui_button("Undo the thingy"))
        {
        }

        static bool b;
        if (ui_checkbox("Redo the thingy", &b))
        {
        }

        ui_begin_node("my node", &node);
        ui_plug("The value");
        ui_plug("The other value");
        ui_end_node();

        ui_begin_node("my other node", &node2);
        ui_plug("amazing value");
        ui_end_node();

        y = sinf(freq * t);
        ui_slider("freq", &freq, 0.0f, 10.0f);
        ui_slider("sin(freq * t)", &y, -1.0f, 1.0f);

        /* char* txt = tprintf(mem, tmp_alloc, "Used : %zu", tmp_alloc->used); */
        /* ui_draw_text(txt, 0, 300); */

        ui_end();

        renderer->do_draw(renderer, input.width, input.height);
        platform_swap_buffers();

        log_flush();
    }

    log_terminate();

    return 0;
}
