#pragma once

#include "base_types.h"

typedef struct renderer_i renderer_i;
typedef struct platform_input_info_t platform_input_info_t;
typedef struct mem_allocator_i mem_allocator_i;

typedef struct oui_api
{
    void (*init)(mem_allocator_i* alloc,
                 renderer_i* renderer,
                 platform_input_info_t* input);
    void (*terminate)();

    void (*push_id)(uint64_t id);
    void (*push_string_id)(const char* txt);
    void (*pop_id)();
    uint64_t (*current_id)();

    void (*begin_draw_region)(int32_t x, int32_t y);
    void (*end_draw_region)();

    bool (*hover_rect)(int32_t x, int32_t y, uint32_t w, uint32_t h);

    bool (*slider)(const char* txt, float* value, float min, float max);
    bool (*button)(const char* txt);
    bool (*checkbox)(const char* txt, bool* value);

    bool (*drag_and_drop_source)(const void* payload, uint32_t size);
    bool (*drag_and_drop_target)(void* payload, uint32_t size);

    void (*begin_frame)();
    void (*end_frame)();
} oui_api;
