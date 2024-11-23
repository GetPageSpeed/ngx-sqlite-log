#!/usr/bin/perl

# (C) Serope.com

# In this scenario, the parent http block is empty, the first server is off,
# and the second server is on.
# Therefore, requests to port 8080 should go unlogged, and requests to port
# 8081 should be logged to server.db.

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
my $total_tests = 11;
my $conf = Util::read_file("conf/sqlitelog_merge_empty_off_on.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get_port("/notlogged", 8080);
http_get_port("/server", 8081);

$t->stop();
###############################################################################



# Check server.db
my $dbpath = File::Spec->catfile($t->testdir(), "server.db");
is(-f $dbpath, 1, "Check if server.db exists");


# Open server.db
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Table should have 1 record
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 1, "Count records in server.db");
$stmt->finish;


# Get record
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;
my $row = $stmt->fetchrow_hashref;


# Check record length
my $row_len = keys %$row;
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


# End
$stmt->finish;
$db->disconnect;
