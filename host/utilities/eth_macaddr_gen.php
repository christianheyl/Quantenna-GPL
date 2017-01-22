<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<?php
	if(file_exists("eth_macaddrs.txt"))
	{
		$curr_wmacaddr = file_get_contents("eth_macaddrs.txt");

        	list($new_wifi[0], $new_wifi[1], $new_wifi[2], $new_wifi[3], $new_wifi[4], $new_wifi[5]) = explode(":", $curr_wmacaddr);

        	$new_wifi[5] = hexdec($new_wifi[5]);

		if($new_wifi[5]  >= 255 )
		{
			$new_wifi[5] = 0;
			$new_wifi[5] = sprintf("%02X", $new_wifi[5]);
        		$new_wifi[4] = hexdec($new_wifi[4]);

			if($new_wifi[4]  >= 255 )
			{
				$new_wifi[4] = 0;
				$new_wifi[4] = sprintf("%02X", $new_wifi[4]);
        			$new_wifi[3] = hexdec($new_wifi[3]);

				if($new_wifi[3]  >= 255 )
				{
					$new_wifi[3] = 0;
					$new_wifi[3] = sprintf("%02X", $new_wifi[3]);
        				$new_wifi[2] = hexdec($new_wifi[2]);

					if($new_wifi[2]  >= 255 )
					{
						$new_wifi[2] = 0;
						$new_wifi[2] = sprintf("%02X", $new_wifi[2]);
        					$new_wifi[1] = hexdec($new_wifi[1]);

						if($new_wifi[1]  >= 255 )
						{
							$new_wifi[1] = 0;
							$new_wifi[1] = sprintf("%02X", $new_wifi[1]);
        						$new_wifi[0] = hexdec($new_wifi[0]);
							$new_wifi[0] = $new_wifi[0] + 1;
							$new_wifi[0] = sprintf("%02X", $new_wifi[0]);
						}
						else
						{
        						$new_wifi[1] = $new_wifi[1] + 1;
							$new_wifi[1] = sprintf("%02X", $new_wifi[1]);
						}
					}
					else
					{
        					$new_wifi[2] = $new_wifi[2] + 1;
						$new_wifi[2] = sprintf("%02X", $new_wifi[2]);
					}
				}
				else
				{
        				$new_wifi[3] = $new_wifi[3] + 1;
					$new_wifi[3] = sprintf("%02X", $new_wifi[3]);
				}
			}
			else
			{
				$new_wifi[4] = $new_wifi[4] + 1;
				$new_wifi[4] = sprintf("%02X", $new_wifi[4]);
			}
		}
		else
		{
        		$new_wifi[5] = $new_wifi[5] + 1;
			$new_wifi[5] = sprintf("%02X", $new_wifi[5]);
		}

        	$arr = array($new_wifi[0], $new_wifi[1], $new_wifi[2], $new_wifi[3], $new_wifi[4], $new_wifi[5]);
        	$new_wmacaddr1 = implode(":", $arr);
		file_put_contents("eth_macaddrs.txt", $new_wmacaddr1);
	}
	else
	{
		//This is first time file is created
		$first_wmacaddr = "00:26:86:00:00:00";
		file_put_contents("eth_macaddrs.txt", $first_wmacaddr);
	}
?>
</HTML>
