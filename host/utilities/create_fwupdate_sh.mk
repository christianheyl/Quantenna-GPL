FLASHPART = /dev/mtd0

%.update.sh: %
	rm -f $@
	echo	'#!/bin/sh'						>>$@
	echo	''							>>$@
	echo	'flashpart=$(FLASHPART)'				>>$@
	echo	'newfw=${<F}'						>>$@
	echo	'flashtmp=/tmp/flash.tmp'				>>$@
	echo	'realsize='`cat $< | wc -c | awk '{print $$1}'`		>>$@
	echo	'realmd5='`cat $< | md5sum | awk '{print $$1}'`		>>$@
	echo	''							>>$@
	echo	'getsize()'						>>$@
	echo	'{'							>>$@
	echo	"	cat \$$1 | wc -c | awk '{print \$$1}'"		>>$@
	echo	'}'							>>$@
	echo	''							>>$@
	echo	'getmd5()'						>>$@
	echo	'{'							>>$@
	echo	"	cat \$$1 | md5sum | awk '{print \$$1}'"		>>$@
	echo	'}'							>>$@
	echo	''							>>$@
	echo	'echo "$$0: Extracting $$newfw..."'			>>$@
	echo	'uudecode > $$newfw <<EOF'				>>$@
	cat $< | uuencode -m ${<F} >>$@
	echo	'EOF'							>>$@
	echo	''							>>$@
	echo	'if [ ! -e $$flashpart -o ! -e $$newfw ] ; then'		>>$@
	echo	'	echo Upgrade FAILED: files do not exist'	>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'echo "$$0: Checking $$newfw..."'				>>$@
	echo	'filesize=$$(getsize $$newfw)'				>>$@
	echo	'if [ $$filesize -ne $$realsize ] ; then'		>>$@
	echo	'	echo Upgrade FAILED: $$newfw should be size $$realsize'	>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'echo "$$0: Checking $$flashpart..."'			>>$@
	echo	'flashsize=$$(getsize $$flashpart)'			>>$@
	echo	'if [ $$filesize -gt $$flashsize ] ; then'		>>$@
	echo	'	echo Upgrade FAILED: $$flashpart too small for $$newfw'	>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'if [ $$(($$filesize * 2)) -lt $$flashsize ] ; then'		>>$@
	echo	'	echo Upgrade FAILED: $$newfw seems too small for $$flashpart... aborting'	>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'filemd5=$$(getmd5 $$newfw)'				>>$@
	echo	'if [ "$$filemd5" != "$$realmd5" ] ; then' 		>>$@
	echo	'	echo Upgrade FAILED: md5sum incorrect'		>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'dd if=$$flashpart bs=1 count=$$filesize of=$$flashtmp'	>>$@
	echo	'if [ $$? -ne 0 ] ; then'				>>$@
	echo	'	echo Upgrade FAILED: could not read $$flashpart'	>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'flashmd5=$$(getmd5 $$flashtmp)'			>>$@
	echo	'if [ "$$filemd5" == "$$flashmd5" ] ; then'		>>$@
	echo	'	echo $$flashpart already matches $$newfw, no upgrade necessary'	>>$@
	echo	'	exit 0'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'echo "$$0: Writing $$newfw to $$flashpart..."'		>>$@
	echo	'(sync					&& \'		>>$@
	echo	'	flash_eraseall $$flashpart	&& \'		>>$@
	echo	'	sleep 1				&& \'		>>$@
	echo	'	cat $$newfw > $$flashpart		&& \'		>>$@
	echo	'	sleep 1)			|| \'		>>$@
	echo	'	(echo Upgrade FAILED, flash likely corrupt && exit 1)'	>>$@
	echo	''							>>$@
	echo	'echo "$$0: Verifying $$flashpart..."'			>>$@
	echo	'dd if=$$flashpart bs=1 count=$$filesize of=$$flashtmp'	>>$@
	echo	'if [ $$? -ne 0 ] ; then'				>>$@
	echo	'	echo Verify FAILED: could not read $$flashpart'	>>$@
	echo	'	exit 2'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'flashmd5=$$(getmd5 $$flashtmp)'			>>$@
	echo	'if [ "$$filemd5" != "$$flashmd5" ] ; then'		>>$@
	echo	'	echo Verify FAILED: $$flashpart does not match $$newfw after write'	>>$@
	echo	'	exit 1'						>>$@
	echo	'fi'							>>$@
	echo	''							>>$@
	echo	'echo Upgrade and verify successful'			>>$@
	chmod a+rx $@

