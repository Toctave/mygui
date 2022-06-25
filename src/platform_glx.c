#include "platform.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

// Global variables
static Display* dpy;
static Window window;
static GLXWindow glxWindow;
static Atom WM_DELETE_WINDOW;

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

bool platform_init(const char* window_title, uint32_t width, uint32_t height)
{
    FOR_ALL_GLX_FUNCTIONS(DO_LOAD_GL_FUNCTION);
    FOR_ALL_GL_FUNCTIONS(DO_LOAD_GL_FUNCTION);

    dpy = XOpenDisplay(0);
    if (!dpy)
    {
        fprintf(stderr, "No X11 display available.\n");
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
            fprintf(stderr,
                    "No GLX extension on this X server connection, exiting.\n");
            return false;
        }

        int major, minor;
        if (!glXQueryVersion(dpy, &major, &minor))
        {
            fprintf(stderr, "Could not query GLX version, exiting.\n");
            return false;
        }

        if (major < 1 || minor < 1)
        {
            fprintf(stderr,
                    "GLX version 1.1 or later is required but %d.%d was found, "
                    "exiting.\n",
                    major,
                    minor);
            return false;
        }

        int requiredConfigAttributes[] = {
            GLX_X_RENDERABLE,
            true,
            GLX_DRAWABLE_TYPE,
            GLX_WINDOW_BIT,
            GLX_X_VISUAL_TYPE,
            GLX_TRUE_COLOR,
            GLX_DRAWABLE_TYPE,
            GLX_WINDOW_BIT,
            GLX_BUFFER_SIZE,
            32,
            GLX_DOUBLEBUFFER,
            true,
            GLX_SAMPLES,
            16,
            None,
        };

        int matchingConfigCount;
        GLXFBConfig* availableConfigs =
            glXChooseFBConfig(dpy,
                              default_screen,
                              requiredConfigAttributes,
                              &matchingConfigCount);
        if (!matchingConfigCount)
        {
            fprintf(stderr, "Could not find a suitable GLX config, exiting.\n");
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
        fprintf(stderr, "Could not create window, exiting.\n");
        return false;
    }

    if (!glXCreateContextAttribsARB)
    {
        fprintf(
            stderr,
            "Could not find glXCreateContextAttribsARB function, exiting.\n");
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
        fprintf(stderr, "Could not create GLX context, exiting.\n");
        return false;
    }

    if (!glXMakeContextCurrent(dpy, glxWindow, glxWindow, glxContext))
    {
        fprintf(stderr, "Could not make GLX context current, exiting.\n");
        return false;
    }

    if (isGLXExtensionPresent(default_screen, "GLX_EXT_swap_control"))
    {
        glXSwapIntervalEXT(dpy, glxWindow, 0);
    }
    else
    {
        fprintf(
            stderr,
            "Cannot find extension GLX_EXT_swap_control, V-sync will be off\n");
    }

    XStoreName(dpy, window, window_title);
    XMapWindow(dpy, window);

    // Keyboard input context setup (used for text decoding)
    {
        XIM x_input_method = XOpenIM(dpy, 0, 0, 0);
        if (!x_input_method)
        {
            fprintf(stderr, "Could not open input method.\n");
        }

        XIMStyles* styles = 0;
        if (XGetIMValues(x_input_method, XNQueryInputStyle, &styles, NULL)
            || !styles)
        {
            fprintf(stderr, "Could not retrieve input styles.\n");
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
            fprintf(stderr, "Could not find matching input style.\n");
        }

        XIC inputContext = XCreateIC(x_input_method,
                                     XNInputStyle,
                                     best_match_style,
                                     XNClientWindow,
                                     window,
                                     XNFocusWindow,
                                     window,
                                     NULL);
        if (!inputContext)
        {
            fprintf(stderr, "Could not create input context\n");
        }
    }

    XFlush(dpy);

    WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", true);
    if (!XSetWMProtocols(dpy, window, &WM_DELETE_WINDOW, 1))
    {
        fprintf(stderr, "Could not register WM_DELETE_WINDOW.\n");
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
    /* memset(input, 0, sizeof(*input)); */

    XWindowAttributes attrs;
    XGetWindowAttributes(dpy, window, &attrs);

    input->width = attrs.width;
    input->height = attrs.height;

    input->mouse_pressed = 0;
    input->mouse_released = 0;

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
            break;
        case ButtonPress:
            input->mouse_pressed |= button_mask(evt.xbutton.button);
            break;
        case ButtonRelease:
            input->mouse_released |= button_mask(evt.xbutton.button);
            break;
        }
    }
}

void platform_swap_buffers() { glXSwapBuffers(dpy, glxWindow); }
