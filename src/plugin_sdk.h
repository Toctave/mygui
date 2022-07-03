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
    void* (*load)();
    void (*unload)();
} plugin_spec_t;
