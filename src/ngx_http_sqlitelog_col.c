
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>


#include "ngx_http_sqlitelog_col.h"
#include "ngx_http_sqlitelog_op.h"
#include "ngx_http_sqlitelog_sqlite3.h"
#include "ngx_http_sqlitelog_util.h"


/**
 * Initialize a column object.
 * 
 * @param   col     a column object to initialized
 * @param   cf      the current Nginx configuration
 * @param   name    the column's name
 * @param   type    the column's type
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_col_init(ngx_http_sqlitelog_col_t *col, ngx_conf_t *cf,
    ngx_str_t name, ngx_str_t type)
{
    ngx_http_sqlitelog_op_t  *op;
    
    /* Name and type */
    col->name.data = name.data;
    col->name.len = name.len;
    col->type.data = type.data;
    col->type.len = type.len;
    
    /* Operation */
    op = ngx_palloc(cf->pool, sizeof(ngx_http_sqlitelog_op_t));
    if (op == NULL) {
        return NGX_ERROR;
    }
    if (ngx_http_sqlitelog_op_compile(cf, op, &name, &type) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "sqlitelog: failed to compile op for column \"%V\"",
                           &name);
        return NGX_ERROR;
    }
    col->op = *op;
    
    /* Bind function */
    if (ngx_str_eq_cs(&type, "BLOB")) {
        col->bind = ngx_http_sqlitelog_sqlite3_bind_blob;
    } else {
        col->bind = ngx_http_sqlitelog_sqlite3_bind_text;
    }
    
    return NGX_OK;
}
