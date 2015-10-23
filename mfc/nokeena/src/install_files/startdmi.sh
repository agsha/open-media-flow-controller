#!/bin/sh
#
#	File : startdmi.sh
#
#	Description : This script starts VJX and sets up some of the
#		essentials in VJX
#
#	Created By : Rajeswaran Arumugasamy (rajaru@juniper.net)
#	Created On : 19 Jan, 2012
#
#	Modified By : Ramanand Narayanan (ramanandn@juniper.net)
#	Modified On : 18 Oct, 2012
#
#	Copyright (c) Juniper Networks Inc., 2012
#


QEMU_IFUP=/config/nkn/qemu-ifup
LOGFILE=/var/log/nkn/startdmi.log
VJX_INTERNAL_IP="10.84.77.10"
SELF_PID="$$";

function vlog()
{
    TIME=`date`;
    echo "${TIME}:[$$]: $*" >> $LOGFILE;
}
#
# function CREATE_HOST_NW()
#
CREATE_HOST_NW() {

    HOST_ONLY_FILE=/config/nkn/dmi-host-only.xml

    HOST_ONLY_NET=`virsh net-dumpxml host-only | grep "host-only" | wc -l`;
    OUT=`virsh net-dumpxml host-only`
    vlog $OUT;

    OUT=`virsh net-info host-only 2>> $LOGIFILE`
    vlog "virsh net-info host-only";
    vlog $OUT;
    if [ "$HOST_ONLY_NET" != "0" ]; then
         vlog "Host only network found"
    else
        if [ -f $HOST_ONLY_FILE ]; then
            vlog "File present already"
        else 
        echo "<network>
            <name>host-only</name>
            <bridge name='virbr1' stp='on' forwardDelay='0' />
            <ip address='10.84.77.1' netmask='255.255.255.0'>
              <dhcp>
                <range start='10.84.77.100' end='10.84.77.200' />
              </dhcp>
            </ip>
          </network>" >> $HOST_ONLY_FILE;
        fi

	virsh net-destroy host-only 2>> $LOGFILE
        if [ "$?" -ne "0" ]; then
	    vlog "net-destroy failed"
	fi

	virsh net-undefine host-only 2>> $LOGFILE
        if [ "$?" -ne "0" ]; then
	    vlog "net-undefine failed"
	fi

        virsh net-define $HOST_ONLY_FILE 2>> $LOGFILE;
        if [ "$?" -ne "0" ]; then
            x=0;
            while [ $x -le 10 ]
            do
            vlog"Failed to create host-only"
            `logger "Failed to create host-only network"`
	    virsh net-destroy host-only 2>> $LOGFILE
	    virsh net-undefine host-only 2>> $LOGFILE
            virsh net-define $HOST_ONLY_FILE 2>> $LOGFILE;
            if [ $? -ne 0 ]; then
               vlog "Failed again $x $?"
               sleep 10;
               let x=$x+1;
               vlog "Print again $x"
            else
               vlog "Success $x $?"
               x=21;
            fi
	    vlog "LOOP: x=${x}"
            done
	    if [ $x -eq 21 ]; then
		vlog "Defined host-only network"
	    else
		vlog "Failed to define host-only network"
		return 1;
	    fi
        else
            vlog "Created host-only"
            `logger "Created host-only network"`
        fi
    fi
    vlog "=== autostart ==="
    OUT=`virsh net-autostart host-only`
    if [ "$?" -ne "0" ]; then
	vlog "Failed to net-autostart"
    fi
    vlog $OUT

    vlog "=== start ==="
    OUT=`virsh net-start host-only`
    if [ "$?" -ne "0" ]; then
	vlog "Failed to net-start"
    fi
    vlog $OUT
    HOST_ONLY_NET_IP=`virsh net-dumpxml host-only | grep "ip address" | cut -d "'" -f 2`;
    vlog "IP Address $HOST_ONLY_NET_IP"
    return 0;
}

#
# function CREATE_HOST_BRIDGE()
#
CREATE_HOST_BRIDGE() {

    HOST_ONLY_NET_BRIDGE=`virsh net-dumpxml host-only | grep "bridge name" | cut -d "'" -f 2`;

    echo "Bridge name $HOST_ONLY_NET_BRIDGE" >> $LOGFILE

    if [ -f $QEMU_IFUP ]; then
        echo "$QEMU_IFUP already exists" >> $LOGFILE
    else
        echo "#!/bin/sh">>$QEMU_IFUP
        echo "echo \"Executing qemu-ifup\"" >>$QEMU_IFUP;
        echo "/sbin/ifconfig \$1 0.0.0.0 promisc up" >>$QEMU_IFUP
        echo "/usr/sbin/brctl addif $HOST_ONLY_NET_BRIDGE \$1">>$QEMU_IFUP
        echo "sleep 2">>$QEMU_IFUP
    fi

    chmod +x $QEMU_IFUP;
    return 0;
}

#
# function CREATE_VM_VTD()
#
CREATE_VM_VTD() {

CREATE_HOST_NW;
if [ "$?" -ne "0" ]; then
    echo "host network creation failed" >> $LOGFILE;
    exit 1;
fi
CREATE_HOST_BRIDGE;
if [ "$?" -ne "0" ]; then
    echo "host bridge creation failed" >> $LOGFILE;
    exit 1;
fi
OUT=`ls -l /nkn/virt/pools/default/`;
echo $OUT >> $LOGFILE;

/usr/libexec/qemu-kvm -M rhel6.3.0 -m 1024 -smp 1 -no-kvm -name dmi-adapter -monitor pty -pidfile /var/run/libvirt/qemu/dmiadapter.pid -boot cd -drive file=/nkn/virt/pools/default/mfc-dmi-adapter.img,if=none,id=drive-ide0-0-0,format=raw -device ide-drive,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1  -net nic,model=e1000 -net tap,script=$QEMU_IFUP,vlan=0,ifname=vif1 -parallel none  -vnc 127.0.0.1:0 -k en-us -serial telnet:127.0.0.1:8888,server,nowait & >> $LOGFILE

echo "VJX started ..." >> $LOGFILE;

}

#
# function STOP_QEMU_VM()
#
# purpose : stop VJX by killing the pid that we have saved for the qemu process
#
STOP_QEMU_VM() {

    # Before we bring down VJX, need to save the current configuration
    # Let us first check we are able to reach VJX, if not we don't bother
    ping -c 1 ${VJX_INTERNAL_IP} > /dev/null;
    if [ $? == 0 ]; then
        # First send command to create a config backup
        ssh -oStrictHostKeyChecking=no -oBatchMode=yes -i /config/nkn/id_dsa admin@${VJX_INTERNAL_IP} "request system configuration rescue save" > /dev/null;

	# Now retreive the backed up config using scp
	scp -oStrictHostKeyChecking=no -oBatchMode=yes -i /config/nkn/id_dsa admin@${VJX_INTERNAL_IP}:/config/rescue.conf.gz /config/nkn/ > /dev/null;

	if [ $? != 0 ]; then
	    logger  "error: failed to backup VJX configuration"
	fi
    fi

    PID=`cat /var/run/libvirt/qemu/dmiadapter.pid`;
    echo "Killing QEMU VM with PID $PID" >> $LOGFILE;
    `kill -9 $PID`;

}

#
# function CHECK_IF_VJX_IS_UP()
#
# purpose : update the hostname on VJX to match the one MFC
#
CHECK_IF_VJX_IS_UP() {

    local count=0;
    local hostname=`hostname`;

    echo
    echo "waiting for VJX to come up ..." >> $LOGFILE;
    # Sleep for 2 min before trying to ping VJX
    sleep 120;

    # ping VJX to see if it is up
    ping -c 1 ${VJX_INTERNAL_IP} > /dev/null;
    while [ $? != 0 ]; do
        # echo "still waiting ..." >> $LOGFILE
    	# VJX not yet up, sleep for a minute and try again
	sleep 60
	count=$((count + 1));
	if [ count == 10 ]; then
	    # We have waited for 10 min VJX is not coming up
	    # Hence exiting 
	    echo "VJX has not come up yet, hence bailing out ..." >> $LOGFILE;
	    return 1;
	fi
        ping -c 1 ${VJX_INTERNAL_IP} > /dev/null;
    done

    # ping worked, hence VJX should be up
    # waiting for 10 sec to make sure ssh server is up
    sleep 10;

    return 0;

} # end of CHECK_IF_VJX_IS_UP

#
# function SETUP_VJX_HOSTNAME()
#
# purpose : update the hostname on VJX to match the one MFC
#
SETUP_VJX_HOSTNAME() {

    local hostname=`hostname`;

    echo "Setting up VJX hostname to ${hostname}" >> $LOGFILE;

    # use ssh to send JUNOS CLI commands to set the hostname
    RET_VAL=1;
    while [ $RET_VAL -ne 0 ]; do
        ssh -oStrictHostKeyChecking=no -oBatchMode=yes -i /config/nkn/id_dsa admin@${VJX_INTERNAL_IP} "edit; set system host-name $hostname;commit" > /dev/null;
        RET_VAL=$?
        sleep 10;
    done

    return 0;

} # end of SETUP_VJX_HOSTNAME

#
# function RESTORE_CONFIG()
#
# purpose : make sure VJX is up and reachable and then restore the 
#	backed up configuration
#
RESTORE_CONFIG() {

    echo "Restoring backed up config to VJX" >> $LOGFILE;

    # Check if backed up config exists
    if [ ! -f /config/nkn/rescue.conf.gz ]; then
        echo "Warning: no backed up VJX config file to restore" >> $LOGFILE;
        return
    fi

    # scp the backed up configuration to VJX
    # loop till it succeeds as first time it could take a few mins till the dsa key is installed
    RET_VAL=1;
    while [ $RET_VAL -ne 0 ]; do
        scp -oStrictHostKeyChecking=no -oBatchMode=yes -i /config/nkn/id_dsa /config/nkn/rescue.conf.gz admin@${VJX_INTERNAL_IP}:/var/home/admin > /dev/null;
        RET_VAL=$?
        sleep 10;
    done

    # now, ask VJX to use the uploaded config
    RET_VAL=1;
    while [ $RET_VAL -ne 0 ]; do
        ssh -oStrictHostKeyChecking=no  -oBatchMode=yes -i /config/nkn/id_dsa admin@${VJX_INTERNAL_IP} "edit;load update /var/home/admin/rescue.conf.gz;commit" > /dev/null;
        RET_VAL=$?
        sleep 10;
    done

} # end of RESTORE_CONFIG

#
# MAIN LOGIC BEGINS HERE
#
vlog "----------------pid= $$ ------------"
if [ $1 = "start" ]; then
    echo "Starting..." >> $LOGFILE
    CREATE_VM_VTD;

    # Check to see if VJX is up and running
    CHECK_IF_VJX_IS_UP;
    if [ $? == 0 ]; then
        # Restore config is VJX is reachable
        RESTORE_CONFIG;

	# Setup the hostname of the VJX
    	SETUP_VJX_HOSTNAME;
    fi

elif [ $1 = "stop" ]; then
    echo "Stopping..." >> $LOGFILE
    STOP_QEMU_VM;
else
    echo "Unknown option" >> $LOGFILE
fi


#
# End of startdmi.sh
#
