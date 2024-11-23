package Util;

use strict;
use warnings;
use Exporter;
use Cwd;
use File::Basename;
use File::Spec;
use IO::Socket;
use feature "signatures";
no warnings "experimental";

our @ISA = qw(Exporter);
our @EXPORT = qw( read_file link_module link_data http_get_port );

# Read the contents of a file into a string.
sub read_file ($filename) {
	open(my $f, '<', $filename);
	my $s = do { local($/); <$f> };
	close($f);
	return $s;
}

# Create a symbolic link to TEST_NGINX_SQLITELOG in the given directory.
sub link_module ($dest_dir) {
	my $src = $ENV{TEST_NGINX_SQLITELOG};
	if (!defined $src or length($src) == 0) {
		die "TEST_NGINX_SQLITELOG environment variable is empty";
	}
	my $base = basename($src);
	my $dst = File::Spec->catfile($dest_dir, $base);
	my $linked = symlink($src, $dst);
	if ($linked == 0) {
		die "symlink() returned $linked: src='$src', dst='$dst'";
	}
}

# Create a symbolic link to a file in the "data" directory.
sub link_data ($filename, $dest_dir) {
	my $src = Util::data($filename);
	my $dst = File::Spec->catfile($dest_dir, $filename);
	my $linked = symlink($src, $dst);
	if ($linked == 0) {
		die "symlink() returned $linked: src='$src', dst='$dst'";
	}
}

# Get the full path of a file in the "data" directory.
sub data ($filename) {
	my $utilpm = Cwd::abs_path(__FILE__);
	my $libdir = dirname($utilpm);
	my $tdir = dirname($libdir);
	my $full_path = File::Spec->catfile($tdir, "data", $filename);
	return $full_path;
}

# Read a file in the "data" directory into an array.
sub read_array ($filename) {
	my $filepath = data($filename);
	my @lines = ();
	open(FH, '<', $filepath);
	
	while (<FH>) {
		# The chomp keyword removes trailing newline symbols ('\n') from
		# the current line, which is represented by the $_ variable.
		# 
		# An alternate way to remove newlines and store the result in
		# a new variable $s is the following:
		# my $s = $_ =~ s/\n//gr;
		chomp;
		push(@lines, $_);
	}
	
	close(FH);
	return @lines;
}

# Send a GET request to the given location at http://127.0.0.1.
# 
# Nginx's http_get() function is unfortunately hardcoded to port 8080, so
# sending requests to any other port has to be done by other means.
# 
# Source: https://www.perlmonks.org/?node_id=51510
sub http_get_port ($location, $port) {
	my $host = "127.0.0.1";
	my $EOL = "\015\012";
	my $BLANK = $EOL x 2;
	my $sep = "-------------------\n";
	
	my $remote = IO::Socket::INET->new(
		Proto     => "tcp",
		PeerAddr  => $host,
		PeerPort  => $port,
	);
	unless ($remote) {
		die "Cannot connect to http daemon on $host\n" 
	}
	
	$remote->autoflush(1);
	print $remote "GET $location HTTP/1.0" . $BLANK;
	while ( <$remote> ) { print }
	print "\n$sep";
	close $remote;
}

# This is the Perl equivalent of ngx_http_log_escape().
sub log_escape ($s) {
	my $hex = '0123456789ABCDEF';
	my @escape = (
		# 1111 1111 1111 1111  1111 1111 1111 1111
		0xffffffff,
		
		# ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!
		# 0000 0000 0000 0000  0000 0000 0000 0100
		0x00000004,
		
		# _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@
		# 0001 0000 0000 0000  0000 0000 0000 0000
		0x10000000, 

		#  ~}| {zyx wvut srqp  onml kjih gfed cba`
		# 1000 0000 0000 0000  0000 0000 0000 0000
		0x80000000,
		
		# 1111 1111 1111 1111  1111 1111 1111 1111
		0xffffffff,
		
		# 1111 1111 1111 1111  1111 1111 1111 1111
		0xffffffff,
		
		# 1111 1111 1111 1111  1111 1111 1111 1111
		0xffffffff,
		
		# 1111 1111 1111 1111  1111 1111 1111 1111
		0xffffffff,
	);
	
	my $dst = "";
	my $n = 0;
	my $size = length($s);
	my $i = 0;
	while ($size) {
		my $src = substr($s, $i, 1);
		if ($escape[ord($src) >> 5] & (1 << (ord($src) & 0x1f))) {
			$dst .= "\\";
			$dst .= "x";
			$dst .= substr($hex, ord($src) >> 4, 1);
			$dst .= substr($hex, ord($src) & 0xf, 1);
		} else {
			$dst .= $src;
		}
		$i += 1;
		$size -= 1;
	}
	
	return $dst;
}
