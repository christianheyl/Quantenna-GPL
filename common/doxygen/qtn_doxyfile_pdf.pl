#!/usr/bin/perl -w
use strict;
use warnings;
use File::Basename;
use Cwd 'abs_path';
use Getopt::Long;

my $script_path = dirname(__FILE__);
my $checkout_root = abs_path("$script_path/../../");

#my $p4rev = `p4 changelists ...\#have | head -1 | awk '{print \$2}'`;
#my $p4client = `p4 info | grep 'Client name' | awk '{print \$NF}'`;

#$p4rev =~ s/^\s*(.*?)\s*$/$1/g;
#$p4client =~ s/^\s*(.*?)\s*$/$1/g;

my @exclusions;
my @predefined;
my $internal = undef;

GetOptions(
	"exclusions=s"	=> \@exclusions,
	"predefined=s"	=> \@predefined,
	"internal"	=> \$internal,
);

my $doxyfile_name = shift @ARGV;
my $project_name = shift @ARGV;
my $output_path = shift @ARGV;
my $revision = shift @ARGV;

if ($internal) {
	$internal = "YES";
} else {
	$internal = "NO";
}

$output_path =~ s/[^a-z0-9_-]+/_/g;
$output_path = "$checkout_root/doxygen/$output_path/";
system("mkdir -p $output_path");

my $inputs = join(" ", @ARGV);

my $doxyfile = `cat $script_path/$doxyfile_name`;
$doxyfile =~ s/__PROJECT_NAME__/\"$project_name\"/;
$doxyfile =~ s/__PROJECT_NUMBER__/\"$revision\"/;
$doxyfile =~ s/__OUTPUT_DIRECTORY__/$output_path/;
$doxyfile =~ s/__EXCLUDE_SYMBOLS__/"@exclusions"/e;
$doxyfile =~ s/__PREDEFINED__/"@predefined"/e;
$doxyfile =~ s/__INTERNAL_DOCS__/$internal/;
$doxyfile =~ s/__INPUT__/$inputs/;

warn "$0: PROJECT_NAME     = $project_name\n";
warn "$0: OUTPUT_DIRECTORY = $output_path\n";
warn "$0: EXCLUDE_SYMBOLS  = @exclusions\n";
warn "$0: PREDEFINED       = @predefined\n";
warn "$0: INPUT            = $inputs\n";

print $doxyfile;

