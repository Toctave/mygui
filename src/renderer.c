#include "renderer.h"

#include "assert.h"
#include "memory.h"
#include "opengl_functions.h"
#include "plugin_sdk.h"
#include "util.h"

#include <math.h>
#include <string.h>

typedef enum shader_stage_e
{
    SHADER_STAGE_VERTEX,
    SHADER_STAGE_FRAGMENT,
} shader_stage_e;

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

typedef struct texture_t
{
    GLuint index;
    uint32_t width;
    uint32_t height;
} texture_t;

typedef struct vertex_t
{
    float position[2];
    float uv[2];
    uint8_t color[4];
} vertex_t;

typedef struct renderer_o
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
} renderer_o;

void* read_alloc_file(mem_api* mem, mem_stack_o* alloc, const char* path)
{
    platform_file_o* f = platform_open_file(path);
    if (!f)
    {
        return 0;
    }
    uint64_t size = platform_get_file_size(f);

    uint64_t prev = mem->stack_get_cursor(alloc);
    void* buf = mem->stack_push(alloc, size);

    if (platform_read_file(f, buf, size) != size)
    {
        mem->stack_revert(alloc, prev);
        buf = 0;
    }

    platform_close_file(f);

    return buf;
}

char* read_alloc_text_file(mem_api* mem, mem_stack_o* alloc, const char* path)
{
    platform_file_o* f = platform_open_file(path);
    if (!f)
    {
        return 0;
    }
    uint64_t size = platform_get_file_size(f);

    uint64_t prev = mem->stack_get_cursor(alloc);
    char* buf = mem->stack_push(alloc, size + 1);

    if (platform_read_file(f, buf, size) != size)
    {
        mem->stack_revert(alloc, prev);
        buf = 0;
    }
    else
    {
        buf[size] = '\0';
    }

    platform_close_file(f);

    return buf;
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

static bool load_bdf(mem_api* mem,
                     mem_stack_o* tmp,
                     mem_stack_o* permanent,
                     font_t* font,
                     const char* bdf_path)
{
    ASSERT(!font->glyphs);
    const char* bdf = read_alloc_text_file(mem, tmp, bdf_path);
    if (!bdf)
    {
        return false;
    }
    const char* line = bdf;

    bool in_properties = false;
    bool in_char = false;

    bool bbox_read = false;
    (void)bbox_read;

    int32_t character_count = 0;
    bool character_count_read = false;
    font_glyph_t* current_glyph = 0;
    uint8_t* current_glyph_bitmap = 0;
    uint32_t glyph_bitmap_size = 0;

    int32_t size[3];
    bool size_read = false;
    (void)size_read;

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
                    mem->stack_push(permanent,
                                    sizeof(font_glyph_t) * font->glyph_count);

                font->stride = (font->bbox.extent[0] + 7) / 8;
                glyph_bitmap_size = font->stride * font->bbox.extent[1];
                font->bitmap =
                    mem->stack_push(permanent,
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
                        (int)(get_next_line(line) - line),
                        line);
        }

        line = get_next_line(line);
    }

    return true;
}

static void texture_create(texture_t* texture, uint32_t width, uint32_t height)
{
    glGenTextures(1, &texture->index);
    texture->width = width;
    texture->height = height;

    glBindTexture(GL_TEXTURE_2D, texture->index);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static void texture_destroy(texture_t* texture)
{
    glDeleteTextures(1, &texture->index);
}

static void texture_set_data(texture_t* texture, uint8_t* data)
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

static void texture_create_from_font(mem_api* mem,
                                     mem_stack_o* tmp,
                                     texture_t* texture,
                                     const font_t* font)
{
    texture_create(
        texture,
        font->bbox.extent[0],
        1 + font->bbox.extent[1] * font->glyph_count); // keep one line of white

    uint32_t texture_stride = 4 * texture->width;
    uint8_t* pixels = mem->stack_push(tmp, texture_stride * texture->height);

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

static GLuint compile_shader_stage(mem_api* mem,
                                   mem_stack_o* tmp,
                                   const char* path,
                                   shader_stage_e stage_type)
{
    GLenum gl_stage_type = 0;
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

    const char* abs_path = tprintf(mem, tmp, "%s/%s", EXECUTABLE_PATH, path);
    const char* source = read_alloc_text_file(mem, tmp, abs_path);
    ASSERT(source);

    const char* sources[] = {source};

    glShaderSource(stage, STATIC_ARRAY_COUNT(sources), sources, 0);

    glCompileShader(stage);
    GLint ok;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        GLsizei info_log_length;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &info_log_length);

        char* info_log = mem->stack_push(tmp, info_log_length);
        glGetShaderInfoLog(stage, info_log_length, 0, info_log);

        log_error("Shader compilation error :\n");
        const char* line = info_log;
        int line_length = 0;
        while (line[line_length])
        {
            if (line[line_length] == '\n')
            {
                log_continue("%s:%.*s\n", path, line_length, line);
                line += line_length + 1;
                line_length = 0;
            }
            else
            {
                line_length++;
            }
        }

        glDeleteShader(stage);
        stage = 0;
    }

    return stage;
}

static GLuint compile_shader(mem_api* mem,
                             mem_stack_o* tmp,
                             const char* vertex_path,
                             const char* fragment_path)
{
    GLuint vs =
        compile_shader_stage(mem, tmp, vertex_path, SHADER_STAGE_VERTEX);
    if (!vs)
    {
        return 0;
    }

    GLuint fs =
        compile_shader_stage(mem, tmp, fragment_path, SHADER_STAGE_FRAGMENT);
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

        char* info_log = mem->stack_push(tmp, info_log_length);
        glGetProgramInfoLog(program, info_log_length, 0, info_log);

        log_error("Shader linking error :\n%s", info_log);

        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

static bool renderer_init(mem_api* mem,
                          mem_stack_o* permanent,
                          mem_stack_o* tmp,
                          renderer_o* renderer)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ebo);
    glGenVertexArrays(1, &renderer->vao);

    renderer->shader =
        compile_shader(mem, tmp, "shaders/shader.vs", "shaders/shader.fs");

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

    renderer->vertex_data = mem->stack_push(permanent, Gibi(1));
    renderer->index_data = mem->stack_push(permanent, Gibi(1));

    // Font texture
    if (!load_bdf(mem, tmp, tmp, &renderer->font, "assets/haxor11.bdf"))
    {
        return false;
    }
    texture_create_from_font(mem,
                             tmp,
                             &renderer->font_texture,
                             &renderer->font);

    return true;
}

static void draw_quad_ex(renderer_o* renderer,
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

static void draw_line(renderer_i* renderer,
                      float x1,
                      float y1,
                      float x2,
                      float y2,
                      float width,
                      color_t color)
{
    vertex_t* vertices =
        &renderer->impl->vertex_data[4 * renderer->impl->quad_count];
    uint32_t* indices =
        &renderer->impl->index_data[6 * renderer->impl->quad_count];

    for (int i = 0; i < 4; i++)
    {
        memcpy(vertices[i].color, color.rgba, sizeof(color.rgba));
    }

    float lx = (x2 - x1);
    float ly = (y2 - y1);
    float length = sqrtf(lx * lx + ly * ly);

    // normal
    float nx = -ly / length;
    float ny = lx / length;

    // offset from center line
    float dx = .5f * width * nx;
    float dy = .5f * width * ny;

    vertices[0].position[0] = x1 - dx;
    vertices[0].position[1] = y1 - dy;
    vertices[1].position[0] = x1 + dx;
    vertices[1].position[1] = y1 + dy;

    vertices[2].position[0] = x2 + dx;
    vertices[2].position[1] = y2 + dy;
    vertices[3].position[0] = x2 - dx;
    vertices[3].position[1] = y2 - dy;

    vertices[0].uv[0] = 0.0f;
    vertices[0].uv[1] = 0.0f;
    vertices[1].uv[0] = 0.0f;
    vertices[1].uv[1] = 0.0f;
    vertices[2].uv[0] = 0.0f;
    vertices[2].uv[1] = 0.0f;
    vertices[3].uv[0] = 0.0f;
    vertices[3].uv[1] = 0.0f;

    uint32_t v = renderer->impl->quad_count * 4;
    indices[0] = v + 0;
    indices[1] = v + 1;
    indices[2] = v + 2;
    indices[3] = v + 0;
    indices[4] = v + 2;
    indices[5] = v + 3;

    renderer->impl->quad_count++;
}

static void draw_quad(renderer_i* renderer, quad_i32_t pos, color_t color)
{
    draw_quad_ex(renderer->impl, pos, (quad_float_t){0}, color);
}

static void draw_glyph(renderer_i* renderer,
                       uint32_t glyph_index,
                       int32_t x,
                       int32_t y,
                       color_t color)
{
    font_glyph_t* glyph = &renderer->impl->font.glyphs[glyph_index];
    quad_float_t uv = {0};
    uv.min[0] = 1.0f / renderer->impl->font_texture.height;
    uv.min[1] =
        (float)(glyph_index * renderer->impl->font.bbox.extent[1] + 1.0f)
        / renderer->impl->font_texture.height;
    uv.extent[0] =
        (float)(glyph->bbox.extent[0]) / renderer->impl->font_texture.width;
    uv.extent[1] =
        (float)(glyph->bbox.extent[1]) / renderer->impl->font_texture.height;

    quad_i32_t pos = {0};
    pos.min[0] = x - glyph->bbox.min[0] - glyph->bbox.extent[0];
    pos.min[1] = y - glyph->bbox.extent[1] - glyph->bbox.min[1]
                 + renderer->impl->font.bbox.extent[1];
    pos.extent[0] = glyph->bbox.extent[0];
    pos.extent[1] = glyph->bbox.extent[1];

    draw_quad_ex(renderer->impl, pos, uv, color);
}

static uint32_t get_glyph_index(font_t* font, uint32_t c)
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

static void draw_text(renderer_i* renderer,
                      const char* text,
                      int32_t x,
                      int32_t y,
                      color_t color)
{
    x += renderer->impl->font.bbox.extent[0];

    const char* s = text;
    char c;
    while ((c = *s++))
    {
        uint32_t idx = get_glyph_index(&renderer->impl->font, c);
        draw_glyph(renderer, idx, x, y, color);
        x += renderer->impl->font.glyphs[idx].dx;
        y += renderer->impl->font.glyphs[idx].dy;
    }
}

static void draw_shadowed_text(renderer_i* renderer,
                               const char* text,
                               int32_t x,
                               int32_t y,
                               color_t text_color,
                               color_t shadow_color)
{
    draw_text(renderer, text, x + 1, y + 1, shadow_color);
    draw_text(renderer, text, x, y, text_color);
}

static void do_draw(renderer_i* renderer, uint32_t width, uint32_t height)
{
    glBindBuffer(GL_ARRAY_BUFFER, renderer->impl->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->impl->ebo);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 renderer->impl->quad_count * 6 * sizeof(uint32_t),
                 renderer->impl->index_data,
                 GL_STREAM_DRAW);
    glBufferData(GL_ARRAY_BUFFER,
                 renderer->impl->quad_count * 4 * sizeof(vertex_t),
                 renderer->impl->vertex_data,
                 GL_STREAM_DRAW);

    glViewport(0, 0, width, height);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderer->impl->shader);
    glUniform1ui(glGetUniformLocation(renderer->impl->shader, "width"), width);
    glUniform1ui(glGetUniformLocation(renderer->impl->shader, "height"),
                 height);

    glBindVertexArray(renderer->impl->vao);

    glDrawElements(GL_TRIANGLES,
                   renderer->impl->quad_count * 6,
                   GL_UNSIGNED_INT,
                   0);

    renderer->impl->quad_count = 0;
}

static uint32_t get_text_width(renderer_i* renderer, const char* txt)
{
    return renderer->impl->font.bbox.extent[0] * strlen(txt);
}

static uint32_t get_font_height(renderer_i* renderer)
{
    return renderer->impl->font.bbox.extent[1];
}

static renderer_i*
create(mem_api* mem, mem_stack_o* permanent, mem_stack_o* tmp)
{
    uint64_t cursor = mem->stack_get_cursor(permanent);

    renderer_i* result = mem->stack_push(permanent, sizeof(renderer_i));
    *result = (renderer_i){0};
    result->impl = mem->stack_push(permanent, sizeof(renderer_o));
    *result->impl = (renderer_o){0};

    if (!renderer_init(mem, permanent, tmp, result->impl))
    {
        mem->stack_revert(permanent, cursor);
        return 0;
    }

    result->draw_quad = draw_quad;
    result->draw_text = draw_text;
    result->draw_shadowed_text = draw_shadowed_text;
    result->draw_line = draw_line;
    result->do_draw = do_draw;
    result->get_text_width = get_text_width;
    result->get_font_height = get_font_height;

    return result;
}

static void load(void* api)
{
    renderer_api* rdr = api;

    rdr->create = create;
    rdr->destroy = 0;
}

plugin_spec_t PLUGIN_SPEC = {
    .name = "renderer",
    .version = {0, 0, 1},
    .load = load,
    .api_size = sizeof(renderer_api),
};
