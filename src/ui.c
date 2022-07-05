#include "ui.h"

#include "assert.h"
#include "color.h"
#include "hash.h"
#include "memory.h"
#include "platform.h"
#include "plugin_sdk.h"
#include "renderer.h"
#include "util.h"

#include <math.h>

enum drag_drop_state_e
{
    DRAG_DROP_NONE,
    DRAG_DROP_DRAGGING,
    DRAG_DROP_DROPPED,
};

struct
{
    mem_api* mem;

    mem_stack_allocator_o* tmp_stack;
    platform_input_info_t* input;
    renderer_i* renderer;

    int32_t cursor_x;
    int32_t cursor_y;

    uint64_t active_id;
    uint64_t hovered_id;

    const void* drag_payload;
    uint32_t drag_state;

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

    quad_i32_t* current_node;
    const char* current_node_name;
} ui;

bool mouse_inside_region(int32_t x, int32_t y, int32_t width, int32_t height)
{
    return ui.input->mouse_x >= x && ui.input->mouse_x < x + width //
           && ui.input->mouse_y >= y && ui.input->mouse_y < y + height;
}

static void push_id(uint64_t id)
{
    ASSERT(ui.id_stack_height < STATIC_ARRAY_COUNT(ui.id_stack));
    uint64_t hashed =
        ui.id_stack_height
            ? hash_combine(ui.id_stack[ui.id_stack_height - 1], id)
            : id;
    ui.id_stack[ui.id_stack_height++] = hashed;
}

static void push_string_id(const char* txt) { push_id(hash_string(txt)); }

static void pop_id()
{
    ASSERT(ui.id_stack_height);
    ui.id_stack_height--;
}

static uint64_t current_id()
{
    ASSERT(ui.id_stack_height);

    return ui.id_stack[ui.id_stack_height - 1];
}

static void begin_draw_region(int32_t x, int32_t y)
{
    ASSERT(ui.draw_region_stack_height
           < STATIC_ARRAY_COUNT(ui.draw_region_stack));

    ui.draw_region_stack[ui.draw_region_stack_height][0] = ui.cursor_x;
    ui.draw_region_stack[ui.draw_region_stack_height][1] = ui.cursor_y;
    ui.draw_region_stack_height++;

    ui.cursor_x = x + ui.margin;
    ui.cursor_y = y + ui.margin;
}

static void end_draw_region()
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

bool handle_hover(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    bool is_inside = mouse_inside_region(x, y, w, h);

    if (!ui.hovered_id && is_inside)
    {
        ui.hovered_id = current_id();
        return true;
    }
    else if (ui.hovered_id == current_id() && !is_inside)
    {
        ui.hovered_id = 0;
        return false;
    }

    return ui.hovered_id == current_id();
}

bool handle_hold_and_release(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    bool hovered = handle_hover(x, y, w, h);

    if (hovered && (ui.input->mouse_pressed & MOUSE_BUTTON_LEFT)
        && !ui.active_id)
    {
        ui.active_id = current_id();
        return false;
    }
    else if (ui.active_id == current_id()
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

bool handle_drag(int32_t x,
                 int32_t y,
                 uint32_t w,
                 uint32_t h,
                 int32_t* dx,
                 int32_t* dy)
{
    handle_hold_and_release(x, y, w, h);

    if (ui.active_id == current_id())
    {
        *dx = ui.input->mouse_dx;
        *dy = ui.input->mouse_dy;

        return *dx || *dy;
    }

    return false;
}

bool handle_drag_x(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t* dx)
{
    int32_t dy;
    return handle_drag(x, y, w, h, dx, &dy) && *dx;
}

bool handle_drag_y(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t* dy)
{
    int32_t dx;
    return handle_drag(x, y, w, h, &dx, dy) && *dy;
}

void start_drag_and_drop(const void* payload)
{
    ASSERT(ui.drag_state == DRAG_DROP_NONE);

    ui.drag_payload = payload;
    ui.drag_state = DRAG_DROP_DRAGGING;
}

static void newline() { ui.cursor_y += ui.line_height + ui.margin; }

static void draw_hover_and_active_overlay(int32_t x,
                                          int32_t y,
                                          uint32_t width,
                                          uint32_t height)
{
    if (ui.active_id == current_id())
    {
        ui.renderer->draw_quad(ui.renderer,
                               (quad_i32_t){{x, y}, {width, height}},
                               ui.colors.active_overlay);
    }
    else if (ui.hovered_id == current_id())
    {
        ui.renderer->draw_quad(ui.renderer,
                               (quad_i32_t){{x, y}, {width, height}},
                               ui.colors.hover_overlay);
    }
}

static void draw_text(const char* txt, int32_t x, int32_t y)
{
    ui.renderer->draw_shadowed_text(
        ui.renderer,
        txt,
        x + ui.padding,
        y + ui.padding - 3, // TODO(octave) : why 3 ? font ascent ?
        ui.colors.text,
        ui.colors.text_shadow);
}

static bool slider(const char* txt, float* value, float min, float max)
{
    push_string_id(txt);

    uint32_t box_width = 200;
    uint32_t box_height = ui.line_height;
    int32_t box_x = ui.cursor_x;
    int32_t box_y = ui.cursor_y;

    float range = max - min;
    bool finiteRange = range < INFINITY;
    float unitsToPixels = finiteRange ? box_width / range : 100.0f;

    bool changed = false;

    int32_t dx;
    if (handle_drag_x(box_x, box_y, box_width, box_height, &dx))
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

    char* value_txt = tprintf(ui.mem, ui.tmp_stack, "%.3f", *value);

    draw_text(value_txt, box_x, box_y);

    draw_hover_and_active_overlay(box_x, box_y, box_width, box_height);

    pop_id();

    newline();

    return changed;
}

bool button(const char* txt)
{
    push_string_id(txt);

    int32_t width =
        ui.renderer->get_text_width(ui.renderer, txt) + 2 * ui.padding;
    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool result = handle_hold_and_release(x, y, width, height);

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    ui.renderer->draw_quad(ui.renderer, pos_quad, ui.colors.main);

    draw_text(txt, x, y);

    draw_hover_and_active_overlay(x, y, width, height);

    pop_id();

    newline();

    return result;
}

quad_i32_t quad_i32_grown(quad_i32_t q, int32_t offset)
{
    return (quad_i32_t){
        .min = {q.min[0] + offset, q.min[1] + offset},
        .extent = {q.extent[0] - 2 * offset, q.extent[1] - 2 * offset}};
}

bool checkbox(const char* txt, bool* value)
{
    push_string_id(txt);

    /* int32_t width = */
    /* ui.renderer->font.bbox.extent[0] * strlen(txt) + 2 * ui.padding; */
    int32_t width = ui.line_height;
    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool clicked = handle_hold_and_release(x, y, width, height);
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

    /* draw_text(txt, x, y); */

    draw_hover_and_active_overlay(x, y, width, height);

    pop_id();

    newline();

    return clicked;
}

static void init(mem_api* mem,
                 mem_stack_allocator_o* tmp_stack,
                 renderer_i* renderer,
                 platform_input_info_t* input)
{
    ui.mem = mem;

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

    push_id(1);
}

static void terminate()
{
    // TODO(octave)
}

static void begin_node(const char* name, quad_i32_t* node)
{
    ASSERT(!ui.current_node);
    ASSERT(!ui.current_node_name);

    ui.current_node = node;
    ui.current_node_name = name;

    push_string_id(name);

    int32_t x = node->min[0];
    int32_t y = node->min[1];
    uint32_t width = node->extent[0];
    /* uint32_t height = node->extent.height; */

    ui.renderer->draw_quad(ui.renderer, *node, ui.colors.background);
    ui.renderer->draw_quad(ui.renderer,
                           (quad_i32_t){{x, y}, {width, ui.line_height}},
                           ui.colors.main);

    draw_text(ui.current_node_name, x, y);

    begin_draw_region(x, y + ui.line_height);
}

static void end_node()
{
    ASSERT(ui.current_node);

    quad_i32_t* node = ui.current_node;

    int32_t x = node->min[0];
    int32_t y = node->min[1];
    uint32_t width = node->extent[0];
    uint32_t height = node->extent[1];

    int32_t dx, dy;
    if (handle_drag(x, y, width, height, &dx, &dy))
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
                node->extent[0] += dx;
            }
            if (on_vertical_border)
            {
                node->extent[1] += dy;
            }
        }
        else
        {
            node->min[0] += dx;
            node->min[1] += dy;
        }
    }

    end_draw_region();

    pop_id();
    ui.current_node = 0;
    ui.current_node_name = 0;
}

static void plug(const char* name)
{
    push_string_id(name);
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

    draw_text(name, ui.cursor_x, ui.cursor_y);
    newline();

    handle_hold_and_release(hover_x, hover_y, hover_width, hover_width);

    if (ui.active_id == current_id())
    {
        if (ui.drag_state == DRAG_DROP_NONE)
        {
            start_drag_and_drop(name);
        }
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
    else if (ui.hovered_id == current_id()
             && ui.drag_state == DRAG_DROP_DROPPED)
    {
        log_debug("Dropped '%s' onto '%s'", (const char*)ui.drag_payload, name);
    }

    pop_id();
}

static void begin()
{
    begin_draw_region(0, 0);
    ui.hovered_id = 0;
    if (ui.drag_state == DRAG_DROP_DRAGGING
        && (ui.input->mouse_released & MOUSE_BUTTON_LEFT))
    {
        ui.drag_state = DRAG_DROP_DROPPED;
    }
}

static void end()
{
    end_draw_region();
    ASSERT(ui.draw_region_stack_height == 0);

    ASSERT(ui.id_stack_height == 1);

    if (ui.drag_state == DRAG_DROP_DROPPED)
    {
        ui.drag_state = DRAG_DROP_NONE;
        ui.drag_payload = 0;
    }
}

static void* load()
{
    static oui_api ui_api;

    ui_api.init = init;
    ui_api.terminate = terminate;
    ui_api.push_id = push_id;
    ui_api.push_string_id = push_string_id;
    ui_api.pop_id = pop_id;
    ui_api.current_id = current_id;
    ui_api.push_id = push_id;
    ui_api.begin_draw_region = begin_draw_region;
    ui_api.end_draw_region = end_draw_region;
    ui_api.slider = slider;
    ui_api.button = button;
    ui_api.checkbox = checkbox;
    ui_api.begin_node = begin_node;
    ui_api.end_node = end_node;
    ui_api.plug = plug;
    ui_api.begin = begin;
    ui_api.end = end;

    return &ui_api;
}

plugin_spec_t PLUGIN_SPEC = {
    .name = "oui",
    .version = {1, 0, 0},
    .load = load,
    .unload = 0,
};
