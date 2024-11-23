
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>


#include "ngx_http_sqlitelog_buf.h"
#include "ngx_http_sqlitelog_db.h"


/*
 * ngx_http_sqlitelog_thread_ctx_t is the data that is passed to a worker thread
 * so that SQLite insertions can be done asynchronously.
 * 
 * db           the database to be written to
 * log_entry    a log entry to insert or unshift in the buffer
 * buf          a buffer to commit
 * pool         a pool for allocating objects, including the context itself
 */
typedef struct {
    ngx_http_sqlitelog_db_t       db;
    ngx_array_t                  *log_entry;
    ngx_http_sqlitelog_buf_t     *buf;
    ngx_pool_t                   *pool;
} ngx_http_sqlitelog_thread_ctx_t;


void ngx_http_sqlitelog_thread_insert_1_handler(void *data, ngx_log_t *log);
void ngx_http_sqlitelog_thread_insert_n_handler(void *data, ngx_log_t *log);
void ngx_http_sqlitelog_thread_flush_handler(void *data, ngx_log_t *log);
void ngx_http_sqlitelog_thread_completed_handler(ngx_event_t *ev);
