#include "console.h"
#include "data.h"
#include "object.h"
#include "shader.h"
#include "log.h"

#include "stb/stb_rectpack.h"
#include "stb/stb_truetype.h"

////////////////////////////////////////////////////////////////////////////////

static const GLchar* CONSOLE_VS_SRC = GLSL(330,
layout(location=0) in vec2 pos;
layout(location=1) in vec2 tex_coord;
layout(location=2) in float shade;

out vec2 frag_tex_coord;
out float frag_shade;

void main() {
    gl_Position = vec4(pos, 1.0f, 1.0f);
    frag_tex_coord = tex_coord;
    frag_shade = shade;
}
);

static const GLchar* CONSOLE_FS_SRC = GLSL(330,
in vec2 frag_tex_coord;
in float frag_shade;

out vec4 out_color;

uniform sampler2D tex;

void main() {
    int ix = int(gl_FragCoord.x + 0.5f);
    int iy = int(gl_FragCoord.y + 0.5f);
    float t = texture(tex, frag_tex_coord).r;

    if (t == 0.0f) {
        discard;
    } else {
        float f = 1.0f - frag_shade;
        out_color = vec4(f, f, f, t);
    }
}
);


////////////////////////////////////////////////////////////////////////////////

const static unsigned FONT_IMAGE_SIZE = 1000;
const static unsigned FONT_SIZE_PX = 64;
struct console_ {
    stbtt_pack_context context;
    stbtt_packedchar* chars;

    GLuint vao;
    GLuint vbo;
    GLuint tex;

    shader_t shader;
    GLint u_tex;
};

console_t* console_new() {
    OBJECT_ALLOC(console);

    uint8_t* pixels = malloc(FONT_IMAGE_SIZE * FONT_IMAGE_SIZE);
    if (!stbtt_PackBegin(&console->context, pixels,
                         FONT_IMAGE_SIZE, FONT_IMAGE_SIZE,
                         0, 1, NULL))
    {
        log_error_and_abort("stbtt_PackBegin failed");
    }
    stbtt_PackSetOversampling(&console->context, 2, 2);

    size_t num_chars = '~' - ' ';
    console->chars = malloc(sizeof(stbtt_packedchar) * num_chars);

    if (!stbtt_PackFontRange(&console->context, FONT, 0, FONT_SIZE_PX, ' ',
                             num_chars, console->chars))
    {
        log_error_and_abort("stbtt_PackFontRange failed");
    }
    stbtt_PackEnd(&console->context);

    glGenTextures(1, &console->tex);
    glBindTexture(GL_TEXTURE_2D, console->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_IMAGE_SIZE, FONT_IMAGE_SIZE, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pixels);
    log_gl_error();

    free(pixels);

    glGenBuffers(1, &console->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, console->vbo);

    glGenVertexArrays(1, &console->vao);
    glBindVertexArray(console->vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
            0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
            1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
            2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            (void*)(4 * sizeof(float)));

    console->shader = shader_new(CONSOLE_VS_SRC, NULL, CONSOLE_FS_SRC);
    glUseProgram(console->shader.prog);

    {   /* Make a temporary struct to unpack local uniforms */
        GLint prog = console->shader.prog;
        struct { GLint tex; } u;
        SHADER_GET_UNIFORM(tex);
        console->u_tex = u.tex;
    }
    log_gl_error();

    return console;
}

void console_delete(console_t* console) {
    free(console->chars);
    glDeleteTextures(1, &console->tex);
    glDeleteBuffers(1, &console->vbo);
    glDeleteVertexArrays(1, &console->vao);
    shader_deinit(console->shader);
    free(console);
}

void console_draw(console_t* console, float aspect_ratio, const char* s) {
    float x = 0.0f;
    float y = 0.0f;
    float buf[4096];
    unsigned i = 0;
    float shade = 1.0f;
    while (*s) {
        if (*s == 1) {
            shade = 0.7f;
        } else if (*s == 2) {
            shade = 1.0f;
        } else {
            stbtt_aligned_quad q;
            stbtt_GetPackedQuad(
                    console->chars, FONT_IMAGE_SIZE, FONT_IMAGE_SIZE,
                    *s - ' ', &x, &y, &q, 0);
#define WRITE(u, v) do {        \
                buf[i++] = q.x##u;  \
                buf[i++] = q.y##v;  \
                buf[i++] = q.s##u;  \
                buf[i++] = q.t##v;  \
                buf[i++] = shade;   \
            } while(0)

            WRITE(0, 0);
            WRITE(1, 0);
            WRITE(0, 1);

            WRITE(0, 1);
            WRITE(1, 0);
            WRITE(1, 1);
        }
        ++s;
    }
    const unsigned index_count = i / 5;

    x /= 2.0f;  // Prepare to center the text
    const float scale = 0.1f / FONT_SIZE_PX;
    for (unsigned j=0; j < i; j += 5) {
        buf[j] = (buf[j] - x) * scale / aspect_ratio;
        buf[j + 1] = -buf[j + 1] * scale + 0.7;
    }

    glUseProgram(console->shader.prog);
    glBindVertexArray(console->vao);
    glBindBuffer(GL_ARRAY_BUFFER, console->vbo);
    glBufferData(GL_ARRAY_BUFFER, index_count * 5 * sizeof(float),
                 buf, GL_DYNAMIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, console->tex);
    glUniform1i(console->u_tex, 0);
    log_gl_error();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, index_count);
    glDisable(GL_BLEND);
}
