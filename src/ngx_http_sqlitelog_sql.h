
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>


ngx_str_t ngx_http_sqlitelog_sql_create_table(ngx_str_t table_name,
    ngx_array_t columns, ngx_pool_t *pool);
ngx_str_t ngx_http_sqlitelog_sql_insert(ngx_str_t table, ngx_uint_t n,
    ngx_pool_t *pool);
