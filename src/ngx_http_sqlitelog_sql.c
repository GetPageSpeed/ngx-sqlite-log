
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>


#include "ngx_http_sqlitelog_col.h"
#include "ngx_http_sqlitelog_util.h"


/**
 * Build a string in the form of "CREATE TABLE IF NOT EXISTS table_name (...)".
 * 
 * @param   table_name  the table name
 * @param   columns     an array of columns (ngx_http_sqlitelog_col_t)
 * @param   pool        a pool in which to allocate the string's data
 * @return              a string whose data is allocated in the given pool,
 *                      or a string with NULL data if an error occurs
 */
ngx_str_t
ngx_http_sqlitelog_sql_create_table(ngx_str_t table_name, ngx_array_t columns,
    ngx_pool_t *pool)
{
    size_t                     buf_size;
    size_t                     sql_len;
    u_char                    *buf;
    ngx_str_t                  sql;
    ngx_uint_t                 i;
    ngx_uint_t                 j;
    ngx_http_sqlitelog_col_t  *col;
    
    col = columns.elts; /* typecast from void* */
    
    /* Compute length */
    sql_len = 0;
    sql_len += ngx_strlen("CREATE TABLE IF NOT EXISTS ");
    sql_len += table_name.len;
    sql_len += ngx_strlen(" (");
    for (i = 0; i < columns.nelts; i++) {
        sql_len += col->name.len;
        sql_len += ngx_strlen(" ");
        sql_len += col->type.len;
        if (i < columns.nelts - 1) {
            sql_len += ngx_strlen(", ");
        }
        col++;
    }
    sql_len += ngx_strlen(")");
    
    /* Create buffer */
    buf_size = sql_len + 1;
    buf = ngx_pcalloc(pool, buf_size);
    if (buf == NULL) {
        return NGX_NULL_STRING;
    }
    
    /* Build string */
    ngx_sprintf(buf, "CREATE TABLE IF NOT EXISTS %V (", &table_name);
    j = ngx_strlen(buf);
    col = columns.elts;
    
    for (i = 0; i < columns.nelts; i++) {
        
        ngx_memcpy(&buf[j], col->name.data, col->name.len);
        j += col->name.len;
        buf[j] = ' ';
        j += 1;
        ngx_memcpy(&buf[j], col->type.data, col->type.len);
        j += col->type.len;
        
        if (i == columns.nelts - 1) {
            buf[j] = ')';
            break;
            
        } else {
            ngx_memcpy(&buf[j], ", ", 2);
            j += 2;
        }
        
        col++;
    }
    
    sql.data = buf;
    sql.len = sql_len;
    return sql;
}


/**
 * Build a string in the form of "INSERT INTO table_name VALUES (?,?,?)".
 * 
 * @param   table_name  the name of the table
 * @param   n           the amount of parameter symbols
 * @param   pool        a pool in which to allocate the string's data
 * @return              a string whose data is allocated in the given pool,
 *                      or a string with NULL data if an error occurs
 */
ngx_str_t
ngx_http_sqlitelog_sql_insert(ngx_str_t table_name, ngx_uint_t n,
    ngx_pool_t *pool)
{
    size_t          buf_size;
    size_t          sql_len;
    u_char         *buf;
    ngx_str_t       sql;
    ngx_uint_t      i;
    ngx_uint_t      j;
    
    /* Compute length */
    sql_len = 0;
    sql_len += ngx_strlen("INSERT INTO ");
    sql_len += table_name.len;
    sql_len += ngx_strlen(" VALUES (");
    sql_len += ngx_strlen("?") * n;
    sql_len += ngx_strlen(",") * (n-1);
    sql_len += ngx_strlen(")");
    
    /* Create buffer */
    buf_size = sql_len + 1;
    buf = ngx_pcalloc(pool, buf_size);
    if (buf == NULL) {
        return NGX_NULL_STRING;
    }
    
    /* Build string */
    ngx_sprintf(buf, "INSERT INTO %V VALUES (?", &table_name);
    j = ngx_strlen(buf);
    for (i = j; i < sql_len - 1; i++) {
        if (buf[j-1] == '?') {
            buf[j] = ',';
        } else {
            buf[j] = '?';
        }
        j++;
    }
    buf[j] = ')';
    
    sql.data = buf;
    sql.len = sql_len;
    return sql;
}
