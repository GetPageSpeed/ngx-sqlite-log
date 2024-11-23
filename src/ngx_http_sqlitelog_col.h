
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>
#include <sqlite3.h>


#include "ngx_http_sqlitelog_op.h"


/* Bind function */
typedef int (*ngx_http_sqlitelog_col_bind_pt) (sqlite3 *db, sqlite3_stmt* stmt,
    int position, ngx_str_t val, sqlite3_destructor_type val_destructor,
    ngx_log_t *log);


/*
 * ngx_http_sqlitelog_col_t represents a column in the database's logging
 * table.
 * 
 * name     the column name
 * type     the column type
 * op       the operation for this column's variable
 * bind     a pointer to a function for binding a value to this column
 */
typedef struct {
    ngx_str_t                        name;
    ngx_str_t                        type;
    ngx_http_sqlitelog_op_t          op;
    ngx_http_sqlitelog_col_bind_pt   bind;
} ngx_http_sqlitelog_col_t;

ngx_int_t ngx_http_sqlitelog_col_init(ngx_http_sqlitelog_col_t *col,
    ngx_conf_t *cf, ngx_str_t name, ngx_str_t type);
