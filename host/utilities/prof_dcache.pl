#!/usr/bin/perl -w
my $instructions = "
$0
Usage:
	cat dump | $0 --bin u-boot/u-boot	# read from stdin, using u-boot for symbols
	$0 --bin linux/vmlinux dump		# read from file 'dump' using linux for symbols

Takes output from dcache sampling code, sort by use and display with symbol data information.

Arguments:	--bin <file>	executable to read symbol table from.
";

use strict;
use warnings;
use Getopt::Long;
use File::Basename;
BEGIN {
	push @INC, dirname(__FILE__);
}
use arcsymbols;

my $swdepot = dirname(__FILE__)."/../..";

my $CACHE_LINE_BYTES = 32;

if (!caller) {
	&main;
}

sub main {
	my $help = undef;
	my $bin_path = "$swdepot/macfw/qtn_ruby";
	my $result = GetOptions(
		"help"		=> \$help,
		"bin=s"		=> \$bin_path,
	);

	if ($help) {
		die "$instructions\n";
	}

	my $symbols = new arcsymbols($bin_path);

	my ($cache_samples, $cache_syms);

	if ($#ARGV > 0) {
		die "$instructions\n";
	} elsif ($#ARGV == 0) {
		foreach my $dump_file (@ARGV) {
			open(F, $dump_file) or die "Could not load dump file: $dump_file: $!\n";
			($cache_samples, $cache_syms) = &read_file_convert(*F, $symbols);
			close(F);
		}
	} else {
		($cache_samples, $cache_syms) = &read_file_convert(*STDIN, $symbols);
	}

	&display_cache_residency($cache_samples, $cache_syms);
}

sub cache_align {
	my $addr = shift;
	return ($addr & (0xffffffff & ~($CACHE_LINE_BYTES-1)));
}	

sub read_file_convert {
	my ($fh, $symbols) = @_;

	my %samples;
	my %usedsyms;
	while(<$fh>) {
		/^(0x[\w\d]+)\s+(0x[\w\d]+)/g;
		my $addr = hex($1);
		my $uses = hex($2);
		$samples{$addr}->{uses} = $uses;
		$samples{$addr}->{syms} = {};
	}

	# now match sorted samples with symbol table
	my @active_symbol_addrs;
	my @sorted_sample_addrs = sort keys %samples;
	my @sorted_symbol_addrs = @{$symbols->get_sorted_addrs_ref()};

	my $done = 0;
	my $sym_index = 0;
	my $sample_index = 0;
	foreach my $addr (@sorted_symbol_addrs) {
		my $sym = $symbols->get_symbol_by_key($addr);
		my $size = $sym->{size};
		for ($a = cache_align($addr); $a < $addr + $size; $a += $CACHE_LINE_BYTES) {
			if ($samples{$a}) {
				$samples{$a}->{syms}->{$addr} = $sym;
				$samples{$a}->{used} = 1;
				$usedsyms{$sym}->{samples}->{$a} = $samples{$a};
				$usedsyms{$sym}->{pts} += 1;
				$usedsyms{$sym}->{uses} += $samples{$a}->{uses};
				$usedsyms{$sym}->{usedp} = $usedsyms{$sym}->{uses} / $usedsyms{$sym}->{pts};
				$usedsyms{$sym}->{addr} = $addr;
				$usedsyms{$sym}->{size} = $size;
				$usedsyms{$sym}->{section} = $sym->{section};
				$usedsyms{$sym}->{name} = $sym->{name};
				#my @k = keys %{$samples{$a}->{syms}};
				#printf "sample at addr: %x fits sym: %s %x %x, matches %d\n", $a, $sym->{name}, $sym->{addr}, $sym->{size}, $#k+1;
			}
		}
	}

	return (\%samples, \%usedsyms);
}

sub display_cache_residency {
	my ($samples, $syms) = @_;

	# add unknown samples into other data, based on usedp sort
	my %all_addrs;
	foreach my $addr (keys %$samples) {
		unless ($samples->{$addr}->{used}) {
			$syms->{$addr}->{uses} = $samples->{$addr}->{uses};
			$syms->{$addr}->{pts} = 1;
			$syms->{$addr}->{usedp} = $samples->{$addr}->{uses};
			$syms->{$addr}->{addr} = $addr;
			$syms->{$addr}->{size} = $CACHE_LINE_BYTES;
			$syms->{$addr}->{section} = "";
			$syms->{$addr}->{name} = "";
		}
	}
	my $pattern = "%10s %10s %10s %5s %9s %10s %8s %-12s %s";
	printf "$pattern\n", "CAddrMin", "CAddrMax", "Uses", "Lines", "Used/L", "Sym Addr", "Sym Size", "Section", "Name";
	foreach my $s (sort { $syms->{$b}->{usedp} <=> $syms->{$a}->{usedp} } keys %$syms) {
		my $saddr = cache_align($syms->{$s}->{addr});
		my $eaddr = cache_align($syms->{$s}->{addr} + $syms->{$s}->{size});
		my $lines = ($eaddr - $saddr) / $CACHE_LINE_BYTES;
		printf "$pattern\n", (
			sprintf("%x", $saddr),
			sprintf("%x", $eaddr),
			sprintf("%d", $syms->{$s}->{uses}),
			sprintf("%d", $lines),
			sprintf("%d", $syms->{$s}->{usedp}),
			sprintf("%x", $syms->{$s}->{addr}),
			sprintf("0x%x", $syms->{$s}->{size}),
			sprintf("%s", $syms->{$s}->{section}),
			sprintf("%s", $syms->{$s}->{name}));
	}
}
