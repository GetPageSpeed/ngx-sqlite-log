
/*
 * Copyright (C) Serope.com
 */


#include <ngx_core.h>
#include <sqlite3.h>


#include "ngx_http_sqlitelog_col.h"
#include "ngx_http_sqlitelog_db.h"
#include "ngx_http_sqlitelog_fmt.h"
#include "ngx_http_sqlitelog_node.h"
#include "ngx_http_sqlitelog_sqlite3.h"
#include "ngx_http_sqlitelog_util.h"


static int ngx_http_sqlitelog_db_try_insert(ngx_http_sqlitelog_db_t *db,
    ngx_str_t *elts, ngx_uint_t nelts, ngx_log_t *log);
static int ngx_http_sqlitelog_db_try_insert_list(ngx_http_sqlitelog_db_t *db,
    ngx_list_t *list, ngx_log_t *log);
static int ngx_http_sqlitelog_db_is_wal(ngx_http_sqlitelog_db_t *db,
    ngx_flag_t *is, ngx_log_t *log);
static int ngx_http_sqlitelog_db_get_busy_timeout(ngx_http_sqlitelog_db_t *db,
    int *ms, ngx_log_t *log);


/**
 * Initialize a database connection.
 * 
 * The database's filename, format, and init script (if any) must be set before
 * calling this function.
 * 
 * @param   db      a database struct
 * @param   log     an Nginx log for writing errors
 * @return          a SQLite3 status code
 */
int
ngx_http_sqlitelog_db_init(ngx_http_sqlitelog_db_t *db, ngx_log_t *log)
{
    int             filemode;
    int             rc_close;
    int             rc_open;
    int             rc_script;
    int             rc_table;
    int             rc_timeout;
    int             timeout;
    char          **error_message_ptr;
    void           *callback;
    void           *callback_data;
    const char     *vfs_module;
    
    /* If necessary, close existing connection */
    if (db->conn) {
        rc_close = ngx_http_sqlitelog_db_close(db, log);
        if (rc_close != SQLITE_OK) {
            return rc_close;
        }
    }
    
    /* Open new connection */
    filemode = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    vfs_module = NULL;
    rc_open = ngx_http_sqlitelog_sqlite3_open_v2(db->filename, &db->conn,
                                                 filemode, vfs_module, log);
    if (rc_open != SQLITE_OK) {
        rc_close = ngx_http_sqlitelog_db_close(db, log);
        if (rc_close != SQLITE_OK) {
            return rc_close;
        }
        return rc_open;
    }
    
    /* Set busy timeout */
    timeout = 1000;
    rc_timeout = ngx_http_sqlitelog_sqlite3_busy_timeout(db->conn,timeout, log);
    if (rc_timeout != SQLITE_OK) {
        return rc_timeout;
    }
    
    /* Create table */
    callback = NULL;
    callback_data = NULL;
    error_message_ptr = NULL;
    rc_table = ngx_http_sqlitelog_sqlite3_exec(db->conn, db->fmt->sql_create,
                               callback, callback_data, error_message_ptr, log);
    if (rc_table != SQLITE_OK) {
        return rc_table;
    }
    
    /* Execute init script */
    if (db->init_sql.data) {
        rc_script = ngx_http_sqlitelog_sqlite3_exec(db->conn, db->init_sql,
                               callback, callback_data, error_message_ptr, log);
        if (rc_script != SQLITE_OK) {
            return rc_script;
        }
    }
    return SQLITE_OK;
}


/**
 * Close a database connection.
 * 
 * The database's format and init script (if any) must be set before calling
 * this function.
 * 
 * @param   db      a database struct
 * @param   log     an Nginx log for writing errors
 * @return          a SQLite3 status code
 */
int
ngx_http_sqlitelog_db_close(ngx_http_sqlitelog_db_t *db, ngx_log_t *log)
{
    int  rc_close;
    
    rc_close = ngx_http_sqlitelog_sqlite3_close(db->conn, log);
    if (rc_close != SQLITE_OK) {
        return rc_close;
    }
    
    db->conn = NULL;
    
    return rc_close;
}


/**
 * Initialize an in-memory database connection.
 * 
 * The database's format and init script (if any) must be set before calling
 * this function.
 * 
 * @param   db      a database struct
 * @param   log     an Nginx log for writing errors
 * @return          a SQLite3 status code
 */
int
ngx_http_sqlitelog_db_test(ngx_http_sqlitelog_db_t db, ngx_log_t *log)
{
    int                       rc_close;
    int                       rc_init;
    ngx_http_sqlitelog_db_t   memdb;
    
    ngx_memzero(&memdb, sizeof(ngx_http_sqlitelog_db_t));
    ngx_str_set(&memdb.filename, ":memory:");
    memdb.fmt = db.fmt;
    memdb.init_sql = db.init_sql;
    
    rc_init = ngx_http_sqlitelog_db_init(&memdb, log);
    rc_close = ngx_http_sqlitelog_db_close(&memdb, log);
    
    if (rc_init != SQLITE_OK) {
        return rc_init;
    }
    return rc_close;
}


/**
 * Insert a record into the database, recreating the file if faced with
 * SQLITE_READONLY_DBMOVED.
 * 
 * @param   db          a database struct
 * @param   elts        a C-style array of Nginx strings for each column
 * @param   nelts       the length of elts
 * @param   log         an Nginx log to write errors to
 * @return              a SQLite3 return code
 */
int
ngx_http_sqlitelog_db_insert(ngx_http_sqlitelog_db_t *db, ngx_str_t *elts,
    ngx_uint_t nelts, ngx_log_t *log)
{
    int  rc_extended;
    int  rc_init;
    int  rc_insert;
    
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "sqlitelog: db insert");
    
    rc_insert = ngx_http_sqlitelog_db_try_insert(db, elts, nelts, log);
    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: db insert, rc_insert: %d", rc_insert);
    
    if (rc_insert != SQLITE_OK) {
        rc_extended = sqlite3_extended_errcode(db->conn);
        if (rc_extended == SQLITE_READONLY_DBMOVED) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                           "sqlitelog: recreate database");
            rc_init = ngx_http_sqlitelog_db_init(db, log);
            if (rc_init != SQLITE_OK) {
                return rc_init;
            }
            rc_insert = ngx_http_sqlitelog_db_try_insert(db, elts, nelts, log);
        }
    }
    return rc_insert;
}


/**
 * Try to insert a record into the database, returning the appropriate error
 * code if an error occurs.
 * 
 * @param   db          a database struct
 * @param   elts        a C-style array of Nginx strings for each column
 * @param   nelts       the length of elts
 * @param   log         an Nginx log to write errors to
 * @return              a SQLite3 return code
 */
static int
ngx_http_sqlitelog_db_try_insert(ngx_http_sqlitelog_db_t *db, ngx_str_t *elts,
    ngx_uint_t nelts, ngx_log_t *log)
{
    int                        rc_bind;
    int                        rc_finalize;
    int                        rc_prepare;
    int                        rc_step;
    int                        param;
    ngx_str_t                  elt;
    ngx_uint_t                 i;
    sqlite3_stmt              *stmt;
    ngx_http_sqlitelog_col_t  *col;
    
    col = db->fmt->columns.elts; /* typecast from void* */
    rc_step = SQLITE_OK;         /* to stop gcc "might be unitialized" warnings */
    rc_bind = SQLITE_OK;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "sqlitelog: db try insert");
    
    /* Prepare */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: db try insert, prepare");
    stmt = NULL;
    rc_prepare = ngx_http_sqlitelog_sqlite3_prepare_v2(db->conn,
                                         db->fmt->sql_insert, &stmt, NULL, log);
    if (rc_prepare != SQLITE_OK) {
        goto finalize;
    }
    
    /* Bind */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: db try insert, bind");
    rc_bind = SQLITE_OK;
    for (i = 0; i < nelts; i++) {
        
        elt = elts[i];
        param = i+1;
        
        if (elt.data) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0,
                           "sqlitelog: db try insert, bind %d: %V", param,&elt);
        } else {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                           "sqlitelog: db try insert, bind %d: (null)", param);
        }
        
        rc_bind = col->bind(db->conn, stmt, param, elt, SQLITE_STATIC, log);
       
        if (rc_bind != SQLITE_OK) {
            goto finalize;
        }
        
        col++;
    }
    
    /* Step (execute) */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,"sqlitelog: db try insert, step");
    rc_step = ngx_http_sqlitelog_sqlite3_step(db->conn, stmt, log);
   
    if (rc_step != SQLITE_DONE) {
        goto finalize;
    }
    
    /*
     * Finalize
     * 
     * sqlite3_finalize() is the final function to call in the
     * prepare->bind->step->finalize chain. The error code that it returns
     * is completely unrelated to the error codes returned by the prior
     * functions in the chain. For example, if sqlite3_prepare_v2() fails,
     * calling sqlite3_finalize() on the same statement could still return
     * SQLITE_OK.
     * 
     * Be careful not to be fooled into believing that a statement was
     * successful just because sqlite3_finalize() returned SQLITE_OK.
     * 
     * The documentation for sqlite3_finalize() is worded to suggest that
     * the last error code returned by any of the prior functions in the
     * chain is propogated through. This is false!
     * 
     * https://www.sqlite.org/c3ref/finalize.html
     */
finalize:
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: db try insert, finalize");
    rc_finalize = ngx_http_sqlitelog_sqlite3_finalize(db->conn, stmt, log);
   
    if (rc_prepare != SQLITE_OK) {
        return rc_prepare;
    }
    else if (rc_bind != SQLITE_OK) {
        return rc_bind;
    }
    else if (rc_step != SQLITE_DONE) {
        return rc_step;
    }
    return rc_finalize;
}


/**
 * Insert a list of log entries into the database, recreating the file if faced
 * with SQLITE_READONLY_DBMOVED.
 * 
 * @param   db          a database struct
 * @param   list        a list of log entries
 * @param   log         an Nginx log to write errors to
 * @return              a SQLite3 return code
 */
int
ngx_http_sqlitelog_db_insert_list(ngx_http_sqlitelog_db_t *db, ngx_list_t *list,
    ngx_log_t *log)
{
    int  rc_extended;
    int  rc_init;
    int  rc_list;
    
    rc_list = ngx_http_sqlitelog_db_try_insert_list(db, list, log);
   
    if (rc_list != SQLITE_OK) {
        rc_extended = sqlite3_extended_errcode(db->conn);
        if (rc_extended == SQLITE_READONLY_DBMOVED || rc_extended == SQLITE_OK) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0,
                           "sqlitelog: recreate database");
            rc_init = ngx_http_sqlitelog_db_init(db, log);
            if (rc_init != SQLITE_OK) {
                return rc_init;
            }
            rc_list = ngx_http_sqlitelog_db_try_insert_list(db, list, log);
        }
    }
    
    return rc_list;
}


/**
 * Try to insert a list of log entries into the database, returning the
 * appropriate error code if an error occurs.
 * 
 * @param   db          a database struct
 * @param   queue       a list of log entries
 * @param   log         an Nginx log to write errors to
 * @return              a SQLite3 return code
 */
static int
ngx_http_sqlitelog_db_try_insert_list(ngx_http_sqlitelog_db_t *db,
    ngx_list_t *list, ngx_log_t *log)
{
    int               rc_begin;
    int               rc_end;
    int               rc_insert;
    char            **error_message_ptr;
    void             *callback;
    void             *callback_data;
    ngx_str_t         sql_begin;
    ngx_str_t         sql_end;
    ngx_list_part_t  *part;
    
    /*
     * Begin transaction
     * 
     * We use EXCLUSIVE mode so that all other connections are locked out and
     * the writing can begin as quickly as possible.
     * 
     * See also:
     *  https://www.sqlite.org/lang_transaction.html#deferred_immediate_and_exclusive_transactions
     *  The Definitive Guide to SQLite, 2nd ed, pg. 117 "Transaction Types"
     *  Using SQLite, pg. 53 "SQL Transactions"
     */
    callback = NULL;
    callback_data = NULL;
    error_message_ptr = NULL;
    ngx_str_set(&sql_begin, "BEGIN EXCLUSIVE TRANSACTION");
    rc_begin = ngx_http_sqlitelog_sqlite3_exec(db->conn, sql_begin, callback,
                                         callback_data, error_message_ptr, log);
    if (rc_begin != SQLITE_OK) {
        return rc_begin;
    }
    
    /* Loop */
    part = &list->part;
    while (part) {
        rc_insert = ngx_http_sqlitelog_db_try_insert(db, part->elts, 
                                                     part->nelts, log);
        if (rc_insert != SQLITE_OK) {
            goto end;
        }
        part = part->next;
    }
    
    /* End */
end:
    if (rc_insert == SQLITE_OK) {
        ngx_str_set(&sql_end, "COMMIT");
    } else {
        ngx_str_set(&sql_end, "ROLLBACK");
    }
    rc_end = ngx_http_sqlitelog_sqlite3_exec(db->conn, sql_end, callback,
                                         callback_data, error_message_ptr, log);
    
    /*
     * We care more about the INSERT error than we do the COMMIT or ROLLBACK
     * error, so prefer returning the former
     */
    if (rc_insert != SQLITE_OK) {
        return rc_insert;
    }
    return rc_end;
}


/**
 * Determine if a database has WAL mode enabled.
 * 
 * @param   db      a database connection
 * @param   is      a flag for storing the result: 1 for true, 0 for false
 * @param   log     a log for writing error messages
 * @return          a SQLite3 return code
 */
static int
ngx_http_sqlitelog_db_is_wal(ngx_http_sqlitelog_db_t *db, ngx_flag_t *is,
    ngx_log_t *log)
{
    int            columns;
    int            rc_finalize;
    int            rc_prepare;
    int            rc_step;
    ngx_str_t      sql;
    sqlite3_stmt  *stmt;
    
    columns = 0;
    ngx_str_set(&sql, "PRAGMA journal_mode");
    
    rc_prepare = ngx_http_sqlitelog_sqlite3_prepare_v2(db->conn, sql, &stmt,
                                                       NULL, log);
    if (rc_prepare != SQLITE_OK) {
        goto finalize;
    }
    
    rc_step = ngx_http_sqlitelog_sqlite3_step(db->conn, stmt, log);
    if (rc_step != SQLITE_ROW) {
        goto finalize;
    }
    
    columns = sqlite3_column_count(stmt);
    if (columns != 1) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to get correct amount of columns from "
                      "\"%V\"; expected 1, got %d", &sql, columns);
        goto finalize;
    }
    
    const unsigned char* text = sqlite3_column_text(stmt, 0);
    if (ngx_strcmp(text, "wal") == 0) {
        *is = 1;
    }
    else {
        *is = 0;
    }
    
finalize:
    rc_finalize = ngx_http_sqlitelog_sqlite3_finalize(db->conn, stmt, log);
    if (rc_prepare != SQLITE_OK) {
        return rc_prepare;
    }
    if (rc_step != SQLITE_ROW) {
        return rc_step;
    }
    if (columns != 1) {
        return SQLITE_ERROR;
    }
    if (rc_finalize != SQLITE_OK) {
        return rc_finalize;
    }
    
    return SQLITE_OK;
}


/**
 * Get the busy timeout (ms) of this database.
 * 
 * @param   db      a database connection
 * @param   ms      a pointer for storing the result
 * @param   log     a log for writing error messages
 * @return          a SQLite3 return code
 */
static int
ngx_http_sqlitelog_db_get_busy_timeout(ngx_http_sqlitelog_db_t *db, int *ms,
    ngx_log_t *log)
{
    int            columns;
    int            rc_finalize;
    int            rc_prepare;
    int            rc_step;
    ngx_str_t      sql;
    sqlite3_stmt  *stmt;
    
    columns = 0;
    ngx_str_set(&sql, "PRAGMA busy_timeout");
    
    rc_prepare = ngx_http_sqlitelog_sqlite3_prepare_v2(db->conn, sql, &stmt,
                                                       NULL, log);
    if (rc_prepare != SQLITE_OK) {
        goto finalize;
    }
    
    rc_step = ngx_http_sqlitelog_sqlite3_step(db->conn, stmt, log);
    if (rc_step != SQLITE_ROW) {
        goto finalize;
    }
    
    columns = sqlite3_column_count(stmt);
    if (columns != 1) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to get correct amount of columns from "
                      "\"%V\"; expected 1, got %d", &sql, columns);
        goto finalize;
    }
    
    *ms = sqlite3_column_int(stmt, 0);
    
finalize:
    rc_finalize = ngx_http_sqlitelog_sqlite3_finalize(db->conn, stmt, log);
    if (rc_prepare != SQLITE_OK) {
        return rc_prepare;
    }
    if (rc_step != SQLITE_ROW) {
        return rc_step;
    }
    if (columns != 1) {
        return SQLITE_ERROR;
    }
    if (rc_finalize != SQLITE_OK) {
        return rc_finalize;
    }
    
    return SQLITE_OK;
}


/**
 * Execute a WAL checkpoint.
 * 
 * @param   db      a database connection
 * @param   log     a log for writing error messages
 * @return          a SQLite3 return code
 */
int
ngx_http_sqlitelog_db_checkpoint(ngx_http_sqlitelog_db_t *db, ngx_log_t *log)
{
    int          busy_timeout;
    int          emode;
    int         *pn_log;
    int         *pn_ckpt;
    int          rc_ckpt;
    int          rc_timeout;
    int          rc_wal;
    int          sleep_dur;
    int          total_slept;
    const char  *zdb;
    ngx_flag_t   is_wal;
    ngx_uint_t   r;
    
    /* WAL check */
    is_wal = 0;
    rc_wal = ngx_http_sqlitelog_db_is_wal(db, &is_wal, log);
    if (rc_wal != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to get journal mode prior to "
                      "WAL checkpoint");
        return rc_wal;
    }
    if (!is_wal) {
        return SQLITE_OK;
    }
    
    /* Get busy timeout */
    busy_timeout = 0;
    rc_timeout = ngx_http_sqlitelog_db_get_busy_timeout(db, &busy_timeout, log);
    if (rc_timeout != SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: failed to get busy timeout prior to "
                      "WAL checkpoint");
        return rc_timeout;
    }
    
    /* First attempt */
    zdb = NULL,
    emode = SQLITE_CHECKPOINT_TRUNCATE;
    pn_log = NULL;
    pn_ckpt = NULL;
    rc_ckpt = ngx_http_sqlitelog_sqlite3_wal_checkpoint_v2(db->conn, zdb, emode,
                                                          pn_log, pn_ckpt, log);
    
    /*
     * This loop serves as an application-level busy handler for
     * sqlite3_wal_checkpoint_v2() because the function, for whatever reason,
     * doesn't abide by busy timeouts set by sqlite3_busy_timeout().
     */
    total_slept = 0;
    while (rc_ckpt == SQLITE_BUSY) {
        r = ngx_random();
        while (r > 100) {
            r = r/10;
        }
        
        sleep_dur = 50 + r;
        ngx_msleep(sleep_dur);
        total_slept += sleep_dur;
        
        rc_ckpt = ngx_http_sqlitelog_sqlite3_wal_checkpoint_v2(db->conn, zdb,
                                                   emode, pn_log, pn_ckpt, log);
        if (total_slept >= busy_timeout) {
            break;
        }
    }
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "sqlitelog: checkpoint loop, slept %d ms", total_slept);

    /*
     * The loop ended unsuccessfully, but at the same time, it's impossible to
     * tell if another worker process was successful in their WAL checkpoint.
     * Print an error message that describes the sleeping time and return
     * SQLITE_OK, since an explicit "sqlitelog: failed to..." message could be
     * misleading.
     */
    if (rc_ckpt == SQLITE_BUSY) {
        ngx_log_error(NGX_LOG_ERR, log, 0,
                      "sqlitelog: checkpoint loop concluded with SQLITE_BUSY, "
                      "total slept: %d ms, busy timeout: %d ms",
                      total_slept, busy_timeout);
        return SQLITE_OK;
    }
    
    return rc_ckpt;
}
