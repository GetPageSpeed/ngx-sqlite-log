#!/usr/bin/perl

# (C) Serope.com

# Test WAL mode with an empty buffer.
# During development, there was a bug wherein Nginx would not execute a WAL
# checkpoint upon exiting if the transaction buffer was empty. The correct
# behavior is for Nginx to execute the checkpoint upon exiting, regardless of
# whether the buffer is empty or not. This test was written to ensure it's been
# fixed.

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
my $total_tests = 3;
my $conf = Util::read_file("conf/sqlitelog_wal_buffer.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests);
Util::link_module($t->testdir());
Util::link_data("wal.sql", $t->testdir());
$t->write_file_expand('nginx.conf', $conf);


###############################################################################
$t->run();

# No requests

$t->stop();
###############################################################################


# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Table should have 0 records
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 0, "Count records in access.db");
$stmt->finish;

# The WAL checkpoint should be executed even if the buffer is empty
like($t->read_file('error.log'), qr/\[debug\] .* sqlitelog: exit worker, buffer empty/, "Check error.log");
like($t->read_file('error.log'), qr/\[debug\] .* sqlitelog: exit worker, checkpoint/, "Check error.log");

# End
$stmt->finish;
$db->disconnect;
