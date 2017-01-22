#!/bin/sh
TARGET_DIR=../../../doxygen/pktlogger/latex
targetfile=${TARGET_DIR}/refman.tex
tmpfile=${TARGET_DIR}/refman.tex.tmp

if [ -f $targetfile ] ; then
	echo "refman.tex exists....."
	# change latex type from "book" to "report"
	sed -e 's/book/report/' $targetfile > $tmpfile

	cp -f $tmpfile $targetfile
fi
