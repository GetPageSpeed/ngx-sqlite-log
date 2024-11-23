#!/usr/bin/perl

# (C) Serope.com

# In this scenario, both the parent http block and child server block have their
# own individual databases.
# Therefore, the child shouldn't inherit anything from the parent.
# The request to /hello should be written to server.db, whereas global.db
# should be nonexistent.

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
my $total_tests = 12;
my $conf = Util::read_file("conf/sqlitelog_merge_on_on.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################



# global.db shouldn't exist
my $dbpath = File::Spec->catfile($t->testdir(), "global.db");
isnt(-f $dbpath, 1, "Confirm that global.db doesn't exist");


# server.db should exist
$dbpath = File::Spec->catfile($t->testdir(), "server.db");
is(-f $dbpath, 1, "Confirm that server.db exists");


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
my %hello = (
	"remote_addr"       => "127.0.0.1",
	"remote_user"       => undef,
	"time_local"        => "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*", #26/Aug/2023:16:47:47 -0400
	"request"           => "GET /hello HTTP/1.0",
	"status"            => 200,
	"body_bytes_sent"   => 0,
	"http_referer"      => undef,
	"http_user_agent"   => undef
);
while ((my $k, my $v) = each %hello) {
	my $got = $row->{$k};
	my $want = $v;
	if ($k eq "time_local") {
		my $regex_match = ($got =~ $v);
		is($regex_match, 1, "Check $k in location /hello");
	}
	else {
		is($got, $want, "Check $k in location /hello");
	}
}


# End
$stmt->finish;
$db->disconnect;

