#include "base.h"

typedef struct compositor_ compositor_t;

void compositor_delete(compositor_t* compositor);
compositor_t* compositor_new(uint32_t width, uint32_t height);
void compositor_resize(compositor_t* compositor,
                       uint32_t width, uint32_t height);

/* Binds the compositor framebuffer, so draw calls go to the texture */
void compositor_bind(compositor_t* compositor);

/* Render the texture with edge detection and other fancy things */
void compositor_draw(compositor_t* compositor, int active_state,
                     int wrong_state);

/*  Returns the state at the given window pixel */
int compositor_state_at(compositor_t* compositor, int x, int y);
