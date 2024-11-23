#!/usr/bin/perl

# (C) Serope.com

# In this test, we use the init=script option to execute a SQL script when the
# database is opened. The script creates a view, v_notfound, for holding log
# entries with 404 status.

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
my $total_tests = 41;
my $conf = Util::read_file("conf/sqlitelog_init.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests);
Util::link_module($t->testdir());
Util::link_data("script.sql", $t->testdir());
$t->write_file_expand('nginx.conf', $conf);


###############################################################################
$t->run();

for (1..10) {
	http_get('/hello');
	http_get('/notfound');
}

$t->stop();
###############################################################################



# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Check count
my $stmt = $db->prepare("SELECT COUNT(*) FROM v_notfound");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $count = $arr[0];
is($count, 10, "Check view count");


# Get records from view
$stmt = $db->prepare("SELECT * FROM v_notfound");
$stmt->execute;


# Check records
while (my $hashref = $stmt->fetchrow_hashref) {
	my $row_len = keys %$hashref;
	is($row_len, 3, "Check row length for /notfound");
	
	my $remote_addr = $hashref->{"remote_addr"};
	is($remote_addr, "127.0.0.1", "Check remote_addr column");
	
	my $request = $hashref->{"request"};
	is($request, "GET /notfound HTTP/1.0", "Check request column");
	
	my $time_local = $hashref->{"time_local"};
	my $time_local_regex = "[0-9]{2}.[A-Z][a-z]{2}.[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} .*";
	my $matched = ($time_local =~ $time_local_regex);
	is($matched, 1, "Check time_local column");
}


# End
$stmt->finish;
$db->disconnect;

