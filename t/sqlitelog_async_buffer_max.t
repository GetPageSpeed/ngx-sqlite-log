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
my $total_tests = 91;
my $conf = Util::read_file("conf/sqlitelog_async_buffer_max.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);
my $stmt = $db->prepare("SELECT COUNT(*) FROM combined");

my @counts = ();

for (1..9) {
	# Send request
	http_get('/hello');
	
	# Sleep a bit to give time for the thread task to complete
	system("sleep 0.1");
	
	# Count how many records are currently in the database
	$stmt->execute;
	my @arr = $stmt->fetchrow_array;
	my $count = $arr[0];
	push(@counts, $count);
}

$t->stop();
###############################################################################

# Check table counts.
# Putting the table counts in a @counts array was necessary because calling is()
# between t->start() and t->stop() seemed to be creating TAP parsing errors:
# "Tests out of sequence.  Found (10) but expected (9)"
is($counts[0], 0, "1st request - table should be empty");
is($counts[1], 0, "2nd request - table should be empty");
is($counts[2], 0, "3rd request - table should be empty");
is($counts[3], 0, "4th request - table should be empty");
is($counts[4], 5, "5th request - table should have exactly 5 records");
is($counts[5], 5, "6th request - table should still have 5 records");
is($counts[6], 5, "7th request - table should still have 5 records");
is($counts[7], 5, "8th request - table should still have 5 records");
is($counts[8], 5, "9th request - table should still have 5 records");

# Now that Nginx has stopped, the table should have 9 records
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $count = $arr[0];
is($count, 9, "Confirm that table has exactly 9 records");
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

