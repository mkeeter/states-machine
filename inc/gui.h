#include "base.h"

typedef struct gui_ gui_t;

gui_t* gui_new(void);
void gui_delete(gui_t* gui);

void gui_reset(gui_t* gui);
void gui_backdrop(gui_t* gui);
void gui_print(gui_t* gui, const char* s,
               float aspect_ratio, float y_pos);
void gui_draw(gui_t* gui);

