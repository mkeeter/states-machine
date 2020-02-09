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
                "next TEXT,"
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

    return sm2;
}

void sm2_delete(sm2_t* sm2) {
    if (sqlite3_close(sm2->db) != SQLITE_OK) {
        log_error("Could not close database while deleting sm2");
    }
    free(sm2);
}

/*  Table containing
 *  Item    EF      Last seen at    Repetition count
 */
