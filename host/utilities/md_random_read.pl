#!/usr/bin/perl -w
# Script for testing mem_dbg.ko 

$| = 1;
use strict;
use Net::Telnet;

my $target_addr = "192.168.1.200";
my $clycles = 0; # 0 = infinite number of cycles
my $param;

my $max_block_count = 0xFF; # Maximun number of 4 byte blocks to read
my @safe_regions = ([0x70000000,0x90000000 - $max_block_count]);
                   # [0xE0000000,0xFA000000]); # The end of the last region might be exceeded
my $safe_regions_count = @safe_regions;
my $current_region = 0;

sub generate_read_address {
	my $addr = int(rand($safe_regions[$current_region][1] - $safe_regions[$current_region][0])) + $safe_regions[$current_region][0];
	if(++ $current_region == $safe_regions_count) {
		$current_region = 0;
	}
	return $addr;
}

while($param = shift) {
	if($param eq "-a" ) {
		if(!($target_addr = shift)) {
			print "Target address of target board should be specified after -a switch\n";
			exit 1;
		}
	}
	if($param eq "-c" ) {
		if(!($clycles = shift)) {
			print "Cycles count must be specified after -c switch\n";
			exit 1;
		}
	}
}

my $telnet_session = new Net::Telnet (Timeout => 25,errmode => "return");

while(1)
{
	my $read_addr = generate_read_address();
	print "Connecting to the target... ";
	if($telnet_session->open($target_addr)) {
		print "ok\n";

		$telnet_session->buffer_empty();
		$telnet_session->waitfor("/login/i");
		$telnet_session->print("root");
		$telnet_session->waitfor("/quantenna/i");

		my $read_addr = generate_read_address();

		my $cmd = sprintf("md 0x%08X 0x%08X",$read_addr,int(rand($max_block_count)));
		print "Executing cmd \"$cmd\"... ";
		$telnet_session->print($cmd);
		if($telnet_session->waitfor(String => "quantenna #", Timeout => 10)) {
			printf "success\n";
		} else {
			printf "timeout\n";
		}

		$telnet_session->close();
		
		if($clycles > 0 && --$clycles == 0) {
			exit 0;
		}
	} else {
		print "timeout\n";
	}
}
