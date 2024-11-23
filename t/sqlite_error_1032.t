#!/usr/bin/perl

# (C) Serope.com

# In this test, we delete the database file while Nginx is running. This should
# cause a SQLITE_READONLY_DBMOVED (1032) error, prompting Nginx to create a new
# database and continue logging like nothing happened.
# 
# This ensures compatibility with logrotate.

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
my $total_tests = 7;
my $conf = Util::read_file("conf/sqlite_error_1032.conf");
my $t = Test::Nginx->new()->has(qw/ http rewrite /)->plan($total_tests)->write_file_expand('nginx.conf', $conf);;
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/before');

# Check if database exists while Nginx is running
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
is(-f $dbpath, 1, "Check if access.db exists while Nginx is running");

# Open database while Nginx is running
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Count 1 record
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 1, "Count records in table");
$stmt->finish;

# Check record
$stmt = $db->prepare("SELECT request FROM combined");
$stmt->execute;
my $row = $stmt->fetchrow_hashref;
is($row->{"request"}, "GET /before HTTP/1.0", "Check request column");

# Delete database
$stmt->finish;
$db->disconnect;
unlink($dbpath);

http_get('/after');

$t->stop();
###############################################################################


# Check error.log
like($t->read_file('error.log'), qr/\[error\] .* SQLITE_READONLY_DBMOVED \(1032\)/, "Check for SQLITE_READONLY_DBMOVED (1032) in error.log");


# Check new DB
is(-f $dbpath, 1, "Check if new database was created");


# Count 1 record
$db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);
$stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
@arr = $stmt->fetchrow_array;
$total_rows = $arr[0];
is($total_rows, 1, "Count records in table after original deleted");
$stmt->finish;


# Check record
$stmt = $db->prepare("SELECT request FROM combined");
$stmt->execute;
$row = $stmt->fetchrow_hashref;
is($row->{"request"}, "GET /after HTTP/1.0", "Check request column after original deleted");


# End
$stmt->finish;
$db->disconnect;
