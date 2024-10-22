#include "base.h"

/*  Forward declaration of camera struct */
typedef struct camera_ camera_t;

typedef struct camera_uniforms_ {
    GLint proj;
    GLint view;
    GLint model;
} camera_uniforms_t;

/*  Constructs a new heap-allocated camera */
camera_t* camera_new(float width, float height);
void camera_delete(camera_t* camera);

/*  Sets the camera width and height and updates proj matrix */
void camera_set_size(camera_t* camera, float width, float height);

/*  Sets the framebuffer size, which isn't used for matrices but can be
 *  useful when dealing with high-DPI displays. */
void camera_set_fb_size(camera_t* camera, float width, float height);

/*  Sets the camera's model matrix.
 *  center must be an array of 3 floats */
void camera_set_model(camera_t* camera, float* center, float scale);

/*  Mouse handling functions */
void camera_begin_pan(camera_t* camera);

/*  Returns true if the camera actually dragged anywhere */
bool camera_end_drag(camera_t* camera);
void camera_zoom(camera_t* camera, float amount);

/*  Assigns the current mouse position, panning if the button is down */
void camera_set_mouse_pos(camera_t* camera, float x, float y);

/*  Schedules a camera animation to update projection */
void camera_anim_proj_perspective(camera_t* camera);
void camera_anim_proj_orthographic(camera_t* camera);

/*  Checks whether there is an animation running on the camera.
 *  If so, the values are updated.  If the animation is incomplete,
 *  returns true (so the caller should schedule a redraw). */
bool camera_check_anim(camera_t* camera);

/*  Returns the width/height aspect ratio */
float camera_aspect_ratio(camera_t* camera);

/*  Looks up uniforms for camera binding */
camera_uniforms_t camera_get_uniforms(GLuint prog);

/*  Binds the camera matrices to the given uniforms */
void camera_bind(camera_t* camera, camera_uniforms_t u);

/*  Translates from window to framebuffer pixel locations */
void camera_get_fb_pixel(camera_t* camera, int x, int y, int* fx, int* fy);

/*  If the OpenGL viewport doesn't match the camera,
 *  calls glViewport with the camera's width and height */
void camera_check_viewport(camera_t* camera);
