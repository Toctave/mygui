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
        platform_handle_input_event(&input);

        printf("%d %d\n", input.mouse_x, input.mouse_y);

        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        platform_swap_buffers();
    }

    return 0;
}
