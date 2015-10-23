#!/bin/bash
#
#	File : nvsd_init.sh
#
#	Description : Shell script  that is invoked  by nvsd as the first step
#		when starting up. This is handy script to put any action that
#		needs to be taken before nvsd starts
#
#	Created By : Max He (ramanandn@juniper.net)
#
#	Created On : 15 June, 2011
#
#	Copyright (c) Juniper Networks Inc., 2008-11
#

#
# file is run by nvsd at initialization time
#
echo 3  > /proc/sys/vm/drop_caches

#
# Removing the kernel modules loaded by Samara as a part of setting up 
# NAT vnet  for VMs. We dont plan to use NAT and also ip_conntrack creates
# other issues for nvsd. Hence, we are having to rmmod the ip_conntrack module
# Bugzilla bug : 8713
# Code added by Ramanand Narayanan (ramanandn@juniper.net)
#
#
#Bug 9416
#/sbin/rmmod  iptable_nat ip_nat ip_conntrack

#
# End of nvsd_init.sh
#
