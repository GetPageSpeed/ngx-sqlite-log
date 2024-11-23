
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>
#include <ngx_http.h>


#include "ngx_http_sqlitelog_fmt.h"


typedef struct ngx_http_sqlitelog_op_s ngx_http_sqlitelog_op_t;

/* Compute buffer size for variable's value during request */
typedef size_t (*ngx_http_sqlitelog_op_getlen_pt) (ngx_http_request_t *r,
    ngx_uint_t index);

/* Get variable's value during request */
typedef u_char *(*ngx_http_sqlitelog_op_run_pt) (ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);

/*
 * ngx_http_sqlitelog_op_t represents a variable in a log format and the
 * functions for getting its string value at runtime.
 * It's roughly equivalent to the log module's ngx_http_log_op_t, with two
 * major differences.
 * 
 *  1. The original struct has a len field, which holds a hardcoded maximum
 *     length for the variable in question. For exmaple, $pipe, whose value is
 *     always a single character (either '.' or 'p'), has a len of 1, while
 *     $body_bytes_sent has a len of NGX_OFF_T_LEN.
 *     In this module, we want to allocate our string buffers to exact lengths,
 *     not maximum lengths, so this field serves no purpose and is removed.
 *  2. The original struct has a final field called data, of type uintptr_t,
 *     which is supposed to be a generic field that holds one of two things:
 *       a) the variable's index, if the operation corresponds to a variable, or
 *       b) a string, if the operation corresponds to a piece of text that gets
 *          printed somewhere in the log line (for example, in combined format,
 *          the square brackets around $time_local)
 *     The "log formats" in this module are just SQLite tables, so there is no
 *     intermixing of text and variables going on. data is always an index for
 *     us, therefore, the field has been renamed to index (and its type changed
 *     to ngx_int_t) for clarity.
 * 
 * getlen   a pointer to a function that gets the variable's value's length
 * run      a pointer to a function that gets the variable's value
 * index    the variable's index
 */
struct ngx_http_sqlitelog_op_s {
    ngx_http_sqlitelog_op_getlen_pt  getlen;
    ngx_http_sqlitelog_op_run_pt     run;
    ngx_int_t                        index;
};


ngx_int_t ngx_http_sqlitelog_op_compile(ngx_conf_t *cf,
    ngx_http_sqlitelog_op_t *op, ngx_str_t *name, ngx_str_t *type);

/* Getlen */
size_t ngx_http_sqlitelog_op_getlen(ngx_http_request_t *r,
    ngx_uint_t index);
size_t ngx_http_sqlitelog_op_getlen_unescaped(ngx_http_request_t *r,
    ngx_uint_t index);
size_t ngx_http_sqlitelog_op_getlen_pipe(ngx_http_request_t *r,
    ngx_uint_t index);
size_t ngx_http_sqlitelog_op_getlen_time_local(ngx_http_request_t *r,
    ngx_uint_t index);
size_t ngx_http_sqlitelog_op_getlen_time_iso8601(ngx_http_request_t *r,
    ngx_uint_t index);
size_t ngx_http_sqlitelog_op_getlen_status(ngx_http_request_t *r,
    ngx_uint_t index);

/* Run */
u_char *ngx_http_sqlitelog_op_run(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_unescaped(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_pipe(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_time_local(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_time_iso8601(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_msec(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_request_time(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_status(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_bytes_sent(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_body_bytes_sent(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
u_char *ngx_http_sqlitelog_op_run_request_length(ngx_http_request_t *r,
    u_char *buf, ngx_http_sqlitelog_op_t *op);
