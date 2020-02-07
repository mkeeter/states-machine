#include "app.h"
#include "backdrop.h"
#include "camera.h"
#include "instance.h"
#include "indirect.h"
#include "log.h"
#include "map.h"
#include "mat.h"
#include "object.h"
#include "platform.h"
#include "theme.h"
#include "window.h"

instance_t* instance_new(app_t* parent) {
    const float width = 500;
    const float height = 500;
    GLFWwindow* window = window_new("States Machine", width, height);

    glfwShowWindow(window);
    log_trace("Showed window");

    OBJECT_ALLOC(instance);
    instance->parent = parent;

    /*  Next, build the OpenGL-dependent objects */
    instance->backdrop = backdrop_new();
    instance->camera = camera_new(width, height);

    instance->indirect = indirect_new(width, height);

    instance->map = map_new(instance->camera);

    /*  This needs to happen after setting up the instance, because
     *  on Windows, the window size callback is invoked when we add
     *  the menu, which requires the camera to be populated. */
    window_bind(window, instance);

    return instance;
}

void instance_delete(instance_t* instance) {
    OBJECT_DELETE_MEMBER(instance, backdrop);
    OBJECT_DELETE_MEMBER(instance, camera);
    OBJECT_DELETE_MEMBER(instance, window);
    OBJECT_DELETE_MEMBER(instance, indirect);
    OBJECT_DELETE_MEMBER(instance, map);
    free(instance);
}

/******************************************************************************/

void instance_cb_window_size(instance_t* instance, int width, int height)
{
    /*  Update camera size (and recalculate projection matrix) */
    camera_set_size(instance->camera, width, height);

    /*  Resize buffers for indirect rendering */
    indirect_resize(instance->indirect, width, height);

#ifdef PLATFORM_DARWIN
    /*  Continue to render while the window is being resized
     *  (otherwise it ends up greyed out on Mac)  */
    instance_draw(instance, instance->parent->theme);
#endif

#ifdef PLATFORM_WIN32
    /*  Otherwise, it misbehaves in a high-DPI environment */
    glfwMakeContextCurrent(instance->window);
    glViewport(0, 0, width, height);
#endif
}

void instance_cb_mouse_pos(instance_t* instance, float xpos, float ypos) {
    camera_set_mouse_pos(instance->camera, xpos, ypos);
}

void instance_cb_mouse_click(instance_t* instance, int button,
                             int action, int mods)
{
    (void)mods;
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_1) {
            camera_begin_pan(instance->camera);
        } else if (button == GLFW_MOUSE_BUTTON_2) {
            camera_begin_rot(instance->camera);
        }
    } else {
        camera_end_drag(instance->camera);
    }
}

void instance_cb_mouse_scroll(instance_t* instance,
                              float xoffset, float yoffset)
{
    (void)xoffset;
    camera_zoom(instance->camera, yoffset);
}

void instance_cb_focus(instance_t* instance, bool focus)
{
    instance->focused = focus;
    if (focus) {
        app_set_front(instance->parent, instance);
    }
}

bool instance_draw(instance_t* instance, theme_t* theme) {
    const bool needs_redraw = camera_check_anim(instance->camera);

    glfwMakeContextCurrent(instance->window);
    log_trace("%p", (void*)glfwGetCurrentContext());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    map_draw(instance->map, instance->camera);
    log_trace("drawing");

    glfwSwapBuffers(instance->window);
    return needs_redraw;
}
