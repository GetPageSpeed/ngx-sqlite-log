#!/usr/bin/perl

# (C) Serope.com

# In this test, we attempt to open a non-database file.
# This should cause a SQLITE_NOTADB (26) error.

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
my $conf = Util::read_file("conf/sqlite_error_26.conf");
my $t = Test::Nginx->new()->has(qw/ http /)->plan($total_tests);
Util::link_module($t->testdir());
Util::link_data("not_a_db.txt", $t->testdir());
$t->write_file_expand('nginx.conf', $conf);


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################


# Check error.log
like($t->read_file('error.log'), qr/\[error\] .* SQLITE_NOTADB \(26\)/, "Check for SQLITE_NOTADB (26) in error.log");
