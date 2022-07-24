#include "ui.h"

#include "assert.h"
#include "color.h"
#include "hash.h"
#include "memory.h"
#include "platform.h"
#include "plugin_sdk.h"
#include "renderer.h"
#include "stretchy_buffer.h"
#include "util.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum drag_drop_state_e
{
    DRAG_DROP_NONE,
    DRAG_DROP_DRAGGING,
    DRAG_DROP_DROPPED,
};

typedef struct draw_region_t
{
    int32_t prev_cursor_x;
    int32_t prev_cursor_y;
    quad_i32_t box;
} draw_region_t;

static struct
{
    mem_allocator_i* alloc;
    renderer_i* renderer;

    ui_mouse_t mouse;
    uint8_t typed_utf8[64];

    int32_t cursor_x;
    int32_t cursor_y;

    int32_t prev_cursor_x;
    int32_t prev_cursor_y;

    uint64_t active_id;
    uint64_t hovered_id;
    uint64_t focus_id;

    uint32_t drag_state;
    uint64_t drag_source_id;
    /* array */ uint8_t* drag_payload;

    uint64_t id_stack[1024];
    uint32_t id_stack_height;

    draw_region_t draw_region_stack[1024];
    uint32_t draw_region_stack_height;

    color_t colors[UI_COLOR_COUNT];

    uint32_t padding;
    uint32_t margin;
    uint32_t line_height;
} ui;

static ui_mouse_t get_mouse() { return ui.mouse; }

static color_t* get_color(uint32_t name)
{
    ASSERT(name < UI_COLOR_COUNT);
    return &ui.colors[name];
}

static bool
mouse_inside_region(int32_t x, int32_t y, int32_t width, int32_t height)
{
    return ui.mouse.x >= x && ui.mouse.x < x + width //
           && ui.mouse.y >= y && ui.mouse.y < y + height;
}

static void push_id(uint64_t id)
{
    ASSERT(ui.id_stack_height < STATIC_ARRAY_COUNT(ui.id_stack));
    uint64_t hashed =
        ui.id_stack_height
            ? hash_combine(ui.id_stack[ui.id_stack_height - 1], id)
            : id;
    ASSERT(hashed);
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

static void draw_text(const char* txt, int32_t x, int32_t y)
{
    ui.renderer->draw_shadowed_text(
        ui.renderer,
        txt,
        x + ui.padding + 1, // TODO(octave) : why +1 ?
        y + ui.padding - 3, // TODO(octave) : why 3 ? font ascent ?
        ui.colors[UI_COLOR_TEXT],
        ui.colors[UI_COLOR_TEXT_SHADOW]);
}

static uint32_t get_line_height() { return ui.line_height; }

static uint32_t get_margin() { return ui.margin; }

static void draw_quad(quad_i32_t pos, color_t color)
{
    ui.renderer->draw_quad(ui.renderer, pos, color);
}

static void
draw_line(float x1, float y1, float x2, float y2, float width, color_t color)
{
    ui.renderer->draw_line(ui.renderer, x1, y1, x2, y2, width, color);
}

static void begin_draw_region(int32_t x, int32_t y, int32_t w, int32_t h)
{
    ASSERT(ui.draw_region_stack_height
           < STATIC_ARRAY_COUNT(ui.draw_region_stack));

    ui.draw_region_stack[ui.draw_region_stack_height++] = (draw_region_t){
        .prev_cursor_x = ui.cursor_x,
        .prev_cursor_y = ui.cursor_y,
        .box = {.min = {x, y}, .extent = {w, h}},
    };

    ui.cursor_x = x + ui.margin;
    ui.cursor_y = y + ui.margin;
}

static void end_draw_region()
{
    ASSERT(ui.draw_region_stack_height);

    draw_region_t* region =
        &ui.draw_region_stack[--ui.draw_region_stack_height];

    ui.cursor_x = region->prev_cursor_x;
    ui.cursor_y = region->prev_cursor_y;
}

static double clamped_double(double t, double min, double max)
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

static bool hover_rect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    bool is_inside = mouse_inside_region(x, y, w, h);
    bool result;
    if (!ui.hovered_id && is_inside)
    {
        ui.hovered_id = current_id();
        result = true;
    }
    else if (ui.hovered_id == current_id() && !is_inside)
    {
        ui.hovered_id = 0;
        result = false;
    }
    else
    {
        result = ui.hovered_id == current_id();
    }

    return result;
}

static bool hold_rect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    bool hovered = hover_rect(x, y, w, h);
    bool result;

    if (hovered && (ui.mouse.pressed & MOUSE_BUTTON_LEFT) && !ui.active_id)
    {
        ui.active_id = current_id();
        result = false;
    }
    else if (ui.active_id == current_id()
             && (ui.mouse.released & MOUSE_BUTTON_LEFT))
    {
        ui.active_id = 0;
        result = true;
    }
    else
    {
        result = false;
    }

    return result;
}

static bool
drag_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t* dx, int32_t* dy)
{
    hold_rect(x, y, w, h);

    bool result = false;
    if (ui.active_id == current_id())
    {
        *dx = ui.mouse.dx;
        *dy = ui.mouse.dy;

        result = *dx || *dy;
    }

    return result;
}

static bool drag_rect_x(int32_t x, int32_t y, int32_t w, int32_t h, int32_t* dx)
{
    int32_t dy;
    return drag_rect(x, y, w, h, dx, &dy) && *dx;
}

static bool drag_rect_y(int32_t x, int32_t y, int32_t w, int32_t h, int32_t* dy)
{
    int32_t dx;
    return drag_rect(x, y, w, h, &dx, dy) && *dy;
}

static bool drag_and_drop_source(const void* payload, uint32_t size)
{
    if (current_id() == ui.hovered_id && (ui.mouse.pressed & MOUSE_BUTTON_LEFT)
        && ui.drag_state == DRAG_DROP_NONE)
    {
        array_reserve(ui.alloc, ui.drag_payload, size);
        memcpy(ui.drag_payload, payload, size);

        ui.drag_source_id = current_id();
        ui.drag_state = DRAG_DROP_DRAGGING;

        return true;
    }
    else
    {
        return false;
    }
}

static bool get_drag_and_drop_payload(void* payload, uint32_t size)
{
    if (ui.drag_state == DRAG_DROP_DRAGGING
        || ui.drag_state == DRAG_DROP_DROPPED)
    {
        memcpy(payload, ui.drag_payload, size);
        return true;
    }
    else
    {
        return false;
    }
}

static bool drag_and_drop_target(void* payload, uint32_t size)
{
    if (ui.hovered_id == current_id() && ui.drag_state == DRAG_DROP_DROPPED)
    {
        memcpy(payload, ui.drag_payload, size);
        return true;
    }
    else
    {
        return false;
    }
}

static draw_region_t* current_region()
{
    return &ui.draw_region_stack[ui.draw_region_stack_height - 1];
}

static int32_t get_cursor_x() { return ui.cursor_x; }

static int32_t get_cursor_y() { return ui.cursor_y; }

static void set_cursor(int32_t x, int32_t y)
{
    ui.cursor_x = x;
    ui.cursor_y = y;
}

static void new_line()
{
    ui.prev_cursor_x = ui.cursor_x;
    ui.prev_cursor_y = ui.cursor_y;

    ui.cursor_x = current_region()->box.min[0] + ui.margin;
    ui.cursor_y += ui.line_height + ui.margin;
}

static void same_line()
{
    ui.cursor_x = ui.prev_cursor_x;
    ui.cursor_y = ui.prev_cursor_y;
}

static void draw_hover_and_active_overlay(int32_t x,
                                          int32_t y,
                                          uint32_t width,
                                          uint32_t height)
{
    if (ui.active_id == current_id())
    {
        draw_quad((quad_i32_t){{x, y}, {width, height}},
                  ui.colors[UI_COLOR_ACTIVE_OVERLAY]);
    }
    else if (ui.hovered_id == current_id())
    {
        draw_quad((quad_i32_t){{x, y}, {width, height}},
                  ui.colors[UI_COLOR_HOVER_OVERLAY]);
    }
}

static int32_t remaining_region_x()
{
    return current_region()->box.min[0] + current_region()->box.extent[0]
           - ui.cursor_x;
}

static bool slider_ex(const char* txt,
                      double* value,
                      double min,
                      double max,
                      const char* format)
{
    push_string_id(txt);

    int32_t box_width = remaining_region_x();
    int32_t box_height = ui.line_height;
    int32_t box_x = ui.cursor_x;
    int32_t box_y = ui.cursor_y;

    double range = max - min;
    bool finiteRange = range > 0 && range < INFINITY;
    double unitsToPixels = finiteRange ? box_width / range : 1.0;

    bool changed = false;

    int32_t dx;
    if (drag_rect_x(box_x, box_y, box_width, box_height, &dx))
    {
        *value += (double)dx / unitsToPixels;
        changed = true;
    }

    if (min < max)
    {
        *value = clamped_double(*value, min, max);
    }

    draw_quad((quad_i32_t){{box_x, box_y}, {box_width, box_height}},
              ui.colors[UI_COLOR_SECONDARY]);
    if (finiteRange)
    {
        int32_t slider_position =
            unitsToPixels * (clamped_double(*value, min, max) - min);

        draw_quad((quad_i32_t){{box_x, box_y}, {slider_position, box_height}},
                  ui.colors[UI_COLOR_MAIN]);
    }

    char value_txt[256];
    snprintf(value_txt, sizeof(value_txt), format, *value);

    draw_text(value_txt, box_x, box_y);

    draw_hover_and_active_overlay(box_x, box_y, box_width, box_height);

    pop_id();

    new_line();

    return changed;
}

static bool slider_float(const char* txt, float* value, float min, float max)
{
    double v = *value;

    bool result = slider_ex(txt, &v, min, max, "%.3f");

    *value = v;

    return result;
}

static bool
slider_int(const char* txt, int32_t* value, int32_t min, int32_t max)
{
    double v = *value;

    bool result = slider_ex(txt, &v, min, max, "%.0f");

    *value = v;

    return result;
}

static int32_t get_text_width(const char* txt)
{
    return ui.renderer->get_text_width(ui.renderer, txt) + 2 * ui.padding;
}

static bool button(const char* txt)
{
    push_string_id(txt);

    int32_t width = get_text_width(txt);

    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool result = hold_rect(x, y, width, height);

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    draw_quad(pos_quad, ui.colors[UI_COLOR_MAIN]);

    draw_text(txt, x, y);

    draw_hover_and_active_overlay(x, y, width, height);

    pop_id();

    new_line();

    return result;
}

static bool checkbox(const char* txt, bool* value)
{
    push_string_id(txt);

    int32_t width = ui.line_height;
    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool clicked = hold_rect(x, y, width, height);
    if (clicked)
    {
        *value = !*value;
    }

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    draw_quad(pos_quad, ui.colors[UI_COLOR_SECONDARY]);

    if (*value)
    {
        draw_quad(quad_i32_grown(pos_quad, -width / 4),
                  ui.colors[UI_COLOR_MAIN]);
    }

    draw_text(txt, x + width, y);

    draw_hover_and_active_overlay(x, y, width, height);

    pop_id();

    new_line();

    return clicked;
}

static uint32_t decode_utf8_code_point(uint8_t* bytes, uint32_t* result)
{
    if (!*bytes)
    {
        return 0;
    }

    uint32_t trash;
    if (!result)
    {
        result = &trash;
    }

    uint32_t i = 0;
    enum
    {
        START,
        NEED_3_BYTES,
        NEED_2_BYTES,
        NEED_1_BYTE,
        DONE,
    };

    uint32_t state = START;
    *result = 0;

    while (state != DONE)
    {
        uint8_t byte = bytes[i++];

        bool bit7 = byte & (1 << 7);
        bool bit6 = byte & (1 << 6);
        bool bit5 = byte & (1 << 5);
        bool bit4 = byte & (1 << 4);
        bool bit3 = byte & (1 << 3);

        switch (state)
        {
        case START:
            if (bit7)
            {
                ASSERT(bit6);
                if (bit5)
                {
                    if (bit4)
                    {
                        ASSERT(!bit3);
                        // 11110...
                        state = NEED_3_BYTES;
                        *result = (byte & 0x7) << 18;
                    }
                    else
                    {
                        // 1110....
                        state = NEED_2_BYTES;
                        *result = (byte & 0xF) << 12;
                    }
                }
                else
                {
                    // 110.....
                    state = NEED_1_BYTE;
                    *result = (byte & 0x1F) << 6;
                }
            }
            else
            {
                // 0.......
                *result = byte;
                state = DONE;
            }
            break;
        case NEED_3_BYTES:
            ASSERT(bit7 && !bit6);
            *result |= (byte & 0x3F) << 12;
            state = NEED_2_BYTES;
            break;
        case NEED_2_BYTES:
            ASSERT(bit7 && !bit6);
            *result |= (byte & 0x3F) << 6;
            state = NEED_1_BYTE;
            break;
        case NEED_1_BYTE:
            ASSERT(bit7 && !bit6);
            *result |= (byte & 0x3F) << 0;
            state = DONE;
        case DONE:
            break;
        }
    }

    return i;
}

static void text_box(const char* label, char* buffer, uint32_t size)
{
    push_string_id(label);

    int32_t width = remaining_region_x();

    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool clicked = hold_rect(x, y, width, height);

    if (ui.focus_id == current_id())
    {
        uint32_t cursor = 0;
        while (buffer[cursor] && cursor < size - 1)
        {
            cursor++;
        }

        uint8_t* typed = ui.typed_utf8;
        uint32_t code_point;
        uint32_t bytes;

        while ((bytes = decode_utf8_code_point(typed, &code_point)))
        {
            if (code_point == 8) // backspace
            {
                if (cursor > 0)
                {
                    buffer[--cursor] = '\0';
                }
            }
            else if (bytes <= size - 1 - cursor)
            {
                memcpy(&buffer[cursor], typed, bytes);
            }
            else
            {
                break;
            }

            typed += bytes;
        }
    }

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    draw_quad(pos_quad, ui.colors[UI_COLOR_MAIN]);

    draw_text(buffer, x, y);

    draw_hover_and_active_overlay(x, y, width, height);

    pop_id();

    new_line();
}

static void text(const char* txt)
{
    draw_text(txt, ui.cursor_x, ui.cursor_y);
    ui.cursor_x += get_text_width(txt);

    new_line();
}

static void init(mem_allocator_i* alloc, renderer_i* renderer)
{
    ui.alloc = alloc;
    ui.renderer = renderer;

    {
        ui.colors[UI_COLOR_MAIN] = color_gray(0x60);
        ui.colors[UI_COLOR_SECONDARY] = color_gray(0x30);
        ui.colors[UI_COLOR_BACKGROUND] = color_gray(0x10);

        ui.colors[UI_COLOR_TEXT] = color_gray(0xFF);
        ui.colors[UI_COLOR_TEXT_SHADOW] = color_gray(0x00);

        ui.colors[UI_COLOR_HOVER_OVERLAY] = color_rgba(0xFF, 0xFF, 0xFF, 0x40);
        ui.colors[UI_COLOR_ACTIVE_OVERLAY] = color_rgba(0xFF, 0xFF, 0xFF, 0x20);
    }

    ui.padding = 6;
    ui.line_height = ui.renderer->get_font_height(ui.renderer) + 2 * ui.padding;
    ui.margin = 4;

    push_id(1);
}

static void terminate()
{
    // TODO(octave)
}

static void begin_frame(const platform_input_info_t* input)
{
    begin_draw_region(0, 0, 200, INT32_MAX);

    ASSERT(sizeof(ui.typed_utf8) == sizeof(input->typed_utf8));
    memcpy(ui.typed_utf8, input->typed_utf8, sizeof(ui.typed_utf8));

    ui.mouse.x = input->mouse_x;
    ui.mouse.dx = input->mouse_dx;
    ui.mouse.y = input->mouse_y;
    ui.mouse.dy = input->mouse_dy;
    ui.mouse.pressed = input->mouse_pressed;
    ui.mouse.released = input->mouse_released;

    ui.hovered_id = 0;
    if (ui.drag_state == DRAG_DROP_DRAGGING
        && (ui.mouse.released & MOUSE_BUTTON_LEFT))
    {
        ui.drag_state = DRAG_DROP_DROPPED;
    }
}

static void end_frame()
{
    end_draw_region();

    ASSERT(ui.draw_region_stack_height == 0);
    ASSERT(ui.id_stack_height == 1);

    if (ui.drag_state == DRAG_DROP_DROPPED)
    {
        ui.drag_state = DRAG_DROP_NONE;
        ui.drag_source_id = 0;
        ui.drag_payload = 0;
    }

    if (ui.active_id)
    {
        ui.focus_id = ui.active_id;
    }
    else if (!ui.hovered_id && ui.mouse.pressed & MOUSE_BUTTON_LEFT)
    {
        ui.focus_id = 0;
    }
}

static void load(void* api)
{
    oui_api* ui_api = api;

    ui_api->begin_draw_region = begin_draw_region;
    ui_api->begin_frame = begin_frame;
    ui_api->button = button;
    ui_api->checkbox = checkbox;
    ui_api->current_id = current_id;
    ui_api->drag_and_drop_source = drag_and_drop_source;
    ui_api->drag_and_drop_target = drag_and_drop_target;
    ui_api->draw_line = draw_line;
    ui_api->draw_quad = draw_quad;
    ui_api->draw_text = draw_text;
    ui_api->end_draw_region = end_draw_region;
    ui_api->end_frame = end_frame;
    ui_api->get_line_height = get_line_height;
    ui_api->get_margin = get_margin;
    ui_api->get_mouse = get_mouse;
    ui_api->get_color = get_color;
    ui_api->get_cursor_x = get_cursor_x;
    ui_api->get_cursor_y = get_cursor_y;
    ui_api->get_drag_and_drop_payload = get_drag_and_drop_payload;
    ui_api->set_cursor = set_cursor;
    ui_api->hover_rect = hover_rect;
    ui_api->hold_rect = hold_rect;
    ui_api->drag_rect = drag_rect;
    ui_api->init = init;
    ui_api->new_line = new_line;
    ui_api->pop_id = pop_id;
    ui_api->push_id = push_id;
    ui_api->push_string_id = push_string_id;
    ui_api->same_line = same_line;
    ui_api->slider_float = slider_float;
    ui_api->slider_int = slider_int;
    ui_api->terminate = terminate;
    ui_api->text = text;
    ui_api->text_box = text_box;
}

plugin_spec_t PLUGIN_SPEC = {
    .name = "oui",
    .version = {0, 0, 1},
    .load = load,
    .api_size = sizeof(oui_api),
};
