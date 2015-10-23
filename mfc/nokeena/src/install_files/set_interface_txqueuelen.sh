#! /bin/bash

# Transmit queue length

isixgbe=`/sbin/ethtool -i $1 | grep ixgbe | awk '{printf $2}'`
isbnx2x=`/sbin/ethtool -i $1 | grep bnx2x | awk '{printf $2}'`

if [ "$isixgbe" == "ixgbe" ] || [ "$isbnx2x" == "bnx2x" ]; then
	logger -s "${ME}: ifconfig $1 txqueuelen 100000"
	ifconfig $1 txqueuelen 100000
	else
	logger -s "${ME}: ifconfig $1 txqueuelen 4096"
	ifconfig $1 txqueuelen 4096
fi

# irq support added in main irq logic so commenting the below call
# The below is fix only for Pacifica 
# 	added by Ramanand Narayanan (ramanandn@juniper.net) Apr 5th, 2012
# We call the script that sets up irq smp-affinity for the Broadcom interfaces
#if [ "$isbnx2x" == "bnx2x" ]; then
#
#	# Setup up the irq smp affinity for Broadcom interface
#	logger "${ME}: setting up IRQ smp-affinity for Broadcom interface $1" 
#	/opt/nkn/bin/irq-smpaffinity-bnx2x.sh $1
#
#fi
