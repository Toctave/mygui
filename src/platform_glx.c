#include "platform.h"

#include "assert.h"
#include "logging.h"
#include "util.h"
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

// Global variables
static Display* dpy;
static Window window;
static GLXWindow glxWindow;
static Atom WM_DELETE_WINDOW;
static XIC inputContext;

// TODO(octave) : bulletproof this
static uint8_t x11_keycode_to_key_enum[256] = {
    [9] = KEY_ESCAPE,
    [10] = KEY_1,
    [11] = KEY_2,
    [12] = KEY_3,
    [13] = KEY_4,
    [14] = KEY_5,
    [15] = KEY_6,
    [16] = KEY_7,
    [17] = KEY_8,
    [18] = KEY_9,
    [19] = KEY_0,
    [20] = KEY_MINUS,
    [21] = KEY_EQUALS,
    [22] = KEY_BACKSPACE,
    [23] = KEY_TAB,
    [24] = KEY_Q,
    [25] = KEY_W,
    [26] = KEY_E,
    [27] = KEY_R,
    [28] = KEY_T,
    [29] = KEY_Y,
    [30] = KEY_U,
    [31] = KEY_I,
    [32] = KEY_O,
    [33] = KEY_P,
    [34] = KEY_LEFT_BRACKET,
    [35] = KEY_RIGHT_BRACKET,
    [36] = KEY_RETURN,
    [37] = KEY_LEFT_CONTROL,
    [38] = KEY_A,
    [39] = KEY_S,
    [40] = KEY_D,
    [41] = KEY_F,
    [42] = KEY_G,
    [43] = KEY_H,
    [44] = KEY_J,
    [45] = KEY_K,
    [46] = KEY_L,
    [47] = KEY_SEMICOLON,
    [48] = KEY_APOSTROPHE,
    [49] = KEY_GRAVE_ACCENT,
    [50] = KEY_LEFT_SHIFT,
    [51] = KEY_BACKSLASH,
    [52] = KEY_Z,
    [53] = KEY_X,
    [54] = KEY_C,
    [55] = KEY_V,
    [56] = KEY_B,
    [57] = KEY_N,
    [58] = KEY_M,
    [59] = KEY_COMMA,
    [60] = KEY_PERIOD,
    [61] = KEY_SLASH,
    [62] = KEY_RIGHT_SHIFT,
    [63] = KEY_KP_STAR,
    [64] = KEY_LEFT_ALT,
    [65] = KEY_SPACE,
    [66] = KEY_ESCAPE,
    [67] = KEY_F1,
    [68] = KEY_F2,
    [69] = KEY_F3,
    [70] = KEY_F4,
    [71] = KEY_F5,
    [72] = KEY_F6,
    [73] = KEY_F7,
    [74] = KEY_F8,
    [75] = KEY_F9,
    [76] = KEY_F10,
    [77] = KEY_NUM_LOCK,
    [78] = KEY_SCROLL_LOCK,
    [79] = KEY_KP_7,
    [80] = KEY_KP_8,
    [81] = KEY_KP_9,
    [82] = KEY_KP_MINUS,
    [83] = KEY_KP_4,
    [84] = KEY_KP_5,
    [85] = KEY_KP_6,
    [86] = KEY_KP_PLUS,
    [87] = KEY_KP_1,
    [88] = KEY_KP_2,
    [89] = KEY_KP_3,
    [90] = KEY_KP_0,
    [91] = KEY_KP_PERIOD,
    [95] = KEY_F11,
    [96] = KEY_F12,
    [104] = KEY_KP_ENTER,
    [105] = KEY_RIGHT_CONTROL,
    [106] = KEY_KP_SLASH,
    [107] = KEY_PRINT_SCREEN,
    [108] = KEY_RIGHT_ALT,
    [110] = KEY_HOME,
    [111] = KEY_UP_ARROW,
    [112] = KEY_PAGE_UP,
    [113] = KEY_LEFT_ARROW,
    [114] = KEY_RIGHT_ARROW,
    [115] = KEY_END,
    [116] = KEY_DOWN_ARROW,
    [117] = KEY_PAGE_DOWN,
    [118] = KEY_INSERT,
    [119] = KEY_DELETE,
    [127] = KEY_PAUSE,
    [133] = KEY_SUPER,
};

#define FOR_ALL_GLX_FUNCTIONS(X)                                               \
    X(XCreateContextAttribsARB,                                                \
      GLXContext,                                                              \
      Display* dpy,                                                            \
      GLXFBConfig config,                                                      \
      GLXContext share_context,                                                \
      Bool direct,                                                             \
      const int* attrib_list)                                                  \
    X(XSwapIntervalEXT, void, Display* dpy, GLXDrawable drawable, int interval)

#define DO_DECLARE_GL_FUNCTION(name, rtype, ...) rtype (*gl##name)(__VA_ARGS__);
#define DO_LOAD_GL_FUNCTION(name, rtype, ...)                                  \
    gl##name =                                                                 \
        (rtype(*)(__VA_ARGS__))glXGetProcAddress((const GLubyte*)"gl" #name);

FOR_ALL_GL_FUNCTIONS(DO_DECLARE_GL_FUNCTION)
FOR_ALL_GLX_FUNCTIONS(DO_DECLARE_GL_FUNCTION)

static bool startsWith(const char* text, const char* to_find)
{
    while (*to_find && *text && *text++ == *to_find++)
        ;

    return !*to_find;
}

static void skipWhitespace(const char** cursor)
{
    while (**cursor == ' ')
    {
        (*cursor)++;
    }
}

static bool isGLXExtensionPresent(int screen, const char* extensionName)
{
    const char* extensionString = glXQueryExtensionsString(dpy, screen);
    const char* cursor = extensionString;

    do
    {
        skipWhitespace(&cursor);
        if (startsWith(cursor, extensionName))
        {
            return true;
        }
        else
        {
            while (*cursor != ' ')
            {
                cursor++;
            }
        }
    } while (*cursor);

    return false;
}

bool platform_init(const char* argv0,
                   const char* window_title,
                   uint32_t width,
                   uint32_t height)
{
    strncpy(EXECUTABLE_PATH, argv0, sizeof(EXECUTABLE_PATH) - 1);
    int n = strlen(EXECUTABLE_PATH) - 1;
    while (n >= 0 && EXECUTABLE_PATH[n] != '/')
    {
        n--;
    }
    EXECUTABLE_PATH[n] = '\0';

    FOR_ALL_GLX_FUNCTIONS(DO_LOAD_GL_FUNCTION);
    FOR_ALL_GL_FUNCTIONS(DO_LOAD_GL_FUNCTION);

    dpy = XOpenDisplay(0);
    if (!dpy)
    {
        log_error("No X11 display available.");
        return false;
    }

    int default_screen = XDefaultScreen(dpy);

    XVisualInfo vinfo;
    GLXFBConfig glxConfig;
    {
        int errorBase;
        int eventBase;
        if (!glXQueryExtension(dpy, &errorBase, &eventBase))
        {
            log_error("No GLX extension on this X server connection, exiting.");
            return false;
        }

        int major, minor;
        if (!glXQueryVersion(dpy, &major, &minor))
        {
            log_error("Could not query GLX version, exiting.");
            return false;
        }

        if (major < 1 || minor < 1)
        {
            log_error(
                "GLX version 1.1 or later is required but %d.%d was found, "
                "exiting.",
                major,
                minor);
            return false;
        }

        // clang-format off
        int requiredConfigAttributes[] = {
            GLX_X_RENDERABLE,  true,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_BUFFER_SIZE,   32,
            GLX_DOUBLEBUFFER,  true,
            GLX_SAMPLES,       8,
            None,
        };
        // clang-format on

        int matchingConfigCount;
        GLXFBConfig* availableConfigs =
            glXChooseFBConfig(dpy,
                              default_screen,
                              requiredConfigAttributes,
                              &matchingConfigCount);
        if (!matchingConfigCount)
        {
            log_error("Could not find a suitable GLX config, exiting.");
            return false;
        }

        glxConfig = availableConfigs[0];

        XVisualInfo* pVinfo = glXGetVisualFromFBConfig(dpy, glxConfig);
        vinfo = *pVinfo;
    }

    Window root_window = XDefaultRootWindow(dpy);

    XSetWindowAttributes window_attributes;
    window_attributes.background_pixel = 0;
    window_attributes.colormap =
        XCreateColormap(dpy, root_window, vinfo.visual, AllocNone);
    window_attributes.event_mask = StructureNotifyMask | KeyPressMask
                                   | KeyReleaseMask | PointerMotionMask
                                   | ButtonPressMask | ButtonReleaseMask
                                   | EnterWindowMask | LeaveWindowMask;
    window_attributes.bit_gravity = StaticGravity;

    window =
        XCreateWindow(dpy,
                      root_window,
                      0,
                      0,
                      width,
                      height,
                      0,
                      vinfo.depth,
                      InputOutput,
                      vinfo.visual,
                      CWBackPixel | CWColormap | CWEventMask | CWBitGravity,
                      &window_attributes);

    if (!window)
    {
        log_error("Could not create window, exiting.");
        return false;
    }

    if (!glXCreateContextAttribsARB)
    {
        log_error(
            "Could not find glXCreateContextAttribsARB function, exiting.");
        return false;
    }

    glxWindow = glXCreateWindow(dpy, glxConfig, window, 0);

    // NOTE(octave) : using OpenGL 3.3 core
    int glxContextAttribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB,
        3,
        GLX_CONTEXT_MINOR_VERSION_ARB,
        3,
        GLX_CONTEXT_PROFILE_MASK_ARB,
        GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None,
    };
    GLXContext glxContext =
        glXCreateContextAttribsARB(dpy, glxConfig, 0, true, glxContextAttribs);

    if (!glxContext)
    {
        log_error("Could not create GLX context, exiting.");
        return false;
    }

    if (!glXMakeContextCurrent(dpy, glxWindow, glxWindow, glxContext))
    {
        log_error("Could not make GLX context current, exiting.");
        return false;
    }

    if (isGLXExtensionPresent(default_screen, "GLX_EXT_swap_control"))
    {
        glXSwapIntervalEXT(dpy, glxWindow, 1);
    }
    else
    {
        log_error(
            "Cannot find extension GLX_EXT_swap_control, V-sync will be off");
    }

    XStoreName(dpy, window, window_title);
    XMapWindow(dpy, window);

    // Keyboard input context setup (used for text decoding)
    {
        XIM x_input_method = XOpenIM(dpy, 0, 0, 0);
        if (!x_input_method)
        {
            log_error("Could not open input method.");
        }

        XIMStyles* styles = 0;
        if (XGetIMValues(x_input_method, XNQueryInputStyle, &styles, NULL)
            || !styles)
        {
            log_error("Could not retrieve input styles.");
        }

        XIMStyle best_match_style = 0;
        for (int i = 0; i < styles->count_styles; i++)
        {
            XIMStyle this_style = styles->supported_styles[i];

            if (this_style == (XIMPreeditNothing | XIMStatusNothing))
            {
                best_match_style = this_style;
                break;
            }
        }
        XFree(styles);

        if (!best_match_style)
        {
            log_error("Could not find matching input style.");
        }

        inputContext = XCreateIC(x_input_method,
                                 XNInputStyle,
                                 best_match_style,
                                 XNClientWindow,
                                 window,
                                 XNFocusWindow,
                                 window,
                                 NULL);
        if (!inputContext)
        {
            log_error("Could not create input context");
        }
    }

    XFlush(dpy);

    WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", true);
    if (!XSetWMProtocols(dpy, window, &WM_DELETE_WINDOW, 1))
    {
        log_error("Could not register WM_DELETE_WINDOW.");
    }

    return true;
}

static uint32_t button_mask(int index)
{
    if (index >= Button1 && index <= Button3)
    {
        return 1 << (index - Button1);
    }
    else
    {
        return 0;
    }
}

void platform_handle_input_events(platform_input_info_t* input)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(dpy, window, &attrs);

    input->width = attrs.width;
    input->height = attrs.height;

    input->mouse_pressed = 0;
    input->mouse_released = 0;

    input->mouse_dx = 0;
    input->mouse_dy = 0;

    int32_t prev_x = input->mouse_x;
    int32_t prev_y = input->mouse_y;

    uint32_t typed_size = 0;
    static char prev_buffer[5];

    memset(input->keys_pressed, 0, KEYBOARD_BITFIELD_BYTES);
    memset(input->keys_released, 0, KEYBOARD_BITFIELD_BYTES);

    while (XPending(dpy))
    {
        XEvent evt;

        XNextEvent(dpy, &evt);
        if (XFilterEvent(&evt, window))
        {
            continue;
        }

        switch (evt.type)
        {
        case DestroyNotify:
            input->should_exit = true;
            break;
        case ClientMessage:
            if ((Atom)evt.xclient.data.l[0] == WM_DELETE_WINDOW)
            {
                input->should_exit = true;
            }
            break;
        case MotionNotify:
            input->mouse_x = evt.xmotion.x;
            input->mouse_y = evt.xmotion.y;
            input->mouse_dx = input->mouse_x - prev_x;
            input->mouse_dy = input->mouse_y - prev_y;
            break;
        case ButtonPress:
            input->mouse_pressed |= button_mask(evt.xbutton.button);
            break;
        case ButtonRelease:
            input->mouse_released |= button_mask(evt.xbutton.button);
            break;
        case KeyPress:
        case KeyRelease:
        {
            bool is_repeat = false;
            // NOTE(octave) : if this is slow, might want to pass
            // QueuedAlready or QueuedAfterReading
            if (evt.type == KeyRelease && XEventsQueued(dpy, QueuedAfterFlush))
            {
                XEvent next_evt;
                XPeekEvent(dpy, &next_evt);

                if (next_evt.type == KeyPress
                    && next_evt.xkey.time == evt.xkey.time
                    && next_evt.xkey.keycode == evt.xkey.keycode)
                {
                    // it's a key repeat, pop the next event with it
                    XNextEvent(dpy, &next_evt);
                    is_repeat = true;
                }
            }

            if (is_repeat)
            {
                for (int32_t i = 0; i < 4; i++)
                {
                    if (!prev_buffer[i])
                    {
                        break;
                    }
                    ASSERT(typed_size < sizeof(input->typed_utf8));
                    input->typed_utf8[typed_size++] = prev_buffer[i];
                }
            }

            if (evt.type == KeyPress)
            {
                Status status = 0;
                KeySym keysym;
                char buffer[5] = {0};
                int bytes_read = Xutf8LookupString(inputContext,
                                                   &evt.xkey,
                                                   buffer,
                                                   sizeof(buffer),
                                                   &keysym,
                                                   &status);

                if (status == XBufferOverflow)
                {
                    log_error("Buffer overflow while reading keyboard input.");
                    break;
                }

                // bool have_keysym = (status == XLookupKeySym || status == XLookupBoth);

                bool have_buffer =
                    (status == XLookupChars || status == XLookupBoth);

                if (have_buffer)
                {
                    for (int i = 0; i < bytes_read; i++)
                    {
                        ASSERT(typed_size < sizeof(input->typed_utf8));
                        input->typed_utf8[typed_size++] = buffer[i];
                    }

                    memcpy(prev_buffer, buffer, sizeof(buffer));
                }
            }

            if (!is_repeat)
            {
                uint8_t key_enum = x11_keycode_to_key_enum[evt.xkey.keycode];
                if (evt.type == KeyPress)
                {
                    set_bit(input->keys_pressed, key_enum);
                }

                if (evt.type == KeyRelease)
                {
                    set_bit(input->keys_released, key_enum);
                }
            }

            break;
        }
        }
    }

    uint8_t x11_keys_down[32];
    XQueryKeymap(dpy, (char*)x11_keys_down);

    memset(input->keys_down, 0, KEYBOARD_BITFIELD_BYTES);
    for (uint32_t keycode = 0;
         keycode < STATIC_ARRAY_COUNT(x11_keycode_to_key_enum);
         keycode++)
    {
        if (get_bit(x11_keys_down, keycode))
        {
            uint8_t key_enum = x11_keycode_to_key_enum[keycode];
            set_bit(input->keys_down, key_enum);
        }
    }

    input->typed_utf8[typed_size] = 0;
}

void platform_swap_buffers() { glXSwapBuffers(dpy, glxWindow); }
