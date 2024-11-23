
/*
 * Copyright (C) Serope.com
 *
 * A brief summary of some of Nginx's file I/O functions:
 * 
 * ngx_open_file()
 * This is a wrapper around open(), which returns -1 on failure and any other
 * value on success. Nginx has the NGX_FILE_ERROR and NGX_INVALID_FILE macros,
 * which are both -1.
 * 
 * ngx_fd_info() 
 * This is a wrapper around fstat(), which returns 0 on success and -1 on
 * failure.
 * 
 * ngx_read_file()
 * This returns the total amount of bytes read. If it fails, it returns either
 * NGX_ERROR or some value less than the file's size.
 * 
 * ngx_close_file()
 * This is a wrapper around close(), which returns 0 on success and -1 on
 * failure.
 */

#include <ngx_core.h>


#include "ngx_http_sqlitelog_file.h"
#include "ngx_http_sqlitelog_util.h"


static ssize_t ngx_http_sqlitelog_file_size(ngx_str_t filename, ngx_log_t *log);


/**
 * Read a file into a string.
 * 
 * @param   filename    the filename
 * @param   pool        a pool in which to allocate the string's data
 * @param   log         a log for writing errors
 * @return              a string with the file's contents,
 *                      or a NULL string if an error occurs
 */
ngx_str_t
ngx_http_sqlitelog_file_read(ngx_str_t filename, ngx_pool_t *pool,
    ngx_log_t *log)
{
    off_t       offset;
    size_t      buf_len;
    size_t      buf_size;
    u_char     *buf;
    ssize_t     bytes_read;
    ssize_t     filesize;
    ngx_fd_t    fd;
    ngx_int_t   closed;
    ngx_str_t   s;
    ngx_file_t  f;
    ngx_flag_t  success;
    
    buf = NULL;
    bytes_read = NGX_ERROR;
    success = NGX_OK;
    
    /* Open file */
    fd = ngx_open_file(filename.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to open file \"%V\"", &filename);
        success = NGX_ERROR;
        goto failed;
    }
    
    /* Get size */
    filesize = ngx_http_sqlitelog_file_size(filename, log);
    if (filesize == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to get size of file \"%V\"",
                      &filename);
        success = NGX_ERROR;
        goto failed;
    }
    
    /* Allocate buffer for file contents */
    buf_len = filesize;
    buf_size = buf_len+1;
    buf = ngx_pcalloc(pool, buf_size * sizeof(u_char));
    if (buf == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to allocate %v bytes to read contents "
                      "of file \"%V\"", buf_size, &filename);
        success = NGX_ERROR;
        goto failed;
    }
    
    /* Init ngx_file object to read from */
    f.name = filename;
    f.fd   = fd;
    f.log  = log;
    
    /* Read file */
    offset = 0;
    bytes_read = ngx_read_file(&f, buf, filesize, offset);
    if (bytes_read == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to read file \"%V\"", &filename);
        success = NGX_ERROR;
        goto failed;
        
    }
    else if ((ssize_t) bytes_read != filesize) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to read file \"%V\"; only read %z of "
                      "%z total bytes",
                      &filename, bytes_read, filesize);
        success = NGX_ERROR;
        goto failed;
    }
    
    /* Close */
    closed = ngx_close_file(fd);
    if (closed != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to close file \"%V\"",&filename);
        success = NGX_ERROR;
        goto failed;
    }
    
    /* End */
failed:
    if (success == NGX_ERROR) {
        return NGX_NULL_STRING;
    }
    
    s.data = buf;
    s.len = buf_len;
    return s;
}


/**
 * Get a file's size in bytes.
 * 
 * @param   filename    the filename
 * @param   log         a log for writing errors
 * @return              the file's size,
 *                      or NGX_FILE_ERROR (-1) if an error occurs
 */
ssize_t
ngx_http_sqlitelog_file_size(ngx_str_t filename, ngx_log_t *log)
{
    ssize_t             filesize;
    ngx_fd_t            fd;
    ngx_int_t           closed;
    ngx_int_t           statted;
    ngx_file_info_t     fi;
    
    filesize = NGX_FILE_ERROR;
    
    /* Open file */
    fd = ngx_open_file(filename.data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to open file \"%V\"", &filename);
        return NGX_FILE_ERROR;
    }
    
    /* Stat file */
    statted = ngx_fd_info(fd, &fi);
    if (statted != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0, 
                      "sqlitelog: failed to stat file \"%V\"", &filename);
        goto close;
    }
    filesize = (ssize_t) ngx_file_size(&fi);
    
    /* Close */
close:
    closed = ngx_close_file(fd);
    if (closed != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to close file \"%V\"", &filename);
    }
    return filesize;
}
