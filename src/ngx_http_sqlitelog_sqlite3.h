
/*
 * Copyright (C) Serope.com
 * 
 * These are simply wrappers around a few SQLite functions. They work the same
 * as their sqlite3.h counterparts, with a few exceptions:
 * 
 * 1. Functions that previously didn't take a database connection as a
 *    parameter now do, for the purpose of getting error messages through
 *    sqlite3_errmsg()
 * 2. char* parameters are now ngx_str_t parameters; length parameters are
 *    removed
 * 3. Every function takes an Nginx log as its final parameter for writing
 *    error messages
 */


#pragma once


#include <ngx_core.h>
#include <sqlite3.h>


int ngx_http_sqlitelog_sqlite3_open_v2(ngx_str_t filename, sqlite3 **db_ptr,
    int flags, const char *vfs_module, ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_prepare_v2(sqlite3 *db, ngx_str_t sql,
    sqlite3_stmt **stmt, const char **pz_tail, ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_bind_text(sqlite3 *db, sqlite3_stmt *stmt,
    int position, ngx_str_t val, sqlite3_destructor_type val_destructor,
    ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_bind_null(sqlite3 *db, sqlite3_stmt *stmt,
    int position, ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_bind_blob(sqlite3 *db, sqlite3_stmt *stmt,
    int position, ngx_str_t val, sqlite3_destructor_type val_destructor,
    ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_step(sqlite3 *db, sqlite3_stmt *stmt,
    ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_finalize(sqlite3 *db, sqlite3_stmt *stmt,
    ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_busy_timeout(sqlite3 *db, int duration_ms,
    ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_exec(sqlite3 *db, ngx_str_t sql,
    void *callback, void *callback_data, char **error_message_ptr,
    ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_wal_checkpoint_v2(sqlite3 *db, const char *zdb,
    int emode, int *pn_log, int *pn_ckpt, ngx_log_t *log);

int ngx_http_sqlitelog_sqlite3_close(sqlite3 *db, ngx_log_t *log);
