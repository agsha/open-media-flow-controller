#!/bin/sh
#
#	File : switch_intf_dpdk.sh
#
#	Created By : Ramanand Narayanan (ramanandn@juniper.net)
#
#	Purpose : This script takes a interface name and if possible
#		enables DPDK on that interface


PCI_UNBIND_SCRIPT=/opt/dpdk/pci_unbind.py
REQ_HPAGES=1024

# Check if interface name was provided
if [ $# != 1 ]; then
	echo "Usage: $0 <interface-name>"
	exit 1;
fi

# Make sure root is rw
mount -o rw,remount /
if [ ! -d /mnt/huge ]; then
    mkdir /mnt/huge
fi

# down the given interface
# this is expected to happen only once after system restart
ifconfig -s $1 &> /dev/null
if [ $? == 0 ]; then
    ifconfig $1 down
#    INIT_NR_HUGEPAGES=`cat /proc/sys/vm/nr_hugepages`
#    echo $INIT_NR_HUGEPAGES >/opt/dpdk/initial_nr_hugepages
fi

# For now set hugepages to additional 5000 pages
#INIT_NR_HUGEPAGES=`cat /opt/dpdk/initial_nr_hugepages`
#NR_HUGEPAGES=`cat /proc/sys/vm/nr_hugepages`
#let "req = $NR_HUGEPAGES - $INIT_NR_HUGEPAGES"
#if [ $req -ge $REQ_HPAGES ]; then
#    echo "nr_hugepages already set to $NR_HUGEPAGES, nothing to change"
#else
#	let "to_set = $INIT_NR_HUGEPAGES + $REQ_HPAGES"
#    echo "nr_hugepages not set hence setting it to $to_set"
#    echo $to_set > /proc/sys/vm/nr_hugepages
#fi

# Install igb_uio driver and setup huge page
echo "Installing igb_uio driver .."
modprobe uio
lsmod | grep igb_uio &> /dev/null
if [ $? != 0 ]; then
    insmod /opt/dpdk/igb_uio.ko
fi

#Balance the numa socket hugepages to make dpi happy

node0_free=`cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/free_hugepages`
node1_free=`cat /sys/devices/system/node/node1/hugepages/hugepages-2048kB/free_hugepages`
node0_alloc=`cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages`
node1_alloc=`cat /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages`

echo $node0_alloc $node0_free $node1_alloc $node1_free

if [ $node1_free -eq 1025 ] && [ $node1_free -gt 1 ]; then
        towrite=`expr $node1_alloc - 1536`
        rem_node1=`expr $node1_alloc - $towrite`
        echo $towrite
        echo "node1 huge pages" $towrite
        echo $towrite > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
        total_node0=`expr $node0_alloc + $rem_node1`
        echo $total_node0 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
        echo "node0 huge pages" $total_node0
elif [ $node0_free -le 1025 ] && [ $node0_free -ge 1 ] && [ $node1_free -le 1025 ] && [ $node1_free -ge 1 ]; then
        towrite=`expr $node1_alloc - $node1_free`
        rem_node1=`expr $node1_alloc - $towrite`
        echo "node1 huge pages" $towrite
        echo $towrite > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
        total_node0=`expr $node0_alloc + $rem_node1`
        echo $total_node0 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
        echo "node0 huge pages" $total_node0
fi


# Mount hugetlbfs if not already mounted
mount | grep hugetlbfs
if [ $? != 0 ]; then
    mount -t hugetlbfs nodev /mnt/huge
fi

INTF_NAME=$1;
# Check if interface name is in the PCI list
PCI_DEVICE_DETAILS=`${PCI_UNBIND_SCRIPT} --status | grep "if=${INTF_NAME} drv=ixgbe"`

# Check if we found the device
if [ "_${PCI_DEVICE_DETAILS}" != "_" ]; then
	PCI_DEVICE=`echo $PCI_DEVICE_DETAILS | cut -c1-12`
	echo "enabling DPDK on PCI device : $PCI_DEVICE";
else
	echo "error: unable to find Intel 10GigE interface $INTF_NAME in PCI list"
	echo 
	echo "Interface should be from the following:"
	${PCI_UNBIND_SCRIPT} --status | grep "drv=ixgbe"
	sleep 5
	exit 0;
fi

# Now run pci_unbind.py to enable dpdk on the interface
$PCI_UNBIND_SCRIPT -b igb_uio $PCI_DEVICE

echo 
echo "Current PCI setting: "
${PCI_UNBIND_SCRIPT} --status

sleep 5

exit 0;

#
# End of switch_intf_dpdk.sh
#
