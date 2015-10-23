#! /bin/bash
#
#       File : set_syncookie.sh
#
#       Created by : Ramalingam Chandran
#       Created Date : July 29th, 2010
#
#

echo $1 > /proc/sys/net/ipv4/tcp_syncookies

ret=$?

if [ $ret -ne 0 ]; then
    echo "Failed to set the value in /proc/sys/net/ipv4/tcp_syncookies"
fi

if [ $# -eq 2 ]; then
    echo $2 > /proc/sys/net/ipv4/tcp_max_syn_backlog
    ret=$?

    if [ $ret -ne 0 ]; then
       echo "Failed to set the value in /proc/sys/net/ipv4/tcp_max_syn_backlog"
    fi
fi
