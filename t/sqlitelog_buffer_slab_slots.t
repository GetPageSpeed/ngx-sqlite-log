#!/usr/bin/perl

# (C) Serope.com

# In this test, we test Nginx slab allocation up to 2 pages (8192 bytes on most
# systems). Since log variables can potentially be up to 2 pages in size, we
# need to create our shared memory zone to be large enough to hold objects up
# to that size. This test will help us determine what minimum buffer size to
# enforce for the "buffer" parameter of "sqlitelog".
# 
# Note that Nginx's maximum request URI length is 8192 bytes. If a request has
# a URI longer than that, Nginx truncates it down to 8192 bytes.

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
my $total_tests = 0;
my $conf = Util::read_file("conf/sqlitelog_buffer_slab_slots.conf");
my $t = Test::Nginx->new()->has(qw/http rewrite/)->plan($total_tests)->write_file_expand('nginx.conf', $conf);
Util::link_module($t->testdir());


###############################################################################
$t->run();

my @sizes = (8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192);
for my $s (@sizes) {
	# Build URI string
	#   /8xxxxxxx
	#   /16xxxxxxxxxxxxxx
	#   etc.
	my $n = $s - length($s) - length("GET /") - length(" HTTP/1.1") - 1;
	my $uri = "";
	if ($n > 0) {
		$uri = "/$s" . "x" x $n;
	}
	else {
		$uri = "/$s";
	}
	http_get($uri);
}

$t->stop();
###############################################################################


# No database testing necessary.
# If the error.log doesn't contain any [crit] messages for slab failures, we're
# good.
