#! /bin/bash

#	File : disk_diagnosis.sh
#
#	Description: This script helps identifying dead/dying drives by
#		outputting smart statistics
#

TESTING=0
PATH=$PATH:/sbin

source /opt/nkn/bin/disks_common.sh
if [ $TESTING -eq 0 ]; then
    source /opt/nkn/bin/disks_common.sh
elif [ -e /tmp/disks_common.sh ]; then
    source /tmp/disks_common.sh
else
    source /opt/nkn/bin/disks_common.sh
fi

if [ -x /opt/nkn/bin/smartctl ]; then
    SMARTCTL=/opt/nkn/bin/smartctl
else
    SMARTCTL=/usr/bin/smartctl
fi

function execute_smartctl()
{
    local T
    local saved_output="/nkn/tmp/smartctl_output.txt"

    if is_production_vxa; then
	if is_vxa_2100; then
	    T=vxa_2100
	fi
	T=production_vxa
    else
        T=some_other
    fi

    case ${T} in

    vxa_2100)
	# I have assumed that vxa_2100 and production_vxa have lsi and
        # adaptec controllers. These controllers sometimes show SATA
        # drives as SAS drives. Hence I first execute smartctl thinking
        # the drive is SATA and if it fails, then I execute it against SAS
        for i in {a..z}; do
	    CMD="${SMARTCTL} -d sat -a -i /dev/sd${i}\n"
	    echo ${CMD} >> ${saved_output}
	    ${CMD} >> ${saved_output}
	    if [ $? -ne 0 ]; then
		CMD="${SMARTCTL} -a -i /dev/sd${i}\n"
		echo ${CMD} >> ${saved_output}
		${CMD} >> ${saved_output}
	    else
		continue;
	    fi
	    echo -e "===========================================================\n" >> ${saved_output}
	done
        ;;

    production_vxa)
	# I have assumed that vxa_2100 and production_vxa have lsi and
        # adaptec controllers. These controllers sometimes show SATA
        # drives as SAS drives. Hence I first execute smartctl thinking
        # the drive is SATA and if it fails, then I execute it against SAS
	for i in {a..z}; do
	    CMD="${SMARTCTL} -d sat -a -i /dev/sd${i}\n"
	    echo ${CMD} >> ${saved_output}
	    ${CMD} >> ${saved_output}
	    if [ $? -ne 0 ]; then
		CMD="${SMARTCTL} -a -i /dev/sd${i}\n"
		echo ${CMD} >> ${saved_output}
		${CMD} >> ${saved_output}
	    else
		continue;
	    fi
	    echo -e "===========================================================\n" >> ${saved_output}
	done
        ;;

    some_other)

    if is_smart_array; then
	# Machines having smart_array controllers have a different smartctl
	# syntax
        for i in {0..15}; do
	    CMD="${SMARTCTL} -a -d cciss,${i} /dev/sdg2\n"
	    echo ${CMD} >> ${saved_output}
	    ${CMD} >> ${saved_output}
	    if [ $? -ne 0 ]; then
		continue;
	    fi
	    echo -e "===========================================================\n" >> ${saved_output}
        done
    else
	CMD="${SMARTCTL} -a -i /dev/sd${i}\n"
	echo ${CMD} >> ${saved_output}
	${CMD} >> ${saved_output}
    fi
    ;;

    esac
}

execute_smartctl

