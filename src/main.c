#include <GL/gl.h>
#include <GL/glext.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "logging.h"
#include "memory.h"
#include "opengl_functions.h"
#include "platform.h"
#include "util.h"

typedef struct quad_i32_t
{
    int32_t min[2];
    int32_t extent[2];
} quad_i32_t;

typedef struct quad_float_t
{
    float min[2];
    float extent[2];
} quad_float_t;

typedef struct color_t
{
    uint8_t rgba[4];
} color_t;

typedef struct vertex_t
{
    float position[2];
    float uv[2];
    uint8_t color[4];
} vertex_t;

typedef enum shader_stage_e
{
    SHADER_STAGE_VERTEX,
    SHADER_STAGE_FRAGMENT,
} shader_stage_e;

typedef struct texture_t
{
    GLuint index;
    uint32_t width;
    uint32_t height;
} texture_t;

typedef struct font_glyph_t
{
    int32_t dx;
    int32_t dy;
    uint32_t character;

    quad_i32_t bbox;
} font_glyph_t;

typedef struct font_t
{
    quad_i32_t bbox;
    uint32_t glyph_count;
    uint32_t stride;

    font_glyph_t* glyphs;
    uint8_t* bitmap;
} font_t;

typedef struct renderer_t
{
    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    GLuint shader;

    texture_t font_texture;
    font_t font;

    vertex_t* vertex_data;
    uint32_t* index_data;

    uint32_t quad_count;
} renderer_t;

color_t color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    color_t result;
    result.rgba[0] = r;
    result.rgba[1] = g;
    result.rgba[2] = b;
    result.rgba[3] = a;

    return result;
}

color_t color_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return color_rgba(r, g, b, 0xFF);
}

color_t color_gray(uint8_t w) { return color_rgb(w, w, w); }

static const char* get_next_line(const char* current)
{
    while (*current)
    {
        if (current[0] == '\n')
        {
            return current + 1;
        }
        current++;
    }

    return current;
}

void texture_create(texture_t* texture, uint32_t width, uint32_t height)
{
    glGenTextures(1, &texture->index);
    texture->width = width;
    texture->height = height;

    glBindTexture(GL_TEXTURE_2D, texture->index);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void texture_destroy(texture_t* texture)
{
    glDeleteTextures(1, &texture->index);
}

void texture_set_data(texture_t* texture, uint8_t* data)
{
    glBindTexture(GL_TEXTURE_2D, texture->index);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    texture->width,
                    texture->height,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    data);
}

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

GLuint compile_shader_stage(mem_stack_allocator_t* tmp,
                            const char* path,
                            shader_stage_e stage_type)
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

    const char* source = read_alloc_text_file(tmp, path);
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

        char* info_log = mem_stack_push(tmp, info_log_length);
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

GLuint compile_shader(mem_stack_allocator_t* tmp,
                      const char* vertex_path,
                      const char* fragment_path)
{
    GLuint vs = compile_shader_stage(tmp, vertex_path, SHADER_STAGE_VERTEX);
    if (!vs)
    {
        return 0;
    }

    GLuint fs = compile_shader_stage(tmp, fragment_path, SHADER_STAGE_FRAGMENT);
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

        char* info_log = mem_stack_push(tmp, info_log_length);
        glGetProgramInfoLog(program, info_log_length, 0, info_log);

        log_error("Shader linking error :\n%s", info_log);

        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

static bool read_string(const char** cursor, const char* word)
{
    const char* newcursor = *cursor;
    while (*word && *newcursor == *word)
    {
        newcursor++;
        word++;
    }

    if (!*word)
    {
        *cursor = newcursor;
        return true;
    }
    else
    {
        return false;
    }
}

static bool is_word_boundary(char c)
{
    return c == ' ' || c == '\n' || c == '\0';
}

static bool read_word(const char** cursor, const char* word)
{
    const char* newcursor = *cursor;
    if (!read_string(&newcursor, word))
    {
        return false;
    }

    if (!is_word_boundary(*newcursor))
    {
        return false;
    }

    *cursor = newcursor;
    return true;
}

static void skip_spaces(const char** cursor)
{
    while (**cursor && **cursor == ' ')
    {
        (*cursor)++;
    }
}

static bool read_char(const char** cursor, char c)
{
    if (**cursor == c)
    {
        (*cursor)++;
        return true;
    }
    else
    {
        return false;
    }
}

static bool read_digit(const char** cursor, int32_t* value)
{
    char c = **cursor;
    if (c >= '0' && c <= '9')
    {
        *value = c - '0';
        (*cursor)++;
        return true;
    }
    else
    {
        return false;
    }
}

static bool read_int32(const char** cursor, int32_t* value)
{
    int32_t sign = 1;
    if (read_char(cursor, '-'))
    {
        sign = -1;
    }

    int32_t val = 0;
    int32_t digit;
    uint32_t digit_count = 0;
    while (read_digit(cursor, &digit))
    {
        val = val * 10 + digit;
        digit_count++;
    }

    if (digit_count > 0)
    {
        *value = sign * val;
        return true;
    }
    else
    {
        return false;
    }
}

static bool read_int32_array(const char** cursor,
                             uint32_t count,
                             int32_t* values,
                             const char* sep)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (!read_int32(cursor, &values[i]))
        {
            return false;
        }

        if (i + 1 < count && !read_string(cursor, sep))
        {
            return false;
        }
    }

    return true;
}

static bool read_hex_digit(const char** cursor, uint8_t* nibble)
{
    char c = **cursor;
    if (c >= '0' && c <= '9')
    {
        *nibble = c - '0';
        (*cursor)++;
        return true;
    }
    else if (c >= 'A' && c <= 'F')
    {
        *nibble = 10 + (c - 'A');
        (*cursor)++;
        return true;
    }
    else
    {
        return false;
    }
}

static bool read_hex_byte(const char** cursor, uint8_t* byte)
{
    uint8_t nibble1, nibble2;

    if (read_hex_digit(cursor, &nibble1) && read_hex_digit(cursor, &nibble2))
    {
        *byte = nibble1 << 4 | nibble2;
        return true;
    }
    else
    {
        return false;
    }
}

static bool read_quad_i32(const char** cursor, quad_i32_t* q)
{
    int32_t bb[4];
    if (read_int32_array(cursor, 4, bb, " "))
    {
        q->extent[0] = bb[0];
        q->extent[1] = bb[1];
        q->min[0] = bb[2];
        q->min[1] = bb[3];
        return true;
    }
    else
    {
        return false;
    }
}

bool load_bdf(mem_stack_allocator_t* tmp,
              mem_stack_allocator_t* permanent,
              font_t* font,
              const char* bdf_path)
{
    ASSERT(!font->glyphs);
    const char* bdf = read_alloc_text_file(tmp, bdf_path);
    const char* line = bdf;

    bool in_properties = false;
    bool in_bitmap = false;
    bool in_char = false;

    bool bbox_read = false;

    int32_t character_count = 0;
    bool character_count_read = false;
    font_glyph_t* current_glyph = 0;
    uint8_t* current_glyph_bitmap = 0;
    uint32_t glyph_bitmap_size = 0;

    int32_t size[3];
    bool size_read = false;

    while (*line)
    {
        const char* cursor = line;
        if (read_word(&cursor, "STARTFONT"))
        {
            skip_spaces(&cursor);
            if (!read_word(&cursor, "2.1"))
            {
                log_error("Unsupported BDF version.");
                return false;
            }
        }
        else if (read_word(&cursor, "COMMENT"))
        {
            // ignore line.
        }
        else if (read_word(&cursor, "FONT"))
        {
            // ignore line.
        }
        else if (read_word(&cursor, "SIZE"))
        {
            skip_spaces(&cursor);
            if (read_int32_array(&cursor, 3, size, " "))
            {
                size_read = true;
            }
            else
            {
                log_error("Could not read SIZE");
                return false;
            }
        }
        else if (read_word(&cursor, "FONTBOUNDINGBOX"))
        {
            skip_spaces(&cursor);
            if (read_quad_i32(&cursor, &font->bbox))
            {
                bbox_read = true;
            }
            else
            {
                log_error("Could not read FONTBOUNDINGBOX");
                return false;
            }
        }
        else if (read_word(&cursor, "STARTPROPERTIES"))
        {
            if (in_properties)
            {
                log_error("STARTPROPERTIES while already in properties block");
                return false;
            }
            in_properties = true;
        }
        else if (read_word(&cursor, "ENDPROPERTIES"))
        {
            if (!in_properties)
            {
                log_error("ENDPROPERTIES while outside properties block");
                return false;
            }
            in_properties = false;
        }
        else if (read_word(&cursor, "CHARS"))
        {
            skip_spaces(&cursor);
            if (read_int32(&cursor, &character_count))
            {
                character_count_read = true;
            }
            else
            {
                log_error("Could not read CHARS");
                return false;
            }
        }
        else if (read_word(&cursor, "STARTCHAR"))
        {
            if (in_char)
            {
                log_error("STARTCHAR while already in char block");
                return false;
            }

            if (!character_count_read)
            {
                log_error("STARTCHAR but haven't read character count");
                return false;
            }

            if (!current_glyph)
            {
                font->glyph_count = character_count;
                font->glyphs =
                    mem_stack_push(permanent,
                                   sizeof(font_glyph_t) * font->glyph_count);

                font->stride = (font->bbox.extent[0] + 7) / 8;
                glyph_bitmap_size = font->stride * font->bbox.extent[1];
                font->bitmap =
                    mem_stack_push(permanent,
                                   font->glyph_count * glyph_bitmap_size);
                current_glyph = font->glyphs;
                current_glyph_bitmap = font->bitmap;
            }
            else
            {
                ASSERT(current_glyph);
                current_glyph++;
                current_glyph_bitmap += glyph_bitmap_size;
            }
            in_char = true;
        }
        else if (read_word(&cursor, "ENDCHAR"))
        {
            if (!in_char)
            {
                log_error("ENDCHAR while outside character block");
                return false;
            }
            in_char = false;
        }
        else if (read_word(&cursor, "ENCODING"))
        {
            if (!in_char)
            {
                log_error("ENCODING while outside character block");
                return false;
            }

            skip_spaces(&cursor);
            int32_t encoding;
            if (read_int32(&cursor, &encoding))
            {
                current_glyph->character = encoding;
            }
            else
            {
                log_error("Could not read ENCODING");
                return false;
            }
        }
        else if (read_word(&cursor, "DWIDTH"))
        {
            if (!in_char)
            {
                log_error("DWIDTH while outside character block");
                return false;
            }

            skip_spaces(&cursor);
            int32_t d[2];
            if (read_int32_array(&cursor, 2, d, " "))
            {
                current_glyph->dx = d[0];
                current_glyph->dy = d[1];
            }
            else
            {
                log_error("Could not read DWIDTH");
                return false;
            }
        }
        else if (read_word(&cursor, "BBX"))
        {
            if (!in_char)
            {
                log_error("BBX while outside character block");
                return false;
            }

            skip_spaces(&cursor);
            if (!read_quad_i32(&cursor, &current_glyph->bbox))
            {
                log_error("Could not read BBX");
                return false;
            }
        }
        else if (read_word(&cursor, "BITMAP"))
        {
            uint32_t glyph_stride = (current_glyph->bbox.extent[0] + 7) / 8;
            for (int32_t y = 0; y < current_glyph->bbox.extent[1]; y++)
            {
                line = get_next_line(line);
                cursor = line;

                for (uint32_t x = 0; x < glyph_stride; x++)
                {
                    uint8_t* byte = &current_glyph_bitmap[font->stride * y + x];
                    if (!read_hex_byte(&cursor, byte))
                    {
                        log_error("Could not read hex byte");
                        return false;
                    }
                }
            }
        }
        else if (read_word(&cursor, "SWIDTH"))
        {
            // ignore line.
        }
        else
        {
            log_warning("Unhandled line %.*s",
                        get_next_line(line) - line,
                        line);
        }

        line = get_next_line(line);
    }

    return true;
}

void print_glyph(font_t* font, uint32_t index)
{
    int32_t width = font->glyphs[index].bbox.extent[0];
    int32_t height = font->glyphs[index].bbox.extent[1];

    uint8_t* bytes = &font->bitmap[font->stride * font->bbox.extent[1] * index];

    printf("%c\n", font->glyphs[index].character);
    for (int32_t y = 0; y < height; y++)
    {
        printf("%02X ", bytes[y * font->stride]);
        for (int32_t x = 0; x < width; x++)
        {
            uint8_t byte = bytes[y * font->stride + x / 8];
            printf("%c", byte & (1 << (7 - x % 8)) ? '#' : '.');
        }
        printf("\n");
    }
}

void texture_create_from_font(mem_stack_allocator_t* tmp,
                              texture_t* texture,
                              const font_t* font)
{
    texture_create(
        texture,
        font->bbox.extent[0],
        1 + font->bbox.extent[1] * font->glyph_count); // keep one line of white

    uint32_t texture_stride = 4 * texture->width;
    uint8_t* pixels = mem_stack_push(tmp, texture_stride * texture->height);

    // Fill white line
    for (uint32_t x = 0; x < 4 * texture->width; x++)
    {
        pixels[x] = 0xFF;
    }

    for (uint32_t glyph = 0; glyph < font->glyph_count; glyph++)
    {
        uint8_t* bytes =
            &font->bitmap[font->stride * font->bbox.extent[1] * glyph];
        uint8_t* glyph_pixels =
            &pixels[texture_stride * (font->bbox.extent[1] * glyph + 1)];
        for (int32_t y = 0; y < font->bbox.extent[1]; y++)
        {
            for (int32_t x = 0; x < font->bbox.extent[0]; x++)
            {
                uint8_t* pixel = &glyph_pixels[4 * (y * texture->width + x)];

                if (x >= font->glyphs[glyph].bbox.extent[0]
                    || y >= font->glyphs[glyph].bbox.extent[1])
                {
                    pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
                    continue;
                }

                uint8_t byte = bytes[y * font->stride + x / 8];
                uint8_t bit = (byte >> (7 - x % 8)) & 1;

                pixel[0] = bit ? 0xFF : 0x00;
                pixel[1] = bit ? 0xFF : 0x00;
                pixel[2] = bit ? 0xFF : 0x00;
                pixel[3] = bit ? 0xFF : 0x00;
            }
        }
    }

    texture_set_data(texture, pixels);
}

void renderer_init(mem_stack_allocator_t* tmp,
                   mem_stack_allocator_t* permanent,
                   renderer_t* renderer)
{
    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ebo);
    glGenVertexArrays(1, &renderer->vao);

    renderer->shader =
        compile_shader(tmp, "shaders/shader.vs", "shaders/shader.fs");

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(vertex_t),
                          (void*)offsetof(vertex_t, position));

    // uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(vertex_t),
                          (void*)offsetof(vertex_t, uv));

    // color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          4,
                          GL_UNSIGNED_BYTE,
                          GL_TRUE,
                          sizeof(vertex_t),
                          (void*)offsetof(vertex_t, color));

    glBindVertexArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    renderer->vertex_data = mem_stack_push(permanent, Gibi(1));
    renderer->index_data = mem_stack_push(permanent, Gibi(1));

    // Font texture
    load_bdf(tmp, tmp, &renderer->font, "assets/haxor11.bdf");
    texture_create_from_font(tmp, &renderer->font_texture, &renderer->font);
}

void draw_quad(renderer_t* renderer,
               quad_i32_t pos,
               quad_float_t uv,
               color_t color)
{
    vertex_t* vertices = &renderer->vertex_data[4 * renderer->quad_count];
    uint32_t* indices = &renderer->index_data[6 * renderer->quad_count];

    for (int i = 0; i < 4; i++)
    {
        memcpy(vertices[i].color, color.rgba, sizeof(color.rgba));
    }

    vertices[0].position[0] = pos.min[0];
    vertices[0].position[1] = pos.min[1];
    vertices[1].position[0] = pos.min[0] + pos.extent[0];
    vertices[1].position[1] = pos.min[1];
    vertices[2].position[0] = pos.min[0] + pos.extent[0];
    vertices[2].position[1] = pos.min[1] + pos.extent[1];
    vertices[3].position[0] = pos.min[0];
    vertices[3].position[1] = pos.min[1] + pos.extent[1];

    vertices[0].uv[0] = uv.min[0];
    vertices[0].uv[1] = uv.min[1];
    vertices[1].uv[0] = uv.min[0] + uv.extent[0];
    vertices[1].uv[1] = uv.min[1];
    vertices[2].uv[0] = uv.min[0] + uv.extent[0];
    vertices[2].uv[1] = uv.min[1] + uv.extent[1];
    vertices[3].uv[0] = uv.min[0];
    vertices[3].uv[1] = uv.min[1] + uv.extent[1];

    uint32_t v = renderer->quad_count * 4;
    indices[0] = v + 0;
    indices[1] = v + 1;
    indices[2] = v + 2;
    indices[3] = v + 0;
    indices[4] = v + 2;
    indices[5] = v + 3;

    renderer->quad_count++;
}

void draw_colored_quad(renderer_t* renderer, quad_i32_t pos, color_t color)
{
    draw_quad(renderer, pos, (quad_float_t){0}, color);
}

void draw_glyph(renderer_t* renderer,
                uint32_t glyph_index,
                int32_t x,
                int32_t y,
                color_t color)
{
    font_glyph_t* glyph = &renderer->font.glyphs[glyph_index];
    quad_float_t uv = {0};
    uv.min[0] = 1.0f / renderer->font_texture.height;
    uv.min[1] = (float)(glyph_index * renderer->font.bbox.extent[1] + 1.0f)
                / renderer->font_texture.height;
    uv.extent[0] =
        (float)(glyph->bbox.extent[0]) / renderer->font_texture.width;
    uv.extent[1] =
        (float)(glyph->bbox.extent[1]) / renderer->font_texture.height;

    quad_i32_t pos = {0};
    pos.min[0] = x - glyph->bbox.min[0] - glyph->bbox.extent[0];
    pos.min[1] = y - glyph->bbox.extent[1] - glyph->bbox.min[1]
                 + renderer->font.bbox.extent[1];
    pos.extent[0] = glyph->bbox.extent[0];
    pos.extent[1] = glyph->bbox.extent[1];

    draw_quad(renderer, pos, uv, color);
}

uint32_t get_glyph_index(font_t* font, uint32_t c)
{
    for (uint32_t i = 0; i < font->glyph_count; i++)
    {
        if (font->glyphs[i].character == c)
        {
            return i;
        }
    }
    return 0;
}

void draw_text(renderer_t* renderer,
               const char* text,
               int32_t x,
               int32_t y,
               color_t color)
{
    x += renderer->font.bbox.extent[0];

    const char* s = text;
    char c;
    while ((c = *s++))
    {
        uint32_t idx = get_glyph_index(&renderer->font, c);
        draw_glyph(renderer, idx, x, y, color);
        x += renderer->font.glyphs[idx].dx;
        y += renderer->font.glyphs[idx].dy;
    }
}

void render(renderer_t* renderer, uint32_t width, uint32_t height)
{
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 renderer->quad_count * 6 * sizeof(uint32_t),
                 renderer->index_data,
                 GL_STREAM_DRAW);
    glBufferData(GL_ARRAY_BUFFER,
                 renderer->quad_count * 4 * sizeof(vertex_t),
                 renderer->vertex_data,
                 GL_STREAM_DRAW);

    glViewport(0, 0, width, height);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderer->shader);
    glBindVertexArray(renderer->vao);

    glDrawElements(GL_TRIANGLES, renderer->quad_count * 6, GL_UNSIGNED_INT, 0);

    renderer->quad_count = 0;
}

typedef struct ui_node_t
{
    quad_i32_t bbox;
} ui_node_t;

struct
{
    platform_input_info_t* input;
    renderer_t* renderer;

    int32_t cursor_x;
    int32_t cursor_y;

    uint64_t active_id;
    uint64_t hovered_id;

    uint64_t id_stack[1024];
    uint32_t id_stack_height;

    int32_t draw_region_stack[1024][2];
    uint32_t draw_region_stack_height;

    struct
    {
        color_t main;
        color_t secondary;
        color_t background;
        color_t text;
        color_t text_shadow;
        color_t active_overlay;
        color_t hover_overlay;
    } colors;

    uint32_t padding;
    uint32_t margin;
    uint32_t line_height;

    ui_node_t* current_node;
    const char* current_node_name;
} ui;

bool mouse_inside_region(int32_t x, int32_t y, int32_t width, int32_t height)
{
    return ui.input->mouse_x >= x && ui.input->mouse_x < x + width //
           && ui.input->mouse_y >= y && ui.input->mouse_y < y + height;
}

static uint64_t hash_combine(uint64_t base, uint64_t new)
{
    return base * 37 + new;
}

static uint64_t hash_string(const char* txt)
{
    uint64_t h = 1;
    char c;
    while ((c = *txt++))
    {
        h = hash_combine(h, c);
    }

    return h;
}

static void ui_push_id(uint64_t id)
{
    ASSERT(ui.id_stack_height < ARRAY_COUNT(ui.id_stack));
    uint64_t hashed =
        ui.id_stack_height
            ? hash_combine(ui.id_stack[ui.id_stack_height - 1], id)
            : id;
    ui.id_stack[ui.id_stack_height++] = hashed;
}

static void ui_push_string_id(const char* txt) { ui_push_id(hash_string(txt)); }

static void ui_pop_id()
{
    ASSERT(ui.id_stack_height);
    ui.id_stack_height--;
}

static uint64_t ui_current_id()
{
    ASSERT(ui.id_stack_height);

    return ui.id_stack[ui.id_stack_height - 1];
}

static void ui_begin_draw_region(int32_t x, int32_t y)
{
    ASSERT(ui.draw_region_stack_height < ARRAY_COUNT(ui.draw_region_stack));

    ui.draw_region_stack[ui.draw_region_stack_height][0] = ui.cursor_x;
    ui.draw_region_stack[ui.draw_region_stack_height][1] = ui.cursor_y;
    ui.draw_region_stack_height++;

    ui.cursor_x = x + ui.margin;
    ui.cursor_y = y + ui.margin;
}

static void ui_end_draw_region()
{
    ASSERT(ui.draw_region_stack_height);
    ui.draw_region_stack_height--;

    int32_t(*cursor)[2] = &ui.draw_region_stack[ui.draw_region_stack_height];

    ui.cursor_x = (*cursor)[0];
    ui.cursor_y = (*cursor)[1];
}

static void draw_shadowed_text(renderer_t* renderer,
                               const char* text,
                               int32_t x,
                               int32_t y,
                               color_t text_color,
                               color_t shadow_color)
{
    draw_text(renderer, text, x + 1, y + 1, shadow_color);
    draw_text(renderer, text, x, y, text_color);
}

float clamped_float(float t, float min, float max)
{
    if (t < min)
    {
        return min;
    }
    else if (t > max)
    {
        return max;
    }
    else
    {
        return t;
    }
}

bool ui_handle_hover(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    bool is_inside = mouse_inside_region(x, y, w, h);

    if (!ui.hovered_id && is_inside)
    {
        ui.hovered_id = ui_current_id();
        return true;
    }
    else if (ui.hovered_id == ui_current_id() && !is_inside)
    {
        ui.hovered_id = 0;
        return false;
    }

    return ui.hovered_id == ui_current_id();
}

bool ui_handle_hold_and_release(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    bool hovered = ui_handle_hover(x, y, w, h);

    if (hovered && (ui.input->mouse_pressed & MOUSE_BUTTON_LEFT)
        && !ui.active_id)
    {
        ui.active_id = ui_current_id();
        return false;
    }
    else if (ui.active_id == ui_current_id()
             && (ui.input->mouse_released & MOUSE_BUTTON_LEFT))
    {
        ui.active_id = 0;
        return true;
    }
    else
    {
        return false;
    }
}

bool ui_handle_drag(int32_t x,
                    int32_t y,
                    uint32_t w,
                    uint32_t h,
                    int32_t* dx,
                    int32_t* dy)
{
    ui_handle_hold_and_release(x, y, w, h);

    if (ui.active_id == ui_current_id())
    {
        *dx = ui.input->mouse_dx;
        *dy = ui.input->mouse_dy;

        return *dx || *dy;
    }

    return false;
}

bool ui_handle_drag_x(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t* dx)
{
    int32_t dy;
    return ui_handle_drag(x, y, w, h, dx, &dy) && *dx;
}

bool ui_handle_drag_y(int32_t x, int32_t y, uint32_t w, uint32_t h, int32_t* dy)
{
    int32_t dx;
    return ui_handle_drag(x, y, w, h, &dx, dy) && *dy;
}

float lerp(float t, float min, float max) { return min + t * (max - min); }

float unlerp(float t, float min, float max) { return (t - min) / (max - min); }

void ui_newline() { ui.cursor_y += ui.line_height + ui.margin; }

void ui_draw_hover_and_active_overlay(int32_t x,
                                      int32_t y,
                                      uint32_t width,
                                      uint32_t height)
{
    if (ui.active_id == ui_current_id())
    {
        draw_colored_quad(ui.renderer,
                          (quad_i32_t){{x, y}, {width, height}},
                          ui.colors.active_overlay);
    }
    else if (ui.hovered_id == ui_current_id())
    {
        draw_colored_quad(ui.renderer,
                          (quad_i32_t){{x, y}, {width, height}},
                          ui.colors.hover_overlay);
    }
}

void ui_draw_text(const char* txt, int32_t x, int32_t y)
{
    draw_shadowed_text(ui.renderer,
                       txt,
                       x + ui.padding,
                       y + ui.padding
                           - 3, // TODO(octave) : why 3 ? font ascent ?
                       ui.colors.text,
                       ui.colors.text_shadow);
}

bool ui_slider(const char* txt, float* value, float min, float max)
{
    ui_push_string_id(txt);

    uint32_t box_width = 200;
    uint32_t box_height = ui.line_height;
    int32_t box_x = ui.cursor_x;
    int32_t box_y = ui.cursor_y;

    float range = max - min;
    bool finiteRange = range < INFINITY;
    float unitsToPixels = finiteRange ? box_width / range : 100.0f;

    bool changed = false;

    int32_t dx;
    if (ui_handle_drag_x(box_x, box_y, box_width, box_height, &dx))
    {
        *value += (float)dx / unitsToPixels;
        *value = clamped_float(*value, min, max);
    }

    draw_colored_quad(ui.renderer,
                      (quad_i32_t){{box_x, box_y}, {box_width, box_height}},
                      ui.colors.secondary);
    if (finiteRange)
    {
        int32_t slider_position =
            unitsToPixels * (clamped_float(*value, min, max) - min);

        draw_colored_quad(
            ui.renderer,
            (quad_i32_t){{box_x, box_y}, {slider_position, box_height}},
            ui.colors.main);
    }

    char value_txt[256];
    snprintf(value_txt, sizeof(value_txt), "%.3f", *value);

    ui_draw_text(value_txt, box_x, box_y);

    ui_draw_hover_and_active_overlay(box_x, box_y, box_width, box_height);

    ui_pop_id();

    ui_newline();

    return changed;
}

bool ui_button(const char* txt)
{
    ui_push_string_id(txt);

    int32_t width = ui.renderer->font.bbox.extent[0] * strlen(txt) + 8;
    int32_t height = ui.line_height;
    int32_t x = ui.cursor_x;
    int32_t y = ui.cursor_y;

    bool result = ui_handle_hold_and_release(x, y, width, height);

    quad_i32_t pos_quad = {{x, y}, {width, height}};
    draw_colored_quad(ui.renderer, pos_quad, ui.colors.main);

    ui_draw_text(txt, x, y);

    ui_draw_hover_and_active_overlay(x, y, width, height);

    ui_pop_id();

    ui_newline();

    return result;
}

void ui_init(renderer_t* renderer, platform_input_info_t* input)
{
    ui.renderer = renderer;
    ui.input = input;

    ui.colors.main = color_gray(0x60);
    ui.colors.secondary = color_gray(0x30);
    ui.colors.background = color_gray(0xB0);

    ui.colors.text = color_gray(0xFF);
    ui.colors.text_shadow = color_gray(0x40);

    ui.colors.hover_overlay = color_rgba(0xFF, 0xFF, 0xFF, 0x40);
    ui.colors.active_overlay = color_rgba(0xFF, 0xFF, 0xFF, 0x20);

    ui.padding = 4;
    ui.line_height = ui.renderer->font.bbox.extent[1] + 2 * ui.padding;
    ui.margin = 2;

    ui_push_id(1);
}

void ui_begin_node(const char* name, ui_node_t* node)
{
    ASSERT(!ui.current_node);
    ASSERT(!ui.current_node_name);

    ui.current_node = node;
    ui.current_node_name = name;

    ui_push_string_id(name);

    int32_t x = node->bbox.min[0];
    int32_t y = node->bbox.min[1];
    uint32_t width = node->bbox.extent[0];
    /* uint32_t height = node->bbox.extent[1]; */

    draw_colored_quad(ui.renderer, node->bbox, ui.colors.background);
    draw_colored_quad(ui.renderer,
                      (quad_i32_t){{x, y}, {width, ui.line_height}},
                      ui.colors.main);

    ui_draw_text(ui.current_node_name, x, y);

    ui_begin_draw_region(x, y + ui.line_height);
}

void ui_end_node()
{
    ASSERT(ui.current_node);

    ui_node_t* node = ui.current_node;

    int32_t x = node->bbox.min[0];
    int32_t y = node->bbox.min[1];
    uint32_t width = node->bbox.extent[0];
    uint32_t height = node->bbox.extent[1];

    int32_t dx, dy;
    if (ui_handle_drag(x, y, width, height, &dx, &dy))
    {
        node->bbox.min[0] += dx;
        node->bbox.min[1] += dy;
    }

    ui_end_draw_region();

    ui_pop_id();
    ui.current_node = 0;
    ui.current_node_name = 0;
}

void ui_begin()
{
    ui_begin_draw_region(0, 0);
    ui.hovered_id = 0;
}

void ui_end()
{
    ui_end_draw_region();
    ASSERT(ui.draw_region_stack_height == 0);

    ASSERT(ui.id_stack_height == 1);
}

int main(int argc, const char** argv)
{
    ASSERT(sizeof(void*) == sizeof(uint64_t));
    (void)argc;

    log_init();

    void* tmp_stack_buf = platform_virtual_alloc(Gibi(1024));
    mem_stack_allocator_t* tmp_alloc =
        mem_stack_create(tmp_stack_buf, Gibi(1024));

    void* permanent_stack_buf = platform_virtual_alloc(Gibi(1024));
    mem_stack_allocator_t* permanent_alloc =
        mem_stack_create(permanent_stack_buf, Gibi(1024));

    uint32_t window_width = 640;
    uint32_t window_height = 480;
    if (!platform_init(argv[0], window_width, window_height))
    {
        return 1;
    }

    renderer_t renderer = {0};
    renderer_init(tmp_alloc, permanent_alloc, &renderer);

    platform_input_info_t input = {0};

    ui_init(&renderer, &input);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float freq;

    uint64_t t0 = platform_get_nanoseconds();

    ui_node_t node = {{{10, 10}, {200, 200}}};

    // main loop
    while (!input.should_exit)
    {
        /* log_debug("active = %lu, hovered = %lu", ui.active_id, ui.hovered_id); */
        uint64_t now = platform_get_nanoseconds();

        float t = (now - t0) * 1e-9f;
        float y = sinf(t);

        platform_handle_input_events(&input);

        ui_begin();

        if (ui_button("Do the thingy"))
        {
            log_debug("Did the thingy");
        }
        if (ui_button("Undo the thingy"))
        {
        }

        ui_begin_node("my node", &node);

        if (ui_button("Redo the thingy"))
        {
            log_debug("Did the thingy");
        }
        ui_end_node();
        y = sinf(freq * t);
        ui_slider("freq", &freq, 0.0f, 10.0f);
        ui_slider("sin(freq * t)", &y, -1.0f, 1.0f);

        ui_end();

        render(&renderer, input.width, input.height);
        platform_swap_buffers();

        log_flush();
    }

    log_terminate();

    return 0;
}
