#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <GL/gl.h>

typedef struct platform_input_info_t
{
    bool should_exit;
    int mouse_x;
    int mouse_y;
} platform_input_info_t;

void platform_handle_input_events(platform_input_info_t* input);
bool platform_opengl_init(const char* window_title,
                          uint32_t width,
                          uint32_t height);
void platform_swap_buffers();
