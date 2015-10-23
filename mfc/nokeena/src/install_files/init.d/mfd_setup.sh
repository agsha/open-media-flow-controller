:
# File : mfd_setup.sh
# Description : Sets up the environment for MFD to run
# Created On : 2008-06-28
# Copyright (c) Juniper Networks, 2008-2012

# Setup the ssh keys
function setup_ssh_keys()
{
    if [ -f /config/nkn/id_dsa ]; then
	logger -s "SSH keys found"
	return
    fi

    logger -s "Creating SSH keys"

    ssh-keygen -t dsa -N "" -f /config/nkn/id_dsa > /dev/null

    logger -s "SSH keys successfully created"
}


# Setup the required network parameters
function setup_network_params()
{
    logger -s "${ME}: Network parameters:"
    #TCP max buffer size setable using setsockopt()
    #----------------------------------------------
    logger -s "${ME}:echo 16777216 > /proc/sys/net/core/rmem_max"
    logger -s "${ME}:echo 16777216 > /proc/sys/net/core/wmem_max"
    echo 16777216 > /proc/sys/net/core/rmem_max
    echo 16777216 > /proc/sys/net/core/wmem_max

    #Set min, default, max for receive window and transmit window
    #------------------------------------------------------------
    logger -s "${ME}: 4096 87380 16777216 > /proc/sys/net/ipv4/tcp_rmem"
    logger -s "${ME}: 4096 65536 16777216 > /proc/sys/net/ipv4/tcp_wmem"
    echo "4096 87380 16777216" > /proc/sys/net/ipv4/tcp_rmem
    echo "4096 65536 16777216" > /proc/sys/net/ipv4/tcp_wmem

    logger -s "${ME}: echo 1 > /proc/sys/net/ipv4/tcp_window_scaling"
    logger -s "${ME}: echo 1 > /proc/sys/net/ipv4/tcp_timestamps"
    logger -s "${ME}: echo 1 > /proc/sys/net/ipv4/tcp_no_metrics_save"
    logger -s "${ME}: echo 1 > /proc/sys/net/ipv4/tcp_moderate_rcvbuf"
    logger -s "${ME}: sysctl -w net.ipv4.ip_nonlocal_bind=\"1\""
    logger -s "${ME}: sysctl -w net.ipv4.tcp_fin_timeout=\"15\""
    echo 1 > /proc/sys/net/ipv4/tcp_window_scaling
    echo 1 > /proc/sys/net/ipv4/tcp_timestamps
    echo 1 > /proc/sys/net/ipv4/tcp_no_metrics_save
    echo 1 > /proc/sys/net/ipv4/tcp_moderate_rcvbuf
    sysctl -w net.ipv4.ip_nonlocal_bind="1"
    sysctl -w net.ipv4.tcp_fin_timeout="15"

    # Fix for bug 2094 (IMS to the origin server is failing because no
    # free local ports are available (available sockets in TIME_WAIT),
    # resulting in OM_validate errors which are treated as a miss resulting
    # in OM_get(s) failing).
    # For bug 5689, comment out the setting of tcp_max_tw_buckets.
    # This is not needed now because now MFC supports connection keep-alive,
    # and we should use the default (180,000).
    #logger -s "${ME}: echo 25000 > /proc/sys/net/ipv4/tcp_max_tw_buckets"
    #                  echo 25000 > /proc/sys/net/ipv4/tcp_max_tw_buckets
    logger -s "${ME}: echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse"
    echo 1     > /proc/sys/net/ipv4/tcp_tw_reuse

    # Fix for bug 4912 - Raise local port ranges to allow for more 
    # ephemereal ports
    logger -s "${ME}: 8192 65535 > /proc/sys/net/ipv4/ip_local_port_range"
    echo "8192 65535" > /proc/sys/net/ipv4/ip_local_port_range

    #echo 2500 > /proc/sys/net/core/netdev_max_backlog
    # 30000 is for 10G
    logger -s "${ME}: echo 30000 > /proc/sys/net/core/netdev_max_backlog"
    echo 30000 > /proc/sys/net/core/netdev_max_backlog

    #
    # Tuning general Linux driver parameters
    #
    # Transmit queue length
    for i in `ifconfig -a | grep "^eth" | awk '{ print $1 }'`
    do
        isixgbe=`/sbin/ethtool -i ${i} | grep ixgbe | awk '{printf $2}'`
	isbnx2x=`/sbin/ethtool -i ${i} | grep bnx2x | awk '{printf $2}'`

        if [[ "$isixgbe" == "ixgbe" || "$isbnx2x" == "bnx2x" ]] ; then
        	logger -s "${ME}: ifconfig ${i} txqueuelen 100000"
        	ifconfig ${i} txqueuelen 100000
        else
        	logger -s "${ME}: ifconfig ${i} txqueuelen 4096"
        	ifconfig ${i} txqueuelen 4096
        fi
    done
    ifconfig -a | logger -s

    #
    # Tuning Intel NIC driver parameters. e1000e and igb only so far.
    #
    ise1000e=`/sbin/lsmod | grep "^e1000e" | awk '{printf $1}'`
    if [ "$ise1000e" == "e1000e" ] ; then
        logger -s "${ME}: modprobe e1000e InterruptThrottleRate=4000,4000"
        logger -s "${ME}: modprobe e1000e TxIntDelay=800,800"
        modprobe e1000e InterruptThrottleRate=4000,4000
        modprobe e1000e TxIntDelay=800,800
    fi
    #isigb=`/sbin/lsmod | grep "^igb" | awk '{printf $1}'`
    #if [ "$isigb" == "igb" ] ; then
    #    modprobe igb InterruptThrottleRate=4000,4000
    #    modprobe igb TxIntDelay=800,800
    #fi

    #For very long fast paths, I suggest trying cubic or
    #htcp if reno is not is not performing as desired. To
    #set this, do the following:
    #-----------------------------------------------------
    logger -s "${ME}: sysctl -w net.ipv4.tcp_congestion_control=htcp > /dev/null"
    sysctl -w net.ipv4.tcp_congestion_control=htcp > /dev/null

    #Disable reverse path check
    # This check is enabled by default in TM. It drops packets if the route back
    # to the source isn't same as the ingress interface.
    # So we need to disable it. It would be even better if we could provide
    # a management node and UI to enable it if any customer wants this feature.
    logger -s "${ME}: echo 0 > /proc/sys/net/ipv4/conf/all/rp_filter"
    logger -s "${ME}: echo 0 > /proc/sys/net/ipv4/conf/default/rp_filter"

    echo 0 > /proc/sys/net/ipv4/conf/all/rp_filter
    echo 0 > /proc/sys/net/ipv4/conf/default/rp_filter

    #----------------------------------------------
    # The default numbers of maximum ip neighbors is not enough. (Bug 5484)
    # Default gc_thresh1 = 128
    echo  8192 > /proc/sys/net/ipv4/neigh/default/gc_thresh1
    # Default gc_thresh2 = 512
    echo 16384 > /proc/sys/net/ipv4/neigh/default/gc_thresh2
    # Default gc_thresh3 = 1024
    echo 24576 > /proc/sys/net/ipv4/neigh/default/gc_thresh3
    #----------------------------------------------
 
    #----------------------------------------------  
    # Forcing IGMP protocol version to 2  [Bug 5983]
    echo "2" > /proc/sys/net/ipv4/conf/all/force_igmp_version
    #----------------------------------------------
} # end of setup_network_params()

# Setup the required file system parameters
function setup_fs_params()
{
    logger -s "${ME}: Filesystem parameters:"

    #Max number of file descriptors possible
    #---------------------------------------
    logger -s "${ME}: ulimit -n 64000"
    ulimit -n 64000

    # Setup core file size
    logger -s "${ME}: ulimit -c unlimited"
    ulimit -c unlimited
    
    # Bug 7133/7142 - Increasing shared memory size
    echo 50000000 > /proc/sys/kernel/shmmax

} # end of setup_fs_params ()

# Setup the required file system parameters
function setup_cpu_params()
{
    logger -s "${ME}: CPU parameters:"

    # ------BY Ramanand on 22nd July 2010 -------------
    # COMMENTING OUT irqbalance here as we now have the 
    # irqbalance RPM installed and that has a init.d 
    # script to run it startup. Also there is a config
    # file /etc/sysconfig/irqbalance that controls the 
    # behavior. 
    # Balance the IRQs across all the CPUs.
    #---------------------------------------
    #logger -s "${ME}: /usr/sbin/irqbalance --oneshot"
    #/opt/nkn/bin/irqbalance --oneshot

} # end of setup_cpu_params ()

# Setup the Jail root for policy engine
function setup_jail_root ()
{
    #PE directory
    PE_DIR=/nkn/policy_engine/

    #copy the libraries
    mkdir -p ${PE_DIR}/lib64

    cp /lib64/libselinux.so.1 ${PE_DIR}/lib64/
    cp /lib64/libacl.so.1 ${PE_DIR}/lib64/
    cp /lib64/libc.so.6 ${PE_DIR}/lib64/
    cp /lib64/libdl.so.2 ${PE_DIR}/lib64/
    cp /lib64/ld-linux-x86-64.so.2 ${PE_DIR}/lib64/
    cp /lib64/libattr.so.1 ${PE_DIR}/lib64/
    cp /lib64/libm.so.6 ${PE_DIR}/lib64/
    cp /lib64/libncurses.so.5  ${PE_DIR}/lib64/
    cp /lib64/libtinfo.so.5 ${PE_DIR}/lib64/

    #copy the vi binary
    mkdir -p ${PE_DIR}/bin
    cp /bin/vi ${PE_DIR}/bin/

    #copy the jmfc_sample
    cp /opt/nkn/bin/policy/jmfc_sample.tcl ${PE_DIR}
}
# end if setup_jail_root ()

# Setup route rules based on packet marks
# We only setup the rules here. Unless the packet
# is marked due to some decision by the mangle
# table, no routing decision is made based on
# these route rules.
function setup_route_rules()
{
    logger -s "${ME}: Setting up route rules for marked packets:"
    logger -s "${ME}: ip rule add fwmark 1 lookup 100"
    ip rule add fwmark 1 lookup 100
    logger -s "${ME}: ip route add local 0.0.0.0/0 dev lo table 100"
    ip route add local 0.0.0.0/0 dev lo table 100
} # end of setup_route_rules( ()

function setup_ipfilters()
{
    logger -s "${ME}: Setting up ip filter rules"
    /sbin/iptables -t mangle -N DIVERT
    /sbin/iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
    /sbin/iptables -t mangle -A DIVERT -j MARK --set-mark 1
    /sbin/iptables -t mangle -A DIVERT -j ACCEPT
}

function chown_named_work_dir()
{
    chown  named:named /var/named/chroot
    chown -R named:named /var/named/chroot/var
    chown -R named:named /var/named/chroot/etc
    chmod -R a+rw /var/named/chroot/var
}

function setup_virtual_memory_params()
{
    echo 40 > /proc/sys/vm/dirty_ratio
}

#--------------------------------------------------------------

#
#  MAIN LOGIC BEGINS HERE
#

#--------------------------------------------------------------

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
umask 0022

ME=`basename "${0}"`
logger -s "${ME}: Setting up all the parameters for MFD Server:"

setup_fs_params
setup_network_params
setup_cpu_params
setup_jail_root
setup_route_rules
setup_ssh_keys
#setup_ipfilters
setup_virtual_memory_params

chown_named_work_dir

exit 0

#
# End of mfd_setup.sh
#
