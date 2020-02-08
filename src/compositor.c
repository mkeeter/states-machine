#include "compositor.h"
#include "log.h"
#include "object.h"
#include "shader.h"
#include "theme.h"

static const GLchar* COMPOSITOR_VS_SRC = GLSL(330,
layout(location=0) in vec2 pos;

uniform vec3 corners[4];

out vec4 grad_color;

void main() {
    if (pos.x < 0.0f && pos.y < 0.0f) {
        grad_color = vec4(corners[0], 1.0f);
    } else if (pos.x > 0.0f && pos.y < 0.0f) {
        grad_color = vec4(corners[1], 1.0f);
    } else if (pos.x < 0.0f && pos.y > 0.0f) {
        grad_color = vec4(corners[2], 1.0f);
    } else if (pos.x > 0.0f && pos.y > 0.0f) {
        grad_color = vec4(corners[3], 1.0f);
    }

    gl_Position = vec4(pos, 1.0f, 1.0f);
}
);

static const GLchar* COMPOSITOR_FS_SRC = GLSL(330,
in vec4 grad_color;
out vec4 out_color;

uniform sampler2D tex;

void main() {
    int ix = int(gl_FragCoord.x);
    int iy = int(gl_FragCoord.y);
    float t = texelFetch(tex, ivec2(ix, iy), 0).r;

    bool edge = false;
    for (int x = ix - 1; x <= ix; ++x) {
        for (int y = iy - 1; y <= iy; ++y) {
            float u = texelFetch(tex, ivec2(x, y), 0).r;
            edge = edge || (u != t);
        }
    }
    if (edge) {
        out_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    } else if (t > 0.0f) {
        float r = (fract(sin(t * 43758.54123)) - 0.5f) / 5.0f + 1.0f;
        out_color = vec4(0.5f * r, 0.5f * r, 0.5f * r, 1.0f);
    } else {
        out_color = grad_color;
    }
}
);

////////////////////////////////////////////////////////////////////////////////

struct compositor_ {
    GLuint vao;
    GLuint vbo;

    shader_t shader;
    GLint u_corners;
    GLint u_tex;

    GLuint fbo;
    GLuint tex;
    GLuint rbo;
};

////////////////////////////////////////////////////////////////////////////////

void compositor_resize(compositor_t* compositor,
                       uint32_t width, uint32_t height)
{
    log_trace("Resizing to %u x %u", width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);

    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0,
                 GL_RED, GL_FLOAT, NULL);
    log_gl_error();

    glBindRenderbuffer(GL_RENDERBUFFER, compositor->rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
}

compositor_t* compositor_new(uint32_t width, uint32_t height) {
    OBJECT_ALLOC(compositor);

    glGenFramebuffers(1, &compositor->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);

    glGenTextures(1, &compositor->tex);
    glGenRenderbuffers(1, &compositor->rbo);

    compositor_resize(compositor, width, height);

    /* Bind the color output buffers, which is a texture */
    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, compositor->tex, 0);

    /* Bind the depth buffer, which is write-only (so we use a renderbuffer) */
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, compositor->rbo);

    /* Configure the framebuffer to draw to two color buffers */
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
        struct { GLint corners; GLint tex; } u;
        SHADER_GET_UNIFORM(corners);
        SHADER_GET_UNIFORM(tex);
        compositor->u_corners = u.corners;
        compositor->u_tex = u.tex;
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
    glDeleteRenderbuffers(1, &compositor->rbo);

    glDeleteBuffers(1, &compositor->vbo);
    glDeleteVertexArrays(1, &compositor->vao);
    shader_deinit(compositor->shader);

    free(compositor);
}

void compositor_bind(compositor_t* compositor) {
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);
}

void compositor_draw(compositor_t* compositor, theme_t* theme) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(compositor->shader.prog);
    glUniform3fv(compositor->u_corners, 4, (const float*)&theme->corners);
    glBindVertexArray(compositor->vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glUniform1i(compositor->u_tex, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

float compositor_pixel_at(compositor_t* compositor, int x, int y) {
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);
    float out;
    glReadPixels(x, y, 1, 1, GL_RED, GL_FLOAT, &out);
    return out;
}
