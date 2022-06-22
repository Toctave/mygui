#include <GL/gl.h>
#include <stdbool.h>
#include <stdio.h>

#include "assert.h"
#include "logging.h"
#include "memory.h"
#include "opengl_functions.h"
#include "platform.h"
#include "util.h"

static mem_stack_allocator_t tmp_alloc;

typedef enum shader_stage_e
{
    SHADER_STAGE_VERTEX,
    SHADER_STAGE_FRAGMENT,
} shader_stage_e;

typedef struct renderer_t
{
    GLuint vbo;
    GLuint vao;
    GLuint shader;
} renderer_t;

void* read_alloc_file(mem_stack_allocator_t* alloc, const char* path)
{
    platform_file_t* f = platform_open_file(path);
    if (!f)
    {
        return 0;
    }
    uint64_t size = platform_get_file_size(f);

    uint64_t top = mem_stack_top(alloc);
    void* buf = mem_stack_push(alloc, size);

    if (platform_read_file(f, buf, size) != size)
    {
        mem_stack_reset(alloc, top);
        buf = 0;
    }

    platform_close_file(f);

    return buf;
}

char* read_alloc_text_file(mem_stack_allocator_t* alloc, const char* path)
{
    platform_file_t* f = platform_open_file(path);
    if (!f)
    {
        return 0;
    }
    uint64_t size = platform_get_file_size(f);

    uint64_t top = mem_stack_top(alloc);
    char* buf = mem_stack_push(alloc, size + 1);

    if (platform_read_file(f, buf, size) != size)
    {
        mem_stack_reset(alloc, top);
        buf = 0;
    }
    else
    {
        buf[size] = '\0';
    }

    platform_close_file(f);

    return buf;
}

GLuint compile_shader_stage(const char* path, shader_stage_e stage_type)
{
    GLenum gl_stage_type;
    switch (stage_type)
    {
    case SHADER_STAGE_FRAGMENT:
        gl_stage_type = GL_FRAGMENT_SHADER;
        break;
    case SHADER_STAGE_VERTEX:
        gl_stage_type = GL_VERTEX_SHADER;
        break;
    default:
        ASSERT(false);
    }
    GLuint stage = glCreateShader(gl_stage_type);

    const char* source = read_alloc_text_file(&tmp_alloc, path);
    ASSERT(source);

    const char* sources[] = {source};

    glShaderSource(stage, ARRAY_COUNT(sources), sources, 0);

    glCompileShader(stage);
    GLint ok;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        GLsizei info_log_length;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &info_log_length);

        char* info_log = mem_stack_push(&tmp_alloc, info_log_length);
        glGetShaderInfoLog(stage, info_log_length, 0, info_log);

        log_error("Shader compilation error :\n");
        const char* line = info_log;
        int line_length = 0;
        while (line[line_length])
        {
            if (line[line_length] == '\n')
            {
                log_continue("%s:%*s", path, line_length, line);
                line += line_length;
            }
            line_length++;
        }

        glDeleteShader(stage);
        stage = 0;
    }

    return stage;
}

GLuint compile_shader(const char* vertex_path, const char* fragment_path)
{
    GLuint vs = compile_shader_stage(vertex_path, SHADER_STAGE_VERTEX);
    if (!vs)
    {
        return 0;
    }

    GLuint fs = compile_shader_stage(fragment_path, SHADER_STAGE_FRAGMENT);
    if (!fs)
    {
        glDeleteShader(vs);
        return 0;
    }

    GLuint program = glCreateProgram();

    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);

    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        GLsizei info_log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);

        char* info_log = mem_stack_push(&tmp_alloc, info_log_length);
        glGetProgramInfoLog(program, info_log_length, 0, info_log);

        log_error("Shader linking error :\n%s", info_log);

        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void renderer_init(renderer_t* renderer)
{
    glGenBuffers(1, &renderer->vbo);
    glGenVertexArrays(1, &renderer->vao);

    renderer->shader = compile_shader("shader.vs", "shader.fs");

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    // clang-format off
    float data[] = {
        -1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f,
    };
    // clang-format on
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    glBindVertexArray(renderer->vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0); // position
}

void render(renderer_t* renderer, uint32_t width, uint32_t height)
{
    glViewport(0, 0, width, height);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderer->shader);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int main(int argc, const char** argv)
{
    ASSERT(sizeof(void*) == sizeof(uint64_t));
    (void)argc;

    log_init();

    mem_stack_create(&tmp_alloc, Gibi(1024));

    platform_input_info_t input = {0};

    if (!platform_opengl_init(argv[0], 640, 480))
    {
        return 1;
    }

    renderer_t renderer = {0};
    renderer_init(&renderer);

    // main loop
    while (!input.should_exit)
    {
        platform_handle_input_events(&input);

        render(&renderer, input.width, input.height);
        log_debug("width = %d, height = %d", input.width, input.height);

        platform_swap_buffers();
    }

    log_terminate();

    return 0;
}
