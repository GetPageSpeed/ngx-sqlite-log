
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>
#include <sqlite3.h>


#include "ngx_http_sqlitelog_fmt.h"


/*
 * ngx_http_sqlitelog_db_t contains the database connection and all of the
 * necessary data for manipulating it.
 * 
 * Note that each Nginx worker process has its own individual connection to
 * the database. It's impossible to share a single database connection among
 * multiple processes. When SQLite opens a database via sqlite3_open_v2(), it
 * ties process-specific information to the connection. Attempting to use that
 * connection from another process always results in SQLITE_MISUSE (21). This is
 * true even if the connection was created in an Nginx shared memory zone and
 * we attempt to "share" it with multiple Nginx workers.
 * 
 * conn         the database connection
 * filename     the database filename
 * fmt          the log format
 * init_sql     the contents of the SQL file set by init=script
 */
typedef struct {
    sqlite3                   *conn;
    ngx_str_t                  filename;
    ngx_http_sqlitelog_fmt_t  *fmt;
    ngx_str_t                  init_sql;
} ngx_http_sqlitelog_db_t;

int ngx_http_sqlitelog_db_init(ngx_http_sqlitelog_db_t *db, ngx_log_t *log);
int ngx_http_sqlitelog_db_close(ngx_http_sqlitelog_db_t *db, ngx_log_t *log);
int ngx_http_sqlitelog_db_test(ngx_http_sqlitelog_db_t db, ngx_log_t *log);
int ngx_http_sqlitelog_db_insert(ngx_http_sqlitelog_db_t *db, ngx_str_t *elts,
    ngx_uint_t nelts, ngx_log_t *log);
int ngx_http_sqlitelog_db_insert_list(ngx_http_sqlitelog_db_t *db,
    ngx_list_t* list, ngx_log_t *log);
int ngx_http_sqlitelog_db_checkpoint(ngx_http_sqlitelog_db_t *db,
    ngx_log_t *log);
