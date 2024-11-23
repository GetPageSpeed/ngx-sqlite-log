
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>
#include "ngx_http_sqlitelog_op.h"


/*
 * ngx_http_sqlitelog_var_t associates a variable with specialized getlen
 * and run functions.
 * 
 * The vast majority of Nginx variables use the generic functions
 * ngx_http_sqlitelog_op_getlen and ngx_http_sqlitelog_op_run for evaluating
 * lengths and values at runtime. However, these 10 variables in particular have
 * specialized versions of these functions which are optimized for logging.
 * 
 * This is roughly equivalent to the log module's ngx_http_log_var_t, but
 * instead of having a len field (size_t) which holds the variable's fixed
 * length, we define a special getlen function which returns that len.
 * 
 * name     the variable's name, without '$'
 * getlen   a pointer to a function that gets the variable's value's length
 * run      a pointer to a function that gets the variable's value
 */
typedef struct {
    ngx_str_t                        name;
    ngx_http_sqlitelog_op_getlen_pt  getlen;
    ngx_http_sqlitelog_op_run_pt     run;
} ngx_http_sqlitelog_var_t;


ngx_http_sqlitelog_var_t  ngx_http_sqlitelog_vars[] = {
    {
        ngx_string("binary_remote_addr"),
        ngx_http_sqlitelog_op_getlen_unescaped,
        ngx_http_sqlitelog_op_run_unescaped
    },
    {
        ngx_string("pipe"),
        ngx_http_sqlitelog_op_getlen_pipe,
        ngx_http_sqlitelog_op_run_pipe
    },
    {
        ngx_string("time_local"),
        ngx_http_sqlitelog_op_getlen_time_local,
        ngx_http_sqlitelog_op_run_time_local
    },
    {
        ngx_string("time_iso8601"),
        ngx_http_sqlitelog_op_getlen_time_iso8601,
        ngx_http_sqlitelog_op_run_time_iso8601
    },
    {
        ngx_string("msec"),
        ngx_http_sqlitelog_op_getlen,
        ngx_http_sqlitelog_op_run_msec
    },
    {
        ngx_string("request_time"),
        ngx_http_sqlitelog_op_getlen,
        ngx_http_sqlitelog_op_run_request_time
    },
    {
        ngx_string("status"),
        ngx_http_sqlitelog_op_getlen_status,
        ngx_http_sqlitelog_op_run_status
    },
    {
        ngx_string("bytes_sent"),
        ngx_http_sqlitelog_op_getlen,
        ngx_http_sqlitelog_op_run_bytes_sent
    },
    {
        ngx_string("body_bytes_sent"),
        ngx_http_sqlitelog_op_getlen,
        ngx_http_sqlitelog_op_run_body_bytes_sent
    },
    {
        ngx_string("request_length"),
        ngx_http_sqlitelog_op_getlen,
        ngx_http_sqlitelog_op_run_request_length
    }
};
