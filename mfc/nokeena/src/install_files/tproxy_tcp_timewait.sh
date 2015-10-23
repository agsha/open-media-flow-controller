#! /bin/bash

# Local variables
TIMEWAIT_FILE="/proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_time_wait"
TIMEOUT_FILE="/proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_close_wait"

# Get the parameters
time_value=$1
shift;
OPTIONS=$1
shift;

# Write into "/proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_time_wait"
# if it is available.
# This file will be available only if TPROXY is configured.
if [ "$OPTIONS" = "timewait" ]; then
if [ -f $TIMEWAIT_FILE ]; then
    echo $time_value > $TIMEWAIT_FILE 
fi
fi

# Write into "/proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_close_wait"
# if it is available.
# This file will be available only if TPROXY is configured.
if [ "$OPTIONS" = "timeout" ]; then
if [ -f $TIMEOUT_FILE ]; then
    echo $time_value > $TIMEOUT_FILE
fi
fi
