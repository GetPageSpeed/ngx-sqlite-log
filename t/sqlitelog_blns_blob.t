#!/usr/bin/perl

# (C) Serope.com

# Here, we test the sqlitelog directive with the BLNS and set the column type
# to BLOB, causing the inserted values to be unescaped.

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
my $conf = Util::read_file("conf/sqlitelog_blns_blob.conf");
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
	my $esc = $blns[$i];
	my $want = "GET /$esc HTTP/1.0";
	is($got, $want, "Check if record matches unescaped BLNS line");
	$i += 1;
}

# End
$stmt->finish;
$db->disconnect;
