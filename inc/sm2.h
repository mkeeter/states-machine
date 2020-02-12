#include "base.h"

struct sm2_item_ {
    const char* state;
    enum { ITEM_MODE_NONE,
           ITEM_MODE_POSITION,
           ITEM_MODE_NAME,
           ITEM_MODE_DONE } mode;

    // Internal data for the SuperMemo algorithm
    double ef;
    int reps;
};
typedef struct sm2_item_ sm2_item_t;

typedef struct sm2_ sm2_t;
sm2_t* sm2_new(void);
void sm2_delete(sm2_t* sm2);

/*  Returns the next item to test, or an item with mode = DONE */
sm2_item_t* sm2_next(sm2_t* sm2);
void sm2_item_delete(sm2_item_t* item);

/*  Updates the given item with the quality score */
void sm2_update(sm2_t* sm2, sm2_item_t* item, int q);
