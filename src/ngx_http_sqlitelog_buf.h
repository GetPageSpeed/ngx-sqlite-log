
/*
 * Copyright (C) Serope.com
 * 
 * The transaction buffer is queue that holds log entries to be grouped together
 * while writing to the database. When a request comes in, its corresponding
 * log entry is pushed to the queue, and when the buffer has exceeded its
 * size capacity or it has accumulated max nodes, the transaction is executed.
 * This is easier said than done for a few reasons.
 * 
 *  1. The buffer holds its contents in a shared memory zone. Each Nginx worker
 *     process pushes its log entries to the zone. The shared pool mutex must be
 *     carefully locked and unlocked by the workers to avoid race conditions and
 *     to keep the log entries in order.
 *  2. Performing disk I/O (i.e. using SQLite) while the mutex is locked is
 *     discouraged. In order to actually perform the database write, we must
 *     first transfer the log entries *out* of the shared memory queue and
 *     *into* a worker process's memory, then call SQLite from there.
 * 
 * The basic sequence for executing the transaction is:
 * 
 *  1. Lock
 *  2. List (via ngx_http_sqlitelog_buffer_list_locked() )
 *  5. Reset (flush timer)
 *  4. Unlock
 *  5. Insert (via ngx_http_sqlitelog_db_insert_list() )
 * 
 * An additional step, "Unshift", takes place if the execution is occuring
 * because the module attempted to push a new node to the buffer, but failed
 * due to a size overflow.
 */


# pragma once


#include <ngx_core.h>
#include <ngx_thread_pool.h>


#include "ngx_http_sqlitelog_db.h"
#include "ngx_http_sqlitelog_fmt.h"


/*
 * ngx_http_sqlitelog_buf_t represents the transaction buffer.
 * 
 * shm_zone     the shared memory zone for holding the buffer contents
 * max          the max node count for the queue
 * flush        the flush timer, if set
 * event        the flush event
 */
typedef struct {
    ngx_shm_zone_t            *shm_zone;
    ngx_int_t                  max;
    ngx_msec_t                 flush;
    ngx_event_t               *event;
} ngx_http_sqlitelog_buf_t;


/*
 * ngx_http_sqlitelog_buf_shctx_t is the buffer data stored in shared memory
 * to be read and written by multiple workers.
 * 
 * queue        the queue where log entry nodes are stored
 * queue_len    the queue's current length
 */
typedef struct {
    ngx_queue_t               queue;
    ngx_int_t                 queue_len;
} ngx_http_sqlitelog_buf_shctx_t;


/*
 * ngx_http_sqlitelog_buf_flctx_t is the data that is passed to the flush
 * handler.
 * 
 * buf          the buffer
 * db           a database to write to
 * tp           an optional thread pool
 */
typedef struct {
    ngx_http_sqlitelog_buf_t  *buf;
    ngx_http_sqlitelog_db_t   *db;
    
#if (NGX_THREADS)
    ngx_thread_pool_t        **tp;
#else
    void                     **tp;
#endif
} ngx_http_sqlitelog_buf_flctx_t;


ngx_int_t ngx_http_sqlitelog_buf_push(ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log);
ngx_int_t ngx_http_sqlitelog_buf_push_locked( ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log);

ngx_int_t ngx_http_sqlitelog_buf_unshift(ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log);
ngx_int_t ngx_http_sqlitelog_buf_unshift_locked(ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log);

ngx_int_t ngx_http_sqlitelog_buf_list(ngx_http_sqlitelog_buf_t *buf,
    ngx_pool_t *pool, ngx_uint_t n, ngx_list_t *list);
ngx_int_t ngx_http_sqlitelog_buf_list_locked(ngx_http_sqlitelog_buf_t *buf,
    ngx_pool_t *pool, ngx_uint_t n, ngx_list_t *list);

ngx_int_t ngx_http_sqlitelog_buf_get_len(ngx_http_sqlitelog_buf_t *buf);
ngx_int_t ngx_http_sqlitelog_buf_get_len_locked(ngx_http_sqlitelog_buf_t *buf);

void ngx_http_sqlitelog_buf_timer_reset(ngx_http_sqlitelog_buf_t *buf);
void ngx_http_sqlitelog_buf_timer_start(ngx_http_sqlitelog_buf_t *buf);
void ngx_http_sqlitelog_buf_timer_stop(ngx_http_sqlitelog_buf_t *buf);

void ngx_http_sqlitelog_buf_flush_handler(ngx_event_t *ev);
