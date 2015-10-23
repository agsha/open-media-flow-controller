#! /bin/bash
OUTPUT=`/sbin/ifconfig $1 | grep txqueuelen | awk -F: '{print $3}'`
echo $OUTPUT

