#include "map.h"
#include "shader.h"
#include "log.h"
#include "object.h"

////////////////////////////////////////////////////////////////////////////////
// Generated data in data.c
extern const unsigned STATES_VERT_COUNT;
extern const unsigned STATES_TRI_COUNT;
extern const float STATES_VERTS[];
extern const uint16_t STATES_INDEXES[];

////////////////////////////////////////////////////////////////////////////////

static const GLchar* MAP_VS_SRC = GLSL(330,
layout(location=0) in vec3 pos;

void main() {
    gl_Position = vec4(pos.xy, 0.0f, 1.0f);
}
);

static const GLchar* MAP_FS_SRC = GLSL(330,
out vec4 out_color;

void main() {
    out_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}
);

////////////////////////////////////////////////////////////////////////////////

struct map_ {
    shader_t shader;

    GLuint vao;
    GLuint vbo;
    GLuint ibo;
};

map_t* map_new() {
    OBJECT_ALLOC(map);
    map->shader = shader_new(MAP_VS_SRC, NULL, MAP_FS_SRC);

    glGenVertexArrays(1, &map->vao);
    glBindVertexArray(map->vao);

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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

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

void map_draw(map_t* map) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(map->shader.prog);
    glBindVertexArray(map->vao);
    glDrawElements(GL_TRIANGLES, STATES_TRI_COUNT * 3, GL_UNSIGNED_SHORT, NULL);
    log_gl_error();
}
