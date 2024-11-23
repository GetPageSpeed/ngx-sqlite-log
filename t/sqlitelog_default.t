#!/usr/bin/perl

# (C) Serope.com

# Here, we test the sqlitelog directive with default settings.

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
my $total_tests = 18;
my $conf = Util::read_file("conf/sqlitelog_default.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');
http_get('/notfound');

$t->stop();
###############################################################################



# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Get records
my $stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;


# Check /hello record
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
my $hashref = $stmt->fetchrow_hashref;
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


# Check /notfound record
my %notfound = (
	"remote_addr"       => "127.0.0.1",
	"remote_user"       => undef,
	"time_local"        => "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*",
	"request"           => "GET /notfound HTTP/1.0",
	"status"            => 404,
	"body_bytes_sent"   => "153|157", # 153 for Nginx, 157 for Freenginx
	"http_referer"      => undef,
	"http_user_agent"   => undef
);
$hashref = $stmt->fetchrow_hashref;
$row_len = keys %$hashref;
is($row_len, 8, "Check row length for /notfound");
while ((my $k, my $v) = each %notfound) {
	my $got = $hashref->{$k};
	my $want = $v;
	if ($k eq "time_local" or $k eq "body_bytes_sent") {
		my $regex_match = ($got =~ $v);
		is($regex_match, 1, "Check $k in location /notfound");
	}
	else {
		is($got, $want, "Check $k in location /notfound");
	}
}


# End
$stmt->finish;
$db->disconnect;

