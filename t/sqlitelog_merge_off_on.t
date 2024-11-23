#!/usr/bin/perl

# (C) Serope.com

# In this scenario, the parent http block's "sqlitelog off" should override
# the sqlitelog in the child server block.

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
my $total_tests = 1;
my $conf = Util::read_file("conf/sqlitelog_merge_off_on.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');
http_get('/hello');
http_get('/hello');

$t->stop();
###############################################################################


# server.db shouldn't exist
my $dbpath = File::Spec->catfile($t->testdir(), "server.db");
isnt(-f $dbpath, 1, "Check if server.db exists");
