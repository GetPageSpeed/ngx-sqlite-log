
/*
 * Copyright (C) Serope.com
 * 
 * These are Nginx-centric wrappers around SQLite3 functions. The key
 * differences are:
 * 
 * 1. If a function had a traditional C string (char*) and length parameter, it
 *    now takes an Nginx string (ngx_str_t) instead.
 * 2. If a function didn't have a database connection (sqlite3*) parameter, it
 *    now does, for the purpose of getting error messages from sqlite3_errmsg().
 * 3. The final parameter is always an Nginx log (ngx_log_t) for the purpose of
 *    writing error messages.
 */


#include <ngx_core.h>
#include <sqlite3.h>


static char *ngx_http_sqlitelog_rcname_cs(int rc);
static ngx_str_t ngx_http_sqlitelog_rcname(int rc);
static ngx_str_t ngx_http_sqlitelog_errmsg(sqlite3 *db);


/**
 * Open a SQLite3 database file.
 * 
 * @param   filename    the file to open
 * @param   db_ptr      a pointer to an uninitialized SQLite3 connection object
 * @param   flags       one or more bitwise-or'd flags indicating the file mode
 * @param   vfs_module  an optional SQLite3 VFS module to use
 * @param   log         an Nginx log for writing errors
 * @return              the return code of sqlite3_open_v2()
 */
int
ngx_http_sqlitelog_sqlite3_open_v2(ngx_str_t filename, sqlite3 **db_ptr,
    int flags, const char *vfs_module, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    char        *filename_cs;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    filename_cs = (char*) filename.data;
    
    rc_primary = sqlite3_open_v2(filename_cs, db_ptr, flags, vfs_module);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(*db_ptr);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(*db_ptr);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to open database \"%V\" "
                      "due to %V (%d): \"%V\"",
                      &filename, &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to open database \"%V\" "
                      "due to %V (%d): \"%V\"",
                      &filename,  &rc_extended_name, rc_extended,
                      &error_message);
    }
    return rc_primary;
}


/**
 * Prepare a SQLite3 statement.
 * 
 * @param   db          a database connection
 * @param   sql         the SQL command to be prepared
 * @param   stmt_ptr    a pointer to an uninitialized SQLite3 statement object
 * @param   pz_tail     a pointer to the position within sql where the next SQL
 *                      command begins (if sql contains multiple commands)
 * @param   log         an Nginx log for writing errors
 * @return              the return code of sqlite3_prepare_v2()
 */
int
ngx_http_sqlitelog_sqlite3_prepare_v2(sqlite3 *db, ngx_str_t sql,
    sqlite3_stmt **stmt_ptr, const char **pz_tail, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    char        *sql_cs;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    sql_cs = (char*) sql.data;
    
    rc_primary = sqlite3_prepare_v2(db, sql_cs, sql.len, stmt_ptr, pz_tail);

    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to prepare statement \"%V\" "
                      "due to %V (%d): \"%V\"",
                      &sql, &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to prepare statement \"%V\" "
                      "due to %V (%d): \"%V\"",
                      &sql, &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Bind text to a SQLite3 prepared statement.
 * 
 * @param   db              a database connection
 * @param   stmt            the SQLite3 statement to bind to
 * @param   position        the position to bind to
 * @param   val             the value to bind
 * @param   val_destructor  a destructor function to call on val after binding,
 *                          or SQLITE_STATIC to not take any action
 * @param   log             an Nginx log for writing errors
 * @return                  the return code of sqlite3_bind_text()
 */
int
ngx_http_sqlitelog_sqlite3_bind_text(sqlite3 *db, sqlite3_stmt *stmt,
    int position, ngx_str_t val, sqlite3_destructor_type val_destructor,
    ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    char        *val_cs;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    val_cs = (char*) val.data;
    
    rc_primary = sqlite3_bind_text(stmt, position, val_cs, val.len,
                                   val_destructor);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == SQLITE_TOOBIG) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind text of length %d to "
                      "prepared statement due to %V (%d): \"%V\"",
                      val.len, &rc_primary_name, rc_primary, &error_message);
    }
    else if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind text \"%V\" to "
                      "prepared statement due to %V (%d): \"%V\"",
                      &val, &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind text \"%V\" to "
                      "prepared statement due to %V (%d): \"%V\"",
                      &val, &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Bind NULL to a SQLite3 prepared statement.
 * 
 * @param   db              a database connection
 * @param   stmt            the SQLite3 statement to bind to
 * @param   position        the position to bind to
 * @param   log             an Nginx log for writing errors
 * @return                  the return code of sqlite3_bind_blob()
 */
int
ngx_http_sqlitelog_sqlite3_bind_null(sqlite3 *db, sqlite3_stmt *stmt,
    int position, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_bind_null(stmt, position);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind null to prepared "
                      "statement due to %V (%d): \"%V\"",
                      &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind null to prepared "
                      "statement due to %V (%d): \"%V\"",
                      &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * 
 * Bind a blob (ngx_str_t with blob data and length) to a SQLite3 prepared
 * statement.
 * 
 * @param   db              a database connection
 * @param   stmt            the SQLite3 statement to bind to
 * @param   position        the position to bind to
 * @param   val             the value to bind
 * @param   val_destructor  a destructor function to call on val after binding,
 *                          or SQLITE_STATIC to not take any action
 * @param   log             an Nginx log for writing errors
 * @return                  the return code of sqlite3_bind_blob()
 */
int
ngx_http_sqlitelog_sqlite3_bind_blob(sqlite3 *db, sqlite3_stmt *stmt,
    int position, ngx_str_t val, sqlite3_destructor_type val_destructor,
    ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_bind_blob(stmt, position, val.data, val.len,
                                   val_destructor);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == SQLITE_TOOBIG) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind blob of length %d to "
                      "prepared statement due to %V (%d): \"%V\"",
                      val.len, &rc_primary_name, rc_primary, &error_message);
    }
    else if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind blob to prepared "
                      "statement due to %V (%d): \"%V\"",
                      &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to bind blob to prepared "
                      "statement due to %V (%d): \"%V\"",
                      &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Step a SQLite3 prepared statement.
 * 
 * Note that sqlite3_step() returns SQLITE_DONE or SQLITE_ROW on success, not
 * SQLITE_OK.
 * 
 * @param   db      a database connection
 * @param   stmt    the prepared statement to execute
 * @param   log     an Nginx log for writing errors
 * @return          the return code of sqlite3_step()
 */
int
ngx_http_sqlitelog_sqlite3_step(sqlite3 *db, sqlite3_stmt *stmt, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_step(stmt);
    
    /* Done */
    if (rc_primary == SQLITE_DONE || rc_primary == SQLITE_ROW) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to step prepared statement "
                      "due to %V (%d): \"%V\"",
                      &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to step prepared statement "
                      "due to %V (%d): \"%V\"",
                      &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Finalize a SQLite3 prepared statement.
 * 
 * @param   db      a database connection
 * @param   stmt    the prepared statement to finalize
 * @param   log     an Nginx log for writing errors
 * @return          the return code of sqlite3_finalize()
 */
int
ngx_http_sqlitelog_sqlite3_finalize(sqlite3 *db, sqlite3_stmt *stmt,
    ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_finalize(stmt);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to finalize prepared "
                      "statement due to %V (%d): \"%V\"",
                      &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to finalize prepared "
                      "statement due to %V (%d): \"%V\"",
                      &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Set a busy timeout duration for this database connection.
 * 
 * @param   db              a database connection
 * @param   duration_ms     a timeout duration in milliseconds
 * @param   log             an Nginx log for writing error messages
 * @return                  the return code of sqlite3_busy_timeout()
 */
int
ngx_http_sqlitelog_sqlite3_busy_timeout(sqlite3 *db, int duration_ms,
    ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_busy_timeout(db, duration_ms);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to set busy timeout %d "
                      "due to %V (%d): \"%V\"",
                      duration_ms,&rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to set busy timeout %d "
                      "due to %V (%d): \"%V\"",
                      duration_ms,&rc_extended_name,rc_extended,&error_message);
    }
    return rc_primary;
}


/**
 * Execute a SQL command.
 * 
 * @param   db                  a database connection
 * @param   sql                 the SQL command to execute
 * @param   callback            a callback function that is invoked upon every
 *                              result row
 * @param   callback_data       data to pass to the callback
 * @param   error_message_ptr   a pointer to a string that will take on the
 *                              value of an error message
 * @param   log                 an Nginx log for writing error messages
 * @return                      the return code of sqlite3_exec()
 */
int
ngx_http_sqlitelog_sqlite3_exec(sqlite3 *db, ngx_str_t sql, void *callback,
    void *callback_data, char **error_message_ptr, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    char        *sql_cs;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    sql_cs = (char*) sql.data;
    
    rc_primary = sqlite3_exec(db, sql_cs, callback, callback_data,
                              error_message_ptr);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to execute statement \"%V\" "
                      "due to %V (%d): \"%V\"",
                      &sql, &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to execute statement \"%V\" "
                      "due to %V (%d): \"%V\"",
                      &sql, &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Close a SQLite3 database connection.
 * 
 * @param   db      the connection to close
 * @param   log     an Nginx log for writing error messages
 * @return          the return code of sqlite3_close()
 */
int
ngx_http_sqlitelog_sqlite3_close(sqlite3 *db, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_close(db);
    
    /* OK */
    if (rc_primary == SQLITE_OK) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to close connection "
                      "due to %V (%d): \"%V\"",
                      &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to close connection "
                      "due to %V (%d): \"%V\"",
                      &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Execute a WAL checkpoint.
 * 
 * @param   db      a database connection
 * @param   log     an Nginx log for writing error messages
 * @return          the return code of sqlite3_wal_checkpoint()
 */
int
ngx_http_sqlitelog_sqlite3_wal_checkpoint_v2(sqlite3 *db, const char *zdb,
    int emode, int *pn_log, int *pn_ckpt, ngx_log_t *log)
{
    int          rc_extended;
    int          rc_primary;
    ngx_str_t    error_message;
    ngx_str_t    rc_extended_name;
    ngx_str_t    rc_primary_name;
    
    rc_primary = sqlite3_wal_checkpoint_v2(db, zdb, emode, pn_log, pn_ckpt);
    
    /*
     * SQLITE_OK and SQLITE_BUSY are considered success, the latter because WAL
     * checkpoints don't abide by busy timeouts. Unlike any other SQLite
     * function, in which the busy handler is invoked if 2 connections are
     * trying to use the database at the same time, sqlite3_wal_checkpoint_v2()
     * doesn't invoke it at all. It immediately returns SQLITE_BUSY, even if a
     * sufficient busy timeout has been set. To solve this, we have to implement
     * our own try-sleep loop at the caller level.
     */
    if (rc_primary == SQLITE_OK || rc_primary == SQLITE_BUSY) {
        return rc_primary;
    }
    
    /* Error */
    error_message = ngx_http_sqlitelog_errmsg(db);
    rc_primary_name = ngx_http_sqlitelog_rcname(rc_primary);
    rc_extended = sqlite3_extended_errcode(db);
    
    if (rc_primary == rc_extended) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to execute WAL checkpoint "
                      "due to %V (%d): \"%V\"",
                      &rc_primary_name, rc_primary, &error_message);
    }
    else {
        rc_extended_name = ngx_http_sqlitelog_rcname(rc_extended);
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: sqlite3 failed to execute WAL checkpoint "
                      "due to %V (%d): \"%V\"",
                      &rc_extended_name, rc_extended, &error_message);
    }
    return rc_primary;
}


/**
 * Get the name of a return code as a C string.
 *
 * @param   rc      a SQLite3 return code
 * @return          the return code's macro/identifier as a string
 * @see             https://www.sqlite.org/c3ref/c_abort.html
 * @see             https://www.sqlite.org/c3ref/c_abort_rollback.html
 */
static char *
ngx_http_sqlitelog_rcname_cs(int rc)
{
    switch (rc) {
    case SQLITE_OK:                        return "SQLITE_OK";
    case SQLITE_ERROR:                     return "SQLITE_ERROR";
    case SQLITE_INTERNAL:                  return "SQLITE_INTERNAL";
    case SQLITE_PERM:                      return "SQLITE_PERM";
    case SQLITE_ABORT:                     return "SQLITE_ABORT";
    case SQLITE_BUSY:                      return "SQLITE_BUSY";
    case SQLITE_LOCKED:                    return "SQLITE_LOCKED";
    case SQLITE_NOMEM:                     return "SQLITE_NOMEM";
    case SQLITE_READONLY:                  return "SQLITE_READONLY";
    case SQLITE_INTERRUPT:                 return "SQLITE_INTERRUPT";
    case SQLITE_IOERR:                     return "SQLITE_IOERR";
    case SQLITE_CORRUPT:                   return "SQLITE_CORRUPT";
    case SQLITE_NOTFOUND:                  return "SQLITE_NOTFOUND";
    case SQLITE_FULL:                      return "SQLITE_FULL";
    case SQLITE_CANTOPEN:                  return "SQLITE_CANTOPEN";
    case SQLITE_PROTOCOL:                  return "SQLITE_PROTOCOL";
    case SQLITE_EMPTY:                     return "SQLITE_EMPTY";
    case SQLITE_SCHEMA:                    return "SQLITE_SCHEMA";
    case SQLITE_TOOBIG:                    return "SQLITE_TOOBIG";
    case SQLITE_CONSTRAINT:                return "SQLITE_CONSTRAINT";
    case SQLITE_MISMATCH:                  return "SQLITE_MISMATCH";
    case SQLITE_MISUSE:                    return "SQLITE_MISUSE";
    case SQLITE_NOLFS:                     return "SQLITE_NOLFS";
    case SQLITE_AUTH:                      return "SQLITE_AUTH";
    case SQLITE_FORMAT:                    return "SQLITE_FORMAT";
    case SQLITE_RANGE:                     return "SQLITE_RANGE";
    case SQLITE_NOTADB:                    return "SQLITE_NOTADB";
    case SQLITE_NOTICE:                    return "SQLITE_NOTICE";
    case SQLITE_WARNING:                   return "SQLITE_WARNING";
    case SQLITE_ROW:                       return "SQLITE_ROW";
    case SQLITE_DONE:                      return "SQLITE_DONE";
    case SQLITE_ERROR_MISSING_COLLSEQ:     return "SQLITE_ERROR_MISSING_COLLSEQ";
    case SQLITE_ERROR_RETRY:               return "SQLITE_ERROR_RETRY";
    case SQLITE_ERROR_SNAPSHOT:            return "SQLITE_ERROR_SNAPSHOT";
    case SQLITE_IOERR_READ:                return "SQLITE_IOERR_READ";
    case SQLITE_IOERR_SHORT_READ:          return "SQLITE_IOERR_SHORT_READ";
    case SQLITE_IOERR_WRITE:               return "SQLITE_IOERR_WRITE";
    case SQLITE_IOERR_FSYNC:               return "SQLITE_IOERR_FSYNC";
    case SQLITE_IOERR_DIR_FSYNC:           return "SQLITE_IOERR_DIR_FSYNC";
    case SQLITE_IOERR_TRUNCATE:            return "SQLITE_IOERR_TRUNCATE";
    case SQLITE_IOERR_FSTAT:               return "SQLITE_IOERR_FSTAT";
    case SQLITE_IOERR_UNLOCK:              return "SQLITE_IOERR_UNLOCK";
    case SQLITE_IOERR_RDLOCK:              return "SQLITE_IOERR_RDLOCK";
    case SQLITE_IOERR_DELETE:              return "SQLITE_IOERR_DELETE";
    case SQLITE_IOERR_BLOCKED:             return "SQLITE_IOERR_BLOCKED";
    case SQLITE_IOERR_NOMEM:               return "SQLITE_IOERR_NOMEM";
    case SQLITE_IOERR_ACCESS:              return "SQLITE_IOERR_ACCESS";
    case SQLITE_IOERR_CHECKRESERVEDLOCK:   return "SQLITE_IOERR_CHECKRESERVEDLOCK";
    case SQLITE_IOERR_LOCK:                return "SQLITE_IOERR_LOCK";
    case SQLITE_IOERR_CLOSE:               return "SQLITE_IOERR_CLOSE";
    case SQLITE_IOERR_DIR_CLOSE:           return "SQLITE_IOERR_DIR_CLOSE";
    case SQLITE_IOERR_SHMOPEN:             return "SQLITE_IOERR_SHMOPEN";
    case SQLITE_IOERR_SHMSIZE:             return "SQLITE_IOERR_SHMSIZE";
    case SQLITE_IOERR_SHMLOCK:             return "SQLITE_IOERR_SHMLOCK";
    case SQLITE_IOERR_SHMMAP:              return "SQLITE_IOERR_SHMMAP";
    case SQLITE_IOERR_SEEK:                return "SQLITE_IOERR_SEEK";
    case SQLITE_IOERR_DELETE_NOENT:        return "SQLITE_IOERR_DELETE_NOENT";
    case SQLITE_IOERR_MMAP:                return "SQLITE_IOERR_MMAP";
    case SQLITE_IOERR_GETTEMPPATH:         return "SQLITE_IOERR_GETTEMPPATH";
    case SQLITE_IOERR_CONVPATH:            return "SQLITE_IOERR_CONVPATH";
    case SQLITE_IOERR_VNODE:               return "SQLITE_IOERR_VNODE";
    case SQLITE_IOERR_AUTH:                return "SQLITE_IOERR_AUTH";
    case SQLITE_IOERR_BEGIN_ATOMIC:        return "SQLITE_IOERR_BEGIN_ATOMIC";
    case SQLITE_IOERR_COMMIT_ATOMIC:       return "SQLITE_IOERR_COMMIT_ATOMIC";
    case SQLITE_IOERR_ROLLBACK_ATOMIC:     return "SQLITE_IOERR_ROLLBACK_ATOMIC";
    case SQLITE_LOCKED_SHAREDCACHE:        return "SQLITE_LOCKED_SHAREDCACHE";
    case SQLITE_LOCKED_VTAB:               return "SQLITE_LOCKED_VTAB";
    case SQLITE_BUSY_RECOVERY:             return "SQLITE_BUSY_RECOVERY";
    case SQLITE_BUSY_SNAPSHOT:             return "SQLITE_BUSY_SNAPSHOT";
    case SQLITE_CANTOPEN_NOTEMPDIR:        return "SQLITE_CANTOPEN_NOTEMPDIR";
    case SQLITE_CANTOPEN_ISDIR:            return "SQLITE_CANTOPEN_ISDIR";
    case SQLITE_CANTOPEN_FULLPATH:         return "SQLITE_CANTOPEN_FULLPATH";
    case SQLITE_CANTOPEN_CONVPATH:         return "SQLITE_CANTOPEN_CONVPATH";
    case SQLITE_CANTOPEN_DIRTYWAL:         return "SQLITE_CANTOPEN_DIRTYWAL";
    case SQLITE_CANTOPEN_SYMLINK:          return "SQLITE_CANTOPEN_SYMLINK";
    case SQLITE_CORRUPT_VTAB:              return "SQLITE_CORRUPT_VTAB";
    case SQLITE_CORRUPT_SEQUENCE:          return "SQLITE_CORRUPT_SEQUENCE";
    case SQLITE_READONLY_RECOVERY:         return "SQLITE_READONLY_RECOVERY";
    case SQLITE_READONLY_CANTLOCK:         return "SQLITE_READONLY_CANTLOCK";
    case SQLITE_READONLY_ROLLBACK:         return "SQLITE_READONLY_ROLLBACK";
    case SQLITE_READONLY_DBMOVED:          return "SQLITE_READONLY_DBMOVED";
    case SQLITE_READONLY_CANTINIT:         return "SQLITE_READONLY_CANTINIT";
    case SQLITE_READONLY_DIRECTORY:        return "SQLITE_READONLY_DIRECTORY";
    case SQLITE_ABORT_ROLLBACK:            return "SQLITE_ABORT_ROLLBACK";
    case SQLITE_CONSTRAINT_CHECK:          return "SQLITE_CONSTRAINT_CHECK";
    case SQLITE_CONSTRAINT_COMMITHOOK:     return "SQLITE_CONSTRAINT_COMMITHOOK";
    case SQLITE_CONSTRAINT_FOREIGNKEY:     return "SQLITE_CONSTRAINT_FOREIGNKEY";
    case SQLITE_CONSTRAINT_FUNCTION:       return "SQLITE_CONSTRAINT_FUNCTION";
    case SQLITE_CONSTRAINT_NOTNULL:        return "SQLITE_CONSTRAINT_NOTNULL";
    case SQLITE_CONSTRAINT_PRIMARYKEY:     return "SQLITE_CONSTRAINT_PRIMARYKEY";
    case SQLITE_CONSTRAINT_TRIGGER:        return "SQLITE_CONSTRAINT_TRIGGER";
    case SQLITE_CONSTRAINT_UNIQUE:         return "SQLITE_CONSTRAINT_UNIQUE";
    case SQLITE_CONSTRAINT_VTAB:           return "SQLITE_CONSTRAINT_VTAB";
    case SQLITE_CONSTRAINT_ROWID:          return "SQLITE_CONSTRAINT_ROWID";
    case SQLITE_CONSTRAINT_PINNED:         return "SQLITE_CONSTRAINT_PINNED";
    case SQLITE_NOTICE_RECOVER_WAL:        return "SQLITE_NOTICE_RECOVER_WAL";
    case SQLITE_NOTICE_RECOVER_ROLLBACK:   return "SQLITE_NOTICE_RECOVER_ROLLBACK";
    case SQLITE_WARNING_AUTOINDEX:         return "SQLITE_WARNING_AUTOINDEX";
    case SQLITE_AUTH_USER:                 return "SQLITE_AUTH_USER";
    case SQLITE_OK_LOAD_PERMANENTLY:       return "SQLITE_OK_LOAD_PERMANENTLY";
    case SQLITE_OK_SYMLINK:                return "SQLITE_OK_SYMLINK";
    }
    return "";
}


/**
 * Get the name of a return code as an Nginx string.
 *
 * @param   rc      a SQLite3 return code
 * @return          the return code's macro/identifier as a string
 */
static ngx_str_t
ngx_http_sqlitelog_rcname(int rc)
{
    char      *cs;
    ngx_str_t  s;
    
    cs = ngx_http_sqlitelog_rcname_cs(rc);
    
    s.data = (u_char*) cs;
    s.len = ngx_strlen(cs);
    
    return s;
}


/**
 * Get a SQLite3 error message as a string.
 * 
 * @param   db      a database connection
 * @return          the message as an ngx_str_t
 */
static ngx_str_t
ngx_http_sqlitelog_errmsg(sqlite3 *db)
{
    const char *cs = sqlite3_errmsg(db);
    ngx_str_t   s;
    
    s.data = (u_char *) cs;
    s.len = ngx_strlen(cs);
    
    return s;
}
