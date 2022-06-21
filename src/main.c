#include <stdbool.h>
#include <stdio.h>

#include "platform.h"

int main(int argc, const char** argv)
{
    (void)argc;

    platform_input_info_t input = {0};

    if (!platform_opengl_init(argv[0], 640, 480))
    {
        return 1;
    }

    // main loop
    while (!input.should_exit)
    {
        platform_handle_input_events(&input);

        platform_swap_buffers();
    }

    return 0;
}
