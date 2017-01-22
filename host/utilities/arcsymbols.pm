#!/usr/bin/perl -w
package arcsymbols;
use strict;
use warnings;
use List::Util qw[min max];

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw();
@EXPORT_OK = qw();
$VERSION = '0.01';

my $objdump = "/usr/local/ARC/gcc/bin/arc-linux-uclibc-objdump";

sub new {
	my $package = shift;
	my $bin_path = shift;
	my $section_addrs_ref = shift;
	my $self = {};
	&load_symbols($self, $bin_path, $section_addrs_ref);
	return bless($self, $package);
}

sub find_symbol {
	my $self = shift;
	my $addr = shift;
	my $sym = $self->find_symbol_address_dumb($addr);
	if ($sym) {
		return $self->{symbols_ref}->{$sym};
	} else {
		return undef;
	}
}

sub get_sorted_addrs_ref {
	my $self = shift;
	return $self->{sorted_symbol_keys_ref};
}

sub get_symbol_by_key {
	my $self = shift;
	my $addr = shift;
	return $self->{symbols_ref}->{$addr};
}

sub find_section {
	my $self = shift;
	my $addr = shift;

	foreach my $section_addr (@{$self->{sorted_section_keys_ref}}) {
		my $offset = $addr - $section_addr;
		my $section_size = $self->{sections_ref}->{$section_addr}->{size};
		if ($offset >= 0 && $offset < $section_size) {
			return $self->{sections_ref}->{$section_addr};
		}
	}
	return undef;
}

sub fixfunc {
	my ($self, $name, $offset, $size) = @_;
	# find symbol, compare sizes; some functions are omitted in kallsyms
	my $changed = 0;
	my $sym = undef;
	#warn sprintf "\t\t%s+%x/%x %s\n", $name, $offset, $size, $self->{bin_path};
	if ($self->{symbol_names_ref}->{$name}) {
		#warn sprintf "$name in $self->{bin_path}\n"; 
		$sym = $self->{symbol_names_ref}->{$name};
		my $objdsize = $sym->{size};
		my $origsize = $size;
		$size = $objdsize;	# fix size regardless of oversize
		my $reladdr = $sym->{addr};
		if ($offset >= $size && $objdsize < $origsize) { 
			#warn sprintf "function: %s oversize (%x > %x) and nonzero offset (%x), addr %x+%x/%x (%x), rs %x\n", $name, $origsize, $objdsize, $offset, $reladdr, $offset, $size, $reladdr + $offset, $objdsize;
			if ($self->{symbols_ref}->{$reladdr + $offset}) {
				my $newname = $self->{symbols_ref}->{$reladdr + $offset}->{name};
				my $newsize = $self->{symbols_ref}->{$reladdr + $offset}->{size};
				warn sprintf "%s+0x%x/0x%x (realsize 0x%x) changed for %s+0x%x/0x%x, binary %s\n", $name, $offset, $size, $objdsize, $newname, 0, $newsize, $self->{bin_path};
				#warn sprintf "found match: $name at %x\n", $reladdr + $offset;
				$name = $newname;
				$size = $newsize;
				$offset = 0;
			} else {
				# create a new name to distinguish this, even though
				# we don't know exactly what it is
				$name = sprintf("%s+0x%x", $name, $objdsize);
			}
		}
		$changed = 1;
	}
	$size = 1 if ($size == 0);
	return ($name, $offset, $size, $changed == 1, $sym);
}

sub find_symbol_address_tree {
	my ($self, $addr) = @_;
	my $symbol_addr;
	my @symbol_addrs = @{$self->{sorted_symbol_keys_ref}};
	# binary search to find symbol
	my $min = 0;
	my $max = $#symbol_addrs;
	while ($max >= $min) {
		my $this = int(($max + $min) / 2);
		my $sym = $symbol_addrs[$this];
		#warn sprintf("\n%d %d %d %x %x", $min, $this, $max, $sym, $addr);
		if ($sym == $addr) {
			$symbol_addr = $sym;
			#warn "$sym == $addr";
			last;
		} elsif ($sym > $addr) {
			if ($max == $this) {
				$max--;
			} else {
				$max = $this;
			}
			#warn sprintf("%x > %x", $sym, $addr);
		} else {
			my $symbol_size = $self->{symbols_ref}->{$sym}->{size};
			my $offset = $addr - $sym;
			if ($offset < $symbol_size) {
				#warn "found, offset $offset < $symbol_size";
				$symbol_addr = $sym;
				last;
			} else {
				#warn sprintf("%x + %x < %x", $sym, $symbol_size, $addr);
				if ($min == $this) {
					$min++;
				} else {
					$min = $this;
				}
			}
		}
	}

	return $symbol_addr;
}

sub find_symbol_address_dumb {
	my ($self, $addr) = @_;

	foreach my $sym (@{$self->{sorted_symbol_keys_ref}}) {
		my $offset = $addr - $sym;

		my $symbol_size = $self->{symbols_ref}->{$sym}->{size};

		if ($offset >= 0 && $offset < $symbol_size) {
			return $sym;
		}
	}
}

sub load_symbols {
	my $self = shift;
	my $bin_path = shift;
	my $section_addrs_ref = shift;

	my $is_module = defined($section_addrs_ref);

	my %names;
	my %data;
	my %sections;
	my %section_start_offsets;

	die "Could not find binary: $bin_path\n" unless ( -e $bin_path );

	foreach (`$objdump -ht $bin_path`) {
		if (/^([\w\d]+)\s+\w?\s+\w?\s+([\.\w]+)\s+([\w\d]+)\s+([\.\w]+)\s*$/) {
			my $addr = hex($1);
			my $section = $2;
			my $size = hex($3);
			my $name = $4;
			my $origaddr = $addr;
			my $found = 1;
			if ($is_module) {
				if ($section eq $name) {
					$section_start_offsets{$section} = $addr;
				}
				if ($section_addrs_ref->{$section}) {
					$addr = $addr - $section_start_offsets{$section} + $section_addrs_ref->{$section};
				} else {
					$found = 0;
				}
			}
			if ($found) {
				if ($size > 0) {
					$data{$addr}->{addr} = $addr;
					$data{$addr}->{size} = $size;
					$data{$addr}->{name} = $name;
					$data{$addr}->{section} = $section;
					#warn sprintf("parsed %x: $name $section $size\n", $addr);
				}
				$names{$name}->{addr} = $addr;
				$names{$name}->{size} = $size;
				$names{$name}->{section} = $section;
				#warn sprintf ("Sym: 0x%x (orig 0x%x, section $section) $name\n", $addr, $origaddr);
			}
		}
		elsif (/^\s*\d+\s+([\.\w]+)\s+([\d\w]+)\s+([\d\w]+)\s+([\d\w]+)\s+([\d\w]+)\s+\d*\**\d*\s*$/) {
			my $addr = hex($3);
			my $size = hex($2);
			my $section = $1;
			$sections{$addr}->{addr} = $addr;
			$sections{$addr}->{size} = $size;
			$sections{$addr}->{section} = $section;
		}
	}

	my @sorted_symbol_keys = sort keys %data;
	$self->{sorted_symbol_keys_ref} = \@sorted_symbol_keys;
	$self->{symbols_ref} = \%data;

	my @sorted_section_keys = sort keys %sections;
	$self->{sorted_section_keys_ref} = \@sorted_section_keys;
	$self->{sections_ref} = \%sections;

	my @sorted_names = sort keys %names;
	$self->{sorted_symbol_names_ref} = \@sorted_names;
	$self->{symbol_names_ref} = \%names;

	$self->{bin_path} = $bin_path;
}

1;
