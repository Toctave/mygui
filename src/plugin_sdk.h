#pragma once

#define MAX_PLUGIN_NAME_SIZE 256

typedef struct version_t
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} version_t;

typedef struct plugin_spec_t
{
    char name[MAX_PLUGIN_NAME_SIZE];
    version_t version;
    uint32_t api_size;
    void (*load)(void* api_buffer); // api_buffer must be at least
                                    // api_size bytes large
} plugin_spec_t;
