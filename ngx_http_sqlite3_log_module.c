/*
 * ngx_sqlite3_log
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <stdio.h>
#include <sqlite3.h>
#include "ngx_log.h"

/*
 * This is the main configuration struct, which holds module-specific data.
 */
typedef struct {
    ngx_flag_t   enabled;
    ngx_str_t    file;
    ngx_array_t  *columns;
    ngx_array_t  *indeces;
    ngx_flag_t   null;
    ngx_int_t    transaction_max;
    
    sqlite3      *db;
    ngx_int_t    transaction_counter;
} ngx_http_sqlite3_log_main_conf_t;

/*
 * Module functions
 */
static char*     ngx_http_sqlite3_log_set_columns(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_sqlite3_log_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_sqlite3_log_handler(ngx_http_request_t *r);
static void*     ngx_http_sqlite3_log_create_main_conf(ngx_conf_t *cf);
static char*     ngx_http_sqlite3_log_init_main_conf(ngx_conf_t *cf, void *conf);

/*
 * SQL utility functions
 */
u_char* ngx_http_sqlite3_log_sql_create_table_if_not_exists(ngx_pool_t* pool, ngx_array_t* columns);
u_char* ngx_http_sqlite3_log_sql_insert(ngx_pool_t* pool, ngx_uint_t n);

/*
 * String utility functions
 */
u_char*      ngx_http_sqlite3_log_strings_join(ngx_pool_t *pool, ngx_array_t *strings, const u_char delimiter);
size_t       ngx_http_sqlite3_log_strings_len_total(ngx_array_t *strings);
ngx_array_t* ngx_http_sqlite3_log_strings_array(ngx_pool_t *pool, const char* s, size_t s_len, ngx_uint_t n);

/*
 * SQLite3 utility functions
 */
int  ngx_http_sqlite3_log_db_open(ngx_pool_t *pool, ngx_http_sqlite3_log_main_conf_t *mc);
int  ngx_http_sqlite3_log_db_create_table_if_not_exists(ngx_pool_t *pool, sqlite3 *db, ngx_array_t* columns);
int  ngx_http_sqlite3_log_db_transaction_begin(sqlite3 *db);
int  ngx_http_sqlite3_log_db_transaction_commit(sqlite3 *db);
int  ngx_http_sqlite3_log_db_insert(ngx_http_request_t *r, ngx_http_sqlite3_log_main_conf_t *mc);
void ngx_http_sqlite3_log_db_cleanup(void *data);

/*
 * This array defines the directives provided by the module. See the README.md
 * file for details on how they should be used in a config file.
 */
static ngx_command_t  ngx_http_sqlite3_log_commands[] = {
    {
        ngx_string("sqlite3_log"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_sqlite3_log_main_conf_t, enabled),
        NULL
    },
    {
        ngx_string("sqlite3_log_db"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_sqlite3_log_main_conf_t, file),
        NULL
    },
    {
        ngx_string("sqlite3_log_columns"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
        ngx_http_sqlite3_log_set_columns,
        0,
        0,
        NULL
    },
    {
        ngx_string("sqlite3_log_null"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_flag_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_sqlite3_log_main_conf_t, null),
        NULL
    },
    {
        ngx_string("sqlite3_log_transaction"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_sqlite3_log_main_conf_t, transaction_max),
        NULL
    },
    ngx_null_command
};

/*
 * This is the module context.
 * 
 * ngx_http_sqlite3_log_init() is the postconfiguration function. When Nginx
 * receives a request, it goes through a series of phases, the last one being
 * NGX_HTTP_LOG_PHASE. Nginx also keeps a list of handler functions to call
 * during each phase. This function, ngx_http_sqlite3_log_init(), appends this
 * module's handler function to the NGX_HTTP_LOG_PHASE list. The standard log
 * module and the 3rd-party log_if module were used as references on how to
 * do this.
 * 
 * ngx_http_sqlite3_log_create_main_conf() nullifies, or "unsets", all of the
 * fields in the configuration struct. See the function body for a comment
 * describing how that's done.
 * 
 * ngx_sqlite3_log_init_main_conf() sets the default values for the configuration
 * struct if the user didn't supply their own in the config file. For example,
 * if the user didn't use "sqlite3_log_db" to set a filename, this function
 * sets it as "/tmp/access.db".
 */
static ngx_http_module_t  ngx_http_sqlite3_log_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_sqlite3_log_init,             /* postconfiguration */

    ngx_http_sqlite3_log_create_main_conf, /* create main configuration */
    ngx_http_sqlite3_log_init_main_conf,   /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};

/*
 * This is the module definition.
 * 
 * ngx_http_sqlite3_log_exit_master() is called when the Nginx master process
 * exits. It performs cleanup duties (i.e. executing any SQL inserts waiting on
 * the queue, then closing the database).
 */
ngx_module_t  ngx_http_sqlite3_log_module = {
    NGX_MODULE_V1,
    &ngx_http_sqlite3_log_module_ctx,      /* module context */
    ngx_http_sqlite3_log_commands,         /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_sqlite3_log_init(ngx_conf_t *cf) {
    /*
     * Get the core main conf.
     */
    ngx_http_core_main_conf_t *cmc = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    
    /*
     * We will attach our ngx_http_sqlite3_log_handler to the list of functions
     * that get called when Nginx receives a request and enters the
     * NGX_HTTP_LOG_PHASE.
     */
    ngx_http_handler_pt *h = ngx_array_push(&cmc->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }
    *h = ngx_http_sqlite3_log_handler;
    
    return NGX_OK;
}

static ngx_int_t ngx_http_sqlite3_log_handler(ngx_http_request_t *r) {
    /*
     * If the module is off, exit immediately.
     */
    puts("ngx_int_t ngx_http_sqlite3_log_handler");
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "http sqlite3_log handler");
    ngx_http_sqlite3_log_main_conf_t *mc = ngx_http_get_module_main_conf(r, ngx_http_sqlite3_log_module);
    if (!mc->enabled) {
        return NGX_OK;
    }
    
    /*
     * If the database hasn't been opened, create and/or open it now.
     */
    if (mc->db == NULL) {
        int rc = ngx_http_sqlite3_log_db_open(r->pool, mc);
        printf("ngx_http_sqlite3_log_db_open() returned %d \n", rc);
        if (rc != SQLITE_OK) {
            return NGX_ERROR;
        }
    }
    
    /*
     * If the transaction counter is currently 0, begin a new transaction.
     * Then increment the counter.
     */
    printf("transaction_counter = %ld -> ", mc->transaction_counter);
    if (mc->transaction_counter == 0) {
        int rc = ngx_http_sqlite3_log_db_transaction_begin(mc->db);
        printf("ngx_http_sqlite3_log_db_transaction_begin() returned %d \n", rc);
        if (rc != SQLITE_OK) {
            return NGX_ERROR;
        }
    }
    mc->transaction_counter++;
    printf("%ld \n", mc->transaction_counter);
    
    
    /*
     * Write/insert the log data.
     */
    int rc = ngx_http_sqlite3_log_db_insert(r, mc);
    printf("ngx_http_sqlite3_log_db_insert() returned %d \n", rc);
    printf("%s \n", sqlite3_errmsg(mc->db));
    if (rc != SQLITE_OK) {
        return NGX_ERROR;
    }
    
    /*
     * If we've hit the transaction limit, commit and reset the counter.
     */
    if (mc->transaction_counter >= mc->transaction_max) {
        int rc = ngx_http_sqlite3_log_db_transaction_commit(mc->db);
        printf("ngx_http_sqlite3_log_transaction_commit() returned %d \n", rc);
        if (rc != SQLITE_OK) {
            return NGX_ERROR;
        }
        mc->transaction_counter = 0;
    }
    return NGX_OK;
}

/*
 * This is the set function for the "sqlite3_log_columns" directive.
 */
static char* ngx_http_sqlite3_log_set_columns(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    /*
     * Get the user's arguments.
     */
    printf("ngx_http_sqlite3_log_set_columns \n");
    printf("ngx_http_sqlite3_log_set_columns get arguments \n");
    ngx_str_t* args = cf->args->elts;
    ngx_uint_t args_len = cf->args->nelts;
    printf("ngx_http_sqlite3_log_set_columns args_len = %ld \n", args_len);
    
    /*
     * Allocate the "columns" and "indeces" arrays to hold each argument and
     * its index.
     */
    printf("ngx_http_sqlite3_log_set_columns allocate arrays \n");
    ngx_http_sqlite3_log_main_conf_t* mc = conf;
    mc->columns = ngx_array_create(cf->pool, args_len, sizeof(ngx_str_t));
    mc->indeces = ngx_array_create(cf->pool, args_len, sizeof(ngx_int_t));
    
    /*
     * Perform error-checking on each argument.
     */
    printf("ngx_http_sqlite3_log_set_columns perform error-checking \n");
    for (ngx_uint_t i=0; i<args_len; i++) {
        /*
         * Skip the first argument, which is the directive itself.
         */
        if (i == 0) {
            continue;
        }
        
        /*
         * The argument must be in the form of "$abc", so its length must
         * be >= 2.
         */
        printf("   %ld length \n", i);
        ngx_str_t arg = args[i];
        printf("   %ld len=%ld \n", i, arg.len);
        if (arg.len <= 1) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "sqlite3_log_columns, arg %d, \"%s\" is invalid", i, arg.data);
            return NGX_CONF_ERROR;
        }
        
        /*
         * The first character must be "$".
         */
        printf("   %ld first char \n", i);
        printf("   %ld arg.len = %ld \n", i, arg.len);
        u_char first = arg.data[0];
        if (first != '$') {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "sqlite3_log_columns, arg %d, \"%s\" lacks \"$\" prefix", i, arg.data);
            return NGX_CONF_ERROR;
        }
        
        /*
         * Remove the '$' prefix.
         */
        printf("   %ld prefix \n", i);
        ngx_str_t nopfx = ngx_string(arg.data+1);
        nopfx.len = arg.len-1;
        
        /*
         * Check if the variable exists in Nginx's list of known variables.
         */
        printf("   %ld index \n", i);
        ngx_int_t index = ngx_http_get_variable_index(cf, &nopfx);
        if (index == NGX_ERROR) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "sqlite3_log_columns, arg %d, \"%s\" not recognized", i, arg.data);
            return NGX_CONF_ERROR;
        }
        
        /*
         * If all of the above tests passed, append the argument (without "$")
         * and its index to "columns" and "indeces", respectively.
         */
        printf("   %ld append to arrays \n", i);
        ngx_str_t* col = ngx_array_push(mc->columns);
        ngx_int_t* idx = ngx_array_push(mc->indeces);
        *col = nopfx;
        *idx = index;
    }

    return NGX_CONF_OK;
}

static void* ngx_http_sqlite3_log_create_main_conf(ngx_conf_t *cf) {
    /*
     * There's a lot going on here that warrants some explanation.
     * 
     * The first thing that all Nginx modules do in their create_main_conf()
     * function is call ngx_pcalloc() as follows. This initializes almost every
     * member of the configuration struct to a zero value...
     * 
     * ...AND THEN, you must "unset" any flags, uints, pointers, size_t's, and
     * msecs that your configuration contains, using the following macros that
     * are listed in core/ngx_conf_file.h, respectively:
     * 
     *  NGX_CONF_UNSET       -1
     *  NGX_CONF_UNSET_UINT  (ngx_uint_t) -1
     *  NGX_CONF_UNSET_PTR   (void *) -1
     *  NGX_CONF_UNSET_SIZE  (size_t) -1
     *  NGX_CONF_UNSET_MSEC  (ngx_msec_t) -1
     *
     * If you want to see an example of this in action in the Nginx codebase
     * itself, look at at the create_main_conf() function in 
     * http/modules/ngx_http_proxy_module.c.
     * 
     * In this case, ngx_pcalloc() is intializing the following.
     * 
     *      mc->file = {NULL, 0};
     * 
     * The remaining members must be explicitely unset via the forementioned
     * macros.
     */
    printf("ngx_http_sqlite3_log_create_main_conf() \n");
    ngx_http_sqlite3_log_main_conf_t* mc = ngx_pcalloc(cf->pool, sizeof(ngx_http_sqlite3_log_main_conf_t));
    if (mc == NULL) {
        return NULL;
    }
    
    mc->enabled         = NGX_CONF_UNSET;
    mc->columns         = NGX_CONF_UNSET_PTR;
    mc->indeces         = NGX_CONF_UNSET_PTR;
    mc->null            = NGX_CONF_UNSET;
    mc->transaction_max = NGX_CONF_UNSET;
    return mc;
}

/*
 * ngx_http_sqlite3_log_init_main_conf() sets the default values for all
 * commands/directives.
 * 
 * Despite having "init" in its name, this function actually gets called AFTER
 * each command's "set" function, not before.
 * 
 */
static char* ngx_http_sqlite3_log_init_main_conf(ngx_conf_t *cf, void *conf) {
    printf("ngx_http_sqlite3_log_init_main_conf() \n");
    ngx_http_sqlite3_log_main_conf_t *mc = conf;
    
    /*
     * enabled
     */
    if (mc->enabled == NGX_CONF_UNSET) {
        mc->enabled = 0;
    }
    printf("ngx_http_sqlite3_log_init_main_conf() mc->enabled   = %ld \n", mc->enabled);
    
    /*
     * file
     */
    if (mc->file.data == NULL) {
        ngx_str_t default_file = ngx_string("/tmp/access.db");
        mc->file = default_file;
    }
    printf("ngx_http_sqlite3_log_init_main_conf() mc->file      = %s (len=%ld) \n", mc->file.data, mc->file.len);
    
    /*
     * column
     */
    if (mc->columns == NGX_CONF_UNSET_PTR) {
        const char* default_vars[] = {
            "remote_addr",
            "remote_user",
            "time_local",
            "request",
            "status",
            "body_bytes_sent",
            "http_referer",
            "http_user_agent"
        };
        const int n = 8;
        mc->columns = ngx_array_create(cf->pool, n, sizeof(ngx_str_t));
        mc->indeces = ngx_array_create(cf->pool, n, sizeof(ngx_int_t));
        ngx_str_t* s = ngx_array_push_n(mc->columns, n);
        ngx_int_t* i = ngx_array_push_n(mc->indeces, n);
        for (int j=0; j<n; j++) {
            /*
             * Create an Nginx string from the current element.
             * For some reason, ngx_str_set() isn't setting the len properly.
             */
            const char* dv = default_vars[j];
            ngx_str_set(s, dv);
            s->len = strlen(dv);
            
            /*
             * Just to be safe, check if Nginx recognizes this variable. If
             * this fails, Nginx aborts its startup process with the following
             * message.
             * nginx: [emerg] unknown "abc" variable
             */
            ngx_int_t index = ngx_http_get_variable_index(cf, s);
            if (index == NGX_ERROR) {
                return NGX_CONF_ERROR;
            }
            
            /*
             * Assign the index to the current slot in the indeces array.
             * Then move onto the next element in both arrays.
             */
            *i = index;
            i++;
            s++;
        }
    }
    printf("ngx_http_sqlite3_log_init_main_conf() mc->columns   = %ld nelts \n", mc->columns->nelts);
    
    /*
     * null
     */
    if (mc->null == NGX_CONF_UNSET) {
        mc->null = 0;
    }
    printf("ngx_http_sqlite3_log_init_main_conf() mc->null      = %ld \n", mc->null);
    
    /*
     * queue
     */
    if (mc->transaction_max == NGX_CONF_UNSET) {
        mc->transaction_max = 1;
    }
    printf("ngx_http_sqlite3_log_init_main_conf() mc->transaction_max = %ld \n", mc->transaction_max);
    
    /*
     * non-directive fields
     */
    mc->db = NULL;
    mc->transaction_counter = 0;
    
    /*
     * Assign the cleanup function to the Nginx global cycle's pool.
     */
    ngx_pool_cleanup_t *cleanup = NULL;
    cleanup = ngx_pool_cleanup_add(cf->pool, 0);
    if (cleanup == NULL) {
        return NGX_CONF_ERROR;
    }
    cleanup->handler = ngx_http_sqlite3_log_db_cleanup;
    cleanup->data = mc;
    
    return NGX_CONF_OK;
}

/*
 * Open the database file for the configuration struct.
 * 
 * @param   pool    an Nginx memory pool to borrow memory from
 * @param   mc      the module's main configuration struct
 * @return          a SQLite3 status code
 */
int ngx_http_sqlite3_log_db_open(ngx_pool_t *pool, ngx_http_sqlite3_log_main_conf_t *mc) {
    /*
     * Open the database connection.
     */
    char* filename = (char*) mc->file.data;
    int filemode = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    const char* vfs_module = NULL;
    int rc = sqlite3_open_v2(filename, &mc->db, filemode, vfs_module);
    if (rc != SQLITE_OK) {
        printf("sqlite3_open_v2(%s) failed with code %d \n", filename, rc);
        printf("%s \n", sqlite3_errmsg(mc->db));
        sqlite3_close(mc->db);
        return rc;
    }
    
    /*
     * Create the log table if it doesn't already exist.
     */
    rc = ngx_http_sqlite3_log_db_create_table_if_not_exists(pool, mc->db, mc->columns);
    if (rc != SQLITE_OK) {
        return rc;
    }
    return rc;
}

/*
 * Check if the log table exists.
 * 
 * @param   db      the database to check
 * @param   exists  a pointer to an integer where 1 or 0 will be stored
 * @return          a SQLite3 return code
 */
int ngx_http_sqlite3_log_db_table_exists(sqlite3 *db, int *exists) {
    /*
     * Prepare the SQL and result values.
     */
    const char* sql = ""
    "SELECT EXISTS ("
    "   SELECT *"
    "   FROM sqlite_master"
    "   WHERE type='table' and name='log'"
    ")";
    char** result = NULL;
    int rows = 0;
    int cols = 0;
    char* error_message = NULL;
    
    /*
     * Execute the SQL.
     */
    int rc = sqlite3_get_table(db, sql, &result, &rows, &cols, &error_message);
    if (rc != SQLITE_OK) {
        if (error_message) {
            printf("error_message = %s \n", error_message);
            goto end;
        }
    }
    
    /*
     * The result should be the following table:
     * 
     * EXISTS
     * ------
     * 1
     */
    if (rows != 2) {
        printf("rows is %d instead of 2 \n", rows);
    }
    else if (cols != 1) {
        printf("cols is %d instead of 1 \n", cols);
    }
    else {
        int cmp = ngx_strcmp(result[1], "1");
        if (cmp == 0) {
            *exists = 1;
        }
        else {
            *exists = 0;
        }
    }
    
    /*
     * Free resources and return.
     */
    end:
    sqlite3_free(error_message);
    sqlite3_free_table(result);
    return rc;
}

/*
 * Create the "log" table according to the arguments provided in the user's
 * .conf file.
 *
 * @param   pool    an Nginx memory pool to borrow memory from
 * @param   db      a connection to the database
 * @param   columns an Nginx array of column names
 * @return          a SQLite3 return code
 */
int ngx_http_sqlite3_log_db_create_table_if_not_exists(ngx_pool_t *pool, sqlite3 *db, ngx_array_t* columns) {
    /*
     * Prepare the "CREATE TABLE IF NOT EXISTS" statement.
     */
    char* sql = (char*) ngx_http_sqlite3_log_sql_create_table_if_not_exists(pool, columns);
    int (*callback)(void* data, int cols, char** rowdata, char** column_names) = NULL;
    void *callback_data = NULL;
    char* error_message = NULL;
    
    /*
     * Execute the statement.
     */
    int rc = sqlite3_exec(db, sql, callback, callback_data, &error_message);
    if (rc != SQLITE_OK) {
        if (error_message) {
            puts(error_message);
            sqlite3_free(error_message);
        }
    }
    return rc;
}

/*
 * Execute a "BEGIN TRANSACTION" statement.
 *
 * @param   db      a connection to the database
 * @return          a SQLite3 return code
 */
int ngx_http_sqlite3_log_db_transaction_begin(sqlite3 *db) {
    /*
     * Prepare the exec args.
     */
    char* sql = "BEGIN TRANSACTION";
    int (*callback)(void* data, int cols, char** rowdata, char** column_names) = NULL;
    void *callback_data = NULL;
    char* error_message = NULL;
    
    /*
     * Execute the statement.
     */
    int rc = sqlite3_exec(db, sql, callback, callback_data, &error_message);
    if (rc != SQLITE_OK) {
        if (error_message) {
            puts(error_message);
            sqlite3_free(error_message);
        }
    }
    return rc;
}

/*
 * Execute a "COMMIT" statement.
 *
 * @param   db      a connection to the database
 * @return          a SQLite3 return code
 */
int ngx_http_sqlite3_log_db_transaction_commit(sqlite3 *db) {
    /*
     * Prepare the exec args.
     */
    char* sql = "COMMIT";
    int (*callback)(void* data, int cols, char** rowdata, char** column_names) = NULL;
    void *callback_data = NULL;
    char* error_message = NULL;
    
    /*
     * Execute the statement.
     */
    int rc = sqlite3_exec(db, sql, callback, callback_data, &error_message);
    if (rc != SQLITE_OK) {
        if (error_message) {
            puts(error_message);
            sqlite3_free(error_message);
        }
    }
    return rc;
}

/*
 * Insert a record into the database.
 * 
 * This function can return 2 possible success codes:
 *      1. SQLITE_OK from sqlite3_prepare_v2() and sqlite3_bind_text()
 *      2. SQLITE_DONE from sqlite3_step()
 * 
 * @param   r   the request to log
 * @param   mc  the module's main configuration struct
 * @return      a SQLite3 return code
 */
int ngx_http_sqlite3_log_db_insert(ngx_http_request_t *r, ngx_http_sqlite3_log_main_conf_t *mc) {
    /*
     * Prepare the INSERT statement.
     */
    char* sql_insert = (char*) ngx_http_sqlite3_log_sql_insert(r->pool, mc->indeces->nelts);
    size_t sql_insert_len = ngx_strlen(sql_insert);
    printf("mc->indeces->nelts = %ld \n", mc->indeces->nelts);
    printf("sql_insert = %s \n", sql_insert);
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(mc->db, (char*) sql_insert, sql_insert_len, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("sqlite3_prepare_v2() failed with code %d \n", rc);
        printf("%s \n", sqlite3_errmsg(mc->db));
        goto end;
    }
    
    /*
     * Bind each log value to the INSERT statement.
     */
    ngx_str_t* s = mc->columns->elts;
    ngx_int_t* i = mc->indeces->elts;
    for (ngx_uint_t j=0; j<mc->columns->nelts; j++) {
        /*
         * Get the current value.
         */
        size_t var_len = ngx_http_log_variable_getlen(r, *i);
        u_char* buf = ngx_pcalloc(r->pool, var_len+1);
        ngx_http_log_variable(r, buf, *i);
        
        /*
         * Bind it to the INSERT statement.
         */
        rc = sqlite3_bind_text(stmt, j+1, (char*) buf, var_len, SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            printf("sqlite3_bind_text(%s) failed with code %d \n", buf, rc);
            printf("%s \n", sqlite3_errmsg(mc->db));
            goto end;
        }
        printf("    %ld %s = %s\n", *i, s->data, buf);
        
        /*
         * Next
         */
        s++;
        i++;
    }
    
    /*
     * Execute the statement.
     */
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("sqlite3_step() failed with code %d \n", rc);
        printf("%s \n", sqlite3_errmsg(mc->db));
        goto end;
    }
    
    /*
     * SQLite3 has 3 different codes indicating success:
     * 
     *  1. SQLITE_OK
     *  2. SQLITE_ROW
     *  3. SQLITE_DONE
     * 
     * To keep things simple, all success codes will be decayed to SQLITE_OK.
     */
    end:
    if (rc == SQLITE_ROW || rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    }
    sqlite3_finalize(stmt);
    return rc;
}

/*
 * Execute any pending inserts and close the database.
 * 
 * @param   data    the module's custom configuration struct
 */
void ngx_http_sqlite3_log_db_cleanup(void *data) {
    /*
     * If the database was never opened, there's nothing to do.
     */
    puts("ngx_http_sqlite3_log_db_cleanup()");
    ngx_http_sqlite3_log_main_conf_t *mc = data;
    if (mc->db == NULL) {
        return;
    };
    
    /*
     * If there are any pending inserts, commit them now.
     */
    if (mc->transaction_counter > 0) {
        int rc = ngx_http_sqlite3_log_db_transaction_commit(mc->db);
        if (rc != SQLITE_OK) {
            //...
        }
    }
    
    /*
     * Close the database.
     */
    int rc = sqlite3_close(mc->db);
    if (rc != SQLITE_OK) {
        //...
    }
}

/*
 * Create a "CREATE TABLE IF NOT EXISTS" string with the given column names.
 * 
 * @param   pool    an Nginx memory pool to borrow memory from
 * @param   columns an Nginx array of column names
 * @return          a heap string allocated in the given pool
 */
u_char* ngx_http_sqlite3_log_sql_create_table_if_not_exists(ngx_pool_t* pool, ngx_array_t* columns) {
    /*
     * Count how many characters the SQL statement will have.
     */
    ngx_str_t init_text = ngx_string("CREATE TABLE IF NOT EXISTS log (");
    size_t sql_len = init_text.len;                          // initial text
    sql_len += ngx_http_sqlite3_log_strings_len_total(columns); // column names
    sql_len += columns->nelts;                               // commas
    sql_len += 1;                                            // closing parenthesis
    sql_len += 1;                                            // terminating byte
    
    /*
     * Allocate memory for the statement.
     */
    u_char *buf = ngx_pcalloc(pool, sql_len);
    
    /*
     * Create the statement.
     */
    u_char *joined = ngx_http_sqlite3_log_strings_join(pool, columns, ',');
    ngx_sprintf(buf, "CREATE TABLE IF NOT EXISTS log (%s)", joined);
    return buf;
}

/*
 * Create a SQL string in the form of "INSERT INTO log VALUES (?, ?, ?)",
 * where n is the amount of question/placeholder symbols.
 * 
 * @param   pool    an Nginx memory pool to borrow memory from
 * @param   n       the amount of question/placeholder symbols
 * @return          a heap string allocated in the given pool
 */
u_char* ngx_http_sqlite3_log_sql_insert(ngx_pool_t* pool, ngx_uint_t n) {
    /*
     * Count how many characters the SQL statement will have.
     */
    ngx_str_t init_text = ngx_string("INSERT INTO log VALUES (");
    size_t sql_len = init_text.len; // initial text
    sql_len += n;                   // "?"
    sql_len += n;                   // ","
    sql_len += 1;                   // ")"
    
    /*
     * Allocate memory for the statement.
     */
    u_char *buf = ngx_pcalloc(pool, sql_len);
    
    /*
     * Create the statement.
     */
    ngx_array_t *questions_arr = ngx_http_sqlite3_log_strings_array(pool, "?", 1, n);
    u_char *questions_str = ngx_http_sqlite3_log_strings_join(pool, questions_arr, ',');
    ngx_sprintf(buf, "INSERT INTO log VALUES (%s)", questions_str);
    return buf;
}

/*
 * Join multiple strings.
 * 
 * @param   pool        an Nginx memory pool to borrow memory from
 * @param   strings     an Nginx array of strings to join together
 * @param   delimiter   the delimiter symbol to separate each string
 * @return              a heap string allocated in the given pool
 */
u_char* ngx_http_sqlite3_log_strings_join(ngx_pool_t *pool, ngx_array_t *strings, const u_char delimiter) {
    /*
     * Count how many characters the final string will have.
     */
    size_t len = ngx_http_sqlite3_log_strings_len_total(strings);
    len += (size_t) strings->nelts;
    
    /*
     * Allocate memory for the final string.
     */
    u_char *buf = ngx_pcalloc(pool, len);
    ngx_uint_t j = 0; // current position
    
    /*
     * Join the strings.
     */
    ngx_str_t *s = strings->elts;
    for (ngx_uint_t i=0; i<strings->nelts; i++) {
        /*
         * Append the current string to the buffer.
         */
        ngx_memcpy(&buf[j], s->data, s->len);
        j += s->len;
        
        /*
         * If this isn't the final element, append the delimiter symbol.
         */
        ngx_int_t is_final_element = (i == strings->nelts-1);
        if (!is_final_element) {
            buf[j] = delimiter;
            j += 1;
        }
        s++;
    }
    return buf;
}

/*
 * Sum the lengths of multiple strings.
 * 
 * @param   strings     an Nginx array of Nginx strings
 * @return              the total of each string's len field
 */
size_t ngx_http_sqlite3_log_strings_len_total(ngx_array_t *strings) {
    size_t sum = 0;
    
    ngx_str_t* s = strings->elts;
    for (ngx_uint_t i=0; i<strings->nelts; i++) {
        sum += s->len;
        s++;
    }
    
    return sum;
}

/*
 * Create an array containing n copies of s.
 * 
 * @param   pool        an Nginx memory pool to borrow memory from
 * @param   s           the string to create copies of
 * @param   s_len       the length of s
 * @param   n           how many copies to make
 * @return              an Nginx array allocated in the given pool
 */
ngx_array_t* ngx_http_sqlite3_log_strings_array(ngx_pool_t *pool, const char* s, size_t s_len, ngx_uint_t n) {
    ngx_array_t* arr = ngx_array_create(pool, n, sizeof(ngx_str_t));
    for (ngx_uint_t i=0; i<n; i++) {
        ngx_str_t* ns = ngx_array_push(arr);
        ngx_str_set(ns, s);
        ns->len = s_len; // ngx_str_set doesn't set the len properly for some reason
    }
    return arr;
}
