
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>


ngx_str_t ngx_http_sqlitelog_file_read(ngx_str_t filename, ngx_pool_t *pool,
    ngx_log_t *log);
ngx_int_t ngx_http_sqlitelog_file_chown(ngx_str_t filename, ngx_conf_t *cf);
