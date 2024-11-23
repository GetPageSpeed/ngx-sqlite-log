#!/usr/bin/perl

# (C) Serope.com

# Test WAL mode with a non-empty buffer.

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
my $total_tests = 47;
my $conf = Util::read_file("conf/sqlitelog_wal_buffer.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests);
Util::link_module($t->testdir());
Util::link_data("wal.sql", $t->testdir());
$t->write_file_expand('nginx.conf', $conf);


###############################################################################
$t->run();

http_get('/hello');
http_get('/hello');
http_get('/hello');
http_get('/hello');
http_get('/hello');

$t->stop();
###############################################################################



# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Table should have 5 records
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 5, "Count records in access.db");
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

# Look for "checkpoint" message in error.log
like($t->read_file('error.log'), qr/\[debug\] .* sqlitelog: exit worker, checkpoint/, "Check error.log");

# End
$stmt->finish;
$db->disconnect;
