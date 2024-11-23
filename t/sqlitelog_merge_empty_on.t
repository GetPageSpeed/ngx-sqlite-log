#!/usr/bin/perl

# (C) Serope.com

# In this scenario, the parent http block is empty of sqlitelog, but the
# child server block has one. Therefore, the child shouldn't inherit anything
# from the implicit ngx_http_sqlitelog_srv_conf_t that is automatically created
# in the parent block.

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
my $total_tests = 9;
my $conf = Util::read_file("conf/sqlitelog_merge_empty_on.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

http_get('/hello');

$t->stop();
###############################################################################



# Open database
my $dbpath = File::Spec->catfile($t->testdir(), "server.db");
my $db = DBI->connect("dbi:SQLite:dbname=${dbpath}", "", "", undef);


# Get record
my $stmt = $db->prepare("SELECT * FROM combined");
$stmt->execute;
my $row = $stmt->fetchrow_hashref;

# Check record
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
my $row_len = keys %$row;
is($row_len, 8, "Check row length for /hello");
while ((my $k, my $v) = each %hello) {
	my $got = $row->{$k};
	my $want = $v;
	if ($k eq "time_local") {
		my $regex_match = ($got =~ $v);
		is($regex_match, 1, "Check $k in location /hello");
	}
	else {
		is($got, $want, "Check $k in location /hello");
	}
}


# End
$stmt->finish;
$db->disconnect;

