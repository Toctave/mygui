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

enum ui_color_e
{
    UI_COLOR_MAIN,
    UI_COLOR_SECONDARY,
    UI_COLOR_BACKGROUND,
    UI_COLOR_TEXT,
    UI_COLOR_TEXT_SHADOW,
    UI_COLOR_ACTIVE_OVERLAY,
    UI_COLOR_HOVER_OVERLAY,
    UI_COLOR_COUNT,
};

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

    void (*begin_draw_region)(int32_t x, int32_t y, int32_t w, int32_t h);
    void (*end_draw_region)();

    void (*draw_quad)(quad_i32_t pos, color_t color);
    void (*draw_line)(float x1,
                      float y1,
                      float x2,
                      float y2,
                      float width,
                      color_t color);
    void (*draw_text)(const char* txt, int32_t x, int32_t y);

    bool (*hover_rect)(int32_t x, int32_t y, int32_t w, int32_t h);
    bool (*hold_rect)(int32_t x, int32_t y, int32_t w, int32_t h);
    bool (*drag_rect)(int32_t x,
                      int32_t y,
                      int32_t w,
                      int32_t h,
                      int32_t* dx,
                      int32_t* dy);

    uint32_t (*get_line_height)();
    uint32_t (*get_margin)();
    int32_t (*get_cursor_x)();
    int32_t (*get_cursor_y)();
    void (*set_cursor)(int32_t x, int32_t y);
    void (*same_line)();
    void (*new_line)();

    void (*text)(const char* txt);
    bool (*slider_float)(const char* txt, float* value, float min, float max);
    bool (*slider_int)(const char* txt,
                       int32_t* value,
                       int32_t min,
                       int32_t max);
    bool (*button)(const char* txt);
    bool (*checkbox)(const char* txt, bool* value);
    void (*text_box)(const char* label, char* buffer, uint32_t size);

    bool (*drag_and_drop_source)(const void* payload, uint32_t size);
    bool (*drag_and_drop_target)(void* payload, uint32_t size);
    bool (*get_drag_and_drop_payload)(void* payload, uint32_t size);

    color_t* (*get_color)(uint32_t name);
} oui_api;
