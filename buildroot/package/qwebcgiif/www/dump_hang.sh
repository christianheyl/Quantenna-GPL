#!/bin/sh

tcm="E5040000"
dataq="E5040020"
dataq=`printf "%d" "0x$dataq"`
fctl_read_ptr=`readmem e5050404 | cut -d: -f2 | cut -dx -f2 | cut -b6`
fctl_read_ptr=`printf "%d" "0x$fctl_read_ptr"`
fctl_write_ptr=`readmem e5050404 | cut -d: -f2 | cut -dx -f2 | cut -b7`
fctl_write_ptr=`printf "%d" "0x$fctl_write_ptr"`

if [ "$fctl_read_ptr" -ne "$fctl_write_ptr" ]
then
	#echo "Data Queue:\n";
	let fctl_read_ptr=$fctl_read_ptr%8
	let tmp_compare=$fctl_read_ptr%2
	if [ $tmp_compare -eq 0 ]
	then
		let fctl_read_ptr=$fctl_read_ptr+1
	fi
	let fctl_read_ptr=$fctl_read_ptr*4+$dataq
	fctl_read_ptr=`printf "%x" "$fctl_read_ptr"`
	fctl=`readmem $fctl_read_ptr | cut -d ' ' -f2`
	#echo "FCS:\n";
	dumpfctl $fctl
	dec_fct1=`printf "%d" "0x$fctl"`
	let dmanext_ptr=$dec_fct1+16
	dmanext_ptr=`printf "%x" "$dmanext_ptr"`
	dmanext=`readmem $dmanext_ptr | cut -d: -f2 | cut -dx -f2`
	if [ "$dmanext" != "0" ]
	then
	#	echo "DMA Chain:\n";
		dumpdma $dmanext
	fi
	let rrt_ptr=$dec_fct1+44
	rrt_ptr=`printf "%x" "$rrt_ptr"`
	rrt=`readmem $rrt_ptr | cut -d: -f2 | cut -dx -f2`
	rrt=`echo $rrt | sed 's/\(.*\)\(....\)$/\2/g'`
	dec_tcm=`printf "%d" "0x$tcm"`
	dec_rrt=`printf "%d" "0x$rrt"`
	let tmp_tcm_plus_rrt=$dec_tcm+$dec_rrt
	rrt=`printf "%x" "$tmp_tcm_plus_rrt"`
	#echo "Rate Retry Table:\n";
	dumprrt $rrt
fi

mgmtq="E5040060"
mgmtq=`printf "%d" "0x$mgmtq"`
fctl_read_ptr=`readmem e505040C | cut -d: -f2 | cut -dx -f2 | cut -b6`
fctl_read_ptr=`printf "%d" "0x$fctl_read_ptr"`
fctl_write_ptr=`readmem e505040C | cut -d: -f2 | cut -dx -f2 | cut -b7`
fctl_write_ptr=`printf "%d" "0x$fctl_write_ptr"`

if [ "$fctl_read_ptr" -ne "$fctl_write_ptr" ]
then
	#echo "Management Queue:\n";
	let fctl_read_ptr=$fctl_read_ptr%8
	let tmp_compare=$fctl_read_ptr%2
	if [ $tmp_compare -eq 0 ]
	then
		let fctl_read_ptr=$fctl_read_ptr+1
	fi
	let fctl_read_ptr=$fctl_read_ptr*4+$mgmtq;
	fctl_read_ptr=`printf "%x" "$fctl_read_ptr"`
	fctl=`readmem $fctl_read_ptr | cut -d ' ' -f2`
	#echo "FCS:\n";
	dumpfctl $fctl
	dec_fct1=`printf "%d" "0x$fctl"`
	let dmanext_ptr=$dec_fct1+16
	dmanext_ptr=`printf "%x" "$dmanext_ptr"`
	dmanext=`readmem $dmanext_ptr | cut -d: -f2 | cut -dx -f2`
	if [ "$dmanext" != "0" ]
	then
	#	echo "DMA Chain:\n";
		dumpdma $dmanext
	fi
	let rrt_ptr=$dec_fct1+44
	rrt_ptr=`printf "%x" "$rrt_ptr"`
	rrt=`readmem $rrt_ptr | cut -d: -f2 | cut -dx -f2`
	rrt=`echo $rrt | sed 's/\(.*\)\(....\)$/\2/g'`
	dec_tcm=`printf "%d" "0x$tcm"`
	dec_rrt=`printf "%d" "0x$rrt"`
	let tmp_tcm_plus_rrt=$dec_tcm+$dec_rrt
	rrt=`printf "%x" "$tmp_tcm_plus_rrt"`
	#echo "Rate Retry Table:\n";
	dumprrt $rrt
fi

