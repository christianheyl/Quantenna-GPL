#!/usr/bin/perl -w
use strict;
use warnings;

my $src = $ARGV[0];
if (defined($src)) {
	@_ = `cat $src | grep __sram_text`;
	if ($#_ > 0) {
		print "-O3";
	} else {
		print "-Os -mno-millicode";
	}
}

