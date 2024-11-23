#!/usr/bin/perl

# (C) Serope.com

# In this test, we confirm that log entries are inserted in the same order
# their requests arrived.

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
my $total_tests = 1802;
my $conf = Util::read_file("conf/sqlitelog_buffer_overflow_sorted.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

my $uri_prefix = "hello-bonjour-gutentag-a-really-long-uri-to-hopefully-trigger-a-buffer-overflow-aaaaaaa-bbbbbbb-ccccccc-ddddddd-eeeeeee";
for (my $i = 1; $i <= 200; $i++) {
	http_get("/$uri_prefix-$i");
}

$t->stop();
###############################################################################


# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Get count
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $count = $arr[0];
is($count, 200, "Check table count");
$stmt->finish;


# Get records
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;


# Check records
my %hello = (
	"remote_addr"       => "127.0.0.1",
	"remote_user"       => undef,
	"time_local"        => "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*", #26/Aug/2023:16:47:47 -0400
	"status"            => 200,
	"body_bytes_sent"   => 0,
	"http_referer"      => undef,
	"http_user_agent"   => undef
);

my $i = 1;
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
	my $request = $hashref->{"request"};
	is($request, "GET /$uri_prefix-$i HTTP/1.0", "Check request");
	$i += 1;
}


# Confirm that we experienced overflow
like($t->read_file('error.log'), qr/\[debug\] .* sqlitelog: buffer overflow, len: [0-9]+/, "Check for overflow message in error.log");


# End
$stmt->finish;
$db->disconnect;
