<?php
	$rc = exec("readmem e0000010 | awk '{print \$NF}' | cut -dx -f2 | cut -b8");
        if($rc == 4)
        {
                $flash_mount_point = exec("call_qcsapi get_file_path security");
                $swc = exec("ls -l $flash_mount_point | grep soc_event_cntr");
                if($swc != "")
                {
                        $swc = exec("cat $flash_mount_point/soc_event_cntr");
                        $swc = $swc + 1;
                        exec("echo $swc > $flash_mount_point/soc_event_cntr");
                }
                else
                {
                        exec("echo 1 > $flash_mount_point/soc_event_cntr");
                }

        }
?>
