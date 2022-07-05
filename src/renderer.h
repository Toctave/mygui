#pragma once

#include "base_types.h"

typedef struct renderer_o renderer_o;
typedef struct renderer_i renderer_i;

typedef struct mem_stack_allocator_o mem_stack_allocator_o;
typedef struct mem_api mem_api;

struct renderer_i
{
    // TODO(octave) : maybe use a blob here to avoid double
    // dereference?
    renderer_o* impl;
    void (*draw_quad)(struct renderer_i* renderer,
                      quad_i32_t pos,
                      color_t color);
    void (*draw_text)(renderer_i* renderer,
                      const char* text,
                      int32_t x,
                      int32_t y,
                      color_t color);
    void (*draw_shadowed_text)(renderer_i* renderer,
                               const char* text,
                               int32_t x,
                               int32_t y,
                               color_t text_color,
                               color_t shadow_color);
    void (*draw_line)(renderer_i* renderer,
                      float x1,
                      float y1,
                      float x2,
                      float y2,
                      float width,
                      color_t color);
    void (*do_draw)(renderer_i* renderer, uint32_t width, uint32_t height);

    // TODO(octave) : remove font-related stuff from the renderer
    uint32_t (*get_text_width)(renderer_i* renderer, const char* text);
    uint32_t (*get_font_height)(renderer_i* renderer);
};

typedef struct renderer_api
{
    renderer_i* (*create)(mem_api* mem,
                          mem_stack_allocator_o* permanent,
                          mem_stack_allocator_o* tmp);
    void (*destroy)(renderer_i* renderer);
} renderer_api;
