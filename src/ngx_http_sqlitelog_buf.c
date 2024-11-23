
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>
#include <ngx_thread_pool.h>


#include "ngx_http_sqlitelog_buf.h"
#include "ngx_http_sqlitelog_db.h"
#include "ngx_http_sqlitelog_node.h"
#include "ngx_http_sqlitelog_thread.h"


static ngx_int_t ngx_http_sqlitelog_buf_move_locked(
    ngx_http_sqlitelog_buf_t *buf, ngx_list_t *list);

static void ngx_http_sqlitelog_buf_flush(ngx_http_sqlitelog_buf_t *buf,
    ngx_http_sqlitelog_db_t *db, ngx_log_t *log);
#if (NGX_THREADS)
static void ngx_http_sqlitelog_buf_flush_async(ngx_http_sqlitelog_buf_t *buf,
    ngx_http_sqlitelog_db_t *db, ngx_log_t *log);
#endif


/**
 * Push a log entry to the buffer.
 * 
 * @param   buf     the buffer in question
 * @param   entry   the log entry in question
 * @param   log     a log for writing error messages
 * @return          NGX_OK on success, or
 *                  NGX_DONE on success and the buffer reaching its max, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_buf_push(ngx_http_sqlitelog_buf_t *buf, ngx_array_t *entry,
    ngx_log_t *log)
{
    ngx_int_t          rc_push;
    ngx_slab_pool_t   *shpool;
    
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    
    ngx_shmtx_lock(&shpool->mutex);
    rc_push = ngx_http_sqlitelog_buf_push_locked(buf, entry, log);
    ngx_shmtx_unlock(&shpool->mutex);
    
    return rc_push;
}


/**
 * Push a log entry to the buffer.
 * 
 * The shared pool must be locked.
 * 
 * @param   buf     the buffer in question
 * @param   entry   the log entry in question
 * @param   log     a log for writing error messages
 * @return          NGX_OK on success, or
 *                  NGX_DONE on success and the buffer reaching its max, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_buf_push_locked(ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log)
{
    ngx_slab_pool_t                 *shpool;
    ngx_http_sqlitelog_node_t       *node;
    ngx_http_sqlitelog_buf_shctx_t  *shctx;
    
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    shctx = buf->shm_zone->data;
    
    node = ngx_http_sqlitelog_node_create_locked(entry, shpool, log);
    if (node == NULL) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                       "sqlitelog: buffer overflow, len: %d", shctx->queue_len);
        return NGX_ERROR;
    }
    
    ngx_queue_insert_tail(&shctx->queue, &node->link);
    shctx->queue_len += 1;
    
    if (buf->max && shctx->queue_len >= buf->max) {
        return NGX_DONE;
    }
    
    return NGX_OK;
}


/**
 * Put a log entry at the beginning of the buffer.
 * 
 * @param   buf     the buffer in question
 * @param   entry   the log entry in question
 * @param   log     a log for writing error messages
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_buf_unshift(ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log)
{
    ngx_int_t          rc_unshift;
    ngx_slab_pool_t   *shpool;
    
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    
    ngx_shmtx_lock(&shpool->mutex);
    rc_unshift = ngx_http_sqlitelog_buf_unshift_locked(buf, entry, log);
    ngx_shmtx_unlock(&shpool->mutex);
    
    return rc_unshift;
}


/**
 * Put a log entry at the beginning of the buffer.
 * 
 * The shared pool must be locked.
 * 
 * @param   buf     the buffer in question
 * @param   entry   the log entry in question
 * @param   log     a log for writing error messages
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_buf_unshift_locked(ngx_http_sqlitelog_buf_t *buf,
    ngx_array_t *entry, ngx_log_t *log)
{
    ngx_slab_pool_t                 *shpool;
    ngx_http_sqlitelog_node_t       *node;
    ngx_http_sqlitelog_buf_shctx_t  *shctx;
    
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    
    node = ngx_http_sqlitelog_node_create_locked(entry, shpool, log);
    if (node == NULL) {
        return NGX_ERROR;
    }
    
    shctx = buf->shm_zone->data;
    ngx_queue_insert_head(&shctx->queue, &node->link);
    shctx->queue_len += 1;
    
    return NGX_OK;
}


/**
 * Move the buffer's contents from shared memory to local memory, clearing
 * the queue contents in the process.
 * 
 * The shared pool must be locked.
 * 
 * @param   buf     the buffer in question
 * @param   list    an initialized list in local memory to hold the contents
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
static ngx_int_t
ngx_http_sqlitelog_buf_move_locked(ngx_http_sqlitelog_buf_t *buf,
    ngx_list_t *list)
{
    size_t                           sbuf_size;
    u_char                          *sbuf;
    ngx_str_t                       *elt;
    ngx_str_t                       *s;
    ngx_uint_t                       i;
    ngx_queue_t                     *q;
    ngx_slab_pool_t                 *shpool;
    ngx_http_sqlitelog_node_t       *node;
    ngx_http_sqlitelog_buf_shctx_t  *ctx;
    
    shpool = (ngx_slab_pool_t*) buf->shm_zone->shm.addr;
    ctx = buf->shm_zone->data;
    
    /* Copy */
    for (q = ngx_queue_head(&ctx->queue);
         q != ngx_queue_sentinel(&ctx->queue);
         q = ngx_queue_next(q))
    {
        node = ngx_queue_data(q, ngx_http_sqlitelog_node_t, link);
        elt = node->elts;
        
        for (i = 0; i < node->nelts; i++) {
            
            s = ngx_list_push(list);
            if (s == NULL) {
                return NGX_ERROR;
            }
            
            else if (elt->data == NULL) {
                s->data = NULL;
                s->len = 0;
            }
            
            else {
                sbuf_size = elt->len;
                sbuf = ngx_pcalloc(list->pool, sbuf_size * sizeof(u_char));
                if (sbuf == NULL) {
                    return NGX_ERROR;
                }
                ngx_memcpy(sbuf, elt->data, elt->len);
                s->data = sbuf;
                s->len = elt->len;
            }
            
            elt++;
        }
    }
    
    /* Clear */
    for (q = ngx_queue_head(&ctx->queue);
         q != ngx_queue_sentinel(&ctx->queue);
         q = ngx_queue_next(q))
    {
        node = ngx_queue_data(q, ngx_http_sqlitelog_node_t, link);
        ngx_http_sqlitelog_node_destroy_locked(node, shpool);
    }
    
    ctx->queue_len = 0;
    ngx_queue_init(&ctx->queue);
    
    return NGX_OK;
}


/**
 * Move the buffer's contents from shared memory to local memory, clearing
 * the queue contents in the process.
 * 
 * This is similar to ngx_http_sqlitelog_buffer_move(), except the list is
 * initialized in the given pool.
 * 
 * @param   buf     the buffer in question
 * @param   pool    a pool in which to initialize the list
 * @param   n       the amount of elements per list part (log format columns)
 * @param   list    an uninitialized list to hold the contents
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_buf_list(ngx_http_sqlitelog_buf_t *buf, ngx_pool_t *pool,
    ngx_uint_t n, ngx_list_t *list)
{
    ngx_int_t         rc_list;
    ngx_slab_pool_t  *shpool;
    
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    
    ngx_shmtx_lock(&shpool->mutex);
    rc_list = ngx_http_sqlitelog_buf_list_locked(buf, pool, n, list);
    ngx_shmtx_unlock(&shpool->mutex);
    
    return rc_list;
}


/**
 * Move the buffer's contents from shared memory to local memory, clearing
 * the queue contents in the process.
 * 
 * This is similar to ngx_http_sqlitelog_buffer_move(), except the list is
 * initialized in the given pool.
 * 
 * The shared pool must be locked.
 * 
 * @param   buf     the buffer in question
 * @param   pool    a pool in which to initialize the list
 * @param   n       the amount of elements per list part (log format columns)
 * @param   list    an uninitialized list to hold the contents
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_buf_list_locked(ngx_http_sqlitelog_buf_t *buf,
    ngx_pool_t *pool, ngx_uint_t n, ngx_list_t *list)
{
    ngx_int_t  rc_init;
    ngx_int_t  rc_move;
    
    rc_init = ngx_list_init(list, pool, n, sizeof(ngx_str_t));
    if (rc_init != NGX_OK) {
        return NGX_ERROR;
    }
    
    rc_move = ngx_http_sqlitelog_buf_move_locked(buf, list);
    if (rc_move != NGX_OK) {
        return NGX_ERROR;
    }
    
    return NGX_OK;
}


/**
 * Get the buffer's current length.
 * 
 * @param   buf     the buffer in question
 * @return          the buffer's current length
 */
ngx_int_t
ngx_http_sqlitelog_buf_get_len(ngx_http_sqlitelog_buf_t *buf)
{
    ngx_int_t         len;
    ngx_slab_pool_t  *shpool;
    
    shpool = (ngx_slab_pool_t*) buf->shm_zone->shm.addr;
    
    ngx_shmtx_lock(&shpool->mutex);
    len = ngx_http_sqlitelog_buf_get_len_locked(buf);
    ngx_shmtx_unlock(&shpool->mutex);
    
    return len;
}


/**
 * Get the buffer's current length.
 * 
 * The shared pool must be locked.
 * 
 * @param   buf     the buffer in question
 * @return          the buffer's current length
 */
ngx_int_t
ngx_http_sqlitelog_buf_get_len_locked(ngx_http_sqlitelog_buf_t *buf)
{
    ngx_http_sqlitelog_buf_shctx_t  *ctx;
    
    ctx = buf->shm_zone->data;
    
    return ctx->queue_len;
}


/**
 * Reset a buffer's flush timer.
 * 
 * @param   buf     the buffer in question
 */
void
ngx_http_sqlitelog_buf_timer_reset(ngx_http_sqlitelog_buf_t *buf)
{
    ngx_http_sqlitelog_buf_timer_stop(buf);
    ngx_http_sqlitelog_buf_timer_start(buf);
}


/**
 * Start a buffer's flush timer.
 * 
 * @param   buf     the buffer in question
 */
void
ngx_http_sqlitelog_buf_timer_start(ngx_http_sqlitelog_buf_t *buf)
{
    if (buf->flush && buf->event && buf->event->timer_set == 0) {
        ngx_add_timer(buf->event, buf->flush);
    }
}


/**
 * Stop a buffer's flush timer.
 * 
 * @param   buf     the buffer in question
 */
void
ngx_http_sqlitelog_buf_timer_stop(ngx_http_sqlitelog_buf_t *buf)
{
    if (buf->flush && buf->event && buf->event->timer_set) {
        ngx_del_timer(buf->event);
    }
}


/**
 * Perform a buffer flush. This is called when the flush timer has elapsed.
 * 
 * @param   ev      the flush event
 */
void
ngx_http_sqlitelog_buf_flush_handler(ngx_event_t *ev)
{
    ngx_http_sqlitelog_buf_flctx_t  *ctx;

    ctx = ev->data;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "sqlitelog: flush handler");
    
#if (NGX_THREADS)
    if (*(ctx->tp)) {
        ngx_http_sqlitelog_buf_flush_async(ctx->buf, ctx->db, ev->log);
    }
    else
#endif
    ngx_http_sqlitelog_buf_flush(ctx->buf, ctx->db, ev->log);
}


/**
 * Flush the buffer.
 * 
 * @param   buf     the buffer to be flushed
 * @param   db      the database to be written to
 * @param   log     a log for writing error messages
 */
static void
ngx_http_sqlitelog_buf_flush(ngx_http_sqlitelog_buf_t *buf,
    ngx_http_sqlitelog_db_t *db, ngx_log_t *log)
{
    int                       rc_insert;
    ngx_int_t                 buf_len;
    ngx_int_t                 rc_list;
    ngx_list_t                list;
    ngx_uint_t                n;
    ngx_pool_t               *pool;
    ngx_slab_pool_t          *shpool;
    
    pool = NULL;
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    n = db->fmt->columns.nelts;
    
    /* Buffer length */
    ngx_shmtx_lock(&shpool->mutex);
    buf_len = ngx_http_sqlitelog_buf_get_len_locked(buf);
    ngx_shmtx_unlock(&shpool->mutex);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: buf flush, len: %d", buf_len);
    
    /* Empty */
    if (buf_len == 0) {
        ngx_http_sqlitelog_buf_timer_reset(buf);
        return;
    }
    
    /* 1. Lock */
    ngx_shmtx_lock(&shpool->mutex);
    buf_len = ngx_http_sqlitelog_buf_get_len_locked(buf);
    if (buf_len == 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                       "sqlitelog: buf flush, len: 0, previously %d but "
                       "another worker process already flushed", buf_len);
        ngx_http_sqlitelog_buf_timer_reset(buf);
        ngx_shmtx_unlock(&shpool->mutex);
        goto failed;
    }
    
    /* 2. List */
    pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, log);
    if (pool == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: buffer flush failed to create pool of %z "
                      "bytes", NGX_DEFAULT_POOL_SIZE);
        ngx_shmtx_unlock(&shpool->mutex);
        goto failed;
    }
    rc_list = ngx_http_sqlitelog_buf_list_locked(buf, pool, n, &list);
    if (rc_list != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: buffer flush failed to create list "
                      "for database \"%V\"", &db->filename);
        ngx_shmtx_unlock(&shpool->mutex);
        goto failed;
    }
    
    /* 3. Reset */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: buf flush, reset timer");
    ngx_http_sqlitelog_buf_timer_reset(buf);
    
    /* 4. Unlock */
    ngx_shmtx_unlock(&shpool->mutex);
    
    /* 5. Insert */
    rc_insert = ngx_http_sqlitelog_db_insert_list(db, &list, log);
    if (rc_insert != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: buffer flush failed to insert list "
                      "into database \"%V\"", &db->filename);
    }

failed:

    if (pool) {
        ngx_destroy_pool(pool);
    }
}


/**
 * Flush the buffer asynchronously.
 * 
 * @param   buf     the buffer to be flushed
 * @param   db      the database to be written to
 * @param   log     a log for writing error messages
 */
#if (NGX_THREADS)
static void
ngx_http_sqlitelog_buf_flush_async(ngx_http_sqlitelog_buf_t *buf,
    ngx_http_sqlitelog_db_t *db, ngx_log_t *log)
{
    ngx_int_t                         rc_post;
    ngx_uint_t                        buf_len;
    ngx_pool_t                       *pool;
    ngx_slab_pool_t                  *shpool;
    ngx_thread_task_t                *task;
    ngx_http_sqlitelog_buf_flctx_t   *flctx;
    ngx_http_sqlitelog_thread_ctx_t  *thctx;
    
    pool = NULL;
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    flctx = buf->event->data;
    
    /* Buffer length */
    ngx_shmtx_lock(&shpool->mutex);
    buf_len = ngx_http_sqlitelog_buf_get_len_locked(buf);
    ngx_shmtx_unlock(&shpool->mutex);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: buf flush async, len: %d", buf_len);
    
    /* Empty */
    if (buf_len == 0) {
        ngx_http_sqlitelog_buf_timer_reset(buf);
        return;
    }
    
    /* Create pool */
    pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, log);
    if (pool == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: async buffer flush failed to create pool of "
                      "%z bytes", NGX_DEFAULT_POOL_SIZE);
        goto failed;
    }
    
    /* Create thread task */
    task = ngx_thread_task_alloc(pool, sizeof(ngx_http_sqlitelog_thread_ctx_t));
    if (task == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: async buffer flush failed to create thread "
                      "task");
        goto failed;
    }
    
    /* Set async context */
    thctx = task->ctx;
    thctx->log_entry = NULL;
    thctx->buf = buf;
    thctx->pool = pool;
    
    task->handler = ngx_http_sqlitelog_thread_flush_handler;
    task->event.handler = ngx_http_sqlitelog_thread_completed_handler;
    task->event.data = thctx;
    task->event.log = ngx_cycle->log;
    
    /* Begin */
    rc_post = ngx_thread_task_post(*(flctx->tp), task);
    if (rc_post != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: async buffer flush failed to post thread "
                      "task");
        goto failed;
    }
    else {
        return;
    }
    
failed:

    if (pool) {
        ngx_destroy_pool(pool);
    }
}
#endif
