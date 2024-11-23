
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>
#include <ngx_http.h>


#include "ngx_http_sqlitelog_node.h"


/**
 * Create a new queue node.
 * 
 * @param   values  an array of strings to copy into the node's elts field
 * @param   shpool  a locked slab in which to create the node
 * @param   log     a log for writing error messages
 * @return          a new queue node ready to be appended,
 *                  or NULL if an error occurs
 */
ngx_http_sqlitelog_node_t *
ngx_http_sqlitelog_node_create_locked(ngx_array_t *values,
    ngx_slab_pool_t *shpool, ngx_log_t *log)
{
    size_t                      node_elt_len;
    size_t                      node_elt_size;
    size_t                      node_elts_size;
    size_t                      node_size;
    u_char                     *node_elt_data;
    ngx_str_t                  *node_elts;
    ngx_str_t                   values_elt;
    ngx_str_t                  *values_elts;
    ngx_uint_t                  i;
    ngx_uint_t                  j;
    ngx_http_sqlitelog_node_t  *node;
    
    node = NULL;
    j = 0;                      /* total fields set */
    node_elts = NULL;
    values_elts = values->elts; /* typecast from void* */
    
    /* Create node's elts array */
    node_elts_size = values->nelts * sizeof(ngx_str_t);
    node_elts = ngx_slab_calloc_locked(shpool, node_elts_size);
    if (node_elts == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to allocate %d bytes for node "
                      "elements", node_elts_size);
        goto failed;
    }
    
    for (i = 0; i < values->nelts; i++) {
        /* If null string, skip */
        values_elt = values_elts[i];
        if (values_elt.data == NULL) {
            node_elts[i].data = NULL;
            node_elts[i].len = 0;
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                           "sqlitelog: node elts[%d]: NULL", i);
            j++;
            continue;
        }
        
        /* Allocate space for node elt data */
        node_elt_len = sizeof(u_char) * values_elt.len;
        node_elt_size = node_elt_len;
        node_elt_data = ngx_slab_calloc_locked(shpool, node_elt_size);
        if (node_elt_data == NULL) {
            ngx_log_error(NGX_LOG_ERR, log, 0,
                          "sqlitelog: failed to allocate %d bytes for node "
                          "data \"%V\"", node_elt_size, &values_elt);
            goto failed;
        }
        
        /* Copy data */
        ngx_memcpy(node_elt_data, values_elt.data, values_elt.len);
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0,
                       "sqlitelog: node elts[%d]: \"%V\"", i, &values_elt);
        
        /* Set */
        node_elts[i].data = node_elt_data;
        node_elts[i].len = node_elt_len;
        j++;
    }
    
    /* Create node */
    node_size = sizeof(ngx_http_sqlitelog_node_t);
    node = ngx_slab_alloc_locked(shpool, node_size);
    if (node == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to allocate node structure");
        goto failed;
    }
    
    node->elts = node_elts;
    node->nelts = values->nelts;
    ngx_queue_init(&node->link);
    return node;
    
failed:
    for (i = 0; i < j; i++) {
        if (node_elts[i].data) {
            ngx_slab_free_locked(shpool, node_elts[i].data);
        }
    }
    if (node_elts) {
        ngx_slab_free_locked(shpool, node_elts);
    }
    if (node) {
        ngx_slab_free_locked(shpool, node);
    }
    return NULL;
};


/**
 * Free a node.
 * 
 * @param   node    the node to free
 * @param   shpool  the locked slab in which the node is allocated
 */
void
ngx_http_sqlitelog_node_destroy_locked(ngx_http_sqlitelog_node_t *node,
    ngx_slab_pool_t *shpool)
{
    ngx_str_t  s;
    ngx_uint_t i;
    
    for (i = 0; i < node->nelts; i++) {
        s = node->elts[i];
        if (s.data) {
            ngx_slab_free_locked(shpool, s.data);
        }
    }
    
    ngx_slab_free_locked(shpool, node->elts);
    ngx_slab_free_locked(shpool, node);
};
