#!/usr/bin/perl

# (C) Serope.com

# In this scenario, the parent http block has a sqlitelog, but the child server
# block explicitely turns the module off.
# Therefore, global.db should never be created, even after the server block
# gets several requests.
# Note that in the standard log module, the global log file -would- be created,
# but remain empty.

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
my $total_tests = 2;
my $conf = Util::read_file("conf/sqlitelog_merge_on_off.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');
http_get('/hello');
http_get('/hello');

$t->stop();
###############################################################################


# global.db shouldn't exist
my $dbpath = File::Spec->catfile($t->testdir(), "global.db");
isnt(-f $dbpath, 1, "Confirm that global.db doesn't exist");


# server.db shouldn't exist
$dbpath = File::Spec->catfile($t->testdir(), "server.db");
isnt(-f $dbpath, 1, "Confirm that server.db doesn't exist");
