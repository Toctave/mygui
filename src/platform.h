#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "opengl_functions.h"

typedef struct platform_input_info_t
{
    bool should_exit;
    int mouse_x;
    int mouse_y;
} platform_input_info_t;

typedef struct platform_file_t platform_file_t;

void platform_handle_input_events(platform_input_info_t* input);
bool platform_opengl_init(const char* window_title,
                          uint32_t width,
                          uint32_t height);
void platform_swap_buffers();

bool platform_running_under_debugger();

void* platform_virtual_alloc(uint64_t size);
void platform_virtual_free(void* ptr, uint64_t size);

platform_file_t* platform_open_file();
void platform_close_file(platform_file_t* file);

uint64_t platform_get_file_size(platform_file_t* file);
uint64_t platform_read_file(platform_file_t* file, void* buffer, uint64_t size);
