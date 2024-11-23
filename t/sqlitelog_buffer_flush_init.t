#!/usr/bin/perl

# (C) Serope.com

# Test the flush parameter and init parameter simultaneously. The init script
# creates a table and inserts 1 row into it.
# 
# This test exists because the module previously opened a new database
# connection on every flush. Now, it reuses the same connection that is opened
# when Nginx starts. If the new behavior is properly implemented, the timestamp
# table should have only 4 records on it (1 per worker process).

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
my $total_tests = 29;
my $conf = Util::read_file("conf/sqlitelog_buffer_flush_init.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_data("flush_init.sql", $t->testdir());
Util::link_module($t->testdir());


###############################################################################
$t->run();

# Send a few requests
for (1..3) {
	http_get('/hello');
}

# Sleep past flush duration (flush=3s)
sleep(5);

# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Get log count
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $count = $arr[0];
$stmt->finish;

# Get timestamp count
$stmt = $db->prepare("SELECT COUNT(*) FROM times");
$stmt->execute;
@arr = $stmt->fetchrow_array;
my $times = $arr[0];
$stmt->finish;

$t->stop();
###############################################################################


# Confirm counts
is($count, 3, "Check if combined has 3 records after waiting flush duration");
is($times, 4, "Check if times has 4 records after waiting flush duration");


# Get records
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;


# Check records
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

while (my $hashref = $stmt->fetchrow_hashref) {
	my $row_len = keys %$hashref;
	is($row_len, 8, "Check row length for /hello");
	while ((my $k, my $v) = each %hello) {
		my $got = $hashref->{$k};
		my $want = $v;
		if ($k eq "time_local") {
			my $regex_match = ($got =~ $v);
			is($regex_match, 1, "Check $k in location /hello");
		}
		else {
			is($got, $want, "Check $k in location /hello");
		}
	}
}


# End
$stmt->finish;
$db->disconnect;
