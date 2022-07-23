#pragma once

#include "base_types.h"
#include "opengl_functions.h"

extern char EXECUTABLE_PATH[1024];

enum
{
    MOUSE_BUTTON_LEFT = 1 << 0,
    MOUSE_BUTTON_MIDDLE = 1 << 1,
    MOUSE_BUTTON_RIGHT = 1 << 2,
};

typedef struct platform_input_info_t
{
    bool should_exit;

    int32_t mouse_x;
    int32_t mouse_y;
    int32_t mouse_dx;
    int32_t mouse_dy;

    uint32_t mouse_pressed;
    uint32_t mouse_released;

    uint32_t width;
    uint32_t height;

    char typed_utf8[64]; // UTF-8
} platform_input_info_t;

typedef struct platform_file_o platform_file_o;

void platform_handle_input_events(platform_input_info_t* input);
bool platform_init(const char* argv0,
                   const char* window_title,
                   uint32_t width,
                   uint32_t height);
void platform_swap_buffers();

bool platform_running_under_debugger();

void* platform_virtual_alloc(uint64_t size);
void platform_virtual_free(void* ptr, uint64_t size);

platform_file_o* platform_open_file();
void platform_close_file(platform_file_o* file);

uint64_t platform_get_file_size(platform_file_o* file);
uint64_t platform_read_file(platform_file_o* file, void* buffer, uint64_t size);

uint64_t platform_get_nanoseconds();

void platform_get_shared_library_path(char* path,
                                      uint32_t size,
                                      const char* name);
void* platform_open_shared_library(const char* path);
void platform_close_shared_library(void* lib);

void* platform_get_symbol_address(void* lib, const char* name);

typedef struct platform_file_event_t
{
    uint64_t watch_id;
} platform_file_event_t;

uint64_t platform_watch_file(const char* path);
bool platform_poll_file_event(platform_file_event_t* event);
