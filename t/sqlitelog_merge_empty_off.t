#!/usr/bin/perl

# (C) Serope.com

# This is similar to scenario off-empty. The server block is explicitely off and
# shouldn't inherit anything from the implicit ngx_http_sqlitelog_srv_conf_t
# that gets created in the parent http block.

use warnings;
use strict;

use Test::More;
BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib 'lib';
use Test::Nginx;

use DBI;
use Util;
use File::Spec;

select STDERR; $| = 1;
select STDOUT; $| = 1;


# Set up
my $total_tests = 0;
my $conf = Util::read_file("conf/sqlitelog_merge_empty_off.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');
http_get('/hello');
http_get('/hello');

$t->stop();
###############################################################################


# No database to test!
# If Nginx accepted requests without any errors then we're good.
