#!/usr/bin/perl -w
my $instructions = "
$0
Usage:
	cat dump | $0 --bin u-boot/u-boot	# read from stdin, using u-boot for symbols
	$0 --bin linux/vmlinux dump		# read from file 'dump' using linux for symbols

Takes output from iptr sampling code, aggregate functions, sort by use and display with symbol data information.

Arguments:	--bin <file>	executable to read symbol table from.

		--subsamples	show distribution of subsample counters for each function
";

use strict;
use warnings;
use Getopt::Long;
use File::Basename;
use File::Spec;
use Cwd 'abs_path';
BEGIN {
	push @INC, dirname(__FILE__);
}	
use arcsymbols;
my $script_path = dirname(__FILE__);
my $swdepot = dirname(__FILE__)."/../..";

if (!caller) {
	&main;
}

sub fixfunc {
	my ($syms, $name, $offset, $size) = @_;
	my $sym = undef;
	if ($offset > 0) {
		# find symbol, compare sizes; some functions are omitted in kallsyms
		foreach my $bin (reverse sort keys %$syms) {
			my $changed = undef;
			($name, $offset, $size, $changed, $sym) = $syms->{$bin}->fixfunc($name, $offset, $size);
			if ($changed == 1) {
				last;
			}
		}
	}
	return ($name, $offset, $size, $sym);
}

sub main {
	my $help = undef;
	my @bin_path;
	my @def_bin_path = `find $swdepot/drivers/ -name \*.ko`;
	push(@def_bin_path, "$swdepot/linux/vmlinux");
	my $subsamples = undef;
	my $sort = "szweight";
	my $result = GetOptions(
		"help"		=> \$help,
		"bin=s"		=> \@bin_path,
		"subsamples"	=> \$subsamples,
		"sort=s"	=> \$sort,
	);

	if ($help) {
		die "$instructions\n";
	}

	my @allsyms;
	if ($#bin_path < 0) {
		@bin_path = @def_bin_path;
	}
	foreach (@bin_path) {
		chomp;chomp;
		push @allsyms, new arcsymbols($_);
	}

	my $iptr_data;

	if ($#ARGV > 0) {
		die "$instructions\n";
	} elsif ($#ARGV == 0) {
		foreach my $dump_file (@ARGV) {
			open(F, $dump_file) or die "Could not load dump file: $dump_file: $!\n";
			$iptr_data = &read_file_convert(*F, \@allsyms);
			close(F);
		}
	} else {
		$iptr_data = &read_file_convert(*STDIN, \@allsyms);
	}

	&display_iptr_samples($iptr_data, $subsamples, $sort);
}

sub getfunc {
	my ($syms, $addr) = @_;
	foreach my $bin (reverse sort keys %$syms) {
		my $sym = $syms->{$bin}->find_symbol($addr);
		if ($sym) {
			return ($sym->{name}, $addr - $sym->{addr}, $sym->{size});
		}
	}
	warn sprintf "could not find sym for addr: %x\n", $addr;
	return (sprintf(" -unknown_0x%x", $addr), 0, 1);
}

sub read_file_convert {
	my ($fh, $syms) = @_;

	my $last_saw_parsedump_helper = 0;
	my $parsedump_helper_sections = {};

	my %samples;
	my $total_uses = 0;
	while(<$fh>) {
		if ( /^parsedump\s+([\w_]+)\s+([\.\w_]+)\s+(0x[a-fA-F0-9]+)\s*$/ ) {
			my $module_name = $1;
			my $section_name = $2;
			my $section_addr = hex($3);
			$parsedump_helper_sections->{$module_name}->{$section_name} = $section_addr;
			$last_saw_parsedump_helper = 1;
		} else {
			if ($last_saw_parsedump_helper) {
				foreach my $module_name (sort keys %{$parsedump_helper_sections}) {
					foreach my $mod_path (<$script_path/../../drivers/*/$module_name.ko>) {
						my $mod_rel = File::Spec->abs2rel(abs_path($mod_path));
						warn "$0: Loading symbols from '$mod_rel'\n";
						push @{$syms}, new arcsymbols($mod_rel, $parsedump_helper_sections->{$module_name});
					}
				}
				$last_saw_parsedump_helper = 0;
				$parsedump_helper_sections = {};
			}
			
		if (/^(0x[\w\d]+)\s+(0x[\w\d]+)\s+([\w_][_\w\d]+|)\+?(0x[\w\d]+|)\/?(0x[\w\d]+|)/) {
			my $addr = hex($1);
			my $uses = hex($2);

				foreach my $store (@{$syms}) {
					my $symbol = $store->find_symbol($addr);

					if ($symbol) {
						my $func = $symbol->{name};
						my $offset = $addr - $symbol->{addr};
						my $size = $symbol->{size};

				$samples{$func}->{addr} = $addr - $offset;
				$samples{$func}->{size} = $size;
				$samples{$func}->{samples}->{$addr}->{offset} = $offset;
				$samples{$func}->{samples}->{$addr}->{uses} = $uses;
				$samples{$func}->{uses} += $uses;
				$samples{$func}->{pts} += 1;
				$samples{$func}->{ptweight} = $samples{$func}->{uses} / $samples{$func}->{pts};
				$samples{$func}->{szweight} = $samples{$func}->{uses} / $samples{$func}->{size};
				$total_uses += $uses;
			}
		}
	}
		}
	}

	foreach my $func (keys %samples) {
		$samples{$func}->{percentage} = $samples{$func}->{uses} * 100.0 / $total_uses;
	}

	return \%samples;
}

sub display_iptr_samples {
	my ($data_ref, $subsamples, $sort) = @_;
	my %data = %{$data_ref};

	my $pattern = "%10s %10s %10s %10s %10s %10s %6s %s";
	printf "$pattern\n", "Total Uses", "Addr", "Pts", "ptweight", "szweight", "%Pct", "Size", "Name";
	foreach my $func (sort { $data{$b}->{$sort} <=> $data{$a}->{$sort} } keys %data) {
		printf "$pattern\n", (
			sprintf("%d", $data{$func}->{uses}),
			sprintf("%x", $data{$func}->{addr}),
			sprintf("%d", $data{$func}->{pts}),
			sprintf("%-2.4g", $data{$func}->{ptweight}),
			sprintf("%-2.4g", $data{$func}->{szweight}),
			sprintf("%-2.4g", $data{$func}->{percentage}),
			sprintf("%x", $data{$func}->{size}),
			$func);
		if ($subsamples) {
			foreach my $addr (sort keys %{$data{$func}->{samples}}) {
				my $uses = $data{$func}->{samples}->{$addr}->{uses};
				my $objaddr;
				my $offset = $addr - $data{$func}->{addr};
				if ($data{$func}->{sym}) {
					$objaddr = $addr - $data{$func}->{addr} + $data{$func}->{sym}->{addr};
					$offset = $objaddr - $data{$func}->{sym}->{addr};
				}
				printf "$pattern\n", (
					sprintf("%d", $uses),
					sprintf("%x", $objaddr ? $objaddr : $addr),
					sprintf("%x", $offset),
					"",
					"",
					"",
					"",
					"");
			}
		}
	}
}
