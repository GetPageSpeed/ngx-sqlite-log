
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>
#include <ngx_http.h>


#include "ngx_http_sqlitelog_col.h"
#include "ngx_http_sqlitelog_op.h"
#include "ngx_http_sqlitelog_util.h"
#include "ngx_http_sqlitelog_var.h"


static uintptr_t ngx_http_sqlitelog_op_escape(u_char *dst, u_char *src,
    size_t size);


/**
 * Initialize an operation object from a variable's column name and type.
 * 
 * This is like the log module's ngx_http_log_variable_compile() function, whose
 * name is somewhat misleading, since it initializes (or "compiles") an object
 * of type ngx_http_log_op_t, not ngx_http_log_var_t.
 * 
 * @param   cf      the current Nginx configuration
 * @param   op      a pointer to an operation object to initialize
 * @param   name    the variable's column name
 * @param   type    the variable's column type
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 * @see             ngx_http_log_module:ngx_http_log_variable_compile
 */
ngx_int_t
ngx_http_sqlitelog_op_compile(ngx_conf_t *cf, ngx_http_sqlitelog_op_t *op,
    ngx_str_t *name, ngx_str_t *type)
{
    ngx_int_t                        index;
    ngx_int_t                        v;
    ngx_http_sqlitelog_var_t         var;
    ngx_http_sqlitelog_op_run_pt     run;
    ngx_http_sqlitelog_op_getlen_pt  getlen;
    
    run = NULL;
    getlen = NULL;
    
    /* Index */
    index = ngx_http_get_variable_index(cf, name);
    if (index == NGX_ERROR) {
        return NGX_ERROR;
    }
    
    /* Special functions */
    for (v = 0; v < 10; v++) {
        var = ngx_http_sqlitelog_vars[v];
        if (ngx_str_eq(name, &var.name)) {
            getlen = var.getlen;
            run = var.run;
            break;
        }
    }
    
    /* BLOB is always unescaped */
    if (ngx_str_eq_cs(type, "BLOB")) {
        getlen = ngx_http_sqlitelog_op_getlen_unescaped;
        run = ngx_http_sqlitelog_op_run_unescaped;
    }
    
    /* Else, use generic functions */
    if (run == NULL && getlen == NULL) {
        getlen = ngx_http_sqlitelog_op_getlen;
        run = ngx_http_sqlitelog_op_run;
    }
    
    op->getlen = getlen;
    op->run = run;
    op->index = index;
    return NGX_OK;
}


/**
 * Evaluate a variable.
 * 
 * This differs from ngx_http_log_variable() in that empty variables are
 * returned as NULL instead of "-".
 * 
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_variable
 */
u_char *
ngx_http_sqlitelog_op_run(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    ngx_http_variable_value_t  *value;

    value = ngx_http_get_indexed_variable(r, op->index);

    if (value == NULL || value->not_found) {
        return NULL;
    }
    
    if (value->escape == 0) {
        return ngx_cpymem(buf, value->data, value->len);

    } else {
        return (u_char *) ngx_http_sqlitelog_op_escape(buf, value->data,
                                                       value->len);
    }
}


/**
 * Evaluate a variable without escaping the string.
 * 
 * This should be used for non-text variables, like $binary_remote_addr.
 * 
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_unescaped_variable
 */
u_char *
ngx_http_sqlitelog_op_run_unescaped(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    ngx_http_variable_value_t  *value;

    value = ngx_http_get_indexed_variable(r, op->index);

    if (value == NULL || value->not_found) {
        return buf;
    }

    return ngx_cpymem(buf, value->data, value->len);
}


/**
 * Get the buffer length for this variable's value string.
 * 
 * This is identical to ngx_http_log_variable_getlen() except it returns 0
 * instead of 1 for empty variables.
 * 
 * @param   r       the current request
 * @param   index   the variable's index
 * @return          a buffer size
 * @see             ngx_http_log_module.c:ngx_http_log_variable_getlen
 */
size_t
ngx_http_sqlitelog_op_getlen(ngx_http_request_t *r, ngx_uint_t index)
{
    uintptr_t                   len;
    ngx_http_variable_value_t  *value;

    value = ngx_http_get_indexed_variable(r, index);

    if (value == NULL || value->not_found) {
        return 0;
    }

    len = ngx_http_sqlitelog_op_escape(NULL, value->data, value->len);

    value->escape = len ? 1 : 0;

    return value->len + len * 3;
}


/**
 * Get the buffer length for this variable's value string.
 * 
 * This should be used for non-text variables, like $binary_remote_addr.
 * 
 * @param   r       the current request
 * @param   index   the variable's index
 * @return          a buffer size
 * @see             ngx_http_log_module.c:ngx_http_log_unescaped_variable_getlen
 */
size_t
ngx_http_sqlitelog_op_getlen_unescaped(ngx_http_request_t *r,
    ngx_uint_t index)
{
    ngx_http_variable_value_t  *value;

    value = ngx_http_get_indexed_variable(r, index);

    if (value == NULL || value->not_found) {
        return 0;
    }

    value->escape = 0;

    return value->len;
}


/**
 * Get the buffer length for $pipe.
 * 
 * @param   r       the current request
 * @param   index   the variable's index
 * @return          a buffer size
 * @see             ngx_http_log_module.c:ngx_http_log_vars
 */
size_t
ngx_http_sqlitelog_op_getlen_pipe(ngx_http_request_t *r, ngx_uint_t index)
{
    return 1;
}


/**
 * Get the buffer length for $time_local.
 * 
 * @param   r       the current request
 * @param   index   the variable's index
 * @return          a buffer size
 * @see             ngx_http_log_module.c:ngx_http_log_vars
 */
size_t
ngx_http_sqlitelog_op_getlen_time_local(ngx_http_request_t *r, ngx_uint_t index)
{
    return sizeof("28/Sep/1970:12:00:00 +0600") - 1;
}


/**
 * Get the buffer length for $time_iso8601.
 * 
 * @param   r       the current request
 * @param   index   the variable's index
 * @return          a buffer size
 * @see             ngx_http_log_module.c:ngx_http_log_vars
 */
size_t
ngx_http_sqlitelog_op_getlen_time_iso8601(ngx_http_request_t *r,
    ngx_uint_t index)
{
    return sizeof("1970-09-28T12:00:00+06:00") - 1;
}


/**
 * Get the buffer length for $status.
 * 
 * @param   r       the current request
 * @param   index   the variable's index
 * @return          a buffer size
 * @see             ngx_http_log_module.c:ngx_http_log_vars
 */
size_t
ngx_http_sqlitelog_op_getlen_status(ngx_http_request_t *r, ngx_uint_t index)
{
    return 3;
}


/**
 * Escape a string for writing to a text file.
 * 
 * @param   dst     a buffer to hold the new string
 * @param   src     the string to escape
 * @param   size    the length of src
 * @return          the length of dst
 * @see             ngx_http_log_module.c:ngx_http_log_escape
 */
static uintptr_t
ngx_http_sqlitelog_op_escape(u_char *dst, u_char *src, size_t size)
{
    ngx_uint_t      n;
    u_char   hex[] = "0123456789ABCDEF";

    static uint32_t   escape[] = {
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
        0x00000004, /* 0000 0000 0000 0000  0000 0000 0000 0100 */

                    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
        0x10000000, /* 0001 0000 0000 0000  0000 0000 0000 0000 */

                    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
        0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */

        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
        0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    };


    if (dst == NULL) {

        /* find the number of the characters to be escaped */

        n = 0;

        while (size) {
            if (escape[*src >> 5] & (1U << (*src & 0x1f))) {
                n++;
            }
            src++;
            size--;
        }

        return (uintptr_t) n;
    }

    while (size) {
        if (escape[*src >> 5] & (1U << (*src & 0x1f))) {
            *dst++ = '\\';
            *dst++ = 'x';
            *dst++ = hex[*src >> 4];
            *dst++ = hex[*src & 0xf];
            src++;

        } else {
            *dst++ = *src++;
        }
        size--;
    }

    return (uintptr_t) dst;
}


/**
 * Evaluate $pipe.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_pipe
 */
u_char *
ngx_http_sqlitelog_op_run_pipe(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    if (r->pipeline) {
        *buf = 'p';
    } else {
        *buf = '.';
    }

    return buf + 1;
}


/**
 * Evaluate $time_local.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_time
 */
u_char *
ngx_http_sqlitelog_op_run_time_local(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    return ngx_cpymem(buf, ngx_cached_http_log_time.data,
                      ngx_cached_http_log_time.len);
}


/**
 * Evaluate $time_iso8601.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_iso9601
 */
u_char *
ngx_http_sqlitelog_op_run_time_iso8601(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    return ngx_cpymem(buf, ngx_cached_http_log_iso8601.data,
                      ngx_cached_http_log_iso8601.len);
}


/**
 * Evaluate $msec.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_msec
 */
u_char *
ngx_http_sqlitelog_op_run_msec(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    ngx_time_t  *tp;

    tp = ngx_timeofday();

    return ngx_sprintf(buf, "%T.%03M", tp->sec, tp->msec);
}


/**
 * Evaluate $request_time.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_request_time
 */
u_char *
ngx_http_sqlitelog_op_run_request_time(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    ngx_time_t      *tp;
    ngx_msec_int_t   ms;

    tp = ngx_timeofday();

    ms = (ngx_msec_int_t)
             ((tp->sec - r->start_sec) * 1000 + (tp->msec - r->start_msec));
    ms = ngx_max(ms, 0);

    return ngx_sprintf(buf, "%T.%03M", (time_t) ms / 1000, ms % 1000);
}


/**
 * Evaluate $status.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_status
 */
u_char *
ngx_http_sqlitelog_op_run_status(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    ngx_uint_t  status;

    if (r->err_status) {
        status = r->err_status;

    } else if (r->headers_out.status) {
        status = r->headers_out.status;

    } else if (r->http_version == NGX_HTTP_VERSION_9) {
        status = 9;

    } else {
        status = 0;
    }

    return ngx_sprintf(buf, "%03ui", status);
}


/**
 * Evaluate $bytes_sent.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_bytes_sent
 */
u_char *
ngx_http_sqlitelog_op_run_bytes_sent(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    return ngx_sprintf(buf, "%O", r->connection->sent);
}


/**
 * Evaluate $body_bytes_sent.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_body_bytes_sent
 */
u_char *
ngx_http_sqlitelog_op_run_body_bytes_sent(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    off_t  length;

    length = r->connection->sent - r->header_size;

    if (length > 0) {
        return ngx_sprintf(buf, "%O", length);
    }

    *buf = '0';

    return buf + 1;
}


/**
 * Evaluate $request_length.
 *
 * @param   r       the current request
 * @param   buf     a buffer in which to write the value
 * @param   op      this variable's operation object
 * @return          the buffer, after the value has been printed to it
 * @see             ngx_http_log_module.c:ngx_http_log_request_length
 */
u_char *
ngx_http_sqlitelog_op_run_request_length(ngx_http_request_t *r, u_char *buf,
    ngx_http_sqlitelog_op_t *op)
{
    return ngx_sprintf(buf, "%O", r->request_length);
}
