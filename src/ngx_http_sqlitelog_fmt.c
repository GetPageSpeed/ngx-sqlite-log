
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>
#include <ngx_http.h>


#include "ngx_http_sqlitelog_col.h"
#include "ngx_http_sqlitelog_fmt.h"
#include "ngx_http_sqlitelog_sql.h"
#include "ngx_http_sqlitelog_util.h"


static ngx_str_t ngx_http_sqlitelog_fmt_col_type(ngx_str_t col_name);


static char * ngx_http_sqlitelog_fmt_col_types[] = {
    "binary_remote_addr",           "BLOB",
    "body_bytes_sent",              "INTEGER",
    "bytes_sent",                   "INTEGER",
    "connection",                   "INTEGER",
    "connection_requests",          "INTEGER",
    "connection_time",              "REAL",
    "connections_active",           "INTEGER",      /* stub_status module*/
    "connections_reading",          "INTEGER",      /* stub_status module*/
    "connections_waiting",          "INTEGER",      /* stub_status module*/
    "connections_writing",          "INTEGER",      /* stub_status module*/
    "content_length",               "INTEGER",
    "gzip_ratio",                   "REAL",         /* gzip module */
    "limit_rate",                   "INTEGER",
    "msec",                         "REAL",
    "pid",                          "INTEGER",
    "proxy_port",                   "INTEGER",      /* proxy module */
    "proxy_protocol_port",          "INTEGER",
    "proxy_protocol_server_port",   "INTEGER",
    "remote_port",                  "INTEGER",
    "request_time",                 "REAL",
    "server_port",                  "INTEGER",
    "status",                       "INTEGER",
    NULL,                           NULL
};


/**
 * Initialize a format with the given arguments.
 * 
 * @param   cf      the current Nginx configuration
 * @param   args    the current sqlitelog_format line in the config file
 * @param   fmt     a pointer to a format to be initialized
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_fmt_init(ngx_conf_t *cf, ngx_array_t *args,
    ngx_http_sqlitelog_fmt_t *fmt)
{
    ngx_str_t                 col_name;
    ngx_str_t                 col_type;
    ngx_str_t                 table_name;
    ngx_str_t                *next;
    ngx_str_t                 sql_create;
    ngx_str_t                 sql_insert;
    ngx_str_t                *value;
    ngx_uint_t                i;
    ngx_uint_t                j;
    ngx_uint_t                n;
    ngx_array_t               columns;
    ngx_http_sqlitelog_col_t *col;
    
    value = args->elts;
    next = value + 1;
    
    /* Columns array */
    n = ngx_http_sqlitelog_fmt_n_variables(args);
    if (ngx_array_init(&columns, cf->pool, n, sizeof(ngx_http_sqlitelog_col_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }
    
    for (i = 0; i < args->nelts; i++) {
        
        /* 1st argument is directive name */
        if (i == 0) {
            /* void */
        }
        
        /* 2nd argument is table name */
        else if (i == 1) {
            table_name.data = value->data;
            table_name.len = value->len;
        }
        
        else {
            /* Column */
            if (value->data[0] == '$') {
                /* Push */
                col = ngx_array_push(&columns);
                if (col == NULL) {
                    return NGX_ERROR;
                }
                
                /* Column name */
                col_name.data = value->data + 1;
                col_name.len = value->len - 1;
                
                /* Column type */
                if (i < args->nelts-1 && next->len >= 1 && next->data[0] != '$')
                {
                    col_type.data = next->data;
                    col_type.len = next->len;
                    for (j = 0; j < col_type.len; j++) {
                        col_type.data[j] = ngx_toupper(col_type.data[j]);
                    }
                }
                else {
                    col_type = ngx_http_sqlitelog_fmt_col_type(col_name);
                    if (col_type.data == NULL) {
                        ngx_str_set(&col_type, "TEXT");
                    }
                }
                
                /* Init */
                if (ngx_http_sqlitelog_col_init(col, cf, col_name, col_type)
                    != NGX_OK)
                {
                    ngx_log_error(NGX_LOG_ERR, cf->log, 0,
                                  "sqlitelog: failed to initialize column for "
                                  "variable \"%V\"", value);
                    return NGX_ERROR;
                }
            }
        }
        
        value++;
        next++;
    }
    
    /* CREATE TABLE IF NOT EXISTS table (...) */
    sql_create = ngx_http_sqlitelog_sql_create_table(table_name, columns, 
                                                     cf->pool);
    if (sql_create.data == NULL || sql_create.len == 0) {
        return NGX_ERROR;
    }
    
    /* INSERT INTO table VALUES (?,?,?) */
    sql_insert = ngx_http_sqlitelog_sql_insert(table_name, n, cf->pool);
    if (sql_insert.data == NULL || sql_insert.len == 0) {
        return NGX_ERROR;
    }
    
    /* Finalize */
    fmt->name = table_name;
    fmt->columns = columns;
    fmt->sql_create = sql_create;
    fmt->sql_insert = sql_insert;
    
    return NGX_OK;
}


/**
 * Initialize combined format.
 * 
 * @param   cf      the current Nginx configuration
 * @param   fmt     a pointer to a format to be initialized
 * @return          NGX_OK on success, or
 *                  NGX_ERROR on failure
 */
ngx_int_t
ngx_http_sqlitelog_fmt_init_combined(ngx_conf_t *cf,
    ngx_http_sqlitelog_fmt_t *fmt)
{
    ngx_str_t    *a;
    ngx_uint_t    i;
    ngx_uint_t    n;
    ngx_array_t   args;
    
    const char * combined_format_elts[] = {
        "",
        "combined",
        "$remote_addr",
        "$remote_user",
        "$time_local",
        "$request",
        "$status",
        "$body_bytes_sent",
        "$http_referer",
        "$http_user_agent"
    };
    
    n = 10;
    
    if (ngx_array_init(&args, cf->pool, n, sizeof(ngx_str_t)) != NGX_OK) {
        return NGX_ERROR;
    }
    
    for (i = 0; i < n; i++) {
        a = ngx_array_push(&args);
        if (a == NULL) {
            return NGX_ERROR;
        }
        a->data = (u_char *) combined_format_elts[i];
        a->len = ngx_strlen(combined_format_elts[i]);
    }
    
    return ngx_http_sqlitelog_fmt_init(cf, &args, fmt);
}


/**
 * Get this column's hardcoded column type if available.
 *
 * @param   col_name    the column's name
 * @return              a type name, or a string with NULL data
 */
static ngx_str_t
ngx_http_sqlitelog_fmt_col_type(ngx_str_t col_name)
{
    u_char     *key;
    u_char     *val;
    ngx_int_t   i;
    ngx_int_t   j;
    ngx_str_t   col_type;

    ngx_str_null(&col_type);
    
    i = 0;
    j = 1;
    
    for ( ;; ) {
        
        key = (u_char *) ngx_http_sqlitelog_fmt_col_types[i];
        val = (u_char *) ngx_http_sqlitelog_fmt_col_types[j];
        
        if (key == NULL) {
            break;
        }
        else if (ngx_str_eq_cs(&col_name, key)) {
            col_type.data = val;
            col_type.len = ngx_strlen(val);
            break;
        }
        
        i += 2;
        j += 2;
    }
    
    return col_type;
}


/**
 * Count how many variables are passed to the sqlitelog_format directive.
 * 
 * @param   args    an array of ngx_str_t
 * @return          the amount of arguments that begin with '$'
 */
ngx_uint_t
ngx_http_sqlitelog_fmt_n_variables(ngx_array_t *args)
{
    ngx_str_t   *arg;
    ngx_uint_t   i;
    ngx_uint_t   n;
    
    arg = args->elts;
    n = 0;
    
    for (i = 0; i < args->nelts; i++) {
        if (i >= 2 && arg->len >= 1 && arg->data[0] == '$') {
            n++;
        }
        arg++;
    }
    
    return n;
}
