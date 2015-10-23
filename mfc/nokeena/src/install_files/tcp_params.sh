#! /bin/bash
#
#       File : tcp_params.sh
#
#       Created by : Ramalingam Chandran
#	Created Date : August 25, 2010
#
#

CONNTRACK_FIN_WAIT=/proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_fin_wait

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
#--------------------------------------------------------------
# Get the TCP Parameter name
TCP_PARAM=$1;
echo $1;
shift;

FIRST=$1;
shift;

SECOND=$1;
shift;

THIRD=$1;
shift;


if [ "$TCP_PARAM" = "max-orphan" ]; then
	echo $FIRST > /proc/sys/net/ipv4/tcp_max_orphans
elif [ "$TCP_PARAM" = "fin-timeout" ]; then
	echo $FIRST > /proc/sys/net/ipv4/tcp_fin_timeout
	if [ -f $CONNTRACK_FIN_WAIT ]; then
        	echo $FIRST > $CONNTRACK_FIN_WAIT
	fi
elif [ "$TCP_PARAM" = "memory" ]; then
	echo $FIRST $SECOND $THIRD > /proc/sys/net/ipv4/tcp_mem
elif [ "$TCP_PARAM" = "read-memory" ]; then
        echo $FIRST $SECOND $THIRD > /proc/sys/net/ipv4/tcp_rmem
elif [ "$TCP_PARAM" = "write-memory" ]; then
        echo $FIRST $SECOND $THIRD > /proc/sys/net/ipv4/tcp_wmem
elif [ "$TCP_PARAM" = "max-buckets" ]; then
	echo $FIRST $SECOND $THIRD > /proc/sys/net/ipv4/tcp_max_tw_buckets
elif [ "$TCP_PARAM" = "path-mtu" ]; then
	echo $FIRST > /proc/sys/net/ipv4/ip_no_pmtu_disc
fi

ret=$?

if [ $ret -ne 0 ]; then
	echo "Failed to set the value in proc files for this command"
fi



