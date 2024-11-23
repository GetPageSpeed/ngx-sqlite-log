#!/usr/bin/perl

# (C) Serope.com

# In this test, we send requests to Nginx using strings from the BLNS
# (https://github.com/minimaxir/big-list-of-naughty-strings). The file blns.txt
# included here has the following modifications.
#
#   1. All comments are removed
#   2. All but one empty lines are removed
#   3. A NULL line (␀) is present
#   4. The SQL injection table name is changed from "users" to "blns"

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

# BLNS file
my @blns = Util::read_array('blns.txt');
my $total_lines = @blns;

# Set up
my $total_tests = $total_lines + 1;
my $conf = Util::read_file("conf/sqlitelog_blns.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();
foreach my $s (@blns) {
	http_get("/$s");
}
$t->stop();
###############################################################################


# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "access.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);

# Get count
my $stmt = $db->prepare("SELECT COUNT(*) FROM blns");
$stmt->execute;
my @arr = $stmt->fetchrow_array;
my $total_rows = $arr[0];
is($total_rows, $total_lines, "Count records in access.db");
$stmt->finish;

# Check each record
$stmt = $db->prepare('SELECT request FROM blns');
$stmt->execute;
my $i = 0;
while (my @record = $stmt->fetchrow_array) {
	my $got = $record[0];
	my $esc = Util::log_escape($blns[$i]);
	my $want = "GET /$esc HTTP/1.0";
	is($got, $want, "Check if record matches log-escaped BLNS line");
	$i += 1;
}

# End
$stmt->finish;
$db->disconnect;
