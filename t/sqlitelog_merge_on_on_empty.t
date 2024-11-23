#!/usr/bin/perl

# (C) Serope.com

# In this scenario, the parent http block has a sqlitelog (global.db) and so
# does the first server (port 8080) (server.db). However, the second server
# (port 8081) doesn't, and it should inherit the global database.
# Therefore, requests to port 8080 should be logged to server.db, and
# requests to port 8081 should be logged to global.db.

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
my $total_tests = 22;
my $conf = Util::read_file("conf/sqlitelog_merge_on_on_empty.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get_port("/server", 8080);
http_get_port("/global", 8081);

$t->stop();
###############################################################################



# Check global.db
my $dbpath = File::Spec->catfile($t->testdir(), "global.db");
is(-f $dbpath, 1, "Check if global.db exists");


# Open global.db
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Table should have 1 record
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 1, "Count records in global.db");
$stmt->finish;


# Get record
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;
my $row = $stmt->fetchrow_hashref;


# Check record length
my $row_len = keys %$row;
is($row_len, 8, "Check row length of record in global.db");


# Check record values
my %global = (
	"remote_addr"       => "127.0.0.1",
	"remote_user"       => undef,
	"time_local"        => "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*", #26/Aug/2023:16:47:47 -0400
	"request"           => "GET /global HTTP/1.0",
	"status"            => 200,
	"body_bytes_sent"   => 0,
	"http_referer"      => undef,
	"http_user_agent"   => undef
);
while ((my $k, my $v) = each %global) {
	my $got = $row->{$k};
	my $want = $v;
	if ($k eq "time_local") {
		my $regex_match = ($got =~ $v);
		is($regex_match, 1, "Check $k in location /global");
	}
	else {
		is($got, $want, "Check $k in location /global");
	}
}


# Close global.db
$stmt->finish;
$db->disconnect;




# Check server.db
$dbpath = File::Spec->catfile($t->testdir(), "server.db");
is(-f $dbpath, 1, "Check if server.db exists");


# Open server.db
$db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Table should have 1 record
$stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
@arr = $stmt->fetchrow_array;
$total_rows = $arr[0];
is($total_rows, 1, "Count records in server.db");
$stmt->finish;


# Get record
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;
$row = $stmt->fetchrow_hashref;


# Check record length
$row_len = keys %$row;
is($row_len, 8, "Check row length of record in server.db");


# Check record values
my %server = (
	"remote_addr"       => "127.0.0.1",
	"remote_user"       => undef,
	"time_local"        => "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*", #26/Aug/2023:16:47:47 -0400
	"request"           => "GET /server HTTP/1.0",
	"status"            => 200,
	"body_bytes_sent"   => 0,
	"http_referer"      => undef,
	"http_user_agent"   => undef
);
while ((my $k, my $v) = each %server) {
	my $got = $row->{$k};
	my $want = $v;
	if ($k eq "time_local") {
		my $regex_match = ($got =~ $v);
		is($regex_match, 1, "Check $k in location /server");
	}
	else {
		is($got, $want, "Check $k in location /server");
	}
}


# Close server.db
$stmt->finish;
$db->disconnect;
