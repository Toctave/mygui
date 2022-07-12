#pragma once

#include "base_types.h"

typedef struct renderer_i renderer_i;
typedef struct platform_input_info_t platform_input_info_t;
typedef struct mem_allocator_i mem_allocator_i;

typedef struct ui_plug_id_t
{
    uint64_t node;
    uint64_t plug;
} ui_plug_id_t;

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

    bool (*slider)(const char* txt, float* value, float min, float max);
    bool (*button)(const char* txt);
    bool (*checkbox)(const char* txt, bool* value);

    void (*begin_node)(const char* name, quad_i32_t* node, uint64_t id);
    bool (*plug)(const char* name, ui_plug_id_t* source_plug);
    void (*end_node)();

    void (*begin_frame)();
    void (*end_frame)();
} oui_api;
