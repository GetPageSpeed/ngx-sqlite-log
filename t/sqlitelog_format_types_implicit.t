#!/usr/bin/perl

# (C) Serope.com

# In this test, we create a format that holds one of each SQLite column type
# (BLOB, TEXT, INTEGER, and REAL). We then confirm that the records in the
# table indeed have those types, meaning we called the correct SQLite3 bind
# functions.
# 
# These column types are implicit (hardcoded) in the module itself, so no :type
# suffixes are used.
# 
# See also: https://www.sqlite.org/c3ref/bind_blob.html

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
my $total_tests = 6;
my $conf = Util::read_file("conf/sqlitelog_format_types_implicit.conf");
my $t = Test::Nginx->new()->has(qw/ http /)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################


# Check database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
is(-f $dbpath, 1, "Check if access.db exists");


# Open database
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Get column types
# SQLite's typeof() function returns the type of data that has actually been
# inserted into the column; the column's type declaration from the CREATE TABLE
# statement is irrelevant
my $stmt = $db->prepare("SELECT typeof(binary_remote_addr), typeof(request), typeof(status), typeof(msec)
FROM myformat
LIMIT 1");
$stmt->execute;


# Check types
my @row = $stmt->fetchrow_array;
my $row_len = @row;
is($row_len, 4, "Check row length");
is($row[0], "blob", "Check type of binary_remote_addr");
is($row[1], "text", "Check type of request");
is($row[2], "integer", "Check type of status");
is($row[3], "real", "Check type of msec");


# End
$stmt->finish;
$db->disconnect;
