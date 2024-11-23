#!/usr/bin/perl

# (C) Serope.com

# In this test, we explicitely set the format as "combined".

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
my $total_tests = 10;
my $conf = Util::read_file("conf/sqlitelog_combined.conf");
my $t = Test::Nginx->new()->has(qw/ http rewrite /)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################



# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Table should have 1 record
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 1, "Count records in access.db");
$stmt->finish;

# Get record
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;

# Check record
my $row = $stmt->fetchrow_hashref;
my $row_len = keys %$row;
is($row_len, 8, "Check row length");
is($row->{"remote_addr"}, "127.0.0.1", "Check value of remote_addr");
is($row->{"remote_user"}, undef, "check value of remote_user");
like($row->{"time_local"}, qr/[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*/, "Check pattern of time_local");
is($row->{"request"}, "GET /hello HTTP/1.0", "Check value of request");
is($row->{"status"}, 200, "Check value of status");
is($row->{"body_bytes_sent"}, 0, "Check value of body_bytes_sent");
is($row->{"http_referer"}, undef, "Check value of http_referer");
is($row->{"http_user_agent"}, undef, "Check value of http_user_agent");

# End
$stmt->finish;
$db->disconnect;
