#!/usr/bin/perl

# (C) Serope.com

# Here, we test a logging condition. If the request URI has the prefix "texas",
# it should be logged. Otherwise, it shouldn't be logged.

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
my $conf = Util::read_file("conf/sqlitelog_if.conf");
my $t = Test::Nginx->new()->has(qw/ http rewrite /)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

# Logged
http_get('/texas');
http_get('/texas123');
http_get('/texas/houston');
http_get('/texas?city=elpaso');
http_get('/texas?cache=open&session=clear');

# Not logged
http_get('/alabama');
http_get('/michigan?state=texas');
http_get('/newjersey/texas');
http_get('/newjersey-texas');
http_get('/newjersey//texas');
http_get('/newjersey/texas/newjersey');
http_get('/newjersey/texas/texas');
http_get('/texa/southdakota');
http_get('/washingtontexaswashington');
http_get('/texa_s_texas');

$t->stop();
###############################################################################


# Check database
my $dbpath = File::Spec->catfile($t->testdir(), "texas.db");
is(-f $dbpath, 1, "Check if texas.db exists");


# Open database
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Table should have 5 records
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, 5, "Count records in texas.db");
$stmt->finish;


# Get records
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;


# Check records
my %rec = (
	"remote_addr"       => "127.0.0.1",
	"remote_user"       => undef,
	"time_local"        => "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*", #26/Aug/2023:16:47:47 -0400
	"request"           => "GET /texas.* HTTP/1\.0",
	"status"            => 200,
	"body_bytes_sent"   => 0,
	"http_referer"      => undef,
	"http_user_agent"   => undef
);
while (my $hashref = $stmt->fetchrow_hashref) {
	my $row_len = keys %$hashref;
	is($row_len, 8, "Check row length");
	
	while ((my $k, my $v) = each %rec) {
		my $got = $hashref->{$k};
		my $want = $v;
		if ($k eq "time_local" or $k eq "request") {
			my $regex_match = ($got =~ $v);
			is($regex_match, 1, "Check column '$k'");
		}
		else {
			is($got, $want, "Check column '$k'");
		}
	}
}


# End
$stmt->finish;
$db->disconnect;

