#include "data.h"
#include "gui.h"
#include "object.h"
#include "shader.h"
#include "log.h"

#include "stb/stb_rectpack.h"
#include "stb/stb_truetype.h"

////////////////////////////////////////////////////////////////////////////////

static const GLchar* GUI_VS_SRC = GLSL(330,
layout(location=0) in vec3 vert_pos;
layout(location=1) in vec3 vert_tex;

out vec3 frag_tex;

void main() {
    gl_Position = vec4(vert_pos, 1.0f);
    frag_tex = vert_tex;
}
);

static const GLchar* GUI_FS_SRC = GLSL(330,
in vec3 frag_tex;
uniform sampler2D tex;

out vec4 out_color;

void main() {
    float frag_shade = frag_tex.z;
    if (frag_shade >= 0.0f) {
        float t = texture(tex, frag_tex.xy).r;
        float f = (1.0f - frag_shade);
        vec3 full_color = vec3(f, f, f);
        vec3 white = vec3(1.0f, 1.0f, 1.0f);
        vec3 delta = full_color - white;

        out_color = vec4(white + delta * t, 1.0f);
    } else if (frag_shade == -1.0f) {
        out_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    } else if (frag_shade <= -1.99f) {
        out_color = vec4(0.0f, 0.0f, 0.0f, -2.0f - frag_shade);
    } else {
        out_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
}
);


////////////////////////////////////////////////////////////////////////////////

const static unsigned FONT_IMAGE_SIZE = 1000;
const static unsigned FONT_SIZE_PX = 64;
struct gui_ {
    stbtt_pack_context context;
    stbtt_packedchar* chars;

    float* buf;
    size_t buf_index;
    size_t buf_size;

    GLuint vao;
    GLuint vbo;
    GLuint tex;

    shader_t shader;
    GLint u_tex;
};

////////////////////////////////////////////////////////////////////////////////

static void gui_push(gui_t* gui, float t) {
    if (gui->buf_index >= gui->buf_size) {
        if (gui->buf_size) {
            gui->buf_size *= 2;
        } else {
            gui->buf_size = 32;
        }
        gui->buf = realloc(gui->buf, gui->buf_size * sizeof(float));
    }
    gui->buf[gui->buf_index++] = t;
}

static void gui_push_vert(gui_t* gui, float x, float y, float z,
                                      float s, float t, float p)
{
    gui_push(gui, x);
    gui_push(gui, y);
    gui_push(gui, z);
    gui_push(gui, s);
    gui_push(gui, t);
    gui_push(gui, p);
}

static void gui_push_quad(gui_t* gui, stbtt_aligned_quad q, float z, float p) {
    gui_push_vert(gui, q.x0, q.y0, z, q.s0, q.t0, p);
    gui_push_vert(gui, q.x1, q.y0, z, q.s1, q.t0, p);
    gui_push_vert(gui, q.x0, q.y1, z, q.s0, q.t1, p);

    gui_push_vert(gui, q.x0, q.y1, z, q.s0, q.t1, p);
    gui_push_vert(gui, q.x1, q.y0, z, q.s1, q.t0, p);
    gui_push_vert(gui, q.x1, q.y1, z, q.s1, q.t1, p);
}

////////////////////////////////////////////////////////////////////////////////

gui_t* gui_new() {
    OBJECT_ALLOC(gui);

    uint8_t* pixels = malloc(FONT_IMAGE_SIZE * FONT_IMAGE_SIZE);
    if (!stbtt_PackBegin(&gui->context, pixels,
                         FONT_IMAGE_SIZE, FONT_IMAGE_SIZE,
                         0, 1, NULL))
    {
        log_error_and_abort("stbtt_PackBegin failed");
    }
    stbtt_PackSetOversampling(&gui->context, 2, 2);

    size_t num_chars = '~' - ' ';
    gui->chars = malloc(sizeof(stbtt_packedchar) * num_chars);

    if (!stbtt_PackFontRange(&gui->context, FONT, 0, FONT_SIZE_PX, ' ',
                             num_chars, gui->chars))
    {
        log_error_and_abort("stbtt_PackFontRange failed");
    }
    stbtt_PackEnd(&gui->context);

    glGenTextures(1, &gui->tex);
    glBindTexture(GL_TEXTURE_2D, gui->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_IMAGE_SIZE, FONT_IMAGE_SIZE,
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    log_gl_error();
    free(pixels);

    glGenBuffers(1, &gui->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gui->vbo);

    glGenVertexArrays(1, &gui->vao);
    glBindVertexArray(gui->vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
            (void*)(3 * sizeof(float)));

    gui->shader = shader_new(GUI_VS_SRC, NULL, GUI_FS_SRC);
    glUseProgram(gui->shader.prog);

    {   /* Make a temporary struct to unpack local uniforms */
        GLint prog = gui->shader.prog;
        struct { GLint tex; } u;
        SHADER_GET_UNIFORM(tex);
        gui->u_tex = u.tex;
    }
    log_gl_error();

    return gui;
}

void gui_delete(gui_t* gui) {
    free(gui->chars);
    free(gui->buf);
    glDeleteTextures(1, &gui->tex);
    glDeleteBuffers(1, &gui->vbo);
    glDeleteVertexArrays(1, &gui->vao);
    shader_deinit(gui->shader);
    free(gui);
}

void gui_reset(gui_t* gui) {
    gui->buf_index = 0;
}

void gui_backdrop(gui_t* gui, float aspect_ratio) {
    stbtt_aligned_quad q;
    q.x0 = -0.8;
    q.x1 = 0.8;
    q.y0 = 0.55;
    q.y1 = 0.90;

    const float z = 0.75f;
    gui_push_quad(gui, q, z, -1.0f);

    // Drop shadow!
    const float ry = 0.02f;
    const float rx = ry / aspect_ratio;

    // Left side
    gui_push_vert(gui, q.x0 - rx, q.y0, z, 0.0f, 0.0f, -2.0f);
    gui_push_vert(gui, q.x0, q.y0, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x0, q.y1, z, 0.0f, 0.0f, -2.2f);

    gui_push_vert(gui, q.x0 - rx, q.y0, z, 0.0f, 0.0f, -2.0f);
    gui_push_vert(gui, q.x0, q.y1, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x0 - rx, q.y1, z, 0.0f, 0.0f, -2.0f);

    // Right side
    gui_push_vert(gui, q.x1 + rx, q.y0, z, 0.0f, 0.0f, -2.0f);
    gui_push_vert(gui, q.x1, q.y0, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y1, z, 0.0f, 0.0f, -2.2f);

    gui_push_vert(gui, q.x1 + rx, q.y0, z, 0.0f, 0.0f, -2.0f);
    gui_push_vert(gui, q.x1, q.y1, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1 + rx, q.y1, z, 0.0f, 0.0f, -2.0f);

    // Bottom side
    gui_push_vert(gui, q.x0, q.y0, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y0, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y0 - ry, z, 0.0f, 0.0f, -2.0f);

    gui_push_vert(gui, q.x0, q.y0, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y0 - ry, z, 0.0f, 0.0f, -2.0f);
    gui_push_vert(gui, q.x0, q.y0 - ry, z, 0.0f, 0.0f, -2.0f);

    // Top side
    gui_push_vert(gui, q.x0, q.y1, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y1, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y1 + ry, z, 0.0f, 0.0f, -2.0f);

    gui_push_vert(gui, q.x0, q.y1, z, 0.0f, 0.0f, -2.2f);
    gui_push_vert(gui, q.x1, q.y1 + ry, z, 0.0f, 0.0f, -2.0f);
    gui_push_vert(gui, q.x0, q.y1 + ry, z, 0.0f, 0.0f, -2.0f);

    const float corners[4][4] = {
        {q.x0, q.y0, -1, -1},
        {q.x1, q.y0,  1, -1},
        {q.x0, q.y1, -1,  1},
        {q.x1, q.y1,  1,  1},
    };
    for (unsigned j=0; j < 4; ++j) {
        const float cx = corners[j][0];
        const float cy = corners[j][1];
        const float sx = corners[j][2];
        const float sy = corners[j][3];
        for (unsigned i=0; i < 16; ++i) {
            const float ra = i / 16.0f * M_PI / 2.0f;
            const float rb = (i + 1) / 16.0f * M_PI / 2.0f;
                gui_push_vert(gui, cx, cy, z, 0, 0, -2.2);
                gui_push_vert(gui, cx + sx * cos(ra) * rx,
                              cy + sy * sin(ra) * ry,
                              z, 0, 0, -2);
                gui_push_vert(gui, cx + sx * cos(rb) * rx,
                              cy + sy * sin(rb) * ry,
                              z, 0, 0, -2);
        }
    }
}

void gui_print(gui_t* gui, const char* s,
               float aspect_ratio, float y_pos,
               int pad_to)
{
    float x = 0.0f;
    float y = 0.0f;
    float shade = 1.0f;

    // Record the starting index
    unsigned j = gui->buf_index;
    float underline_x = -5.0f;
    float underline_y = -5.0f;
    int underlined = -1;
    while (*s) {
        if (*s == 1) {
            shade = 0.5f;
        } else if (*s == 2) {
            shade = 1.0f;
        } else if (*s == 3) {
            // Begin underline
            underline_x = x;
            underline_y = y;
            underlined = 0;
        } else if (*s == 4) {
            // Add cursor line
            stbtt_aligned_quad q;
            q.x0 = x + FONT_SIZE_PX * 0.05f;
            q.x1 = x + FONT_SIZE_PX * 0.1f;
            q.y0 = underline_y + FONT_SIZE_PX * 0.1f;
            q.y1 = underline_y - FONT_SIZE_PX * 0.7f;
            gui_push_quad(gui, q, 0.5f, -2.5f);

            // End underline, pad based on number of characters
            while (underlined++ <= pad_to) {
                stbtt_aligned_quad q;
                stbtt_GetPackedQuad(
                        gui->chars, FONT_IMAGE_SIZE, FONT_IMAGE_SIZE,
                        0, &x, &y, &q, 0);
                gui_push_quad(gui, q, 0.5f, shade);
            }
        } else {
            stbtt_aligned_quad q;
            stbtt_GetPackedQuad(
                    gui->chars, FONT_IMAGE_SIZE, FONT_IMAGE_SIZE,
                    *s - ' ', &x, &y, &q, 0);
            gui_push_quad(gui, q, 0.5f, shade);
            if (underlined >= 0) {
                underlined++;
            }
        }
        ++s;
    }

    if (underline_x != -5.0f) {
        stbtt_aligned_quad q;
        q.x0 = underline_x;
        q.x1 = x;
        q.y0 = underline_y + FONT_SIZE_PX * 0.3;
        q.y1 = underline_y + FONT_SIZE_PX * 0.25;
        gui_push_quad(gui, q, 0.5f, -3.0f);
    }

    x /= 2.0f;  // Prepare to center the text
    const float scale = 0.1f / FONT_SIZE_PX;
    for (/*j is initial index*/; j < gui->buf_index; j += 6) {
        gui->buf[j] = (gui->buf[j] - x) * scale / aspect_ratio;
        gui->buf[j + 1] = -gui->buf[j + 1] * scale + y_pos;
    }
}

void gui_draw(gui_t* gui) {
    glUseProgram(gui->shader.prog);
    glBindVertexArray(gui->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gui->vbo);
    glBufferData(GL_ARRAY_BUFFER, gui->buf_index * sizeof(float),
                 gui->buf, GL_DYNAMIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gui->tex);
    glUniform1i(gui->u_tex, 0);
    log_gl_error();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, gui->buf_index / 6);
    glDisable(GL_BLEND);
}
