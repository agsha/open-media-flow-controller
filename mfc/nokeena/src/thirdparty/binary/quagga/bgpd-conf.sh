#!/bin/sh

no_of_arguments="`echo $#`"

bgp_cmd=$1
bgp_sub_cmd=$2

(
    # Wait for lock on /usr/local/bin/bgpd-conf.lock for 30 seconds
    flock -x -w 30 200

    if [ $? != "0" ]; then
        logger -p local3.err -t bgpd-conf.sh "cannot get bgpd-conf.lock, bgpd will restart"
        /usr/bin/killall -15 bgpd
        exit 1
    fi        	

    if [ $no_of_arguments -eq 1 ];then
        bgp_stdout=$(/usr/local/bin/vtysh "-c" "conf t" "-c" "$bgp_cmd")
    else
        bgp_stdout=$(/usr/local/bin/vtysh "-c" "conf t" "-c" "$bgp_cmd" "-c" "$bgp_sub_cmd")
    fi

    OUT=$?

    if [[ $OUT -ne 0 || -n "$bgp_stdout" ]];then
        logger -p local3.err -t bgpd-conf.sh "config failed, ret $OUT, error: $bgp_stdout bgpd will restart"
        /usr/bin/killall -15 bgpd  
    fi

) 200</usr/local/bin/bgpd-conf.lock

