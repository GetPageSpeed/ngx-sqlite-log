
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>


/*
 * ngx_http_sqlitelog_fmt_t represents a log format (i.e. the SQLite table
 * where records are stored).
 * 
 * name         the table's name
 * columns      the table's columns
 * sql_create   "CREATE TABLE IF NOT EXISTS name (...)"
 * sql_insert   "INSERT INTO name VALUES (?,?,?)"
 */
typedef struct {
    ngx_str_t    name;
    ngx_array_t  columns;       /* array of ngx_http_sqlitelog_col_t */
    ngx_str_t    sql_create;
    ngx_str_t    sql_insert;
} ngx_http_sqlitelog_fmt_t;


ngx_int_t ngx_http_sqlitelog_fmt_init(ngx_conf_t *cf, ngx_array_t *args,
    ngx_http_sqlitelog_fmt_t *fmt);
ngx_int_t ngx_http_sqlitelog_fmt_init_combined(ngx_conf_t *cf,
    ngx_http_sqlitelog_fmt_t *fmt);
ngx_uint_t ngx_http_sqlitelog_fmt_n_variables(ngx_array_t *args);
