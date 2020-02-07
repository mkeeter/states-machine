#include "base.h"

struct instance_;
struct theme_;

typedef struct app_ {
    struct instance_** instances;
    unsigned instance_count;
    unsigned instances_size;

    struct theme_* theme;
} app_t;

/*  Calls instance_run on every instance */
bool app_run(app_t* app);

/*  Triggered on startup */
struct instance_* app_start(app_t* app);

/*  Moves a specific instance to the front of the list,
 *  which is used to determine the new focused window
 *  when the focused window is closed. */
void app_set_front(app_t* app, struct instance_* instance);

/*  Returns the instance that's currently focused */
struct instance_* app_get_front(app_t* app);
