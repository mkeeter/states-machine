#include "base.h"

typedef struct console_ console_t;

console_t* console_new(void);
void console_draw(console_t* console, const char* s,
                  float aspect_ratio, float y_pos);
