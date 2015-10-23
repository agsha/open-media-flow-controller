#! /bin/bash


# log directory
LOG_DIR=/var/log/nkn/cache


if [ $# -lt 1 ]; then
	echo "usage: mgmt_namespace_cleanup <uid>"
	exit 0
fi

NS_UID=$1

if [ -d "$LOG_DIR" ]; then 
 rm -rf "$LOG_DIR/$NS_UID"*  
fi



