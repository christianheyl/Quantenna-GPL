#!/usr/bin/perl -w
my $instructions = "
Usage: $0 <file> to read statistics from a file. Use '-' to read from stdin.

Options:
	--depth	<n>	Show n levels of call graph edges, default = 1

	--calledby	Show the callers for each entry
";

use strict;
use warnings;
use Math::BigInt;
use Math::BigFloat;
use Getopt::Long;
use File::Basename;
use List::Util qw[min max];
BEGIN {
	push @INC, dirname(__FILE__);
}       
use arcsymbols;

my $swdepot = dirname(__FILE__)."/../..";

my $muc = undef;
my $csv = undef;
my $help = undef;
my $clockrate = 400000000;
my $objdump = "/usr/local/ARC/gcc/bin/arc-linux-uclibc-objdump";
my $muc_path = "$swdepot/macfw/qtn_ruby";
my $sort_field = "cycles_exc_callees";
my $maxdepth = 1;
my $calledby = undef;
my @all_objs = `echo $swdepot/linux/vmlinux ; find $swdepot/drivers -name \*.ko`;

my %allsyms;
foreach (@all_objs) {
	chomp;chomp;
	$allsyms{$_} = new arcsymbols($_);
}

sub fixfunc {
	my ($name, $offset, $size) = @_;
	if ($offset > 0) {
		# find symbol, compare sizes; some functions are omitted in kallsyms
		foreach my $bin (reverse sort keys %allsyms) {
			my $changed = undef;
			($name, $offset, $size, $changed) = $allsyms{$bin}->fixfunc($name, $offset, $size);
			if ($changed == 1) {
				last;
			}
		}
	}	
	return ($name, $offset, $size);
}


my $result = GetOptions(
	"csv"		=> \$csv,
	"help"		=> \$help,
	"clockrate=s"	=> \$clockrate,
	"muc_bin"	=> \$muc_path,
	"sort=s"	=> \$sort_field,
	"depth=s"	=> \$maxdepth,
	"calledby"	=> \$calledby,
);

$clockrate = int($clockrate);
if ($help || $#ARGV < 0) {
	die $instructions;
}

my %funcs = parsedata(readfile($ARGV[0]));

if ($csv) {
	$" = ", ";
	my @fields = (
		"Function",
		"Module",
		"Cycles w/o subs",
		"Cycles w/ subs",
		"Call count",
		"Cycles w/o subs per iter",
		"Offsets",
		"Pct% time w/o subs",
		"Function size",
		"Blockiness (cyc/iter/size)",
		"Time w/o subs (".($clockrate / 1000000)." MHz)",
	);

	print "@fields\n";

	foreach my $func (sort { $funcs{$b}->{$sort_field} <=> $funcs{$a}->{$sort_field} } keys %funcs) {
		my $f = $funcs{$func};
		my @data = (
			$func,
			$f->{module},
			$f->{cycles_exc_callees},
			$f->{cycles_inc_callees},
			$f->{calls},
			$f->{cycles_per_iter_exc_callees},
			$f->{offsets},
			$f->{percent},
			$f->{size},
			$f->{blockiness},
			$f->{time_exc_callees});
		print "@data\n";
	}
} else {
	my $pattern = "%-9s %14s %16s %8s %7s %4s  %10s %8s %8s %s %s\n";
	printf($pattern, "Dpth", "Cycles w/o subs", "Cycles w/ subs", "Calls", "cwospi", "Ofs", "Pct%", "FuncSize", "blocky", "Func", "", "");
	my @sorted_funcs = sort { $funcs{$b}->{$sort_field} <=> $funcs{$a}->{$sort_field} } keys %funcs;
	foreach my $func (@sorted_funcs) {
		recursive_print_func($pattern, $func, 0, $maxdepth, \@sorted_funcs);
	}
}

sub recursive_print_func {
	my ($pattern, $func, $depth, $max_depth, $sorted_funcs_ref) = @_;
	return if ($depth >= $max_depth);
	print_func_line($pattern, $func, $funcs{$func}, $depth);
	foreach my $nextfunc (@{$sorted_funcs_ref}) {
		if ($funcs{$nextfunc}->{parents}->{$func}) {
			recursive_print_func($pattern, $nextfunc, $depth+1, $max_depth, $sorted_funcs_ref);
		}
	}

	if ($depth == 0 && $calledby) {
		# add called by lines...
		my @parents_sorted = sort { 
			$funcs{$func}->{parents}->{$b}->{$sort_field} <=> $funcs{$func}->{parents}->{$a}->{$sort_field} 
		} keys %{$funcs{$func}->{parents}};
		foreach my $parent_name (@parents_sorted) {
			print_func_line($pattern, "   << $parent_name", $funcs{$func}->{parents}->{$parent_name}, -1);
		}
	}
}

sub print_func_line {
	my ($pattern, $func, $f, $depth) = @_;
	my $func_str = "";
	my $indent = "";
	if ($depth > 0) {
		for (my $i = 0; $i < $depth; $i++) {
			$func_str .= "   ";
			$indent .= "  ";
		}
		$func_str .= " |__ ";
		$indent .= "|__";
	} elsif ($depth == 0) {
		$indent = "#_";
	} else {
		$func_str = "  < ";
		$indent .= "  < ";
	}	

	$func_str .= $f->{name};

	my $cwospi = $f->{cycles_per_iter_exc_callees} || 0;
	my $percent = $f->{percent} || 0;
	my $blockiness = $f->{blockiness} || 0;
	my $offsets = $f->{offsets} || 0;
	printf $pattern,
	$indent,
	#sprintf($depth),
	sprintf($f->{cycles_exc_callees}),
	sprintf($f->{cycles_inc_callees}),
	sprintf($f->{calls}),
	sprintf($cwospi),
	sprintf($offsets),
	sprintf("%2.4g", $percent),
	sprintf($f->{size}),
	sprintf("%2.4g", $blockiness),
	$func_str,
	$f->{module};
}



sub readfile {
	my $file = shift;
	my @data;
	if ($file eq "-") {
		while(<STDIN>) {
			push(@data, $_);
		}
	} else {
		open(F, $file) or die "Could not open file '$file': $!\n";
		while(<F>) {
			push(@data, $_);
		}
		close(F);
	}
	return @data;
}

sub load_syms {
	my $path = shift;
	my %data;
	unless ( -e $path ) {
		warn "Could not find binary: $path, no function size data available\n";
		return %data;
	}

	foreach (`$objdump -t $path`) {
		if (/^([\w\d]+)\s+.*?\bF\b\s+\.\w+\s+([\w\d]+)\s+(\w+)\s*$/) {
			my $addr = hex($1);
			my $size = hex($2);
			my $name = $3;
			$data{$name}->{addr} = $addr;
			$data{$name}->{size} = $size;
			$data{$addr}->{name} = $name;
			$data{$addr}->{size} = $size;
		}
	}

	return %data;	
}

sub parsedata {
	my @data = @_;
	my %mucdata;
	my %funcs;
	my $total_cycles = 0;
	foreach my $line (@data) {
		$line =~ /^\s*([\w\d_]+)\+?(0x[\w\d]+|)\/?(0x[\w\d]+|)\s+(\[\w+\]\s+|)([\w\d_]+)\+?(0x[\w\d]+|)\/?(0x[\w\d]+|)\s+(\[\w+\]\s+|)(\d+)\s+(0x[\w\d]+)\s+(0x[\w\d]+)\s+(0x[\w\d]+)\s+(0x[\w\d]+)\s*.*?\s*$/;
		my $funcname = $1;
		my $offset = $2 ? hex($2) : 0;
		my $funcsize = $3 ? hex($3) : 0;
		my $module = $4 ? $4 : "";
		my $calling_funcname = $5;
		my $calling_offset = $6 ? hex($6) : 0;
		my $calling_funcsize = $7 ? hex($7) : 0;
		my $calling_module = $8 ? $8 : "";
		my $calls = $9 ? int($9) : 0;
		my $time_inc_callees = Math::BigInt->from_hex($10);
		$time_inc_callees->blsft(32);
		$time_inc_callees->bior(Math::BigInt->from_hex($11));
		my $time_exc_callees = Math::BigInt->from_hex($12);
		$time_exc_callees->blsft(32);
		$time_exc_callees->bior(Math::BigInt->from_hex($13));

		($funcname, $offset, $funcsize) = fixfunc($funcname, $offset, $funcsize);
		($calling_funcname, $calling_offset, $calling_funcsize) = fixfunc($calling_funcname, $calling_offset, $calling_funcsize);
		#print "FFFF $line\nAAAA $funcname $offset $funcsize $module $calls $time_inc_callees $time_exc_callees\n";
		if ($funcsize == 0) {
			$muc = 1;
			%mucdata = &load_syms($muc_path) unless %mucdata;
		}

		if ($funcname && defined($time_inc_callees)) {
			if ($muc && $mucdata{$funcname}->{size}) {
				$funcsize = $mucdata{$funcname}->{size};
			}
			$funcs{$funcname}->{name} = $funcname;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{name} = $calling_funcname;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{calls} = $calls;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{cycles_inc_callees} = $time_inc_callees;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{cycles_exc_callees} = $time_exc_callees;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{offset} = $calling_offset;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{size} = $calling_funcsize;
			$funcs{$funcname}->{parents}->{$calling_funcname}->{module} = $calling_module;
			$funcs{$funcname}->{offset} = $offset;
			$funcs{$funcname}->{size} = $funcsize;
			$funcs{$funcname}->{module} = $module;
			$funcs{$funcname}->{calls} += $calls;
			$funcs{$funcname}->{offsets} += 1;
			$funcs{$funcname}->{cycles_inc_callees} += $time_inc_callees;
			$funcs{$funcname}->{cycles_exc_callees} += $time_exc_callees;
			$total_cycles += $time_exc_callees;
		} else {
			warn "Error parsing line: $line";
		}
	}
	foreach my $func (keys %funcs) {
		my $f = Math::BigFloat->new(sprintf($funcs{$func}->{cycles_exc_callees}));
		$f->bdiv(Math::BigFloat->new(sprintf($total_cycles)));
		$f->bmul(100);
		$funcs{$func}->{percent} = $f;

		$f = Math::BigInt->new(sprintf($funcs{$func}->{cycles_exc_callees}));
		$f->bdiv($funcs{$func}->{calls});
		$funcs{$func}->{cycles_per_iter_exc_callees} = sprintf($f);

		if ($funcs{$func}->{size} == 0) {
			$funcs{$func}->{blockiness} = 0;
		} else {
			$funcs{$func}->{blockiness} = $funcs{$func}->{cycles_per_iter_exc_callees} / $funcs{$func}->{size};
		}

		$f = Math::BigFloat->new(sprintf($funcs{$func}->{cycles_exc_callees}));
		$f->bdiv($clockrate);
		$funcs{$func}->{time_exc_callees} = sprintf($f);
	}
	return %funcs;
}

