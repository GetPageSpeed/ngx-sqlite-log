
set follow-fork-mode child

set breakpoint pending on
#break ngx_http_sqlitelog_init_main_conf
#break ngx_http_sqlitelog_column_types
#break ngx_http_sqlitelog_handler
#break ngx_http_sqlitelog_format_append


# Print a string (ngx_str_t).
# Note that Nginx strings typically aren't null-terminated.
define print_ngx_str
    set $i = 0
    set $s = $arg0
    while ($i < $s.len)
        printf "%c", $s.data[$i]
        set $i = $i + 1
    end
    printf " (len=%d)", $s.len
    printf "\n"
end

# Print an array (ngx_array_t*) of string elements (ngx_str_t).
define print_ngx_array
    set $j = 0
    set $arr = $arg0
    set $elts = (ngx_str_t*) $arr->elts
    while ($j < $arr->nelts)
        set $s = $elts[$j]
        printf "elts[%d] = ", $j
        print_ngx_str $s
        set $j = $j + 1
    end
end


# Print the main configuration struct (ngx_http_sqlitelog_main_conf_t*).
define print_mccf
    set $mccf = $arg0
    printf "enabled          = %d \n", $mccf->enabled
    printf "db_filename      = "
        print_ngx_str $mccf->db_filename
    printf "pragmas          = %p \n", $mccf->pragmas
    printf "script_filename  = "
        print_ngx_str $mccf->script_filename
    printf "queue_enabled    = %d \n", $mccf->queue_enabled
    printf "queue_len        = %d \n", $mccf->queue_len
    printf "queue_size       = %d \n", $mccf->queue_size
    printf "table_name       = "
        print_ngx_str $mccf->table_name
    printf "columns          = %p \n", $mccf->columns
    printf "sql_create_table = "
        print_ngx_str $mccf->sql_create_table
    printf "sql_script       = "
        print_ngx_str $mccf->sql_script
    printf "sql_insert       = "
        print_ngx_str $mccf->sql_insert
    printf "db               = %p \n", $mccf->db
    printf "shm_zone         = %p \n", $mccf->shm_zone
    printf "\n"


# Print a node (ngx_http_sqlitelog_queue_node_t*) on the transaction queue.
define print_queue_node
    set $node = $arg0
    printf "node            = %p \n", $node
    printf "node->elts      = %p \n", $node->elts
    printf "node->nelts     = %d \n", $node->nelts
    printf "node->link.next = %p \n", $node->link.next
    printf "node->link.prev = %p \n", $node->link.prev
    set $j = 0
    while ($j < $node.nelts)
        set $s = $node.elts[$j]
        print_ngx_str $s
        set $j = $j + 1
    printf "\n"
    end
end

