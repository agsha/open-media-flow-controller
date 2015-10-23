#!/bin/sh
#	File : irqaffinity_bnx2x.sh
#
#	Description : This is a temporary script  that sets up irq affinity 
#			for the broadcom 10G interfaces specifically for pacifica 
#
#	Created By : Ramanand Narayanan (ramanandn@juniper.net)
#
#	Created On : 04 April, 2012
#
#	Copyright (c) Juniper Networks Inc., 2012
#

# This is just a temporary logic. - Ramanand Narayanan
if [ "_$1" = "_eth3" ]; then
	irq=86;
fi

# Set the value for the 16 queues
echo 2 > /proc/irq/$irq/smp_affinity
irq=$[$irq+1]
echo 4 > /proc/irq/$irq/smp_affinity

#map irqs of eth3 to eth6
irq=86
echo 2 > /proc/irq/$irq/smp_affinity
echo 2 > /proc/irq/$(($irq+17))/smp_affinity
echo 2 > /proc/irq/$(($irq+34))/smp_affinity
echo 2 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 4 > /proc/irq/$irq/smp_affinity
echo 4 > /proc/irq/$(($irq+17))/smp_affinity
echo 4 > /proc/irq/$(($irq+34))/smp_affinity
echo 4 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 8 > /proc/irq/$irq/smp_affinity
echo 8 > /proc/irq/$(($irq+17))/smp_affinity
echo 8 > /proc/irq/$(($irq+34))/smp_affinity
echo 8 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 10 > /proc/irq/$irq/smp_affinity
echo 10 > /proc/irq/$(($irq+17))/smp_affinity
echo 10 > /proc/irq/$(($irq+34))/smp_affinity
echo 10 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 20 > /proc/irq/$irq/smp_affinity
echo 20 > /proc/irq/$(($irq+17))/smp_affinity
echo 20 > /proc/irq/$(($irq+34))/smp_affinity
echo 20 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 40 > /proc/irq/$irq/smp_affinity
echo 40 > /proc/irq/$(($irq+17))/smp_affinity
echo 40 > /proc/irq/$(($irq+34))/smp_affinity
echo 40 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 80 > /proc/irq/$irq/smp_affinity
echo 80 > /proc/irq/$(($irq+17))/smp_affinity
echo 80 > /proc/irq/$(($irq+34))/smp_affinity
echo 80 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 100 > /proc/irq/$irq/smp_affinity
echo 100 > /proc/irq/$(($irq+17))/smp_affinity
echo 100 > /proc/irq/$(($irq+34))/smp_affinity
echo 100 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 200 > /proc/irq/$irq/smp_affinity
echo 200 > /proc/irq/$(($irq+17))/smp_affinity
echo 200 > /proc/irq/$(($irq+34))/smp_affinity
echo 200 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 400 > /proc/irq/$irq/smp_affinity
echo 400 > /proc/irq/$(($irq+17))/smp_affinity
echo 400 > /proc/irq/$(($irq+34))/smp_affinity
echo 400 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 800 > /proc/irq/$irq/smp_affinity
echo 800 > /proc/irq/$(($irq+17))/smp_affinity
echo 800 > /proc/irq/$(($irq+34))/smp_affinity
echo 800 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 1000 > /proc/irq/$irq/smp_affinity
echo 1000 > /proc/irq/$(($irq+17))/smp_affinity
echo 1000 > /proc/irq/$(($irq+34))/smp_affinity
echo 1000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 2000 > /proc/irq/$irq/smp_affinity
echo 2000 > /proc/irq/$(($irq+17))/smp_affinity
echo 2000 > /proc/irq/$(($irq+34))/smp_affinity
echo 2000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 4000 > /proc/irq/$irq/smp_affinity
echo 4000 > /proc/irq/$(($irq+17))/smp_affinity
echo 4000 > /proc/irq/$(($irq+34))/smp_affinity
echo 4000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 8000 > /proc/irq/$irq/smp_affinity
echo 8000 > /proc/irq/$(($irq+17))/smp_affinity
echo 8000 > /proc/irq/$(($irq+34))/smp_affinity
echo 8000 > /proc/irq/$(($irq+51))/smp_affinity

#map irqs of eth7 to eth10

irq=154
echo 20000 > /proc/irq/$irq/smp_affinity
echo 20000 > /proc/irq/$(($irq+17))/smp_affinity
echo 20000 > /proc/irq/$(($irq+34))/smp_affinity
echo 20000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 40000 > /proc/irq/$irq/smp_affinity
echo 40000 > /proc/irq/$(($irq+17))/smp_affinity
echo 40000 > /proc/irq/$(($irq+34))/smp_affinity
echo 40000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 80000 > /proc/irq/$irq/smp_affinity
echo 80000 > /proc/irq/$(($irq+17))/smp_affinity
echo 80000 > /proc/irq/$(($irq+34))/smp_affinity
echo 80000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 100000 > /proc/irq/$irq/smp_affinity
echo 100000 > /proc/irq/$(($irq+17))/smp_affinity
echo 100000 > /proc/irq/$(($irq+34))/smp_affinity
echo 100000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 200000 > /proc/irq/$irq/smp_affinity
echo 200000 > /proc/irq/$(($irq+17))/smp_affinity
echo 200000 > /proc/irq/$(($irq+34))/smp_affinity
echo 200000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 400000 > /proc/irq/$irq/smp_affinity
echo 400000 > /proc/irq/$(($irq+17))/smp_affinity
echo 400000 > /proc/irq/$(($irq+34))/smp_affinity
echo 400000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 800000 > /proc/irq/$irq/smp_affinity
echo 800000 > /proc/irq/$(($irq+17))/smp_affinity
echo 800000 > /proc/irq/$(($irq+34))/smp_affinity
echo 800000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 1000000 > /proc/irq/$irq/smp_affinity
echo 1000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 1000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 1000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 2000000 > /proc/irq/$irq/smp_affinity
echo 2000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 2000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 2000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 4000000 > /proc/irq/$irq/smp_affinity
echo 4000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 4000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 4000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 8000000 > /proc/irq/$irq/smp_affinity
echo 8000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 8000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 8000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 10000000 > /proc/irq/$irq/smp_affinity
echo 10000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 10000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 10000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 20000000 > /proc/irq/$irq/smp_affinity
echo 20000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 20000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 20000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 40000000 > /proc/irq/$irq/smp_affinity
echo 40000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 40000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 40000000 > /proc/irq/$(($irq+51))/smp_affinity
irq=$[$irq+1]
echo 80000000 > /proc/irq/$irq/smp_affinity
echo 80000000 > /proc/irq/$(($irq+17))/smp_affinity
echo 80000000 > /proc/irq/$(($irq+34))/smp_affinity
echo 80000000 > /proc/irq/$(($irq+51))/smp_affinity
