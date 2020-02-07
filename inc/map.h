#include "base.h"

// Forward declarations
struct camera_;

typedef struct map_ map_t;

/*  Constructs a new map from data in data.c, updating the
 *  camera's model matrix to center the map at 0. */
map_t* map_new(struct camera_* camera);
void map_delete(map_t* map);

void map_draw(map_t* map, struct camera_* camera);
