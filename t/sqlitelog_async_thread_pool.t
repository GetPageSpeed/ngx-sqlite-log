#!/usr/bin/perl

# (C) Serope.com

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
my $total_tests = 182;
my $conf = Util::read_file("conf/sqlitelog_async_thread_pool.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

for (1 .. 20) {
	http_get('/hello');
	system("sleep 0.1");
}

$t->stop();
###############################################################################


# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Count records
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $count = $arr[0];
is($count, 20, 'Count records');
$stmt->finish;


# Check records
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;
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


# Check error.log to confirm the async functions were called
like($t->read_file('error.log'), qr/\[debug\] .* sqlitelog: thread completed handler/, "Check error.log");
