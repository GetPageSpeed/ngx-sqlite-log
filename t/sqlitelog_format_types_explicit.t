#!/usr/bin/perl

# (C) Serope.com

# In this test, we assign intentionally-nonsensical types to core module
# variables.
# 
# $request is BLOB instead of TEXT and its values should be of type BLOB.
# $status is REAL instead of INTEGER and its values should be of type REAL.
# $msec is INTEGER instead of REAL and its values should remain REAL.

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
my $total_tests = 5;
my $conf = Util::read_file("conf/sqlitelog_format_types_explicit.conf");
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
my $stmt = $db->prepare("SELECT typeof(request), typeof(status), typeof(msec) FROM newtypes");
$stmt->execute;


# Check types
my @row = $stmt->fetchrow_array;
my $row_len = @row;
is($row_len, 3, "Check row length");
is($row[0], "blob", "Check type of request (blob)");
is($row[1], "real", "Check type of status (real)");
is($row[2], "text", "Check type of msec (text)");


# End
$stmt->finish;
$db->disconnect;
