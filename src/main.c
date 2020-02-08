#include "instance.h"
#include "log.h"
#include "platform.h"
#include "window.h"

int main(int argc, char** argv) {
    log_info("Startup!");
    if (argc > 1) {
        log_error_and_abort("Too many arguments (expected 0)");
    }

    instance_t* instance = instance_new();

    /* Platform-specific initialization */
    platform_init(argc, argv);

    while (instance) {
        if (glfwWindowShouldClose(instance->window)) {
            instance_delete(instance);
            instance = NULL;
        } else if (instance_draw(instance)) {
            glfwPostEmptyEvent();
        }
        glfwWaitEvents();
    }

    return 0;
}
