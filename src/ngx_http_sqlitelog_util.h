
/*
 * Copyright (C) Serope.com
 */


#pragma once


#include <ngx_core.h>


#define NGX_NULL_STRING (ngx_str_t) {   \
    .data = NULL,                       \
    .len = 0                            \
}


/**
 * Return true if an Nginx string has a C-string prefix.
 * 
 * @param  ns    an Nginx string
 * @param  cs    a C-string prefix
 * @return       1 if ns begins with cs
 */
#define ngx_has_prefix(ns, cs) \
    ((ns)->len >= ngx_strlen(cs) && \
    (ngx_strncmp((ns)->data, cs, ngx_strlen(cs)) == 0))


/**
 * Return true if two Nginx strings are equal.
 * 
 * @param  s1    the first string
 * @param  s2    the second string
 * @return       1 if both strings have the same length and data
 */
#define ngx_str_eq(s1, s2) \
    ((s1)->len == (s2)->len && \
    ngx_strncmp((s1)->data, (s2)->data, (s1)->len) == 0)


/**
 * Return true if an Nginx string equals a C-string.
 * 
 * @param  ns    an Nginx string
 * @param  cs    a C-string
 * @return       1 if the strings have equal contents
 */
#define ngx_str_eq_cs(ns, cs) \
    ((ns)->len == ngx_strlen(cs) && ngx_strncmp((ns)->data, cs, (ns)->len) == 0)


/**
 * Return true if an Nginx string is either "" or "0".
 * 
 * @param   ns  an Nginx string
 */
#define ngx_str_is_false(ns) \
    ((ns)->len == 0 || ((ns)->len == 1 && (ns)->data[0] == '0'))
