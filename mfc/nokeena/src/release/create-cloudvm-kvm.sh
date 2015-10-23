#!/bin/sh
#  File : create-cloudvm-kvm.sh
#  Create a CloudVM type KVM from a standard MFC installation CD iso.
#
DEFAULT_DISK_SIZE=27G
#DEFAULT_DISK_SIZE=100G
DEFAULT_RAM_KIB=8388608
DEFAULT_AUTO_UUID_BASE=00000001-0001-0001-0001-00000000000
DEFAULT_AUTO_MACADDRESS_BASE=52:54:00:00:00:

ME="${0}"
if [ ${UID} -ne 0 ] ; then
  echo "Error, must run ${ME} as root"
  exit 1
fi

export LC_ALL=C

function print_usage()
{
    echo "usage: ${0} [--a1|--a2|...|--ae|--af] | [ input-ISO-file-or-dir [ output-dir [ created-vdisk-size [ virt-instance-name [ RAM KiB [ uuid [ macaddr ]]]]]]"
    echo "For defaults, use '.' or omit when trailing"
    echo "For random uuid or macaddres, use 'random'"
    echo "When macaddress is not specified, a default one will be picked that is not in use."
    echo "Default input: most recent mfgcd-mfc-*.iso file in current directory"
    echo "Default iso file when directory is specified: most recent mfgcd-mfc-*.iso file"
    echo "Default output dir: current directory"
    echo "Default Created disk size=${DEFAULT_DISK_SIZE}"
    echo "Default virt-instance-name based on iso file name"
    echo "Default RAM_KIB=${DEFAULT_RAM_KIB}"
    echo "Default UUID=${DEFAULT_AUTO_UUID_BASE}N"
    echo "Default MACADDRESS=${DEFAULT_AUTO_MACADDRESS_BASE}"'*0'
    echo "When a macaddr is NOT specified, the --aN option can be used."
    echo "  Range of --aN : N is 1 thru f"
    echo "  When '--aN' is specified, a special macaddr is used to cause automatic install,"
    echo "  passing the digit (1 thru f) as the AUTO_MODE parameter to the pre-insall script."
    echo "  Currently this is used to cause alternate partition model to be used."
    echo "  Note: '-a' is the same as '--a1'."
}

# When booting the install environment, the script S30_mfc_install is run,
# and it checks for special UUID and MACADDRESS values.
# When the UUID matches "00000001-0001-0001-0001-*" and the MACADDRESS
# matches 52:54:00:00:00:0[1-f] or 00:16:3e:00:00:0[1-f] then it runs
# /sbin/automatic-install.sh with the parameter being the last digit of
# the MACADDRSS (which is 1 thru f).
# The automatic-install.sh script runs "/sbin/pre-install.sh --automatic ${*}"
# So to have fully automatic install use the appropriate UUID and MACADDRESS.
# See src/base_os/common/rootflop_files/S30_mfc_install
#
# Also note that when the UUID is "00000001-0001-0001-0001-000000000001"
# through "00000001-0001-0001-0001-000000000009" then at MFC firstboot the
# SSL license is installed for that host id.

AUTO_MODE=0
case "_${1}" in
_-h*|_--h*) print_usage; exit 0 ;;
_-a)   AUTO_MODE=1 ; shift ;;
_--a1) AUTO_MODE=1 ; shift ;;
_--a2) AUTO_MODE=2 ; shift ;;
_--a3) AUTO_MODE=3 ; shift ;;
_--a4) AUTO_MODE=4 ; shift ;;
_--a5) AUTO_MODE=5 ; shift ;;
_--a6) AUTO_MODE=6 ; shift ;;
_--a7) AUTO_MODE=7 ; shift ;;
_--a8) AUTO_MODE=8 ; shift ;;
_--a9) AUTO_MODE=9 ; shift ;;
_--aa) AUTO_MODE=a ; shift ;;
_--ab) AUTO_MODE=b ; shift ;;
_--ac) AUTO_MODE=c ; shift ;;
_--ad) AUTO_MODE=d ; shift ;;
_--ae) AUTO_MODE=e ; shift ;;
_--af) AUTO_MODE=f ; shift ;;
_--a*) print_usage; exit 0 ;;
esac

INPUT_ISO="${1}"
OUTPUT_DIR="${2}"
USE_DISK_SIZE="${3}"
export VM_INSTANCE="${4}"
USE_RAM_KIB="${5}"
USE_UUID="${6}"
USE_MACADDRESS="${7}"
LIBVIRT_IMAGES_DIR=/var/lib/libvirt/images

# The OUTPUT_DIR must be full path.
[ -z "${OUTPUT_DIR}" ] && OUTPUT_DIR=.
case "${OUTPUT_DIR}" in
/*) ;;
.)  OUTPUT_DIR=`pwd` ;;
*)  OUTPUT_DIR=`pwd`/${OUTPUT_DIR} ;;
esac

rm -f vm_current.dot
rm -f failure-kvm-install.txt

INPUT_DIR=
if [ -z "${INPUT_ISO}" ] ; then
  INPUT_DIR=.
elif [ -d "${INPUT_ISO}" ] ; then
  INPUT_DIR="${INPUT_ISO}"
fi
if [ ! -z "${INPUT_DIR}" ] ; then
  INPUT_ISO=`ls -t ${INPUT_DIR}/mfgcd-mfc-*.iso 2> /dev/null | head -1 `
  if [ -z "${INPUT_ISO}" ] ; then
    echo Error, no iso file specified and there is none in dir: ${INPUT_DIR}
    echo Parameters error > failure-kvm-install.txt
    exit 1
  fi
  echo Using iso file in the current directory: ${INPUT_ISO}
fi
if [ ! -f "${INPUT_ISO}" ] ; then
  echo Error, input file does not exist: ${INPUT_ISO}
  echo Parameters error > failure-kvm-install.txt
  exit 1
fi

[ -z "${USE_DISK_SIZE}" ] && USE_DISK_SIZE=.
[ "${USE_DISK_SIZE}" = "." ] && USE_DISK_SIZE=${DEFAULT_DISK_SIZE}

[ -z "${VM_INSTANCE}" ] && VM_INSTANCE=.
if [ "${VM_INSTANCE}" = "." ] ; then
  VM_INSTANCE=`basename ${INPUT_ISO} .iso | sed "/mfgcd-/s///" | sed "/+/s//_/g"`
fi

Pick_Unused_MAC()
{
  BASE=${1}
  END=${2}
  # Make a list of all the MAC addresses in use by VMs right now
  MACS=","
  for U in `virsh list --uuid`
  do
    MACS="${MACS}"`virsh domiflist ${U} | grep : | awk '{print $5}'`","
  done
  echo MACs in use: ${MACS}
  for N in 1 2 3 4 5 6 7 8 9 a b c d e f 0
  do
    echo ${MACS} | grep -q ,${BASE}${N}${END},
    [ ${?} -ne 0 ] && return ${N}
  done
  return FAIL
}

[ -z "${USE_MACADDRESS}" ] && USE_MACADDRESS=.
if [ "${USE_MACADDRESS}" = "." ] ; then
  Pick_Unused_MAC ${DEFAULT_AUTO_MACADDRESS_BASE} ${AUTO_MODE}
  DIGIT=${?}
  if [ "${DIGIT}" = "FAIL" ] ; then
    echo No available MAC address in the set ${DEFAULT_AUTO_MACADDRESS_BASE}N${AUTO_MODE} where N ranges from 0 thru f
    exit 1
  fi
  USE_MACADDRESS=${DEFAULT_AUTO_MACADDRESS_BASE}${DIGIT}${AUTO_MODE}
fi
echo USE_MACADDRESS=${USE_MACADDRESS}

[ -z "${USE_RAM_KIB}" ] && USE_RAM_KIB=.
[ "${USE_RAM_KIB}" = "." ] && USE_RAM_KIB=${DEFAULT_RAM_KIB}

USE_QCOW2_FILE=${LIBVIRT_IMAGES_DIR}/${VM_INSTANCE}.qcow2
XML_FILE=${OUTPUT_DIR}/${VM_INSTANCE}.xml
export LOG_FILE=${OUTPUT_DIR}/${VM_INSTANCE}.log

echo Installation started | tee ${LOG_FILE}
date >> ${LOG_FILE}
date +%T | tee -a ${LOG_FILE}

STATE=`virsh domstate ${VM_INSTANCE} 2> /dev/null`
echo STATE=${STATE}
if [ ! -z "${STATE}" ] ; then
  if [ "${STATE}" != "shut off" ] ; then
    echo virsh destroy ${VM_INSTANCE} >> ${LOG_FILE}
    virsh destroy ${VM_INSTANCE}
  fi
  echo virsh undefine ${VM_INSTANCE} >> ${LOG_FILE}
  virsh undefine ${VM_INSTANCE}
fi
rm -f "${USE_QCOW2_FILE}"
rm -f "${XML_FILE}"
if [ -f "${USE_QCOW2_FILE}" ] ; then
  echo "Error, failed to remove old ${USE_QCOW2_FILE}" | tee -a ${LOG_FILE}
  echo qcow2 file removal failed > failure-kvm-install.txt
  exit 1
fi
if [ -f "${XML_FILE}" ] ; then
  echo "Error, failed to remove old ${XML_FILE}" | tee -a ${LOG_FILE}
  echo xml file removal failed > failure-kvm-install.txt
  exit 1
fi

[ -z "${USE_UUID}" ] && USE_UUID=.
if [ "${USE_UUID}" = "." ] ; then
  for i in 1 2 3 4 5 6 7 8 9
  do
    USE_UUID=${DEFAULT_AUTO_UUID_BASE}${i}
    virsh list --uuid | grep ${USE_UUID}
    RV=${?}
    [ ${RV} -eq 1 ] && break
  done
  if [ ${RV} -eq 0 ] ; then
    echo "Error, Automatic UUIDs all in use" | tee -a ${LOG_FILE}
    echo Automatic UUIDs all in use > failure-kvm-install.txt
    exit 1
  fi
else
  virsh list --uuid | grep ${USE_UUID}
  if [ ${?} -eq 0 ] ; then
    echo "Error, Specified UUID currently in use: ${USE_UUID}" | tee -a ${LOG_FILE}
    echo Specified UUID in use: ${USE_UUID} > failure-kvm-install.txt
    exit 1
  fi
fi

cat << EOF1 \
> ${XML_FILE}
<domain type='kvm'>
  <name>${VM_INSTANCE}</name>
  <memory unit='KiB'>${USE_RAM_KIB}</memory>
  <currentMemory unit='KiB'>${USE_RAM_KIB}</currentMemory>
  <vcpu placement='static'>1</vcpu>
EOF1

if [ ! -z "${USE_UUID}" ] ; then
  echo "  <uuid>${USE_UUID}</uuid>"  >> ${XML_FILE}
fi

cat << EOF2 \
 | sed "/QCOW2_FILE_HERE/s##${USE_QCOW2_FILE}#" \
>> ${XML_FILE}
  <os>
    <type arch='x86_64' machine='pc'>hvm</type>
    <boot dev='hd'/>
  </os>
  <features>
    <acpi/>
    <apic/>
    <pae/>
  </features>
  <clock offset='utc'/>
  <on_poweroff>destroy</on_poweroff>
  <on_reboot>restart</on_reboot>
  <on_crash>restart</on_crash>
  <devices>
    <emulator>/usr/libexec/qemu-kvm</emulator>
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2' cache='none'/>
      <source file='QCOW2_FILE_HERE'/>
      <target dev='vda' bus='virtio'/>
      <alias name='virtio-disk0'/>
      <address type='pci' domain='0x0000' bus='0x00' slot='0x05' function='0x0'/>
    </disk>
    <disk type='block' device='cdrom'>
      <driver name='qemu' type='raw'/>
      <target dev='hdc' bus='ide'/>
      <readonly/>
      <address type='drive' controller='0' bus='1' target='0' unit='0'/>
    </disk>
    <controller type='usb' index='0'>
      <address type='pci' domain='0x0000' bus='0x00' slot='0x01' function='0x2'/>
    </controller>
    <controller type='ide' index='0'>
      <address type='pci' domain='0x0000' bus='0x00' slot='0x01' function='0x1'/>
    </controller>
    <interface type='network'>
EOF2

if [ ! -z "${USE_MACADDRESS}" ] ; then
  echo "<mac address='${USE_MACADDRESS}'/>" >> ${XML_FILE}
fi

cat << EOF3 \
>> ${XML_FILE}
      <source network='default'/>
      <model type='virtio'/>
      <address type='pci' domain='0x0000' bus='0x00' slot='0x03' function='0x0'/>
    </interface>
    <serial type='pty'>
      <target port='0'/>
    </serial>
    <console type='pty'>
      <target type='serial' port='0'/>
    </console>
    <memballoon model='virtio'>
      <address type='pci' domain='0x0000' bus='0x00' slot='0x04' function='0x0'/>
    </memballoon>
  </devices>
  <seclabel type='none'/>
</domain>
EOF3

if [ ! -f "${XML_FILE}" ] ; then
  echo "Error, failed to create ${XML_FILE}" | tee -a ${LOG_FILE}
  echo xml file create failed > failure-kvm-install.txt
  exit 1
fi
echo "Created ${XML_FILE}" | tee -a ${LOG_FILE}

echo VM_INSTANCE=${VM_INSTANCE} >  vm_current.dot

# First create the qcow2 disk image
qemu-img create -f qcow2 ${USE_QCOW2_FILE} ${USE_DISK_SIZE} | tee -a ${LOG_FILE}
RV=$?
if [ ! -f "${USE_QCOW2_FILE}" ] ; then
  echo "Error, failed to create ${USE_QCOW2_FILE}" | tee -a ${LOG_FILE}
  echo "qemu-img create returned ${RV}" | tee -a ${LOG_FILE}
  echo qcow2 create failed > failure-kvm-install.txt
  exit 1
fi
echo "Created disk image file ${USE_QCOW2_FILE} size ${USE_DISK_SIZE}" | tee -a ${LOG_FILE}

# Note from virt-install man page about specifying the UUID:
# -u UUID , --uuid=UUID
# UUID for the guest; if none is given a random UUID will be generated.
# If you specify UUID , you should use a 32-digit hexadecimal number.
# UUID are intended to be unique across the entire data center, and
# indeed the world. Bear this in mind if manually specifying a UUID.
if [ -z "${USE_UUID}" ] ; then
  UUID_OPTION=""
elif [ "${USE_UUID}" = "random" ] ; then
  UUID_OPTION=""
else
  UUID_OPTION="--uuid=${USE_UUID}"
fi

# Note from virt-install man page about specifying the network mac address
# with the "--network NETWORK,mac=" option:
# If the mac= sub parameter is omitted, or the value "RANDOM" is specified
# then a suitable address will be randomly generated.
# For Xen virtual machines it is required that the first 3 pairs in the
# MAC address be the sequence '00:16:3e', while for QEMU or KVM virtual
# machines it must be '52:54:00'.
if [ -z "${USE_MACADDRESS}" ] ; then
  MAC_OPTION=""
elif [ "${USE_MACADDRESS}" = "random" ] ; then
  MAC_OPTION=""
else
  MAC_OPTION=",mac=${USE_MACADDRESS}"
fi

GRAPHICS_OPT=--nographics
#GRAPHICS_OPT="--graphics vnc,listen=0.0.0.0"
# Run virt-install to create the VM
echo "Starting VM booting from ${INPUT_ISO} to install on ${USE_QCOW2_FILE}" | tee -a ${LOG_FILE}
export LC_ALL=C
# virt-type needs to be kvm on CentOS 6.3, 6.4,
# but seems to need to be qemu on CentOS 6.6.
export VIRT_TYPE=kvm
export VIRT_TYPE=qemu
D='$'
cat << EOF4 > expect.script
#!/usr/bin/expect -d -f
puts ""
puts "Expect: START ================================================="
puts "Expect: running virt-install =================================="
spawn \
virt-install --virt-type ${VIRT_TYPE} --name ${VM_INSTANCE} --ram 2048 \
  --cdrom=${INPUT_ISO} \
  --vcpus=1 \
  --disk=${USE_QCOW2_FILE},bus=virtio,format=qcow2 \
  --network network=default${MAC_OPTION} \
  ${GRAPHICS_OPT} \
  --noreboot \
  --os-type=linux \
  --os-variant=rhel6 ${UUID_OPTION}
puts "Expect: waiting for login prompt =============================="
set timeout 240
expect {
  " login: " { send "root\r" ; puts "-root-" }
  eof {
    catch wait result
    puts "Expect: Failure-- Did not get login prompt" 
    exit [lindex ${D}result 3]
  }
  timeout { puts "Expect: Failure-- Did not get login prompt" ; exit 1 }
}
puts "Expect: Wait minute 1=========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 2=========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 3========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 4========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 5========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 6========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 7========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 8========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 9========================================="
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 10========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 11========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 12========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 13========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 14========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 15========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Expect: Wait minute 16========================================"
expect {
  eof { catch wait result ; exit [lindex ${D}result 3] }
  "Installation failed" { puts "Expect: Installation failed" ; exit 1 }
}
puts "Timed out waiting for virt-install to finish"
exit 1
EOF4
expect expect.script
RV=${?}

echo =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- | tee -a ${LOG_FILE}
echo virt-install retrned ${RV}         | tee -a ${LOG_FILE}
echo =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- | tee -a ${LOG_FILE}
sleep 4
if [ ${RV} -ne 0 ] ; then
  echo Error, virt-install did not return 0 | tee -a ${LOG_FILE}
  echo virt-install returned ${RV}
  virsh list --all
  echo Shutting down vm ${VM_INSTANCE} | tee -a ${LOG_FILE}
  echo virsh destroy ${VM_INSTANCE} | tee -a ${LOG_FILE}
  virsh destroy ${VM_INSTANCE} > /dev/null
  RV=${?}
  if [ ${RV} -ne 0 ] ; then
    echo Failed: virsh destroy ${VM_INSTANCE} | tee -a ${LOG_FILE}
  fi
  if [ "_${BUILD_SAVE_VM}" = "_savevm" ] ; then
    echo Keeping VM ${VM_INSTANCE} | tee -a ${LOG_FILE}
  else
    virsh undefine ${VM_INSTANCE} > /dev/null
    RV=${?}
    if [ ${RV} -ne 0 ] ; then
      echo Failed: virsh undefine ${VM_INSTANCE} | tee -a ${LOG_FILE}
    fi
  fi
  virsh list --all
  echo MFC Install failed > failure-kvm-install.txt
  exit 1
fi

echo VM_INSTANCE=${VM_INSTANCE} | tee -a ${LOG_FILE}

# First remove previous instance.
# The state should already be "shut off" at this point.
STATE=`virsh domstate ${VM_INSTANCE}`
echo STATE=${STATE} >> ${LOG_FILE}
if [ "${STATE}" != "shut off" ] ; then
  echo Unexpected state: ${STATE} | tee -a ${LOG_FILE}
  echo virsh destroy ${VM_INSTANCE} | tee -a ${LOG_FILE}
  virsh destroy ${VM_INSTANCE} > /dev/null
  RV=${?}
  if [ ${RV} -ne 0 ] ; then
    echo Failed: virsh destroy ${VM_INSTANCE} | tee -a ${LOG_FILE}
    echo Unexpected state ${STATE} and virsh destroy failed > failure-kvm-install.txt
    virsh undefine ${VM_INSTANCE} > /dev/null
    exit 1
  fi
fi
virsh undefine ${VM_INSTANCE} > /dev/null
RV=${?}
if [ ${RV} -ne 0 ] ; then
  echo Failed: virsh undefine ${VM_INSTANCE} | tee -a ${LOG_FILE}
  echo virsh undefine failed > failure-kvm-install.txt
  exit 1
fi

virsh define ${XML_FILE}
RV=${?}
if [ ${RV} -ne 0 ] ; then
  echo Failed: virsh define ${XML_FILE} | tee -a ${LOG_FILE}
  echo virsh define failed > failure-kvm-install.txt
  exit 1
fi
echo +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+= | tee -a ${LOG_FILE}
echo Starting VM again to finish installation | tee -a ${LOG_FILE}
echo +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+= | tee -a ${LOG_FILE}
sleep 2
virsh start ${VM_INSTANCE} | tee -a ${LOG_FILE}
RV=${?}
if [ ${RV} -ne 0 ] ; then
  echo Failed: virsh start ${VM_INSTANCE} | tee -a ${LOG_FILE}
  echo virsh start failed > failure-kvm-install.txt
  exit 1
fi
DOM_ID=`virsh domid ${VM_INSTANCE}`
echo DOM_ID=${DOM_ID} | tee -a ${LOG_FILE}

# Get the virtual serial console device.
CONS_DEV=`virsh ttyconsole ${VM_INSTANCE}`
echo CONS_DEV=${CONS_DEV} | tee -a ${LOG_FILE}

export STOP_FILE=/tmp/stop.file
rm -f ${STOP_FILE}
trap_int_handler_1()
{
  echo
  echo "(Interrupt)"
  echo
  touch ${STOP_FILE}
}
trap "trap_int_handler_1" INT

# Note: When the VM is powered down, this cat command will exit.
# Run it in the background so we can check to see if
# is running much too long, so we can kill it.
cat ${CONS_DEV} >> ${LOG_FILE} 2> /dev/null &
PID1=${!}

echo Wait for VM to stop running | tee -a ${LOG_FILE}
echo
# Note: The --pid option causes it to exit when the specified PID exits.
tail -f --pid ${PID1} ${LOG_FILE} &
PID2=${!}

export INCREMENT=20
export LIMIT=600
export TOTAL=0
while true
do
  sleep ${INCREMENT}
  # Loop until the state is not "running", or we have gone too long.
  STATE=`virsh domstate ${VM_INSTANCE}`
  [ "${STATE}" != "running" ] && break
  [ -f ${STOP_FILE} ] && break

  #date +%T >> ${LOG_FILE}
  tail -100 ${LOG_FILE} | grep mfc-unconfigured | grep login: > /dev/null
  RV=${?}
  if [ ${RV} -eq 0 ] ; then
    echo Warning, did not expect to get to login prompt >> ${LOG_FILE}
    break
  fi

  TOTAL=`expr ${TOTAL} + ${INCREMENT}`
  if [ ${TOTAL} -gt ${LIMIT} -o -f stop_file ] ; then
    echo Killing VM because it did not finish the install in time >> ${LOG_FILE}
    echo virsh destroy ${VM_INSTANCE} >> ${LOG_FILE}
    virsh destroy ${VM_INSTANCE}
    break
  fi
done
# Under normal circumstances PID1 should not be running at this point.
# But if it got hung or the script was interrupted then we need to kill it.
kill ${PID1} > /dev/null 2>&1
kill ${PID2} > /dev/null 2>&1
sleep 2

STATE=`virsh domstate ${VM_INSTANCE}`
echo STATE=${STATE} >> ${LOG_FILE}
rm -f ${STOP_FILE}
if [ "${STATE}" != "shut off" ] ; then
  echo Unexected state: ${STATE} | tee -a ${LOG_FILE}
  virsh list --all | head -2 | tee -a ${LOG_FILE}
  virsh list --all | grep ${VM_INSTANCE} | tee -a ${LOG_FILE}
  echo virsh destroy ${VM_INSTANCE} | tee -a ${LOG_FILE}
  virsh      destroy ${VM_INSTANCE} | tee -a ${LOG_FILE}
  date +%T | tee -a ${LOG_FILE}
fi
echo virsh undefine ${VM_INSTANCE} | tee -a ${LOG_FILE}
virsh      undefine ${VM_INSTANCE} | tee -a ${LOG_FILE}
ls -lh ${USE_QCOW2_FILE} | tee -a ${LOG_FILE}
echo qemu-img convert -O qcow2 ${USE_QCOW2_FILE} ${OUTPUT_DIR}/${VM_INSTANCE}.qcow2 | tee -a ${LOG_FILE}
qemu-img convert -O qcow2 ${USE_QCOW2_FILE} ${OUTPUT_DIR}/${VM_INSTANCE}.qcow2
echo rm -f ${USE_QCOW2_FILE} | tee -a ${LOG_FILE}
rm -f ${USE_QCOW2_FILE}
ls -lh ${OUTPUT_DIR}/${VM_INSTANCE}.qcow2 | tee -a ${LOG_FILE}

echo Installation completed | tee -a ${LOG_FILE}
date +%T | tee -a ${LOG_FILE}

#virsh list --all | head -2 | tee -a ${LOG_FILE}
#virsh list --all | grep ${VM_INSTANCE} | tee -a ${LOG_FILE}

# Useful virsh commands:
#   virst list --all              - List all defined VMs
#   virsh shutdown <instance>     - Tell OS in the VM to shutdown
#   virsh destroy  <instance>     - To force shut off a VM
#   virst undefine <instance>     - Remove a VM completely
