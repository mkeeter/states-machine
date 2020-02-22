#include "compositor.h"
#include "log.h"
#include "object.h"
#include "shader.h"

static const GLchar* COMPOSITOR_VS_SRC = GLSL(330,
layout(location=0) in vec2 pos;

void main() {
    gl_Position = vec4(pos, 1.0f, 1.0f);
}
);

static const GLchar* COMPOSITOR_FS_SRC = GLSL(330,
out vec4 out_color;

uniform isampler2D tex;
uniform int active_state;
uniform int wrong_state;

vec3 color_hex(int c) {
    return vec3(((c >> 16) & 255) / 255.0f,
                ((c >>  8) & 255) / 255.0f,
                ((c >>  0) & 255) / 255.0f);
}

bool is_edge(float state, int ix, int iy) {
    for (int x = ix - 1; x <= ix; ++x) {
        for (int y = iy - 1; y <= iy; ++y) {
            if (x != ix || y != iy) {
                int u = texelFetch(tex, ivec2(x, y), 0).r;
                if (u != state) {
                    return true;
                }
            }
        }
    }
    return false;
}

void main() {
    int ix = int(gl_FragCoord.x + 0.5f);
    int iy = int(gl_FragCoord.y + 0.5f);
    int t = texelFetch(tex, ivec2(ix, iy), 0).r;

    bool e = is_edge(t, ix, iy);
    if (e) {
        /* State-state edge */
        out_color = vec4(color_hex(0x2E7D32), 1.0f);
    } else if (t > 0.0f) {
        if (t == active_state) {
            /* Active state (brighter) */
            out_color = vec4(color_hex(0xFFB74D), 1.0f);
        } else if (t == wrong_state) {
            /* Incorrect state (red) */
            out_color = vec4(color_hex(0xE57373), 1.0f);
        } else {
            /*  Normal state */
            out_color = vec4(color_hex(0x9CCC65), 1.0f);
        }
    } else {
        /* Ocean */
        out_color = vec4(color_hex(0x303F9F), 1.0f);
    }
}
);

////////////////////////////////////////////////////////////////////////////////

struct compositor_ {
    GLuint vao;
    GLuint vbo;

    shader_t shader;
    GLint u_tex;
    GLint u_active_state;
    GLint u_wrong_state;

    GLuint fbo;
    GLuint tex;
};

////////////////////////////////////////////////////////////////////////////////

void compositor_resize(compositor_t* compositor,
                       uint32_t width, uint32_t height)
{
    log_trace("Resizing to %u x %u", width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);

    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, width, height, 0,
                 GL_RED_INTEGER, GL_INT, NULL);
    log_gl_error();
}

compositor_t* compositor_new(uint32_t width, uint32_t height) {
    OBJECT_ALLOC(compositor);

    glGenFramebuffers(1, &compositor->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);

    glGenTextures(1, &compositor->tex);

    compositor_resize(compositor, width, height);

    /* Bind the color output buffer, which is a texture */
    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, compositor->tex, 0);

    /* Configure the framebuffer to draw to one color buffer */
    GLuint attachments[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachments);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        const char* err = NULL;
        switch (status) {
#define LOG_FB_ERR_CASE(s) case s: err = #s; break
            LOG_FB_ERR_CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
            LOG_FB_ERR_CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
            LOG_FB_ERR_CASE(GL_FRAMEBUFFER_UNSUPPORTED);
            default: break;
#undef LOG_FB_ERR_CASE
        }
        if (err) {
            log_error_and_abort("Framebuffer error: %s (0x%x)", err, status);
        } else {
            log_error_and_abort("Unknown framebuffer error: %u", status);
        }
    }

    /* Build the compositor's shader program */
    compositor->shader = shader_new(COMPOSITOR_VS_SRC, NULL, COMPOSITOR_FS_SRC);
    glUseProgram(compositor->shader.prog);

    {   /* Make a temporary struct to unpack local uniforms */
        GLint prog = compositor->shader.prog;
        struct { GLint tex, active_state, wrong_state; } u;
        SHADER_GET_UNIFORM(tex);
        SHADER_GET_UNIFORM(active_state);
        SHADER_GET_UNIFORM(wrong_state);
        compositor->u_tex = u.tex;
        compositor->u_active_state = u.active_state;
        compositor->u_wrong_state = u.wrong_state;
    }

    /* Build a single quad to draw the full-screen texture */
    const float corners[] = {-1.0f, -1.0f,
                             -1.0f,  1.0f,
                              1.0f, -1.0f,
                              1.0f,  1.0f};
    glGenBuffers(1, &compositor->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, compositor->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

    glGenVertexArrays(1, &compositor->vao);
    glBindVertexArray(compositor->vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    return compositor;
}

void compositor_delete(compositor_t* compositor) {
    glDeleteFramebuffers(1, &compositor->fbo);
    glDeleteTextures(1, &compositor->tex);

    glDeleteBuffers(1, &compositor->vbo);
    glDeleteVertexArrays(1, &compositor->vao);
    shader_deinit(compositor->shader);

    free(compositor);
}

void compositor_bind(compositor_t* compositor) {
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);
}

void compositor_draw(compositor_t* compositor, int active_state,
                     int wrong_state) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(compositor->shader.prog);

    glBindVertexArray(compositor->vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glUniform1i(compositor->u_tex, 0);
    glUniform1i(compositor->u_active_state, active_state);
    glUniform1i(compositor->u_wrong_state, wrong_state);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int compositor_state_at(compositor_t* compositor, int x, int y) {
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);
    int out;
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &out);
    return out;
}
