#! /bin/bash


if [ -f /proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_time_wait ]
then
	echo "30" > /proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_time_wait
fi

if [ -f /proc/sys/net/ipv4/tcp_max_tw_buckets ]
then
	echo "65536" > /proc/sys/net/ipv4/tcp_max_tw_buckets
fi
