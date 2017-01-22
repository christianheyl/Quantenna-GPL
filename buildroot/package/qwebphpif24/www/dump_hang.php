<?php
	$tcm = "E5040000";
	$dataq = "E5040020";
	$dataq = hexdec($dataq);
	$fctl_read_ptr = exec("readmem e5050404 | cut -d: -f2 | cut -dx -f2 | cut -b6");
	$fctl_read_ptr = hexdec($fctl_read_ptr);
	$fctl_write_ptr = exec("readmem e5050404 | cut -d: -f2 | cut -dx -f2 | cut -b7");
	$fctl_write_ptr = hexdec($fctl_write_ptr);

	if($fctl_read_ptr != $fctl_write_ptr){
		#echo "Data Queue:\n";
		$fctl_read_ptr = $fctl_read_ptr & 0x7;
		if($fctl_read_ptr % 2 == 0) {
		        $fctl_read_ptr = $fctl_read_ptr + 1;
		}
		$fctl_read_ptr = $fctl_read_ptr * 4 + $dataq;
		$fctl_read_ptr =  dechex($fctl_read_ptr);
		$fctl = exec("readmem $fctl_read_ptr | cut -d: -f2");
		#echo "FCS:\n";
		exec("dumpfctl $fctl");
		$dmanext_ptr = hexdec($fctl) + 16;
		$dmanext_ptr = dechex($dmanext_ptr);
		$dmanext = exec("readmem $dmanext_ptr | cut -d: -f2 | cut -dx -f2");
		if($dmanext) {
		#	echo "DMA Chain:\n";
		    exec("dumpdma $dmanext");
		}
		$rrt_ptr = hexdec($fctl) + 44;
		$rrt_ptr = dechex($rrt_ptr);
		$rrt = exec("readmem $rrt_ptr | cut -d: -f2 | cut -dx -f2");
		$rrt = substr($rrt, -4);
		$rrt = dechex(hexdec($tcm) + hexdec($rrt));
		#echo "Rate Retry Table:\n";
		exec("dumprrt $rrt");
	}

	$mgmtq = "E5040060";
	$mgmtq = hexdec($mgmtq);
	$fctl_read_ptr = exec("readmem e505040C | cut -d: -f2 | cut -dx -f2 | cut -b6");
	$fctl_read_ptr = hexdec($fctl_read_ptr);
	$fctl_write_ptr = exec("readmem e505040C | cut -d: -f2 | cut -dx -f2 | cut -b7");
	$fctl_write_ptr = hexdec($fctl_write_ptr);

	if($fctl_read_ptr != $fctl_write_ptr){
		#echo "Management Queue:\n";
		$fctl_read_ptr = $fctl_read_ptr & 0x7;
		if($fctl_read_ptr % 2 == 0) {
		        $fctl_read_ptr = $fctl_read_ptr + 1;
		}
		$fctl_read_ptr = $fctl_read_ptr * 4 + $mgmtq;
		$fctl_read_ptr =  dechex($fctl_read_ptr);
		$fctl = exec("readmem $fctl_read_ptr | cut -d: -f2");
		#echo "FCS:\n";
		passthru("dumpfctl $fctl");
		$dmanext_ptr = hexdec($fctl) + 16;
		$dmanext_ptr = dechex($dmanext_ptr);
		$dmanext = exec("readmem $dmanext_ptr | cut -d: -f2 | cut -dx -f2");
		if($dmanext) {
		#	echo "DMA Chain:\n";
		    passthru("dumpdma $dmanext");
		}
		$rrt_ptr = hexdec($fctl) + 44;
		$rrt_ptr = dechex($rrt_ptr);
		$rrt = exec("readmem $rrt_ptr | cut -d: -f2 | cut -dx -f2");
		$rrt = substr($rrt, -4);
		$rrt = dechex(hexdec($tcm) + hexdec($rrt));
		#echo "Rate Retry Table:\n";
		passthru("dumprrt $rrt");
	}
?>
