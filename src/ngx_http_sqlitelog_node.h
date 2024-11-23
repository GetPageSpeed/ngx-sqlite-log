
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>
#include <ngx_http.h>


/*
 * ngx_http_sqlitelog_node_t represents a node on the buffer's transaction
 * queue.
 * 
 * elts     a C-style array of strings
 * nelts    the length of elts
 * link     the queue that this node is currently on
 */
typedef struct {
    ngx_str_t    *elts;
    ngx_uint_t    nelts;
    ngx_queue_t   link;
} ngx_http_sqlitelog_node_t;

ngx_http_sqlitelog_node_t *ngx_http_sqlitelog_node_create_locked(
    ngx_array_t *values, ngx_slab_pool_t *shpool, ngx_log_t *log);

void ngx_http_sqlitelog_node_destroy_locked(ngx_http_sqlitelog_node_t *node,
    ngx_slab_pool_t *shpool);
