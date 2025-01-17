#include "camera.h"
#include "compositor.h"
#include "data.h"
#include "gui.h"
#include "instance.h"
#include "log.h"
#include "map.h"
#include "mat.h"
#include "object.h"
#include "platform.h"
#include "sm2.h"
#include "window.h"

instance_t* instance_new(void) {
    const float width = 500;
    const float height = 500;
    GLFWwindow* window = window_new("States Machine", width, height);

    glfwShowWindow(window);
    log_trace("Showed window");

    OBJECT_ALLOC(instance);

    /*  Next, build the OpenGL-dependent objects */
    instance->camera = camera_new(width, height);
    instance->compositor = compositor_new(width, height);
    instance->map = map_new(instance->camera);
    instance->gui = gui_new();

    /*  Find the longest state name and store it as input_size */
    memset(instance->input, 0, sizeof(instance->input));
    instance->input_index = 0;
    instance->input_size = 0;
    for (unsigned i=0; i < STATES_COUNT; ++i) {
        const size_t len = strlen(STATES_NAMES[i]);
        if (len > instance->input_size) {
            instance->input_size = len;
        }
    }

    /*  Load the next item to learn */
    instance->sm2 = sm2_new();
    instance_next(instance);

    /*  This needs to happen after setting up the instance, because
     *  on Windows, the window size callback is invoked when we add
     *  the menu, which requires the camera to be populated. */
    window_bind(window, instance);

    /*  After the window shown, check to see if the framebuffer is a different
     *  size from the given width / height (which is the case in high-DPI
     *  environments).  If that's the case, regenerate textures. */
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    if (w != width || h != height) {
        compositor_resize(instance->compositor, w, h);
        camera_set_fb_size(instance->camera, w, h);
    }

    return instance;
}

void instance_delete(instance_t* instance) {
    OBJECT_DELETE_MEMBER(instance, camera);
    OBJECT_DELETE_MEMBER(instance, compositor);
    OBJECT_DELETE_MEMBER(instance, gui);
    OBJECT_DELETE_MEMBER(instance, map);
    OBJECT_DELETE_MEMBER(instance, window);
    sm2_item_delete(instance->active);
    free(instance);
}

void instance_next(instance_t* instance) {
    if (instance->active) {
        sm2_item_delete(instance->active);
        instance->active = NULL;
    }
    instance->active = sm2_next(instance->sm2);
    instance->ui = UI_QUESTION;

    // Reset the text buffer
    memset(instance->input, 0, sizeof(instance->input));
    instance->input_index = 0;
    instance->wrong_state = 0;

    if (instance->active->mode == ITEM_MODE_NAME) {
        instance->active_state = 0;
        for (unsigned i=0; i < STATES_COUNT; ++i) {
            if (!strcmp(instance->active->state, STATES_NAMES[i])) {
                instance->active_state = i + 1;
            }
        }
        if (!instance->active_state) {
            log_error_and_abort("Could not find state %s",
                                instance->active->state);
        }
    } else if (instance->active->mode == ITEM_MODE_POSITION) {
        instance_update_active_state(instance);
    } else if (instance->active->mode == ITEM_MODE_DONE) {
        instance->active_state = 0;
    }
}

/******************************************************************************/

void instance_cb_window_size(instance_t* instance, int width, int height)
{
    /*  Update camera size (and recalculate projection matrix) */
    camera_set_size(instance->camera, width, height);

#ifdef PLATFORM_DARWIN
    /*  Continue to render while the window is being resized
     *  (otherwise it ends up greyed out on Mac)  */
    instance_draw(instance);
#endif
}

void instance_cb_framebuffer_size(instance_t* instance, int width, int height)
{
    /*  Resize buffers for indirect rendering */
    compositor_resize(instance->compositor, width, height);

    /*  Record size in camera for mouse conversion */
    camera_set_fb_size(instance->camera, width, height);
}

void instance_update_active_state(instance_t* instance) {
    int s = compositor_state_at(
            instance->compositor, instance->mouse_x, instance->mouse_y);
    if (s != instance->active_state) {
        instance->active_state = s;
        glfwPostEmptyEvent();
    }
}

void instance_cb_mouse_pos(instance_t* instance, float xpos, float ypos) {
    camera_get_fb_pixel(instance->camera, xpos, ypos,
                        &instance->mouse_x, &instance->mouse_y);
    if (instance->active->mode == ITEM_MODE_POSITION &&
        instance->ui == UI_QUESTION)
    {
        instance_update_active_state(instance);
    }
    camera_set_mouse_pos(instance->camera, xpos, ypos);
}

void instance_cb_mouse_click(instance_t* instance, int button,
                             int action, int mods)
{
    (void)mods;
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_1) {
            camera_begin_pan(instance->camera);
        }
    } else if (!camera_end_drag(instance->camera) &&
               instance->ui == UI_QUESTION &&
               instance->active->mode == ITEM_MODE_POSITION &&
               instance->active_state)
    {
        if (!strcmp(instance->active->state,
                    STATES_NAMES[instance->active_state - 1]))
        {
            instance->ui = UI_ANSWER_RIGHT;
        } else {
            instance->wrong_state = instance->active_state;
            for (unsigned i=0; i < STATES_COUNT; ++i) {
                if (!strcmp(instance->active->state, STATES_NAMES[i])) {
                    instance->active_state = i + 1;
                    break;
                }
            }
            instance->ui = UI_ANSWER_WRONG;
        }
    }
}

void instance_cb_mouse_scroll(instance_t* instance,
                              float xoffset, float yoffset)
{
    (void)xoffset;
    camera_zoom(instance->camera, yoffset);
}

void instance_cb_key(instance_t* instance, int key, int scancode,
                     int action, int mods)
{
    if (instance->active->mode == ITEM_MODE_NAME) {
        if (key == GLFW_KEY_BACKSPACE &&
            (action == GLFW_PRESS || action == GLFW_REPEAT) &&
            instance->input_index)
        {
            instance->input[--instance->input_index] = 0;
        }
        else if (key == GLFW_KEY_ENTER && action == GLFW_RELEASE)
        {
            if (!strncmp(instance->input, instance->active->state,
                         instance->input_index - 1))
            {
                instance->ui = UI_ANSWER_RIGHT;
            } else {
                instance->wrong_state = 0;
                for (unsigned i=0; i < STATES_COUNT; ++i) {
                    if (!strncmp(instance->input, STATES_NAMES[i],
                                 instance->input_index - 1)) {
                        instance->wrong_state = i + 1;
                    }
                }
                instance->ui = UI_ANSWER_WRONG;
            }
        }
    }
}

void instance_cb_char(instance_t* instance, unsigned codepoint)
{
    if (instance->active->mode == ITEM_MODE_NAME &&
        instance->ui == UI_QUESTION &&
        instance->input_index <= instance->input_size &&
        codepoint >= ' ' && codepoint < '~')
    {
        instance->input[instance->input_index++] = codepoint;
    } else {
        int q = -1;
        if (instance->ui == UI_ANSWER_RIGHT &&
            codepoint >= '4' && codepoint <= '6')
        {
            q = codepoint - '1';
        } else if (instance->ui == UI_ANSWER_WRONG &&
                   codepoint >= '1' && codepoint <= '3')
        {
            q = codepoint - '1';
        }

        if (q >= 0) {
            sm2_update(instance->sm2, instance->active, q);
            instance_next(instance);
        }
    }
}

bool instance_draw(instance_t* instance) {
    const bool needs_redraw = camera_check_anim(instance->camera);

    glfwMakeContextCurrent(instance->window);

#ifdef PLATFORM_WIN32
    /*  This works around an issue in the Windows build where it ends
     *  up with a half-size viewport.  Not sure whether this is my code,
     *  GLFW, or running in Wine, but this check fixes it. */
    camera_check_viewport(instance->camera);
#endif

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);

    // Draw to the compositor texture
    compositor_bind(instance->compositor);
    glClear(GL_COLOR_BUFFER_BIT);
    map_draw(instance->map, instance->camera);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    compositor_draw(instance->compositor, instance->active_state,
                    instance->wrong_state);

    char buf[64];
    switch (instance->active->mode) {
        case ITEM_MODE_NONE:    break;
        case ITEM_MODE_DONE:
            snprintf(buf, sizeof(buf), "Nothing to review!");
            break;
        case ITEM_MODE_NAME: {
            switch (instance->ui) {
                case UI_QUESTION:
                    snprintf(buf, sizeof(buf), "\x01This is \x02\x03%s\x04",
                             instance->input);
                    break;
                case UI_ANSWER_RIGHT:
                    snprintf(buf, sizeof(buf), "Correct!");
                    break;
                case UI_ANSWER_WRONG:
                    snprintf(buf, sizeof(buf), "\x01No, it is \x02%s",
                             instance->active->state);
                    break;
            }
            break;
        }
        case ITEM_MODE_POSITION: {
            switch (instance->ui) {
                case UI_QUESTION:
                    snprintf(buf, sizeof(buf), "\x01Where is \x02%s?",
                             instance->active->state);
                    break;
                case UI_ANSWER_RIGHT:
                    snprintf(buf, sizeof(buf), "Correct!");
                    break;
                case UI_ANSWER_WRONG:
                    snprintf(buf, sizeof(buf), "\x01No, that is \x02%s",
                             STATES_NAMES[instance->wrong_state - 1]);
                    break;
            }
            break;
        }
    }
    gui_reset(instance->gui);
    const float aspect_ratio = camera_aspect_ratio(instance->camera);
    gui_backdrop(instance->gui, aspect_ratio);
    if (instance->ui == UI_QUESTION) {
        gui_print(instance->gui, buf, aspect_ratio, 0.7f, instance->input_size);
    } else {
        gui_print(instance->gui, buf,
                  camera_aspect_ratio(instance->camera), 0.77f, 0);
        if (instance->ui == UI_ANSWER_RIGHT) {
            gui_print(instance->gui,
                      "\x06Hard  \x05""1  \x05""2  \x05"
                      "3\x02  \x05""4  \x05""5  \x05""6  Easy",
                      aspect_ratio, 0.63f, 0);
        } else {
            gui_print(instance->gui,
                      "\x02Hard  \x05""1  \x05""2  \x05"
                      "3\x06  \x05""4  \x05""5  \x05""6  Easy",
                      aspect_ratio, 0.63f, 0);
        }
    }
    gui_draw(instance->gui);

    glfwSwapBuffers(instance->window);
    return needs_redraw;
}
