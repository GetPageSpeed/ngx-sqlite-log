#!/usr/bin/perl

# (C) Serope.com

# Here we test an init script with a SQLite trigger.

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
my $total_tests = 278;
my $conf = Util::read_file("conf/sqlitelog_init_trigger.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests);
Util::link_module($t->testdir());
Util::link_data("trigger.sql", $t->testdir());
$t->write_file_expand('nginx.conf', $conf);


###############################################################################
$t->run();

for (1..10) {
	http_get('/hello');
}
for (1..10) {
	http_get('/notfound');
}
for (1..10) {
	http_get('/teapot');
}

$t->stop();
###############################################################################



# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Count combined
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $count = $arr[0];
is($count, 30, "Check count of combined table");

# Count requests_uniq
$stmt = $db->prepare("SELECT COUNT(*) FROM requests_uniq");
$stmt->execute;
@arr = $stmt->fetchrow_array;
$count = $arr[0];
is($count, 3, "Check count of requests_uniq table");

# Get records from combined
$stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;

# Check records from combined
my $i = 0;
while (my $hashref = $stmt->fetchrow_hashref) {
	my $row_len = keys %$hashref;
	is($row_len, 8, "Count columns");
	
	my $remote_addr = $hashref->{"remote_addr"};
	is($remote_addr, "127.0.0.1", "Check remote_addr column");
	
	my $remote_user = $hashref->{"remote_user"};
	is($remote_user, undef, "Check remote_user column");
	
	my $time_local = $hashref->{"time_local"};
	my $time_local_regex = "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*";
	my $matched = ($time_local =~ $time_local_regex);
	is($matched, 1, "Check time_local column");
	
	my $request = $hashref->{"request"};
	my $status = $hashref->{"status"};
	my $body_bytes_sent = $hashref->{"body_bytes_sent"};
	if ($i < 10) {
		is($request, "GET /hello HTTP/1.0", "Check request column");
		is($status, 200, "Check status column");
		is($body_bytes_sent, 0, "Check body_bytes_sent column");
	}
	elsif ($i >= 10 && $i < 20) {
		is($request, "GET /notfound HTTP/1.0", "Check request column");
		is($status, 404, "Check status column");
		
		# The size of the "404 not found" page is 153 bytes for Nginx and
		# 157 for Freenginx.
		my $body_bytes_sent_expected = ($body_bytes_sent == 153 or $body_bytes_sent == 157);
		is($body_bytes_sent_expected, 1, "Check body_bytes_sent column");
	}
	else {
		is($request, "GET /teapot HTTP/1.0", "Check request column");
		is($status, 418, "Check status column");
		is($body_bytes_sent, 0, "Check body_bytes_sent column");
	}
	
	my $http_referer = $hashref->{"http_referer"};
	is($http_referer, undef, "Check http_referer column");
	
	my $http_user_agent = $hashref->{"http_user_agent"};
	is($http_user_agent, undef, "Check http_user_agent column");
	
	$i += 1;
}

# Get records from requests_uniq
$stmt = $db->prepare("SELECT * FROM requests_uniq");
$stmt->execute;

# Check records from requests_uniq
$i = 0;
while (my $hashref = $stmt->fetchrow_hashref) {
	my $row_len = keys %$hashref;
	is($row_len, 1, "Count columns");
	
	my $r = $hashref->{"r"};
	my $want = "";
	if ($i == 0) {
		$want = "/hello";
	}
	elsif ($i == 1) {
		$want = "/notfound";
	}
	else {
		$want = "/teapot";
	}
	is($r, "GET $want HTTP/1.0", "Check request column");
	
	$i += 1;
}

# End
$stmt->finish;
$db->disconnect;
