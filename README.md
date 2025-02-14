# ngx-sqlitelog
* [Summary](#summary)
* [Directives](#directives)
    * [sqlitelog](#sqlitelog)
    * [sqlitelog_format](#sqlitelog_format)
    * [sqlitelog_async](#sqlitelog_async)
* [Install](#install)
* [Example](#example)
* [Errors](#errors)
* [Usage](#usage)
    * [Locations](#locations)
    * [Inheritance](#inheritance)
    * [WAL mode](#wal-mode)
    * [Logrotate](#logrotate)
    * [Column types](#column-types)

## Summary

This module uses SQLite format for access logs.
GitHub fork of https://git.serope.com/me/ngx-sqlitelog

## Installation

RPM-based systems:

```bash
dnf -y install https://extras.getpagespeed.com/release-latest.rpm 
dnf -y install nginx-module-log-sqlite
```

Enable the module by adding the following at the top of `/etc/nginx/nginx.conf`:

```nginx
load_module modules/ngx_http_sqlitelog_module.so;
```

### Configuration Example

```nginx
http {
    sqlitelog_format  myformat $remote_addr $time_local $host $request_uri $request_time $status;
    sqlitelog         /tmp/access.db myformat buffer=128K max=5;
    ...
    server {
        ...
    }
}
```

A few requests later...

```sh
$ sqlite3 /tmp/access.db ".mode columns" ".headers on" "SELECT * FROM myformat WHERE remote_addr='192.168.1.1'"
remote_addr  time_local                  host        request_uri                    request_time  status
-----------  --------------------------  ----------  -----------------------------  ------------  ------
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /                              0.0           200   
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /css/base.min.css              0.0           200   
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /index.js                      0.0           200   
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /js/modules/bullet-list.js     0.0           200   
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /bullet-lists/software.json    0.0           200   
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /bullet-lists/misc.json        0.0           200   
192.168.1.1  07/Jan/2024:08:28:09 -0500  serope.com  /favicon.ico                   0.0           200   
192.168.1.1  07/Jan/2024:08:28:10 -0500  serope.com  /banner-links.json             0.0           200   
192.168.1.1  07/Jan/2024:08:28:13 -0500  serope.com  /gpu/websocket?check=          3.108         502   
192.168.1.1  07/Jan/2024:08:28:14 -0500  serope.com  /gpu/                          0.0           200   
192.168.1.1  07/Jan/2024:08:28:14 -0500  serope.com  /gpu/style.css                 0.0           200   
192.168.1.1  07/Jan/2024:08:28:14 -0500  serope.com  /gpu/index.js                  0.0           200   
192.168.1.1  07/Jan/2024:08:28:14 -0500  serope.com  /gpu/smoothie.js               0.0           200   
192.168.1.1  07/Jan/2024:08:28:14 -0500  serope.com  /gpu/modules/watch-window.js   0.0           200   
192.168.1.1  07/Jan/2024:08:28:14 -0500  serope.com  /gpu/modules/watch-window.css  0.0           200   
192.168.1.1  07/Jan/2024:08:28:16 -0500  serope.com  /gpu/websocket                 2.079         502  
```

## Directives

### sqlitelog

* Syntax: `sqlitelog` *`path`* <code>[<i>format</i>]</code> <code>[buffer=<i>size</i> [max=<i>n</i>] [flush=<i>time</i>]]</code>  <code>[init=<i>script</i>]</code> <code>[if=<i>condition</i>]</code> | `off`
* Default: `sqlitelog` `off`
* Context: http, server

This directive defines a logging database.

The *`path`* parameter is the path of the database file. It must be located in a directory where the user or group that owns Nginx worker processes (defined by the [`user` directive](https://nginx.org/en/docs/ngx_core_module.html#user)) has write permission so that it can create the database file and any possible [temporary files](https://sqlite.org/tempfiles.html).

The *`format`* parameter is the name of a log format defined by the `sqlitelog_format` directive. If not given, the default combined format is used.

The `buffer` parameter creates a memory zone where log entries are batched together and written to the database in a single `BEGIN` ... `COMMIT` transaction. This greatly improves performance as grouped inserts [are faster](https://www.sqlite.org/faq.html#q19) than separate ones. The buffer is commited when one of the following happens: its *`size`* is exceeded; it accumulates *`n`* log entries; the flush *`time`* elapses; Nginx reloads or exits.

The `init` parameter is a path to a SQL script file which is executed on each database connection. This can be used to run [pragma commands](https://www.sqlite.org/pragma.html#toc) or to create additional tables, views, and triggers to complement the logging table; such statements should include `IF NOT EXISTS` since they can be executed more than once.

The `if` parameter sets a logging condition. Like in the standard [log module](https://nginx.org/en/docs/http/ngx_http_log_module.html#access_log), if *`condition`* evaluates to 0 or an empty string, logging is skipped for the current request.

### sqlitelog_format

* Syntax: `sqlitelog_format` *`table`* *`var1`* <code>[<i>type1</i>]</code> *`var2`* <code>[<i>type2</i>]</code> ... *`varN`* <code>[<i>typeN</i>]</code>
* Default: `sqlitelog_format` `combined` `$remote_addr` `$remote_user` `$time_local` `$request` `$status` `$body_bytes_sent` `$http_referer` `$http_user_agent`
* Context: http

This directive defines a logging table.

The first argument is the table's name. The remaining arguments are variables with optional column types. Some variables have [preset column types](#column-types), otherwise the default is `TEXT`. If a variable is `BLOB` type, its value is written as unescaped bytes.

### sqlitelog_async

* Syntax: `sqlitelog_async` *`pool`* | `on` | `off`
* Default: `sqlitelog_async` `off`
* Context: http

This directive enables a [thread pool](https://nginx.org/en/docs/ngx_core_module.html#thread_pool), allowing SQLite file writes to occur without blocking. The argument can be an existing *`pool`* name, `on` for the default pool, or `off`. This directive is only available if Nginx is compiled with `--with-threads`.

## Errors

When a SQLite error occurs, the module is disabled (equivalent to `sqlitelog off`) for the worker process that encountered the error. This is to prevent error.log from being quickly flooded with error messages if the database is unusable (e.g. located in a directory where worker processes don't have write permission).

* [SQLITE_ERROR (1)](https://www.sqlite.org/rescode.html#error): This is a generic error code that covers several cases, such as SQL syntax errors in an `init` script.
* [SQLITE_BUSY (5)](https://www.sqlite.org/rescode.html#busy): Multiple worker processes attempted to use the database simultaneously and exceeded the busy timeout (1000 ms by default). This can be solved by creating a `buffer` to speed up insertions or by setting a longer timeout with `PRAGMA busy_timeout` in an `init` script.
* [SQLITE_READONLY (8)](https://www.sqlite.org/rescode.html#readonly): Nginx can open the database, but can't write to it. This is likely due to file permissions.
* [SQLITE_CANTOPEN (14)](https://www.sqlite.org/rescode.html#cantopen): Nginx can't open or create the database. This is likely due to directory permissions. The user or group that owns worker processes (defined by the [`user` directive](https://nginx.org/en/docs/ngx_core_module.html#user)) must have write permission on the directory.
* [SQLITE_READONLY_DBMOVED (1032)](https://www.sqlite.org/rescode.html#readonly_dbmoved): The file was moved, renamed, or deleted at runtime. When this happens, Nginx attempts to recreate the file; if successful, the error is ignored and logging continues normally.

## Usage

### Locations

The `sqlitelog` directive can't be used in location contexts, but a regex condition can achieve a similar effect. In this example, only requests that start with "/mylocation" are logged.

<!--
Don't forget to compile Nginx with `--with-pcre`!
-->

```nginx
map $request_uri $is_my_loc {
    default            0;
    ~^/mylocation.*$   1;
}

sqlitelog access.db if=$is_my_loc;
```

### Inheritance

Only one `sqlitelog` is allowed per context, with lower contexts taking priority. In this example, requests to server A are logged to global.db, while requests to server B are logged to b.db.

```nginx
http {
    sqlitelog global.db;
    ...
    
    server {
        server_name a;
        ...
    }
    
    server {
        server_name b;
        sqlitelog b.db;
        ....
    }
````

### WAL mode

[WAL mode](https://www.sqlite.org/wal.html) is enabled by `PRAGMA journal_mode=wal` in an `init` script. [WAL checkpointing](https://www.sqlite.org/wal.html#ckpt) occurs when Nginx reloads or exits.

### Logrotate

[Logrotate](https://man.archlinux.org/man/logrotate.8) should be configured to stop Nginx, rotate logs, and start Nginx again. This way, Nginx gracefully closes its connections to the previous day's database(s) and opens new ones to the current day's database(s).

Below is an example script for Debian (`/etc/logrotate.d/nginx`). It assumes the worker process user, `www-data`, has been granted write permission on `/var/log/nginx`, which is normally only writeable by `root`.

```sh
/var/log/nginx/*.log
/var/log/nginx/*.db
{
    daily
    missingok
    rotate 52
    compress
    delaycompress
    notifempty
    create 640 www-data adm
    sharedscripts
    
    # Force Logrotate to work in this directory even though
    # its permissions have been modified to allow a non-root
    # user to write in it
    su root adm
    
    # Send a quit signal to Nginx and wait for its PID file
    # to be destroyed
    firstaction
        systemctl stop nginx.service
        while [ -f /var/run/nginx.pid ]; do  
            sleep 0.1s
        done
    endscript
    
    # Start Nginx again
    lastaction
        systemctl restart nginx.service
    endscript
}
```

### Column types

The following variables have preset column types, but can be overridden if needed.

Variable | Type
-------- | ----
[$binary_remote_addr](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_binary_remote_addr) | `BLOB`
[$body_bytes_sent](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_body_bytes_sent) | `INTEGER`
[$bytes_sent](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_bytes_sent) | `INTEGER`
[$connection](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_connection) | `INTEGER`
[$connection_requests](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_connection_requests) | `INTEGER`
[$connection_time](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_connection_time) | `REAL`
[$connections_active](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html#var_connections_active) | `INTEGER`
[$connections_reading](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html#var_connections_reading) | `INTEGER`
[$connections_waiting](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html#var_connections_waiting) | `INTEGER`
[$connections_writing](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html#var_connections_writing) | `INTEGER`
[$content_length](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_content_length) | `INTEGER`
[$gzip_ratio](https://nginx.org/en/docs/http/ngx_http_gzip_module.html#var_gzip_ratio) | `REAL`
[$limit_rate](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_limit_rate) | `INTEGER`
[$msec](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_msec) | `REAL`
[$pid](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_pid) | `INTEGER`
[$proxy_port](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#var_proxy_port) | `INTEGER`
[$proxy_protocol_port](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_proxy_protocol_port) | `INTEGER`
[$proxy_protocol_server_port](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_proxy_protocol_server_port) | `INTEGER` 
[$remote_port](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_remote_port) | `INTEGER`
[$request_time](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_request_time) | `REAL`
[$server_port](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_server_port) | `INTEGER`
[$status](https://nginx.org/en/docs/http/ngx_http_core_module.html#var_status) | `INTEGER`
