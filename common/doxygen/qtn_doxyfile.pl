#!/usr/bin/perl -w
use strict;
use warnings;
use File::Basename;
use Cwd 'abs_path';

my $script_path = dirname(__FILE__);
my $checkout_root = abs_path("$script_path/../../");

my $p4rev = `p4 changelists ...\#have | head -1 | awk '{print \$2}'`;
my $p4client = `p4 info | grep 'Client name' | awk '{print \$NF}'`;

$p4rev =~ s/^\s*(.*?)\s*$/$1/g;
$p4client =~ s/^\s*(.*?)\s*$/$1/g;

my $project_name = shift @ARGV;
my $output_path = shift @ARGV;

$output_path =~ s/[^a-z0-9_-]+/_/g;
$output_path = "$checkout_root/doxygen/$output_path/";
system("mkdir -p $output_path");

my $project_number = $p4client . "@" . $p4rev;

my $inputs = join(" ", @ARGV);

my $doxyfile = `cat $script_path/Doxyfile_template`;
$doxyfile =~ s/__PROJECT_NAME__/\"$project_name\"/;
$doxyfile =~ s/__PROJECT_NUMBER__/$project_number/;
$doxyfile =~ s/__OUTPUT_DIRECTORY__/$output_path/;
$doxyfile =~ s/__INPUT__/$inputs/;

warn "$0: PROJECT_NAME     = $project_name\n";
warn "$0: PROJECT_NUMBER   = $project_number\n";
warn "$0: OUTPUT_DIRECTORY = $output_path\n";
warn "$0: INPUT            = $inputs\n";

print $doxyfile;

