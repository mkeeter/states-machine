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
        return 0.0f;
    } else {
        return fmax(1.3f, 0.1 - (5 - q) * (0.08 + (5 - q) * 0.02));
    }
}

struct sm2_ {
    sqlite3* db;
    sqlite3_stmt* selector;
};

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
    if (sqlite3_prepare_v2(sm2->db,
        "INSERT OR IGNORE INTO sm2(type, item, ef)"
        "VALUES (?, ?, 2.5)",
        -1, &stmt, NULL) != SQLITE_OK)
    {
        log_error_and_abort("Could not prepare statement");
    }
    for (unsigned m=0; m < 2; ++m) {
        for (unsigned state=0; state < STATES_COUNT; ++state) {
            sqlite3_reset(stmt);
            // No need to clear bindings, since we rebind everything here
            sqlite3_bind_int(stmt, 1, m);
            sqlite3_bind_text(stmt, 2, STATES_NAMES[state], -1, SQLITE_STATIC);
            int err;
            do {
                err = sqlite3_step(stmt);
                if (err == SQLITE_ERROR) {
                    log_error_and_abort("Could not insert initial shape");
                }
            } while (err != SQLITE_DONE);
        }
    }
    sqlite3_finalize(stmt);

    if (sqlite3_prepare_v2(sm2->db,
        "SELECT type, item FROM sm2"
        "    WHERE next IS NULL OR next >= strftime('%s', 'now')"
        "    ORDER BY RANDOM()"
        "    LIMIT 1",
        -1, &sm2->selector, NULL) != SQLITE_OK)
    {
        log_error_and_abort("Failed to prepare selector: %s",
                            sqlite3_errmsg(sm2->db));
    }

    return sm2;
}

void sm2_delete(sm2_t* sm2) {
    sqlite3_finalize(sm2->selector);
    if (sqlite3_close(sm2->db) != SQLITE_OK) {
        log_error("Could not close database while deleting sm2");
    }
    free(sm2);
}

sm2_item_t sm2_next(sm2_t* sm2) {
    sqlite3_reset(sm2->selector);
    int r;
    do {
        r = sqlite3_step(sm2->selector);
        if (r == SQLITE_ERROR) {
            log_error_and_abort("Could not select next item");
        } else if (r == SQLITE_ROW) {
            const int type = sqlite3_column_int(sm2->selector, 0);
            const int len = sqlite3_column_bytes(sm2->selector, 1);
            const unsigned char* txt = sqlite3_column_text(sm2->selector, 1);

            // The item must own its own string
            char* state = malloc(len + 1);
            memcpy(state, txt, len + 1);

            return (sm2_item_t){ state, type };
        }
    } while (r != SQLITE_DONE);

    return (sm2_item_t){ NULL, DONE };
}

/*  Table containing
 *  Item    EF      Last seen at    Repetition count
 */
