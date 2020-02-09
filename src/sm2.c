#include "data.h"
#include "log.h"
#include "object.h"
#include "sm2.h"

/*
 *  Implements the SM2 algorithm described at
 *  https://www.supermemo.com/en/archives1990-2015/english/ol/sm2
 */
float sm2_repetition_interval(unsigned n, float ef) {
    switch (n) {
        case 0: log_error_and_abort("Invalid repetition count");
        case 1: return 1;
        case 2: return 6;
        default: return sm2_repetition_interval(n - 1, ef) * ef;
    }
}

float sm2_updated_ef(unsigned q, float ef) {
    if (q < 3) {
        log_error_and_abort("EF should not be updated if q < 3");
        return 0.0f;
    } else {
        return fmax(1.3f, ef + 0.1 - (5 - q) * (0.08 + (5 - q) * 0.02));
    }
}

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

sm2_t* sm2_new() {
    OBJECT_ALLOC(sm2);

    if (sqlite3_open("sm.sqlite", &sm2->db) != SQLITE_OK) {
        log_error_and_abort("Could not open database");
    }

    char* err_msg;
    if (sqlite3_exec(sm2->db, "CREATE TABLE IF NOT EXISTS sm2 ("
                "type INTEGER NOT NULL,"
                "item TEXT NOT NULL,"
                "ef REAL NOT NULL,"
                "next INT,"
                "reps INT"
                ")", NULL, NULL, &err_msg) != SQLITE_OK)
    {
        log_error_and_abort("Could not create table: %s", err_msg);
    }

    sqlite3_stmt* stmt;
    sm2_prepare_statement(sm2, &stmt,
        "INSERT OR IGNORE INTO sm2(type, item, ef)"
        "    VALUES (?, ?, 2.5)");
    for (unsigned m=0; m < 2; ++m) {
        for (unsigned state=0; state < STATES_COUNT; ++state) {
            sqlite3_reset(stmt);
            // No need to clear bindings, since we rebind everything here
            sqlite3_bind_int(stmt, 1, m);
            sqlite3_bind_text(stmt, 2, STATES_NAMES[state], -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                log_error_and_abort("Unexpected SQLite error: %s",
                                    sqlite3_errmsg(sm2->db));
            }
        }
    }
    sqlite3_finalize(stmt);

    sm2_prepare_statement(sm2, &sm2->selector,
        "SELECT type, item FROM sm2"
        "    WHERE next IS NULL OR next >= strftime('%s', 'now')"
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

sm2_item_t sm2_next(sm2_t* sm2) {
    sqlite3_reset(sm2->selector);
    switch (sqlite3_step(sm2->selector)) {
        case SQLITE_ROW: {
            const int type = sqlite3_column_int(sm2->selector, 0);
            const int len = sqlite3_column_bytes(sm2->selector, 1);
            const unsigned char* txt = sqlite3_column_text(sm2->selector, 1);

            // The item must own its own string
            char* state = malloc(len + 1);
            memcpy(state, txt, len + 1);

            return (sm2_item_t){ state, type };
        }
        case SQLITE_DONE: break;
        default: log_error_and_abort("Unexpected SQLite result: %s",
                                     sqlite3_errmsg(sm2->db));
    };
    return (sm2_item_t){ NULL, DONE };
}

/*  Binds the two item parameters */
static void item_bind(sqlite3_stmt* s, sm2_item_t item) {
    sqlite3_reset(s);
    sqlite3_bind_int(s, 1, item.mode);
    sqlite3_bind_text(s, 2, item.state, -1, SQLITE_STATIC);
}

void sm2_update(sm2_t* sm2, sm2_item_t item, int q) {
    if (q < 3) {
        item_bind(sm2->incorrect, item);
        if (sqlite3_step(sm2->incorrect) != SQLITE_DONE) {
            log_error_and_abort("Unexpected SQLite error");
        }
    } else {
        double new_ef = 0.0f;
        item_bind(sm2->correct, item);
        sqlite3_bind_double(sm2->correct, 3, new_ef);
        if (sqlite3_step(sm2->correct) != SQLITE_DONE) {
            log_error_and_abort("Unexpected SQLite error");
        }
    }


    if (q < 4) {
        item_bind(sm2->retrain, item);
        // Schedule for immediate re-training
    } else {
        item_bind(sm2->reschedule, item);
        // Calculate next training time based on repetition count
    }
}
