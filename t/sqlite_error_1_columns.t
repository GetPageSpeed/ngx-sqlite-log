#!/usr/bin/perl

# (C) Serope.com

# In this test, we create a database and format with n columns. Then we exit
# Nginx, modify the format to have n+1 columns, and start Nginx again.
# Attempting to append a record to the table in this case should cause a
# SQLITE_ERROR (1) error.

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


# Set up: config A
my $total_tests = 5;
my $conf = Util::read_file("conf/sqlite_error_1_columns_a.conf");
my $t = Test::Nginx->new()->has(qw/ http /)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################


# Check database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
is(-f $dbpath, 1, "Check if database exists");


# Check table count
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);
my $stmt = $db->prepare("SELECT COUNT(*) FROM misused_format");
$stmt->execute;
my @row = $stmt->fetchrow_array;
my $total = $row[0];
is($total, 1, "Check if table has 1 record");


# Check record
$stmt = $db->prepare("SELECT * FROM misused_format");
$stmt->execute;
@row = $stmt->fetchrow_array;
is($row[0], "127.0.0.1", "Check value of remote_addr column");
is($row[1], "GET /hello HTTP/1.0", "Check value of request column");


# Close
$stmt->finish;
$db->disconnect;


# Set up: config B
$total_tests = 1;
$conf = Util::read_file("conf/sqlite_error_1_columns_b.conf");
$t->write_file_expand('nginx.conf', $conf);


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################


# Check error.log
like($t->read_file('error.log'), qr/\[error\] .* SQLITE_ERROR \(1\)/, "Check for SQLITE_ERROR (1) in error.log");
