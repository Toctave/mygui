#pragma once

#include "base_types.h"

typedef struct renderer_i renderer_i;
typedef struct platform_input_info_t platform_input_info_t;
typedef struct mem_allocator_i mem_allocator_i;

typedef struct ui_mouse_t
{
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;

    uint32_t pressed;
    uint32_t released;
} ui_mouse_t;

typedef struct oui_api
{
    void (*init)(mem_allocator_i* alloc, renderer_i* renderer);
    void (*terminate)();

    void (*begin_frame)(const platform_input_info_t* input);
    void (*end_frame)();
    ui_mouse_t (*get_mouse)();

    void (*push_id)(uint64_t id);
    void (*push_string_id)(const char* txt);
    void (*pop_id)();
    uint64_t (*current_id)();

    void (*begin_draw_region)(int32_t x, int32_t y);
    void (*end_draw_region)();

    void (*draw_quad)(quad_i32_t pos, color_t color);
    void (*draw_line)(float x1,
                      float y1,
                      float x2,
                      float y2,
                      float width,
                      color_t color);
    void (*draw_text)(const char* txt, int32_t x, int32_t y);

    uint32_t (*get_line_height)();

    bool (*hover_rect)(int32_t x, int32_t y, uint32_t w, uint32_t h);
    bool (*hold_rect)(int32_t x, int32_t y, uint32_t w, uint32_t h);
    bool (*drag_rect)(int32_t x,
                      int32_t y,
                      uint32_t w,
                      uint32_t h,
                      int32_t* dx,
                      int32_t* dy);

    bool (*slider)(const char* txt, float* value, float min, float max);
    bool (*button)(const char* txt);
    bool (*checkbox)(const char* txt, bool* value);

    bool (*drag_and_drop_source)(const void* payload, uint32_t size);
    bool (*drag_and_drop_target)(void* payload, uint32_t size);
} oui_api;
