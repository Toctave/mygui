#include "plugin_manager.h"

#include "assert.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

typedef struct plugin_t
{
    plugin_spec_t spec;
    void* api;
    void* lib;

    uint64_t watch_id;
    bool dirty;
} plugin_t;

typedef union plugin_slot_t
{
    bool taken;
    uint32_t next_free;
    plugin_t plugin;
} plugin_slot_t;

static plugin_slot_t plugin_table[1024];
static uint8_t api_memory[Mebi(32)];
static uint32_t api_memory_used = 0;

void plugin_manager_init()
{
    for (uint32_t i = 0; i < STATIC_ARRAY_COUNT(plugin_table); i++)
    {
        plugin_table[i].next_free = i + 1;
    }

    plugin_table[STATIC_ARRAY_COUNT(plugin_table) - 1].next_free = 0;
}

static plugin_t* alloc_plugin()
{
    uint32_t idx = plugin_table[0].next_free;
    if (!idx)
    {
        return 0;
    }

    plugin_table[0].next_free = plugin_table[idx].next_free;
    plugin_table[idx].taken = true;

    plugin_table[idx].plugin = (plugin_t){0};
    return &plugin_table[idx].plugin;
}

static void free_plugin(plugin_t* plugin)
{
    (void)plugin;
    // TODO(octave)
}

bool versions_compatible(version_t required_version, version_t found_version)
{
    return required_version.major == found_version.major
           && required_version.minor == found_version.minor
           && required_version.patch == found_version.patch;
}

static plugin_t* find_plugin(const char* name, version_t version)
{
    for (uint32_t i = 1; i < STATIC_ARRAY_COUNT(plugin_table); i++)
    {
        if (plugin_table[i].taken
            && !strncmp(name,
                        plugin_table[i].plugin.spec.name,
                        MAX_PLUGIN_NAME_SIZE)
            && versions_compatible(version,
                                   plugin_table[i].plugin.spec.version))
        {
            return &plugin_table[i].plugin;
        }
    }
    return 0;
}

void* load_plugin(const char* name, version_t version)
{
    ASSERT(strlen(name) < MAX_PLUGIN_NAME_SIZE);
    plugin_t* found = find_plugin(name, version);
    if (!found)
    {
        void* lib = platform_open_shared_library(name);

        if (lib)
        {
            found = alloc_plugin();

            strncpy(found->spec.name, name, MAX_PLUGIN_NAME_SIZE - 1);

            found->lib = lib;
            found->api = 0;

            char lib_path[2048];
            platform_get_shared_library_path(lib_path, sizeof(lib_path), name);
            found->watch_id = platform_watch_file(lib_path);

            plugin_spec_t* spec =
                platform_get_symbol_address(found->lib, "PLUGIN_SPEC");

            if (!spec || !versions_compatible(version, spec->version))
            {
                platform_close_shared_library(found->lib);
                free_plugin(found);
                found = 0;
            }
            else
            {
                found->spec = *spec;
            }
        }
    }

    if (!found)
    {
        return 0;
    }
    else if (!found->api)
    {
        ASSERT(found->spec.load);
        ASSERT(api_memory_used + found->spec.api_size <= sizeof(api_memory));

        found->api = &api_memory[api_memory_used];
        api_memory_used += found->spec.api_size;

        found->spec.load(found->api);
    }

    return found->api;
}
