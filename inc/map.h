#include "base.h"

typedef struct map_ map_t;

map_t* map_new();
void map_delete(map_t* map);
void map_draw(map_t* map);
