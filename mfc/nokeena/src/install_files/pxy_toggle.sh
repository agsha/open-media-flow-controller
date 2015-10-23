#!/bin/sh
if [ $# -lt 1 ]; then
  echo "Usage: pxy_toggle.sh <1/0>"
  exit 0
fi

PXY_ENABLE=$1
shift

#PXY_TBL_FILE
PXY_IP_TBL_FILE=/vtmp/pxy_tbl_rules

#Remove IP-Tables
function pxy_remove_iptables()
{

    #Remove all existing IP-Table rules
    logger "L3-Proxy: Removing IP-table rules"
    /sbin/iptables -X
    /sbin/iptables -t mangle -F
    /sbin/iptables -t mangle -X
    /sbin/iptables -P INPUT ACCEPT
    /sbin/iptables -P FORWARD ACCEPT
    /sbin/iptables -P OUTPUT ACCEPT

}

#Enable PXY
function pxy_enable()
{

    #Store the current IP-Table rules
    /sbin/iptables-save > ${PXY_IP_TBL_FILE}

    #Remove IPtables
    pxy_remove_iptables;

    #Enable IPFW
    logger "L3-Proxy: Enabling IP-FWD"
    echo 1 > /proc/sys/net/ipv4/ip_forward

}

function pxy_disable_iptable()
{
    #check if ip-table file exists
    if [ ! -f $PXY_IP_TBL_FILE ]; then
	#logger "INFO:No IP-Table rules to restore";
	exit 0;
    fi
    #Re-Store the current IP-Table rules
    logger "L3-Proxy: Restoring IP-table rules"
    /sbin/iptables-restore < ${PXY_IP_TBL_FILE}

    #RM the rules
    rm ${PXY_IP_TBL_FILE}
}

#Disable PXY
function pxy_disable()
{
    #check if ip-table file exists

    if [ ! -f $PXY_IP_TBL_FILE ]; then
	#logger "INFO:No IP-Table rules to restore";
	exit 0;
    fi
    #Re-Store the current IP-Table rules
    logger "L3-Proxy: Restoring IP-table rules"
    /sbin/iptables-restore < ${PXY_IP_TBL_FILE}

    #RM the rules
    rm ${PXY_IP_TBL_FILE}

    #Disable IPFWD
    logger "L3-Proxy: Disabling IP-FWD"
    echo 0 > /proc/sys/net/ipv4/ip_forward

}


if [ "$PXY_ENABLE" == "0" ]; then
	pxy_disable; 
elif [ "$PXY_ENABLE" == "1" ]; then
	pxy_enable;
elif [ "$PXY_ENABLE" == "2" ]; then
	pxy_disable_iptable;

fi

