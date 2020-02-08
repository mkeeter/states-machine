#include "camera.h"
#include "log.h"
#include "map.h"
#include "mat.h"
#include "object.h"
#include "shader.h"

////////////////////////////////////////////////////////////////////////////////
// Generated data in data.c
extern const unsigned STATES_VERT_COUNT;
extern const unsigned STATES_TRI_COUNT;
extern const float STATES_VERTS[];
extern const uint16_t STATES_INDEXES[];

////////////////////////////////////////////////////////////////////////////////

static const GLchar* MAP_VS_SRC = GLSL(330,
layout(location=0) in vec3 pos;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;

out float state_color;

void main() {
    vec4 p = vec4(pos.xy, 0.0f, 1.0f);
    gl_Position = proj * view * model * p;
    state_color = pos.z;
}
);

static const GLchar* MAP_FS_SRC = GLSL(330,
in float state_color;
layout(location=0) out vec4 out_color;

void main() {
    int i = int(state_color) * (1 << 24) / 50;
    out_color = vec4((i % 255) / 255.0f,
                 ((i / 255) % 255) / 255.0f,
                 ((i / 255 / 255) % 255) / 255.0f, 1.0f);
}
);

////////////////////////////////////////////////////////////////////////////////

struct map_ {
    shader_t shader;

    GLuint vao;
    GLuint vbo;
    GLuint ibo;

    mat4_t model_mat;

    camera_uniforms_t u_camera;
};

map_t* map_new(camera_t* camera) {
    OBJECT_ALLOC(map);
    map->shader = shader_new(MAP_VS_SRC, NULL, MAP_FS_SRC);
    map->u_camera = camera_get_uniforms(map->shader.prog);

    glGenVertexArrays(1, &map->vao);
    glBindVertexArray(map->vao);

    // Find the bounding box of the map
    float xmin = STATES_VERTS[0];
    float xmax = STATES_VERTS[0];
    float ymin = STATES_VERTS[1];
    float ymax = STATES_VERTS[1];
    for (unsigned i=0; i < STATES_VERT_COUNT; i++) {
        xmin = fminf(xmin, STATES_VERTS[3*i]);
        xmax = fmaxf(xmax, STATES_VERTS[3*i]);
        ymin = fminf(ymin, STATES_VERTS[3*i + 1]);
        ymax = fmaxf(ymax, STATES_VERTS[3*i + 1]);
    }
    float scale = fmaxf(xmax - xmin, ymax - ymin);
    float center[3] = {(xmin + xmax) / 2.0f, (ymin + ymax) / 2.0f, 0.0f};
    camera_set_model(camera, center, scale / 2);

    glGenBuffers(1, &map->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 STATES_VERT_COUNT * 3 * sizeof(float),
                 STATES_VERTS, GL_STATIC_DRAW);

    glGenBuffers(1, &map->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 STATES_TRI_COUNT * 3 * sizeof(uint16_t),
                 STATES_INDEXES, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    log_trace("finished building map");
    log_gl_error();
    return map;
}

void map_delete(map_t* map) {
    shader_deinit(map->shader);
    glDeleteVertexArrays(1, &map->vao);
    glDeleteBuffers(1, &map->vbo);
    glDeleteBuffers(1, &map->ibo);
    free(map);
}

void map_draw(map_t* map, camera_t* camera) {
    glDisable(GL_DEPTH_TEST);

    glUseProgram(map->shader.prog);
    camera_bind(camera, map->u_camera);

    glBindVertexArray(map->vao);
    glDrawElements(GL_TRIANGLES, STATES_TRI_COUNT * 3,
                   GL_UNSIGNED_SHORT, NULL);

    log_gl_error();
}
