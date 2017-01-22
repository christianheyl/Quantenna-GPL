#!/usr/bin/perl -w
use strict;
use warnings;
use List::Util qw(max); 
use Cwd qw(abs_path getcwd); 
use File::Spec; 

my $cwd = getcwd(); 

my @allfiles;
my @allversions;

if ($#ARGV < 0) {
	warn "$0, finds the most recent perforce version from files provided as arguments\n";
	warn "Usage: $0 <files>\n";
	die "\n";
}

# get all filenames, converting them to real path; allows use of symlinks which p4 doesn't allow
for (my $i = 0; $i <= $#ARGV; $i++) {
	my $filename = $ARGV[$i];
	unless (-e $filename) { 
		die "File: $filename doesnt exist\n";
	}
	my $relfile = File::Spec->abs2rel(abs_path($filename), $cwd);
	push (@allfiles, $relfile);
}

# extract versions from 'p4 files' 
my @p4output = `p4 files @allfiles`;
foreach my $p4line (@p4output) {
	my $version = $p4line; 
	$version =~ s/^.*\s+\b(\d+)\b\s+.*?\s*$/$1/g;
	#print "$p4line has version $version\n";
	push(@allversions, $version);
}

my $max = max(@allversions);
print "$max\n";


