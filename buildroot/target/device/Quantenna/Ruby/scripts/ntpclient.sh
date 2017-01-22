#!/bin/sh

if [ $# != 1 ]
then
	echo "Usage: ntpclient.sh 1/2 "
	exit
fi

#Load ntpclient Config file
ntpclient_file=/mnt/jffs2/ntpclient.conf
ntpstatus_file=/tmp/ntpstatus

# if no configuration file, copy default one
if [ ! -f $ntpclient_file ]
then
	cp /etc/ntpclient.conf $ntpclient_file
fi

# check again. if no file, ntp client is not supported
if [ ! -f $ntpclient_file ]
then
	exit
fi

# check if ntp client is enabled
ntpclient_enable=`cat $ntpclient_file | grep Enable | awk -F '=' '{print $2}'`

if [ "$ntpclient_enable" == "0" ]
then
	exit
fi

# crontab information
crondir=/tmp/cron/crontabs
crontab=$crondir/cron.update
cronroot=$crondir/root

if [ ! -d $crondir ]
then
	mkdir -p $crondir
fi

if [ ! -f $crontab ]
then
	echo "root" > $crontab
fi

if [ ! -f $cronroot ]
then
	touch $cronroot
fi

# check ntp synchronization status
year=`date +%Y`
ntpserver1=`cat $ntpclient_file | grep "NTPServer=" | awk -F '=' '{print $2}'`
ntpserver2=`cat $ntpclient_file | grep "NTPServer2=" | awk -F '=' '{print $2}'`
ntpindex=`expr 3 - $1`

if [ $year -lt 2011 ]
then
	# Unsynchronized
	echo "Unsynchronized" > $ntpstatus_file
	# Check the configured server addresses
	# If ntpserver1 is not configured, we'll only use ntpserver2
	if [ ! -n "$ntpserver1" ]
	then
		ntpindex=2
	else
	# If ntpserver2 is not configured, we'll only use ntpserver1
	if [ ! -n "$ntpserver2" ]
	then
		ntpindex=1
	fi
	fi
	# If both ntpserver1 and ntpserver2 are not configured, "Error" will be reported. This is handled in /scripts/ntpclient.
	# Try another ntp server
	sed -i -e '/ntpclient/d' -e "/#NTP Client Start#/a* * * * * /scripts/ntpclient.sh $ntpindex" $cronroot
else
	# Synchronized
	echo "Synchronized" > $ntpstatus_file

	# Generate random numbers to start ntp client
	let randnum=0x`cat /proc/sys/kernel/random/uuid | cut -f1 -d"-" `
	start=`expr $randnum % 13 + 12 + \`date +%H\``
	starttime=`expr $start % 24 `

	# We'll keep using this working server, and start ntp client in sometime 12 hours later
	ntpindex=$1
	sed -i -e '/ntpclient/d' -e "/#NTP Client Start#/a1 ${starttime} * * * /scripts/ntpclient.sh $ntpindex" $cronroot
fi

if pidof ntpclient
then
	killall -q ntpclient
fi

# Determine which server we should use
case $ntpindex in
1)
	ntpserver=$ntpserver1
	;;
2)
	ntpserver=$ntpserver2
	;;
*)
	ntpserver=$ntpserver1
	;;
esac

/usr/sbin/ntpclient -s -h $ntpserver &
