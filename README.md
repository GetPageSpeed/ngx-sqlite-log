
# ngx-sqlite3-log

* [Summary](#summary)
* [Directives](#directives)
    * [sqlite3_log](#sqlite3_log)
    * [sqlite3_log_db](#sqlite3_log_db)
    * [sqlite3_log_columns](#sqlite3_log_columns)
    * [sqlite3_log_null](#sqlite3_log_null)
    * [sqlite3_log_transaction](#sqlite3_log_transaction)
* [Example](#example)
* [Compilation](#compilation)
* [Why this module is good](#why-this-module-is-good)
* [Why this module is bad](#why-this-module-is-bad)

## Summary

This module writes log data to a SQLite3 database.

## Directives

### sqlite3_log

* Context: main
* Type: flag
* Default: `off`
* Arguments: 1

`sqlite3_log` enables the module.

### sqlite3_log_db

* Context: main
* Type: file path
* Default: `"/tmp/access.db"`
* Arguments: 1

`sqlite3_log_db` specifies the file to create or write to. The database schema is a single table named `log` whose columns are defined by `sqlite3_log_columns`.

### sqlite3_log_columns

* Context: main
* Type: list
* Default: `$remote_addr` `$remote_user` `$time_local` `$request` `$status` `$body_bytes_sent` `$http_referer` `$http_user_agent`
* Arguments: 1 or more

`sqlite3_log_columns` defines the table's columns. Each column's name is its variable name without `$`, and its type is untyped with `BLOB` affinity (see [Datatypes in SQLite](https://www.sqlite.org/datatype3.html)). The default values are the variables in combined log format. The following "log-only" variables can also be used: `$bytes_sent`, `$connection`, `$connection_requests`, `$msec`, `$pipe`, `$request_time`, `$status`, `$time_iso8601`, `$time_local`

### sqlite3_log_null

* Context: main
* Type: flag
* Default: `off`
* Arguments: 1

If `sqlite3_log_null` is on, empty values (written as `-` in the standard log module) are inserted as `NULL`.

### sqlite3_log_transaction

* Context: main
* Type: int
* Default: `1`
* Arguments: 1

`sqlite3_log_transaction` declares how many SQL insert statements should be queued up before being executed as one transaction. Pending statements are executed when Nginx reloads or exits.

## Example

```nginx
main {
    sqlite3_log          on;
    sqlite3_log_db       "/var/log/nginx/access.db";
    sqlite3_log_columns  $remote_addr $remote_user $status $http_user_agent;
    sqlite3_log_null     on;
    
    server {
        listen       80;
        server_name  localhost;

        location / {
            root   html;
            index  index.html;
        }
    }
}
```

The above config creates the database with the following table.

```sql
CREATE TABLE IF NOT EXISTS log (
    remote_addr,
    remote_user,
    status,
    http_user_agent
);
```

An incoming request is then logged like so.

```sql
INSERT INTO log VALUES ('123.45.67.89', NULL, '200', 'curl/7.68.0');
```

The second value, `$remote_user`, is an empty string which would normally be evaluated as `-`, but because `sqlite3_log_null` is enabled here, it's `NULL` instead.

If `sqlite3_log_transaction` is 1, the insert is executed immediately. If it's higher, say 10, the module will wait for 9 more requests to come in before inserting them all at once. Generally, a higher value means fewer disk writes, but statements will be perpetually waiting in the meantime.

## Compilation

Install the SQLite3 library, then add the module with `--add-module=...` when compiling Nginx.

* Debian: `apt install libsqlite3-dev`
* Fedora: `dnf install sqlite-devel`

## Why this module is good

* You can use SQL to compute statistics about your logs.
* You can view your logs in human-readable format.

## Why this module is bad

* You can accomplish the same thing by using a CSV-style `log_format` and importing the file ([How to import load a .sql or .csv file into SQLite?](https://stackoverflow.com/questions/1045910/how-to-import-load-a-sql-or-csv-file-into-sqlite)).
* It can only be used in `main`/`http` context.
* There are no tests whatsoever.

