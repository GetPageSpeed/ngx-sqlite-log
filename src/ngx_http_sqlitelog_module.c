
/*
 * Copyright (C) Serope.com
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <stdio.h>

#include "ngx_http_sqlitelog_buf.h"
#include "ngx_http_sqlitelog_col.h"
#include "ngx_http_sqlitelog_db.h"
#include "ngx_http_sqlitelog_file.h"
#include "ngx_http_sqlitelog_fmt.h"
#include "ngx_http_sqlitelog_op.h"
#include "ngx_http_sqlitelog_sql.h"
#include "ngx_http_sqlitelog_thread.h"
#include "ngx_http_sqlitelog_util.h"


/*
 * ngx_http_sqlitelog_main_conf_t holds all defined log formats (including the
 * predefined combined format) and the thread pool named by sqlitelog_async,
 * if given.
 * 
 * formats          an array of log formats (ngx_http_sqlitelog_fmt_t)
 * combined_init    a flag set to 1 if "combined" format has been initialized
 * tp               a thread pool set by sqlitelog_async
 */
typedef struct {
    ngx_array_t                 formats;
    ngx_flag_t                  combined_init;
#if (NGX_THREADS)
    ngx_thread_pool_t          *tp;
#else
    void                       *tp; /* unused */
#endif
} ngx_http_sqlitelog_main_conf_t;


/*
 * ngx_http_sqlitelog_srv_conf_t represents an instance of the sqlitelog
 * directive. This configuration can exist in both http contexts and server
 * contexts.
 * 
 * The enabled flag has a value of either 0, 1, or NGX_CONF_UNSET to indicate
 * that the sqlitelog directive is off, on, or unused in the current context,
 * respectively.
 * 
 * enabled       a flag set to 0, 1, or NGX_CONF_UNSET
 * db            the database associated with this sqlitelog
 * buf           a buffer for holding multiple log entries
 * filter        a logging condition
 */
typedef struct {
    ngx_flag_t                  enabled; 
    ngx_http_sqlitelog_db_t     db;
    ngx_http_sqlitelog_buf_t   *buf;
    ngx_http_complex_value_t   *filter;
} ngx_http_sqlitelog_srv_conf_t;

#if (NGX_THREADS)
static char* ngx_http_sqlitelog_async(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_sqlitelog_handle_1_async(ngx_http_request_t *r,
    ngx_array_t *log_entry, ngx_pool_t *pool);
static ngx_int_t ngx_http_sqlitelog_handle_n_async(ngx_http_request_t *r,
    ngx_array_t *log_entry, ngx_pool_t *pool);
#endif

static char* ngx_http_sqlitelog(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char* ngx_http_sqlitelog_opt_format(ngx_conf_t *cf, ngx_str_t arg);
static char* ngx_http_sqlitelog_opt_buffer(ngx_conf_t *cf, ngx_str_t arg,
    ssize_t *sizep);
static char* ngx_http_sqlitelog_opt_max(ngx_conf_t *cf, ngx_str_t arg,
    ngx_int_t *max);
static char* ngx_http_sqlitelog_opt_flush(ngx_conf_t *cf, ngx_str_t arg,
    ngx_msec_t *flush);
static char* ngx_http_sqlitelog_opt_init(ngx_conf_t *cf, ngx_str_t arg);
static char* ngx_http_sqlitelog_opt_if(ngx_conf_t *cf, ngx_str_t arg);
static char* ngx_http_sqlitelog_format(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_int_t ngx_http_sqlitelog_handler(ngx_http_request_t *r);
static ngx_array_t *ngx_http_sqlitelog_log_entry(ngx_http_request_t *r,
    ngx_array_t columns, ngx_pool_t *pool);
static ngx_int_t ngx_http_sqlitelog_handle_1(ngx_http_request_t *r,
    ngx_array_t *log_entry, ngx_pool_t *pool);
static ngx_int_t ngx_http_sqlitelog_handle_n(ngx_http_request_t *r,
    ngx_array_t *log_entry, ngx_pool_t *pool);

static void *ngx_http_sqlitelog_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_sqlitelog_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_sqlitelog_merge_srv_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_shm_zone_t *ngx_http_sqlitelog_shm_zone(ngx_conf_t *cf, ssize_t size,
    ngx_str_t format, ngx_str_t filename);
static ngx_int_t ngx_http_sqlitelog_init_shm_zone(ngx_shm_zone_t *shm_zone,
    void *old_data);

static ngx_int_t ngx_http_sqlitelog_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_sqlitelog_init_worker(ngx_cycle_t *cycle);
static void ngx_http_sqlitelog_exit_worker(ngx_cycle_t *cycle);
static void ngx_http_sqlitelog_exit_master(ngx_cycle_t *cycle);


static ngx_command_t  ngx_http_sqlitelog_commands[] = {
    { ngx_string("sqlitelog"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_sqlitelog,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },
    
    { ngx_string("sqlitelog_format"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_2MORE,
      ngx_http_sqlitelog_format,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },
    
#if (NGX_THREADS)
    { ngx_string("sqlitelog_async"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_sqlitelog_async,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },
#endif
    
    ngx_null_command
};


static ngx_http_module_t  ngx_http_sqlitelog_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_sqlitelog_init,               /* postconfiguration */

    ngx_http_sqlitelog_create_main_conf,   /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_sqlitelog_create_srv_conf,    /* create server configuration */
    ngx_http_sqlitelog_merge_srv_conf,     /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_sqlitelog_module = {
    NGX_MODULE_V1,
    &ngx_http_sqlitelog_module_ctx,        /* module context */
    ngx_http_sqlitelog_commands,           /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_http_sqlitelog_init_worker,        /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_http_sqlitelog_exit_worker,        /* exit process */
    ngx_http_sqlitelog_exit_master,        /* exit master */
    NGX_MODULE_V1_PADDING
};


/**
 * Initialize the module.
 * 
 * This is called after creating all the configuration struct objects for
 * this module and parsing all of the directives.
 * 
 * @param   cf  the current Nginx configuration file
 * @return      NGX_OK on success,
 *              NGX_ERROR on error
 */
static ngx_int_t
ngx_http_sqlitelog_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt             *h;
    ngx_http_core_main_conf_t       *cmc;
    
    cmc = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    h = ngx_array_push(&cmc->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_sqlitelog_handler;
    
    return NGX_OK;
}


/**
 * Handle the current web request.
 * 
 * @param   r   the request to handle
 * @return      NGX_OK on success,
 *              NGX_ERROR if an error occurs
 */
static ngx_int_t
ngx_http_sqlitelog_handler(ngx_http_request_t *r)
{
    int                              rc_close;
    ngx_int_t                      (*handle_entry) (ngx_http_request_t *r,
                                     ngx_array_t *log_entry, ngx_pool_t *pool);
    ngx_str_t                        condition;
    ngx_pool_t                      *pool;
    ngx_array_t                     *log_entry;
    ngx_http_sqlitelog_srv_conf_t   *lscf;
    ngx_http_sqlitelog_main_conf_t  *lmcf;
    
    lscf = ngx_http_get_module_srv_conf(r, ngx_http_sqlitelog_module);
    lmcf = ngx_http_get_module_main_conf(r, ngx_http_sqlitelog_module);
    handle_entry = NULL;
    
    /* Enabled check */
    if (lscf->enabled == 0) {
        return NGX_OK;
    }
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handler, server=\"%V\", request=\"%V\"",
                   &r->headers_in.server, &r->request_line);
    
    /* Condition */
    if (lscf->filter) {
        if (ngx_http_complex_value(r, lscf->filter, &condition) != NGX_OK) {
            return NGX_ERROR;
        }
        if (ngx_str_is_false(&condition)) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "sqlitelog: condition rejected");
            return NGX_OK;
        }
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "sqlitelog: condition passed");
    }
    
    /*
     * Choose a pool for allocating the log entry. If async is on, we must
     * create a new pool because r->pool is unreliable (Nginx might destroy r
     * before our thread handler has a chance to read from it).
     */
# if (NGX_THREADS)
    if (lmcf->tp) {
        pool = ngx_create_pool(NGX_DEFAULT_POOL_SIZE, r->connection->log);
        if (pool == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "sqlitelog: failed to allocate %z bytes for pool "
                          "during request handler", NGX_DEFAULT_POOL_SIZE);
            return NGX_ERROR;
        }
    }
    else
#endif
    pool = r->pool;
    
    /* Get log entry */
    log_entry = ngx_http_sqlitelog_log_entry(r, lscf->db.fmt->columns, pool);
    if (log_entry == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: failed to get log entry for request, ",
                      "server=\"%V\", request=\"%V\"",
                      &r->headers_in.server, &r->request_line);
        goto failed;
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handler, log entry fields: %d",log_entry->nelts);
    
    /* Choose function for handling log entry */
    if (lmcf->tp) {
#if (NGX_THREADS)
        if (lscf->buf) {
            handle_entry = ngx_http_sqlitelog_handle_n_async;
        } else {
            handle_entry = ngx_http_sqlitelog_handle_1_async;
        }
#endif
    }
    else {
        if (lscf->buf) {
            handle_entry = ngx_http_sqlitelog_handle_n;
        } else {
            handle_entry = ngx_http_sqlitelog_handle_1;
        }
    }
    
    /* Handle log entry */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handler, handle log entry...");
    
    if (handle_entry(r, log_entry, pool) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: failed to handle log entry "
                      "for database \"%V\"", &lscf->db.filename);
        goto failed;
    }
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handler, handle log entry OK");
    
    return NGX_OK;
    
failed:
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "sqlitelog: handler disabled for worker process %d",
                  ngx_getpid());
    rc_close = ngx_http_sqlitelog_db_close(&lscf->db, r->connection->log);
    if (rc_close != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: handler failed to close database \"%V\"",
                      &lscf->db.filename);
    }
    lscf->enabled = 0;

#if (NGX_THREAD)
    if (lmcf->tp) {
        ngx_destroy_pool(pool);
    }
#endif
    
    return NGX_ERROR;
}


/**
 * Handle the current web request without a transaction queue.
 * 
 * @param   r           the current web request
 * @param   log_entry   values to write to the database
 * @param   pool        a pool for object allocations
 * @return              NGX_OK on success,
 *                      NGX_ERROR if an error occurs
 */
static ngx_int_t
ngx_http_sqlitelog_handle_1(ngx_http_request_t *r, ngx_array_t *log_entry,
    ngx_pool_t *pool)
{
    int                             rc_insert;
    ngx_http_sqlitelog_srv_conf_t  *lscf;
    
    lscf = ngx_http_get_module_srv_conf(r, ngx_http_sqlitelog_module);
    
    rc_insert = ngx_http_sqlitelog_db_insert(&lscf->db, log_entry->elts,
                                             log_entry->nelts,
                                             r->connection->log);
    if (rc_insert != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: handle 1 failed to insert record into "
                      "database \"%V\"", &lscf->db.filename);
        return NGX_ERROR;
    }
    
    return NGX_OK;
}


/**
 * Handle the current web request without a transaction queue in a worker
 * thread.
 * 
 * @param   r           the current web request
 * @param   log_entry   values to write to the database
 * @param   pool        a pool for object allocations
 * @return              NGX_OK on success,
 *                      NGX_ERROR if an error occurs
 */
#if (NGX_THREADS)
static ngx_int_t
ngx_http_sqlitelog_handle_1_async(ngx_http_request_t *r, ngx_array_t *log_entry,
    ngx_pool_t *pool)
{
    ngx_int_t                         rc_post;
    ngx_thread_task_t                *task;
    ngx_http_sqlitelog_srv_conf_t    *lscf;
    ngx_http_sqlitelog_main_conf_t   *lmcf;
    ngx_http_sqlitelog_thread_ctx_t  *ctx;
    
    lscf = ngx_http_get_module_srv_conf(r, ngx_http_sqlitelog_module);
    lmcf = ngx_http_get_module_main_conf(r, ngx_http_sqlitelog_module);
    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle 1 async, log entry fields: %d",
                   log_entry->nelts);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle 1 async, log_entry: %p",
                   log_entry);
    
    task = ngx_thread_task_alloc(pool, sizeof(ngx_http_sqlitelog_thread_ctx_t));
    if (task == NULL) {
        return NGX_ERROR;
    }
    
    ctx = task->ctx;
    ctx->db = lscf->db;
    ctx->log_entry = log_entry;
    ctx->buf = NULL;
    ctx->pool = pool;
    
    task->handler = ngx_http_sqlitelog_thread_insert_1_handler;
    task->event.handler = ngx_http_sqlitelog_thread_completed_handler;
    task->event.data = ctx;
    task->event.log = ngx_cycle->log;
    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle 1 async, ctx->log_entry: %p",
                   ctx->log_entry);
    
    rc_post = ngx_thread_task_post(lmcf->tp, task);
    
    if (rc_post != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: handle 1 async, rc_post: %d", rc_post);
        return rc_post;
    }
    
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle 1 async, rc_post: %d, task: %p, "
                   "task->next: %p", rc_post, task, task->next);
    
    r->main->blocked++;
    
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle 1 async, r: %p, r->blocked: %d, "
                   "r->main->blocked: %d", r, r->blocked, r->main->blocked);
    
    return NGX_OK;
}
#endif


/**
 * Handle the current web request with a transaction buffer from a worker
 * thread.
 * 
 * @param   r           the current web request
 * @param   log_entry   values to write to the database
 * @param   pool        a pool for object allocations
 * @return              NGX_OK on success,
 *                      NGX_ERROR if an error occurs
 */
#if (NGX_THREADS)
static ngx_int_t
ngx_http_sqlitelog_handle_n_async(ngx_http_request_t *r, ngx_array_t *log_entry,
    ngx_pool_t *pool)
{
    ngx_int_t                         rc_push;
    ngx_thread_task_t                *task;
    ngx_http_sqlitelog_srv_conf_t    *lscf;
    ngx_http_sqlitelog_main_conf_t   *lmcf;
    ngx_http_sqlitelog_thread_ctx_t  *ctx;
    
    lscf = ngx_http_get_module_srv_conf(r, ngx_http_sqlitelog_module);
    lmcf = ngx_http_get_module_main_conf(r, ngx_http_sqlitelog_module);
    rc_push = ngx_http_sqlitelog_buf_push(lscf->buf, log_entry,
                                          r->connection->log);
    
    /* Push success - nothing else to do */
    if (rc_push == NGX_OK) {
        return NGX_OK;
    }
    
    /* Create thread task */
    task = ngx_thread_task_alloc(pool, sizeof(ngx_http_sqlitelog_thread_ctx_t));
    if (task == NULL) {
        return NGX_ERROR;
    }
    
    /* Set async context */
    ctx = task->ctx;
    ctx->db = lscf->db;
    ctx->log_entry = log_entry;
    ctx->buf = lscf->buf;
    ctx->pool = pool;
    
    /*
     * Set ctx->log_entry depending on the push return code.
     * 
     * If we got NGX_DONE, it means the push was successful and, additionally,
     * the buffer has reached its max node count, so the transaction is ready
     * to be executed. We set the log_entry to null to indicate that no action
     * is to be taken after execution.
     * 
     * However, if we got NGX_ERROR, it means the push failed, most likely
     * because the buffer's size has been overflowed. In that case, we need to
     * execute the transaction in order to empty out the buffer, and then we
     * can add the current log entry again (unshift). So, we set the log_entry
     * field to this log entry, indicating that we want to add it after
     * execution.
     */
    if (rc_push == NGX_DONE) {
        ctx->log_entry = NULL;
    }
    else if (rc_push == NGX_ERROR) {
        ctx->log_entry = log_entry;
    }
    
    task->handler = ngx_http_sqlitelog_thread_insert_n_handler;
    task->event.handler = ngx_http_sqlitelog_thread_completed_handler;
    task->event.data = ctx;
    task->event.log = ngx_cycle->log;
    
    return ngx_thread_task_post(lmcf->tp, task);
}
#endif


/**
 * Handle the current web request with a transaction buffer.
 * 
 * @param   r           the current web request
 * @param   log_entry   values to write to the database
 * @param   pool        a pool for object allocations
 * @return              NGX_OK on success,
 *                      NGX_ERROR if an error occurs
 */
static ngx_int_t
ngx_http_sqlitelog_handle_n(ngx_http_request_t *r, ngx_array_t *log_entry,
    ngx_pool_t *pool)
{
    ngx_int_t                        rc_insert;
    ngx_int_t                        rc_list;
    ngx_int_t                        rc_push;
    ngx_int_t                        rc_unshift;
    ngx_list_t                       list;
    ngx_slab_pool_t                 *shpool;
    ngx_http_sqlitelog_buf_t        *buf;
    ngx_http_sqlitelog_srv_conf_t   *lscf;
    
    lscf = ngx_http_get_module_srv_conf(r, ngx_http_sqlitelog_module);
    buf = lscf->buf;
    shpool = (ngx_slab_pool_t *) buf->shm_zone->shm.addr;
    
    /* 1. Lock */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle n, step 1: lock");
    ngx_shmtx_lock(&shpool->mutex);
    
    /* 
     * Push node to buffer.
     * 
     * If the return code is NGX_OK, it means the node was successfully pushed
     * and we don't need to take any further action; we unlock the mutex and
     * return.
     * 
     * If the return code is NGX_DONE, it means the buffer has reached its max
     * node count and we must execute the transaction.
     * 
     * If the return code is NGX_ERROR, it means the node failed to be pushed
     * due to a size overflow and we have to empty the buffer (i.e. execute the
     * transaction) before trying to insert (unshift) the node again.
     */
    rc_push = ngx_http_sqlitelog_buf_push_locked(buf, log_entry,
                                                 r->connection->log);
    if (rc_push == NGX_OK) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_OK;
    }
    
    /* 2. List */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle n, step 2: list");
    rc_list = ngx_http_sqlitelog_buf_list_locked(buf, pool,
                                                 log_entry->nelts, &list);
    if (rc_list != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: handle n failed to create list after buffer "
                      "push on database \"%V\"", &lscf->db.filename);
        ngx_shmtx_unlock(&shpool->mutex);
        goto failed;
    }
    
    /* 3. Reset */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle n, step 3: reset");
    ngx_http_sqlitelog_buf_timer_reset(buf);
    
    /* 4. Unlock */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle n, step 4: unlock");
    ngx_shmtx_unlock(&shpool->mutex);
    
    /* 5. Insert */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "sqlitelog: handle n, step 5: insert");
    rc_insert = ngx_http_sqlitelog_db_insert_list(&lscf->db, &list,
                                                  r->connection->log);
    if (rc_insert != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "sqlitelog: handle n failed to insert list "
                      "into database \"%V\"", &lscf->db.filename);
        goto failed;
    }
    
    /* Unshift */
    if (rc_push == NGX_ERROR) {
        rc_unshift = ngx_http_sqlitelog_buf_unshift(buf, log_entry,
                                                    r->connection->log);
        if (rc_unshift != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "sqlitelog: handle n failed to unshift node after "
                          "overflow on database \"%V\"", &lscf->db.filename);
            return NGX_ERROR;
        }
    }
    
    return NGX_OK;
    
failed:
    return NGX_ERROR;
}


/**
 * Create a log entry from this request. The returned value is an array of
 * values to be inserted as a row in the database.
 * 
 * @param   r           the current request
 * @param   columns     an array of ngx_http_sqlitelog_col_t
 * @param   pool        a pool in which to allocate the log entry
 * @return              an array of ngx_str_t on success, or
 *                      NULL on failure
 */
static ngx_array_t *
ngx_http_sqlitelog_log_entry(ngx_http_request_t *r, ngx_array_t columns,
    ngx_pool_t *pool)
{
    u_char                    *buf;
    size_t                     buf_size;
    ngx_str_t                 *s;
    ngx_uint_t                 buf_len;
    ngx_uint_t                 i;
    ngx_uint_t                 n;
    ngx_array_t               *row;
    ngx_http_sqlitelog_col_t  *c;
    
    n = columns.nelts;
    
    row = ngx_array_create(pool, n, sizeof(ngx_str_t));
    if (row == NULL) {
        return NULL;
    }
    
    c = columns.elts;
    for (i = 0; i < n; i++) {
        /* Push */
        s = ngx_array_push(row);
        if (s == NULL) {
            return NULL;
        }
        
        /* Get length */
        buf_len = c->op.getlen(r, c->op.index);
        
        /* Empty */
        if (buf_len == 0) {
            s->data = NULL;
            s->len = 0;
        }
        
        /* Get value */
        else {
            buf_size = buf_len + 1;
            buf = ngx_pcalloc(pool, buf_size);
            if (buf == NULL) {
                return NULL;
            }
            c->op.run(r, buf, &c->op);
            s->data = buf;
            if (buf_len > 4096) {
                s->len = 4096; /* Truncate large objects */
            } else {
                s->len = buf_len;
            }
        }
        c++;
    }
    
    return row;
}


/**
 * Perform per-worker initalization tasks (i.e. opening/creating the database
 * file and starting the flush timer, if set).
 * 
 * @param   cycle   the cycle of the current Nginx session
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
static ngx_int_t
ngx_http_sqlitelog_init_worker(ngx_cycle_t *cycle)
{

    int                               rc_init;
    ngx_uint_t                        i;
    ngx_http_core_srv_conf_t        **cscfp;
    ngx_http_core_main_conf_t        *cmcf;
    ngx_http_sqlitelog_srv_conf_t    *lscf;
    ngx_http_sqlitelog_buf_flctx_t   *ctx;
    
    cmcf  = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module);
    cscfp = cmcf->servers.elts;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                   "sqlitelog: init worker");
    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                   "sqlitelog: init worker cmcf->servers.nelts: %d",
                   cmcf->servers.nelts);
    
    /*
     * Iterate through the server configurations and initialize the database
     * connection(s) in each one.
     * https://mailman.nginx.org/pipermail/nginx-devel/2016-October/008882.html
     */
    for (i = 0; i < cmcf->servers.nelts; i++) {
        
        /* Get server configuration */
        lscf = cscfp[i]->ctx->srv_conf[ngx_http_sqlitelog_module.ctx_index];
        if (lscf == NULL || lscf->enabled == 0) {
           
            continue;
        }
        
        /* 
         * Initialize database connection.
         * 
         * In theory, it makes sense to return NGX_ERROR if an error occurs
         * here.
         * 
         * In practice, returning NGX_ERROR causes the worker process to
         * silently exit via exit(2), effectively making the current cycle 1
         * worker short.
         * 
         * To keep things simple, we disable the module for this worker and
         * write a message to error.log.
         */
        rc_init = ngx_http_sqlitelog_db_init(&lscf->db, cycle->log);
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                       "sqlitelog: init worker, rc_init: %d", rc_init);
        if (rc_init != SQLITE_OK) {
            ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                          "sqlitelog: worker process %d failed to initialize "
                          "database \"%V\"", ngx_getpid(), &lscf->db.filename);
            lscf->enabled = 0;
            continue;
        }
        
        /* Flush setup */
        if (lscf->buf && lscf->buf->flush) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                           "sqlitelog: init worker, start flush timer");
            ngx_http_sqlitelog_buf_timer_start(lscf->buf);
            
            ctx = lscf->buf->event->data;
            ctx->db = &lscf->db;
        }
    }
    
    return NGX_OK;
}


/**
 * Perform per-worker exit tasks.
 * 
 * @param   cycle   the cycle of the current Nginx session
 */
static void
ngx_http_sqlitelog_exit_worker(ngx_cycle_t *cycle)
{
    int                              rc_ckpt;
    int                              rc_close;
    int                              rc_insert;
    ngx_int_t                        rc_list;
    ngx_list_t                       list;
    ngx_uint_t                       i;
    ngx_slab_pool_t                 *shpool;
    ngx_http_core_srv_conf_t       **cscfp;
    ngx_http_core_main_conf_t       *cmcf;
    ngx_http_sqlitelog_srv_conf_t   *lscf;
    
    cmcf  = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module);
    cscfp = cmcf->servers.elts;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0, "sqlitelog: exit worker");
    
    /*
     * Loop through each server configuration and:
     * - commit any pending buffer transactions
     * - perform a WAL checkpoint
     * - close the connection
     */
    for (i = 0; i < cmcf->servers.nelts; i++) {
        lscf = cscfp[i]->ctx->srv_conf[ngx_http_sqlitelog_module.ctx_index];
        if (lscf == NULL || lscf->enabled == 0) {
            continue;
        }
        
        /* Buffered transaction */
        if (lscf->db.conn && lscf->buf) {
            /* 1. Lock */
            shpool = (ngx_slab_pool_t *) lscf->buf->shm_zone->shm.addr;
            ngx_shmtx_lock(&shpool->mutex);
            if (ngx_http_sqlitelog_buf_get_len_locked(lscf->buf) == 0) {
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                               "sqlitelog: exit worker, buffer empty");
                ngx_shmtx_unlock(&shpool->mutex);
                goto checkpoint;
            }
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                           "sqlitelog: exit worker, execute buffer "
                           "transaction");
            
            /* 2. List */
            rc_list = ngx_http_sqlitelog_buf_list_locked(lscf->buf, cycle->pool,
                                            lscf->db.fmt->columns.nelts, &list);
            if (rc_list != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                              "sqlitelog: worker process %d failed to create "
                              "list for database \"%V\"",
                              ngx_getpid(), &lscf->db.filename);
                ngx_shmtx_unlock(&shpool->mutex);
                goto checkpoint;
            }
            
            /* 3. Reset */
            if (lscf->buf->flush) {
                ngx_http_sqlitelog_buf_timer_stop(lscf->buf);
            }
            
            /* 4. Unlock */
            ngx_shmtx_unlock(&shpool->mutex);
            
            /* 5. Insert */
            rc_insert = ngx_http_sqlitelog_db_insert_list(&lscf->db, &list,
                                                          cycle->log);
            if (rc_insert != SQLITE_OK) {
                ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                              "sqlitelog: worker process %d failed to execute "
                              "buffered transaction on database \"%V\"",
                              ngx_getpid(), &lscf->db.filename);
            }
        }
        
        /* Checkpoint */
checkpoint:
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                       "sqlitelog: exit worker, checkpoint");
        rc_ckpt = ngx_http_sqlitelog_db_checkpoint(&lscf->db, cycle->log);
        if (rc_ckpt != SQLITE_OK) {
            ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                          "sqlitelog: worker process %d failed to execute WAL "
                          "checkpoint on database \"%V\"",
                          ngx_getpid(), &lscf->db.filename);
        }
        
        /* Close */
        if (lscf->db.conn) {
            rc_close = ngx_http_sqlitelog_db_close(&lscf->db, cycle->log);
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                           "sqlitelog: exit worker, rc_close: %d", rc_close);
            if (rc_close != SQLITE_OK) {
                ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                              "sqlitelog: worker process %d failed to close "
                              "database connection on \"%V\"",
                              ngx_getpid(), &lscf->db.filename);
            }
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                           "sqlitelog: exit worker, db conn: %p",
                           lscf->db.conn);
        }
    }
}


/**
 * Perform master exit tasks.
 * 
 * @param   cycle   the cycle of the current Nginx session
 */
static void
ngx_http_sqlitelog_exit_master(ngx_cycle_t *cycle)
{
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                   "sqlitelog: sqlite3 %d", SQLITE_VERSION);
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0, "sqlitelog: exit master");
}


/**
 * Set up a server configuration from the sqlitelog directive.
 * 
 * @param   cf      the current line of the config file
 * @param   cmd     a pointer to the directive object
 * @param   conf    this module's custom configuration struct
 * @return          NGX_CONF_OK on success,
 *                  or NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sqlitelog_srv_conf_t *lscf = conf;
    
    int                              rc_test;
    ssize_t                          size;
    ngx_int_t                        max;
    ngx_str_t                        path;
    ngx_str_t                       *value;
    ngx_msec_t                       flush;
    ngx_uint_t                       i;
    ngx_shm_zone_t                  *shm_zone;
    ngx_http_sqlitelog_buf_t        *buf;
    ngx_http_sqlitelog_fmt_t        *cmb;
    ngx_http_sqlitelog_buf_flctx_t  *ctx;
    ngx_http_sqlitelog_main_conf_t  *lmcf;
    
    lmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_sqlitelog_module);
    cmb = lmcf->formats.elts;
    size = 0;
    max = 0;
    flush = 0;
    
    /* Duplicate check */
    if (lscf->db.filename.data != NULL) {
        return "is duplicate";
    }
    
    /* "off" check */
    value = cf->args->elts;
    if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        lscf->enabled = 0;
        return NGX_OK;
    }
    lscf->enabled = 1;
    
    /* Path */
    path = value[1];
    if (path.len == 0) {
        return "path is empty string";
    }
    if (ngx_conf_full_name(cf->cycle, &path, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    lscf->db.filename = path;
    
    /* Options */
    for (i = 2; i < cf->args->nelts; i++) {
        
        /* buffer=size */
        if (ngx_has_prefix(&value[i], "buffer=")) {
            if (ngx_http_sqlitelog_opt_buffer(cf, value[i], &size)
                != NGX_CONF_OK) {
                return NGX_CONF_ERROR;
            }
        }
        
        /* max=n */
        else if (ngx_has_prefix(&value[i], "max=")) {
            if (ngx_http_sqlitelog_opt_max(cf, value[i], &max) != NGX_CONF_OK) {
                return NGX_CONF_ERROR;
            }
        }
        
        /* flush=time */
        else if (ngx_has_prefix(&value[i], "flush=")) {
            if (ngx_http_sqlitelog_opt_flush(cf, value[i], &flush)
                != NGX_CONF_OK)
            {
                return NGX_CONF_ERROR;
            }
        }
        
        /* init=script */
        else if (ngx_has_prefix(&value[i], "init=")) {
            if (ngx_http_sqlitelog_opt_init(cf, value[i]) != NGX_CONF_OK) {
                return NGX_CONF_ERROR;
            }
        }
        
        /* if=condition */
        else if (ngx_has_prefix(&value[i], "if=")) {
            if (ngx_http_sqlitelog_opt_if(cf, value[i]) != NGX_CONF_OK) {
                return NGX_CONF_ERROR;
            }
        }
        
        /* If none of the above, it must be a format name */
        else {
            if (ngx_http_sqlitelog_opt_format(cf, value[i]) != NGX_CONF_OK) {
                return NGX_CONF_ERROR;
            }
        }
    }
    
    /* If no format specified, default to combined */
    if (lscf->db.fmt == NULL || lscf->db.fmt == cmb) {
        lscf->db.fmt = cmb;
        if (lmcf->combined_init == 0) {
            if (ngx_http_sqlitelog_fmt_init_combined(cf, cmb) != NGX_OK)
            {
                return NGX_CONF_ERROR;
            }
        }
        lmcf->combined_init = 1;
    }
    
    /* Test */
    rc_test = ngx_http_sqlitelog_db_test(lscf->db, cf->log);
    if (rc_test != SQLITE_OK) {
        return NGX_CONF_ERROR;
    }
    
    /* Buffer */
    if (size) {
        shm_zone = ngx_http_sqlitelog_shm_zone(cf,size,lscf->db.fmt->name,path);
        if (shm_zone == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                               "failed to create shared memory zone "
                               "for database \"%V\"", &path);
            return NGX_CONF_ERROR;
        }
        shm_zone->init = ngx_http_sqlitelog_init_shm_zone;
        
        buf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sqlitelog_buf_t));
        if (buf == NULL) {
            return NGX_CONF_ERROR;
        }
        buf->shm_zone = shm_zone;
        buf->max = max;
        
        if (flush) {
            buf->flush = flush;
            
            ctx = ngx_palloc(cf->pool,sizeof(ngx_http_sqlitelog_buf_flctx_t));
            if (ctx == NULL) {
                return NGX_CONF_ERROR;
            }
            ctx->buf = buf;
            ctx->db = NULL; /* Set later in worker initialization */
            ctx->tp = &lmcf->tp;
            
            buf->event = ngx_palloc(cf->pool, sizeof(ngx_event_t));
            if (buf->event == NULL) {
                return NGX_CONF_ERROR;
            }
            buf->event->handler = ngx_http_sqlitelog_buf_flush_handler;
            buf->event->log = &cf->cycle->new_log;
            buf->event->cancelable = 1;
            buf->event->data = ctx;
        }
        
        lscf->buf = buf;
    }
    
    return NGX_CONF_OK;
}


/**
 * Parse the format name of the sqlitelog directive.
 * 
 * @param   cf      the current config
 * @param   arg     the format name
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_opt_format(ngx_conf_t *cf, ngx_str_t arg)
{
    ngx_uint_t                       i;
    ngx_http_sqlitelog_fmt_t        *fmt;
    ngx_http_sqlitelog_srv_conf_t   *lscf;
    ngx_http_sqlitelog_main_conf_t  *lmcf;
    
    lscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_sqlitelog_module);
    lmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_sqlitelog_module);
    fmt = lmcf->formats.elts;
    
    /* Empty check */
    if (arg.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "empty format name");
        return NGX_CONF_ERROR;
    }
    
    /* Combined check */
    if (ngx_str_eq_cs(&arg, "combined")) {
        lscf->db.fmt = fmt;
        return NGX_CONF_OK;
    }
    
    /* Find */
    for (i = 0; i < lmcf->formats.nelts; i++) {
        if (ngx_str_eq(&fmt->name, &arg)) {
            lscf->db.fmt = fmt;
            break;
        }
        fmt++;
    }
    
    if (lscf->db.fmt == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unknown format \"%V\"", &arg);
        return NGX_CONF_ERROR;
    }
    return NGX_CONF_OK;
}


/**
 * Parse the buffer=size argument from the sqlitelog directive.
 * 
 * @param   cf      the current config
 * @param   arg     buffer=size
 * @param   sizep   a pointer for storing the parsed value
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_opt_buffer(ngx_conf_t *cf, ngx_str_t arg, ssize_t *sizep)
{
    ssize_t                          min;
    ssize_t                          size;
    ngx_str_t                        s;
    
    /*
     * We enforce a minimum size of 8 pages for the buffer's shared memory zone.
     * Nginx can allocate smaller sizes, but they can't be used for anything
     * useful.
     * https://trac.nginx.org/nginx/ticket/1665
     */
    min = 8 * ngx_pagesize;
    
    s.data = arg.data + ngx_strlen("buffer=");
    s.len = arg.len - ngx_strlen("buffer=");
    
    size = ngx_parse_size(&s);
    
    if (size == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid buffer size \"%V\"", &s);
        return NGX_CONF_ERROR;
    }
    else if (size < min) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "buffer size \"%V\" is too small; "
                           "must be at least 8 pages (%z bytes)", &s, min);
        return NGX_CONF_ERROR;
    }
    
    *sizep = size;
    return NGX_CONF_OK;
}


/**
 * Parse the max=n argument from the sqlitelog directive.
 * 
 * @param   cf      the current config
 * @param   arg     max=n
 * @param   max     a pointer for storing the parsed value
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_opt_max(ngx_conf_t *cf, ngx_str_t arg, ngx_int_t *max)
{
    ngx_int_t  n;
    ngx_str_t  s;
    
    s.data = arg.data + ngx_strlen("max=");
    s.len = arg.len - ngx_strlen("max=");
    
    n = ngx_atoi(s.data, s.len);
    if (n == NGX_ERROR || n <= 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid max capacity \"%V\"", &s);
        return NGX_CONF_ERROR;
    }
    
    *max = n;
    return NGX_CONF_OK;
}


/**
 * Parse the flush=time argument from the sqlitelog directive.
 * 
 * @param   cf      the current config
 * @param   arg     flush=time
 * @param   flush   a pointer for storing the parsed value
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_opt_flush(ngx_conf_t *cf, ngx_str_t arg, ngx_msec_t *flush)
{
    ngx_str_t   s;
    ngx_msec_t  t;
    
    s.data = arg.data + ngx_strlen("flush=");
    s.len = arg.len - ngx_strlen("flush=");
    
    t = ngx_parse_time(&s, 0);
    
    if (t == (ngx_msec_t) NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid flush timer duration \"%V\"", &s);
        return NGX_CONF_ERROR;
    }
    else if (t < 1000) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "flush timer duration \"%V\" is too short; "
                           "must be at least 1s", &s);
        return NGX_CONF_ERROR;
    }
    
    *flush = t;
    return NGX_CONF_OK;
}


/**
 * Read the SQL init script from the sqlitelog directive.
 * 
 * @param   cf      the current config
 * @param   arg     init=script
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_opt_init(ngx_conf_t *cf, ngx_str_t arg)
{
    ngx_str_t                        s;
    ngx_str_t                        sql;
    ngx_http_sqlitelog_srv_conf_t   *lscf;
    
    lscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_sqlitelog_module);
    
    s.data = arg.data + ngx_strlen("init=");
    s.len = arg.len - ngx_strlen("init=");
    
    if (ngx_conf_full_name(cf->cycle, &s, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    
    sql = ngx_http_sqlitelog_file_read(s, cf->pool, cf->log);
    if (sql.data == NULL) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "failed to read contents of \"%V\"", &s);
        return NGX_CONF_ERROR;
    }
    
    lscf->db.init_sql = sql;
    return NGX_CONF_OK;
}


/**
 * Set the logging condition from the sqlitelog directive.
 * 
 * @param   cf      the current config
 * @param   arg     if=condition
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_opt_if(ngx_conf_t *cf, ngx_str_t arg)
{
    ngx_str_t                           s;
    ngx_http_sqlitelog_srv_conf_t      *lscf;
    ngx_http_compile_complex_value_t    ccv;
    
    lscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_sqlitelog_module);
    
    s.data = arg.data + ngx_strlen("if=");
    s.len = arg.len - ngx_strlen("if=");

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &s;
    
    ccv.complex_value = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (ccv.complex_value == NULL) {
        return NGX_CONF_ERROR;
    }
    
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "failed to compile condition \"%V\"", &s);
        return NGX_CONF_ERROR;
    }

    lscf->filter = ccv.complex_value;
    return NGX_CONF_OK;
}


/**
 * Create a shared memory zone of the given size.
 * 
 * @param   cf          the current configuration
 * @param   size        the size of the zone
 * @param   format      the format name
 * @param   filename    the database's filename
 * @return              a shared memory zone (ngx_shm_zone_t), or
 *                      NULL on failure
 */
ngx_shm_zone_t *
ngx_http_sqlitelog_shm_zone(ngx_conf_t *cf, ssize_t size, ngx_str_t format,
    ngx_str_t filename)
{
    u_char          *buf;
    void            *tag;
    ngx_str_t        name;
    ngx_uint_t       name_len;
    ngx_shm_zone_t  *shm_zone;
    
    /* Name: sqlitelog_<format>_<path> */
    name_len = 0;
    name_len += ngx_strlen("sqlitelog_");
    name_len += format.len;
    name_len += ngx_strlen("_");
    name_len += filename.len;
    buf = ngx_pcalloc(cf->pool, name_len * sizeof(u_char));
    if (buf == NULL) {
        return NULL;
    }
    ngx_sprintf(buf, "sqlitelog_%V_%V", &format, &filename);
    name.data = buf;
    name.len = name_len;
     
    /* Create */
    tag = &ngx_http_sqlitelog_module;
    shm_zone = ngx_shared_memory_add(cf, &name, size, tag);
    if (shm_zone == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "failed to add shared memory zone \"%V\"", &name);
        return NULL;
    }
    return shm_zone;
}


/**
 * Create a new log format from the sqlitelog_format directive.
 * 
 * @param   cf      the current line of the config file
 * @param   cmd     a pointer to the directive object
 * @param   conf    this module's custom configuration struct
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_format(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sqlitelog_main_conf_t *lmcf = conf;
    
    ngx_str_t                  fmt_name;
    ngx_str_t                 *value;
    ngx_uint_t                 i;
    ngx_uint_t                 n;
    ngx_http_sqlitelog_fmt_t  *fmt;
    
    value = cf->args->elts;
    fmt = lmcf->formats.elts;
    fmt_name = value[1];
    n = 0;
    
    /* Position check */
    if (lmcf->combined_init) {
        return "must come before \"sqlitelog\" directive";
    }
    
    /* Empty check */
    for (i = 0; i < cf->args->nelts; i++) {
        if (value->len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "empty argument at position %d", i);
            return NGX_CONF_ERROR;
        }
        value++;
    }
    
    /* Combined check */
    if (ngx_str_eq_cs(&fmt_name, "combined")) {
        return "format name is reserved";
    }
    
    /* Duplicate check */
    for (i = 0; i < lmcf->formats.nelts; i++) {
        if (ngx_str_eq(&fmt_name, &fmt->name)) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "format \"%V\" is duplicate", &fmt_name);
            return NGX_CONF_ERROR;
        }
        fmt++;
    }
    
    /* Variable check */
    value = cf->args->elts;
    for (i = 0; i < cf->args->nelts; i++) {
        if (i >= 2 && value->len >= 1 && value->data[0] == '$') {
            n++;
        }
        value++;
    }
    if (n == 0) {
        return "doesn't contain any variables";
    }
    
    /* Dangling type check */
    value = cf->args->elts;
    for (i = 0; i < cf->args->nelts; i++) {
        if (i >= 2 && value->data[0] != '$') {
            value--;
            if (value->data[0] != '$') {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "column type \"%V\" doesn't follow a "
                                   "variable", ++value);
                return NGX_CONF_ERROR;
            }
            value++;
        }
        value++;
    }
    
    /* Push and initialize new format */
    fmt = ngx_array_push(&lmcf->formats);
    if (fmt == NULL) {
        return NGX_CONF_ERROR;
    }
    else if (ngx_http_sqlitelog_fmt_init(cf, cf->args, fmt) != NGX_OK) {
        return NGX_CONF_ERROR;
    }
    
    return NGX_CONF_OK;
}


/**
 * Set the thread pool name from the sqlitelog_async directive.
 * 
 * @param   cf      the current line of the config file
 * @param   cmd     a pointer to the directive object
 * @param   conf    this module's custom configuration struct
 * @return          NGX_CONF_OK on success,
 *                  or NGX_CONF_ERROR on failure
 */
#if (NGX_THREADS)
static char *
ngx_http_sqlitelog_async(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_sqlitelog_main_conf_t *lmcf = conf;
    
    ngx_str_t   arg;
    ngx_str_t   default_pool_name;
    ngx_str_t  *value;
    
    value = cf->args->elts; /* typecast from void* */
    arg = value[1];
    ngx_str_set(&default_pool_name, "default");
    
    /* Duplicate check */
    if (lmcf->tp) {
        return "is duplicate";
    }
    
    /* on */
    if (ngx_str_eq_cs(&arg, "on")) {
        lmcf->tp = ngx_thread_pool_add(cf, &default_pool_name);
        if (lmcf->tp == NULL) {
            return NGX_CONF_ERROR;
        }
    }
    
    /* off */
    else if (ngx_str_eq_cs(&arg, "off")) {
        /* void */
    }
    
    /* Pool name */
    else {
        lmcf->tp = ngx_thread_pool_get(cf->cycle, &arg);
        if (lmcf->tp == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "unknown thread pool \"%V\"", &arg);
            return NGX_CONF_ERROR;
        }
    }
    
    return NGX_CONF_OK;
}
#endif


/**
 * Create a server configuration.
 * 
 * @param   cf      the current Nginx configuration file
 */
static void *
ngx_http_sqlitelog_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_sqlitelog_srv_conf_t  *lscf;
    
    lscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sqlitelog_srv_conf_t));
    if (lscf == NULL) {
        return NULL;
    }
    
    /*
     * set by ngx_pcalloc():
     *      lscf->db.conn       = NULL;
     *      lscf->db.filename   = { NULL, 0 };
     *      lscf->db.fmt        = NULL;
     *      lscf->db.init_sql   = { NULL, 0 };
     *      lscf->buffer        = NULL;
     *      lscf->filter        = NULL;
     */
    
    lscf->enabled = NGX_CONF_UNSET;
    
    return lscf;
}


/**
 * Create this module's main configuration.
 * 
 * @param   cf  the current Nginx configuration file
 */
static void *
ngx_http_sqlitelog_create_main_conf(ngx_conf_t *cf)
{
    ngx_int_t                        init;
    ngx_http_sqlitelog_fmt_t        *cmb;
    ngx_http_sqlitelog_main_conf_t  *lmcf;
    
    lmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sqlitelog_main_conf_t));
    if (lmcf == NULL) {
        return NULL;
    }
    
    /*
     * set by ngx_pcalloc():
     *      lmcf->formats       = NULL;
     *      lmcf->combined_init = 0;
     *      lmcf->tp            = NULL;
     */
    
    /* Initialize formats array */
    init = ngx_array_init(&lmcf->formats, cf->pool, 1,
                          sizeof(ngx_http_sqlitelog_fmt_t));
    if (init != NGX_OK) {
        return NULL;
    }
    
    /*
     * Following the same pattern used in the log module, we push an empty
     * format named "combined" to the list of available formats, but we hold off
     * on compiling/initializing it unless a database actually uses it (i.e.
     * combined_used is set to 1).
     * If so, the format is compiled/initialized in ngx_http_sqlitelog_init().
     */
    cmb = ngx_array_push(&lmcf->formats);
    if (cmb == NULL) {
        return NULL;
    }
    ngx_str_set(&cmb->name, "combined");
    
    return lmcf;
}


/**
 * Inherit values from the parent server or http block if necessary.
 * 
 * @param   cf      the current Nginx configuration
 * @param   parent  the srv_conf_t from the parent "http" block
 * @param   child   the srv_conf_t from the current "server" block
 * @return          NGX_CONF_OK on success, or
 *                  NGX_CONF_ERROR on failure
 */
static char *
ngx_http_sqlitelog_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_sqlitelog_srv_conf_t *prev = parent;
    ngx_http_sqlitelog_srv_conf_t *conf = child;
    
    /* Parent block has "sqlitelog off", so disable current */
    if (prev->enabled == 0) {
        conf->enabled = 0;
    }
    
    /* Current block has "sqlitelog off" */
    else if (conf->enabled == 0) {
        /* void */
    }
    
    /* Current block has nothing, but parent has "sqlitelog", so inherit */
    else if (prev->enabled == 1 && conf->enabled == NGX_CONF_UNSET) {
        conf->enabled     = prev->enabled;
        conf->db.filename = prev->db.filename;
        conf->db.fmt      = prev->db.fmt;
        conf->db.init_sql = prev->db.init_sql;
        conf->buf         = prev->buf;
        conf->filter      = prev->filter;
    }
    
    /* Current block has nothing, and parent block has nothing, so disable */
    else if (prev->enabled == NGX_CONF_UNSET && conf->enabled == NGX_CONF_UNSET)
    {
        conf->enabled = 0;
    }
    
    return NGX_CONF_OK;
}


/**
 * Initialize the shared memory zone for the transaction buffer.
 * 
 * @param   shm_zone    the shared memory zone to initialize
 * @param   old_data    the data of the previous cycle's shared memory zone
 * @return              NGX_OK on success, or
 *                      NGX_ERROR on failure
 */    
static ngx_int_t
ngx_http_sqlitelog_init_shm_zone(ngx_shm_zone_t *shm_zone, void *old_data)
{
    ngx_slab_pool_t                 *shpool;
    ngx_http_sqlitelog_buf_shctx_t  *ctx;
    
    shpool = (ngx_slab_pool_t*) shm_zone->shm.addr;
    
    /* Reuse shared context from last cycle, if any */
    if (old_data) {
        shm_zone->data = old_data;
        return NGX_OK;
    }
    
    /* Shared context */
    ctx = ngx_slab_calloc(shpool, sizeof(ngx_http_sqlitelog_buf_shctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }
    ngx_queue_init(&ctx->queue);
    
    shm_zone->data = ctx;
    return NGX_OK;
}
