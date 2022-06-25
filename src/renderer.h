#pragma once

#include <inttypes.h>

typedef enum render_command_kind_e
{
    CLEAR,
    DRAW_PRIMITIVE,
} render_command_kind_e;

typedef enum primitive_kind_e
{
    PRIMITIVE_TRIANGLES,
} primitive_kind_e;

typedef struct render_command_t
{
    render_command_kind_e kind;

    union
    {
        float clear_color[4];
        struct
        {
            primitive_kind_e kind;
            uint32_t count;
            uint32_t offset;
        } primitive;
    };
} render_command_t;
