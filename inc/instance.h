#include "base.h"

struct theme_;

typedef struct instance_ {
    struct camera_* camera;
    struct compositor_* compositor;
    struct gui_* gui;
    struct map_* map;
    struct sm2_* sm2;

    /*  Mouse position is in framebuffer pixels */
    int mouse_x;
    int mouse_y;

    /* pulled from active item in name mode, otherwise pulled
     * from mouse cursor position in callback. */
    int active_state;

    struct sm2_item_* active;

    /*  Text buffer to type in the state name */
    char buf[32];
    unsigned buf_index;

    /*  Current state of the UI */
    enum { UI_QUESTION, UI_ANSWER_RIGHT, UI_ANSWER_WRONG } ui;

    /*  If we've selected the wrong state, then store the index here.
     *  Otherwise, this is zero. */
    int wrong_state;

    GLFWwindow* window;
} instance_t;

instance_t* instance_new(void);
void instance_delete(instance_t* instance);

void instance_update_active_state(instance_t* instance);
void instance_next(instance_t* instance);

/*  Draws an instance
 *
 *  Returns true if the main loop should schedule a redraw immediately
 *  (e.g. because there's an animation running in this instance). */
bool instance_draw(instance_t* instance);

void instance_view_shaded(instance_t* instance);
void instance_view_wireframe(instance_t* instance);
void instance_view_orthographic(instance_t* instance);
void instance_view_perspective(instance_t* instance);

/*  Callbacks */
void instance_cb_window_size(instance_t* instance, int width, int height);
void instance_cb_framebuffer_size(instance_t* instance, int width, int height);
void instance_cb_mouse_pos(instance_t* instance, float xpos, float ypos);
void instance_cb_mouse_click(instance_t* instance, int button, int action, int mods);
void instance_cb_mouse_scroll(instance_t* instance, float xoffset, float yoffset);
void instance_cb_char(instance_t* instance, unsigned codepoint);
void instance_cb_key(instance_t* instance, int key, int scancode, int action, int mods);
