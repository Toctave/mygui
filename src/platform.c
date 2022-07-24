#include "platform.h"
#include "util.h"

const char* KEY_NAMES[KEY_COUNT] = {FOR_ALL_KEY_INDICES(DO_KEY_NAME)};

char* platform_get_relative_path(mem_allocator_i* alloc, const char* name)
{
    return tprintf(alloc, "%s/%s", EXECUTABLE_PATH, name);
}
