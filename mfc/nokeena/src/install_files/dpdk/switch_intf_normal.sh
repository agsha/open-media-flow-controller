#!/bin/sh
#
#       File : switch_intf_normal.sh
#
#       Created By : Muthukumar Sundaresan
#
#       Purpose : This script finds out the interface in the DPDK mode
#                 and switches it back to normal mode

PCI_UNBIND_SCRIPT=/opt/dpdk/pci_unbind.py

# Check if interface name was provided
if [ $# != 1 ]; then
        echo "Usage: $0 <interface-name>"
        exit 1;
fi

# Find the PCI device using the dpdk driver
PCI_DEVICE=`$PCI_UNBIND_SCRIPT --status | grep "drv=igb_uio" | cut -c1-12`

# Now run pci_unbind.py to disable dpdk on the interface
$PCI_UNBIND_SCRIPT -b ixgbe $PCI_DEVICE

ETH_NUM=`/opt/dpdk/pci_unbind.py --status | grep "$PCI_DEVICE" | grep -o "eth[0-9]*"`

# Check if the eth number is same as one passed in command line.
# Else change it to the interface name
if [[ ! -z $ETH_NUM ]]; then 
    if [ $ETH_NUM != $1 ]; then
	echo "Changing $ETH_NUM to $1"
	ip link set dev $ETH_NUM name $1
    fi
fi

# Do not bring up the interface, let it be done in MFC so 
#    that correct IP address is set
#
#ifconfig $1 up


huge=`cat /proc/mounts | grep huge`
if [ ! -z "$huge" ];
then
       umount /mnt/huge
fi

sleep 5

# End of switch_intf_normal.sh
#
