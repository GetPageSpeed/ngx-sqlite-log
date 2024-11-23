
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>
#include <ngx_http.h>


#include "ngx_http_sqlitelog_buf.h"
#include "ngx_http_sqlitelog_db.h"
#include "ngx_http_sqlitelog_thread.h"


/**
 * Insert a log entry into the database.
 * 
 * @param  data    the thread context data
 * @param  log     a log for writing error messages
 */
void
ngx_http_sqlitelog_thread_insert_1_handler(void *data, ngx_log_t *log)
{
    int                               rc_insert;
    ngx_http_sqlitelog_thread_ctx_t  *ctx;
    
    ctx = data;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: thread insert 1 handler");
    
    rc_insert = ngx_http_sqlitelog_db_insert(&ctx->db, ctx->log_entry->elts,
                                             ctx->log_entry->nelts, log);
    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: thread insert 1 handler, rc_insert: %d",
                   rc_insert);
    
    if (rc_insert != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: thread insert 1 handler failed to insert "
                      "log entry into database \"%V\"", &ctx->db.filename);
    }
}


/**
 * Execute the buffered transaction.
 * 
 * @param  data    the thread context data
 * @param  log     a log for writing error messages
 */
void
ngx_http_sqlitelog_thread_insert_n_handler(void *data, ngx_log_t *log)
{
    int                               rc_insert;
    ngx_int_t                         rc_list;
    ngx_int_t                         rc_unshift;
    ngx_list_t                        list;
    ngx_pool_t                       *pool;
    ngx_uint_t                        n;
    ngx_slab_pool_t                  *shpool;
    ngx_http_sqlitelog_thread_ctx_t  *ctx;
    
    ctx = data;
    pool = ctx->pool;
    shpool = (ngx_slab_pool_t *) ctx->buf->shm_zone->shm.addr;
    n = ctx->db.fmt->columns.nelts;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: thread insert n handler");
    
    /* 1. Lock */
    ngx_shmtx_lock(&shpool->mutex);
    
    /* 2. List */
    rc_list = ngx_http_sqlitelog_buf_list_locked(ctx->buf, pool, n, &list);
    if (rc_list != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: thread insert n handler failed to create "
                      "list for database \"%V\"", &ctx->db.filename);
        ngx_shmtx_unlock(&shpool->mutex);
        return;
    }
    
    /* 3. Reset */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: thread insert n handler, reset flush timer");
    if (ctx->buf->flush) {
        ngx_http_sqlitelog_buf_timer_reset(ctx->buf);
    }
    
    /* 4. Unlock */
    ngx_shmtx_unlock(&shpool->mutex);
    
    /* 5. Insert */
    rc_insert = ngx_http_sqlitelog_db_insert_list(&ctx->db, &list, log);
    if (rc_insert != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: thread n handler failed to insert list into "
                      "database \"%V\"", &ctx->db.filename);
        return;
    }
    
    /* Unshift */
    if (ctx->log_entry) {
        rc_unshift = ngx_http_sqlitelog_buf_unshift(ctx->buf,
                                                    ctx->log_entry, log);
        if (rc_unshift != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, log, 0,
                          "sqlitelog: thread n handler failed to unshift log "
                          "entry for database \"%V\"", &ctx->db.filename);
        }
    }
}


/**
 * Flush the buffered transaction.
 * 
 * @param  data    the thread context data
 * @param  log     a log for writing error messages
 */
void
ngx_http_sqlitelog_thread_flush_handler(void *data, ngx_log_t *log)
{
    int                               rc_insert;
    ngx_int_t                         rc_list;
    ngx_list_t                        list;
    ngx_pool_t                       *pool;
    ngx_uint_t                        buffer_len;
    ngx_uint_t                        n;
    ngx_slab_pool_t                  *shpool;
    ngx_http_sqlitelog_db_t          *db;
    ngx_http_sqlitelog_buf_flctx_t   *flctx;
    ngx_http_sqlitelog_thread_ctx_t  *thctx;
    
    thctx = data;
    pool = thctx->pool;
    shpool = (ngx_slab_pool_t *) thctx->buf->shm_zone->shm.addr;
    flctx = thctx->buf->event->data;
    db = flctx->db;
    n = db->fmt->columns.nelts;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: thread flush handler");
    
    /* 1. Lock */
    ngx_shmtx_lock(&shpool->mutex);
    buffer_len = ngx_http_sqlitelog_buf_get_len_locked(thctx->buf);
    if (buffer_len == 0) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                       "sqlitelog: thread flush handler, already flushed");
        ngx_http_sqlitelog_buf_timer_reset(thctx->buf);
        ngx_shmtx_unlock(&shpool->mutex);
        return;
    }
    
    /* 2. List */
    rc_list = ngx_http_sqlitelog_buf_list_locked(thctx->buf, pool, n, &list);
    if (rc_list != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: thread flush handler failed to create list "
                      "for database \"%V\"", &thctx->db.filename);
        ngx_shmtx_unlock(&shpool->mutex);
        goto failed;
    }
    
    /* 3. Reset */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: thread flush handler, reset flush timer");
    if (thctx->buf->flush) {
        ngx_http_sqlitelog_buf_timer_reset(thctx->buf);
    }
    
    /* 4. Unlock */
    ngx_shmtx_unlock(&shpool->mutex);
    
    /* 5. Insert */
    rc_insert = ngx_http_sqlitelog_db_insert_list(db, &list, log);
    if (rc_insert != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: thread flush handler failed to insert list "
                      "into database \"%V\"", &db->filename);
        goto failed;
    }
    
    return;
    
failed:
    ngx_log_error(NGX_LOG_ERR, log, 0, 
                  "sqlitelog: thread flush handler failed on database \"%V\"",
                  &db->filename);
}


/**
 * Perform post-asynchronous tasks.
 * 
 * @param  ev     the event associated with this task
 */
void
ngx_http_sqlitelog_thread_completed_handler(ngx_event_t *ev)
{
    ngx_pool_t                       *pool;
    ngx_http_sqlitelog_thread_ctx_t  *ctx;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "sqlitelog: thread completed handler");
    
    ctx = ev->data;
    pool = ctx->pool;
    
    if (pool) {
        ngx_destroy_pool(pool);
    }
}
