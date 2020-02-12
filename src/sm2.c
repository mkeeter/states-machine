#include "data.h"
#include "log.h"
#include "object.h"
#include "sm2.h"

#define SQLITE_CHECKED(cond) do {       \
    if ((cond) != SQLITE_OK) {          \
        log_sqlite_error_and_abort();   \
    }                                   \
} while(0)

struct sm2_ {
    sqlite3* db;

    /* Select a new random item to train on */
    sqlite3_stmt* selector;

    /* Reset the repetition count to 1, don't change EF */
    sqlite3_stmt* incorrect;

    /* Increment the repetition count, update EF */
    sqlite3_stmt* correct;

    /* Clear the next training date, so this is rescheduled */
    sqlite3_stmt* retrain;

    /* Reschedule some number of days from now */
    sqlite3_stmt* reschedule;
};

static void sm2_prepare_statement(sm2_t* sm2, sqlite3_stmt** ptr,
                                  const char* stmt)
{
    if (sqlite3_prepare_v2(sm2->db, stmt, -1, ptr, NULL) != SQLITE_OK) {
        log_error_and_abort("Failed to prepare statement %s: %s",
                            stmt, sqlite3_errmsg(sm2->db));
    }
}

/*  Binds the two item parameters */
static void sm2_item_bind(sm2_t* sm2, sqlite3_stmt* s, sm2_item_t* item) {
    SQLITE_CHECKED(sqlite3_reset(s));
    SQLITE_CHECKED(sqlite3_bind_int(s, 1, item->mode));
    SQLITE_CHECKED(sqlite3_bind_text(s, 2, item->state, -1, SQLITE_STATIC));
}

sm2_t* sm2_new() {
    OBJECT_ALLOC(sm2);

    SQLITE_CHECKED(sqlite3_open("sm.sqlite", &sm2->db));

    char* err_msg;
    SQLITE_CHECKED(sqlite3_exec(sm2->db, "CREATE TABLE IF NOT EXISTS sm2 ("
                "type INTEGER NOT NULL,"
                "item TEXT NOT NULL,"
                "ef REAL NOT NULL,"
                "next INT,"
                "reps INT"
                ")", NULL, NULL, &err_msg));

    sqlite3_stmt* check_if_present;
    sm2_prepare_statement(sm2, &check_if_present,
        "SELECT * FROM sm2 WHERE type = ?1 AND item = ?2");

    sqlite3_stmt* insert_state;
    sm2_prepare_statement(sm2, &insert_state,
        "INSERT INTO sm2(type, item, ef, reps)"
        "    VALUES (?1, ?2, 2.5, 0)");

    for (unsigned state=0; state < STATES_COUNT; ++state) {
        for (unsigned j=ITEM_MODE_POSITION; j <= ITEM_MODE_NAME; ++j) {
            sm2_item_t item = (sm2_item_t){
                .mode=j,
                .state=STATES_NAMES[state]
            };
            sm2_item_bind(sm2, check_if_present, &item);

            const int r = sqlite3_step(check_if_present);
            if (r == SQLITE_ROW) {
                // The item is already inserted, keep going
            } else if (r == SQLITE_DONE) {
                sm2_item_bind(sm2, insert_state, &item);
                if (sqlite3_step(insert_state) != SQLITE_DONE) {
                    log_sqlite_error_and_abort();
                }
            } else {
                log_sqlite_error_and_abort();
            }
        }
    }
    sqlite3_finalize(check_if_present);
    sqlite3_finalize(insert_state);

    sm2_prepare_statement(sm2, &sm2->selector,
        "SELECT type, item, ef, reps FROM sm2"
        "    WHERE next IS NULL OR next <= strftime('%s', 'now')"
        "    ORDER BY RANDOM()"
        "    LIMIT 1");
    sm2_prepare_statement(sm2, &sm2->incorrect,
        "UPDATE sm2 SET reps = 1 WHERE type = ?1 AND item = ?2");
    sm2_prepare_statement(sm2, &sm2->correct,
        "UPDATE sm2 SET reps = reps + 1, ef = ?3 WHERE type = ?1 AND item = ?2");
    sm2_prepare_statement(sm2, &sm2->retrain,
        "UPDATE sm2 SET next = NULL WHERE type = ?1 AND item = ?2");
    sm2_prepare_statement(sm2, &sm2->reschedule,
        "UPDATE sm2"
        "    SET next = strftime('%s', 'now') + ?3 * 86400.0"
        "    WHERE type = ?1 AND item = ?2");

    return sm2;
}

void sm2_delete(sm2_t* sm2) {
    sqlite3_finalize(sm2->selector);
    sqlite3_finalize(sm2->incorrect);
    sqlite3_finalize(sm2->correct);
    sqlite3_finalize(sm2->retrain);
    sqlite3_finalize(sm2->reschedule);
    if (sqlite3_close(sm2->db) != SQLITE_OK) {
        log_error("Could not close database while deleting sm2");
    }
    free(sm2);
}

/*  Picks a random item that's scheduled for learning */
sm2_item_t* sm2_next(sm2_t* sm2) {
    sm2_item_t* out = calloc(sizeof(sm2_item_t), 1);
    sqlite3_reset(sm2->selector);
    switch (sqlite3_step(sm2->selector)) {
        case SQLITE_ROW: {
            const int type = sqlite3_column_int(sm2->selector, 0);
            const int len = sqlite3_column_bytes(sm2->selector, 1);
            const unsigned char* txt = sqlite3_column_text(sm2->selector, 1);
            const double ef = sqlite3_column_double(sm2->selector, 2);
            const int reps = sqlite3_column_int(sm2->selector, 3);

            // The item must own its own string
            char* state = malloc(len + 1);
            memcpy(state, txt, len + 1);

            *out = (sm2_item_t){
                .state = state,
                .mode = type,
                .ef = ef,
                .reps = reps
            };
            break;
        }
        case SQLITE_DONE: {
            *out = (sm2_item_t){ .mode = ITEM_MODE_DONE };
            break;
        }
        default: log_sqlite_error_and_abort();
    };
    return out;
}

void sm2_update(sm2_t* sm2, sm2_item_t* item, int q) {
    /*  Implements the SM2 algorithm described at
     *  https://www.supermemo.com/en/archives1990-2015/english/ol/sm2 */
    if (q < 3) {
        /* Reset the repetition count without changing EF */
        sm2_item_bind(sm2, sm2->incorrect, item);
        if (sqlite3_step(sm2->incorrect) != SQLITE_DONE) {
            log_sqlite_error_and_abort();
        }
    } else {
        /* Update the EF for the given item */
        item->ef = fmax(item->ef + 0.1 - (5 - q) * (0.08 + (5 - q) * 0.02),
                        1.3);
        sm2_item_bind(sm2, sm2->correct, item);
        SQLITE_CHECKED(sqlite3_bind_double(sm2->correct, 3, item->ef));
        if (sqlite3_step(sm2->correct) != SQLITE_DONE) {
            log_sqlite_error_and_abort();
        }
    }

    if (q < 4) {
        /* Schedule for immediate re-training */
        sm2_item_bind(sm2, sm2->retrain, item);
        if (sqlite3_step(sm2->retrain) != SQLITE_DONE) {
            log_sqlite_error_and_abort();
        }
    } else {
        /* Calculate next training time based on repetition count */
        const float days = (item->reps <= 1)
            ? 1 : (6 * pow(item->ef, item->reps - 2));
        sm2_item_bind(sm2, sm2->reschedule, item);
        SQLITE_CHECKED(sqlite3_bind_double(sm2->reschedule, 3, days));
        if (sqlite3_step(sm2->reschedule) != SQLITE_DONE) {
            log_sqlite_error_and_abort();
        }
    }
}

void sm2_item_delete(sm2_item_t* item) {
    free((void*)item->state);
    free(item);
}
