<?php
$handle = file("/file_hex");

foreach($handle as $value)
{
        $value = hexdec($value);
        $num2 = ($value >> 12);
        $num1 = ($value & 0xFFF);
        if($num1 < 2048)
                echo $num1;
        else
                echo "-".(4096-$num1);
        echo "\t";
        if($num2 < 2048)
                echo $num2;
        else
                echo "-".(4096-$num2);
        echo "\n";
}
?>
