#include "base.h"

typedef struct sm2_item_ sm2_item_t;
struct sm2_item_ {
    const char* state;
    enum { POSITION, NAME } mode;
};

typedef struct sm2_ sm2_t;
sm2_t* sm2_new(void);
void sm2_delete(sm2_t* sm2);
