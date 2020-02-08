#include "compositor.h"
#include "log.h"
#include "object.h"
#include "shader.h"

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

vec4 shade_state(int state, int ix, int iy) {
    for (int x = ix - 1; x <= ix; ++x) {
        for (int y = iy - 1; y <= iy; ++y) {
            float u = texelFetch(tex, ivec2(x, y), 0).r;
            if (u != state) {
                // State boundaries are black
                return vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        }
    }

    float r = (fract(sin(state * 43758.54123)) - 0.5f) / 5.0f + 1.0f;
    return vec4(0.5f * r, 0.5f * r, 0.5f * r, 1.0f);
}

vec4 shade_backdrop(vec4 base, int ix, int iy) {
    int d = 10;
    float r_min = 100.0f;
    for (int x = ix - d; x <= ix + d; ++x) {
        for (int y = iy - d; y <= iy + d; ++y) {
            float u = texelFetch(tex, ivec2(x, y), 0).r;
            if (u > 0.0f) {
                float r = sqrt((x - ix)*(x - ix) + (y - iy)*(y - iy));
                r_min = min(r, r_min);
            }
        }
    }
    float r_shadow = 1;
    return base * (1.0 - exp(-r_min / (2*r_shadow*r_shadow)));
}

void main() {
    int ix = int(gl_FragCoord.x);
    int iy = int(gl_FragCoord.y);
    float t = texelFetch(tex, ivec2(ix, iy), 0).r;

    if (t > 0.0f) {
        out_color = shade_state(int(t), ix, iy);
    } else {
        out_color = shade_backdrop(vec4(1.0f, 1.0f, 1.0f, 1.0f), ix, iy);
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

static void from_hex(const char* hex, float f[3]) {
    if (strlen(hex) != 6) {
        log_error_and_abort("Invalid hex string '%s'", hex);
    }
    uint8_t c[3] = {0};
    for (unsigned i=0; i < 6; ++i) {
        unsigned v = 0;
        if (hex[i] >= 'a' && hex[i] <= 'f') {
            v = hex[i] - 'a' + 10;
        } else if (hex[i] >= 'A' && hex[i] <= 'F') {
            v = hex[i] - 'A' + 10;
        } else if (hex[i] >= '0' && hex[i] <= '9') {
            v = hex[i] - '0';
        } else {
            log_error_and_abort("Invalid hex character '%c'", hex[i]);
        }
        c[i / 2] = (c[i / 2] << 4) + v;
    }
    for (unsigned i=0; i < 3; ++i) {
        f[i] = c[i] / 255.0f;
    }
}

void compositor_draw(compositor_t* compositor) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(compositor->shader.prog);

    float corners[4][3];
    from_hex("002833", corners[0]);
    from_hex("002833", corners[1]);
    from_hex("003440", corners[2]);
    from_hex("002833", corners[3]);

    glUniform3fv(compositor->u_corners, 4, (const float*)corners);
    glBindVertexArray(compositor->vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, compositor->tex);
    glUniform1i(compositor->u_tex, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int compositor_state_at(compositor_t* compositor, int x, int y) {
    glBindFramebuffer(GL_FRAMEBUFFER, compositor->fbo);
    float out;
    glReadPixels(x, y, 1, 1, GL_RED, GL_FLOAT, &out);
    return (int)out - 1;
}
