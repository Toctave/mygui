#pragma once

#include "base_types.h"

#include "plugin_sdk.h"

void plugin_manager_init();
void* load_plugin(const char* name, version_t version);
void unload_plugin(void* plugin);
