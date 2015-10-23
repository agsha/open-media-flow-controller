#! /bin/bash
# Get the ethernet device to name mapping based on Model number and
# the pci slots that are used.

# For testing on a non MFC machine, you must have SUDO=/usr/bin/sudo
# set in your environment or run as root.
# Also have the MODEL_NUMBER set in /tmp/model-info.txt

[ "_${SUDO}" != "_/usr/bin/sudo" ] && SUDO=

ETH_X=`/sbin/ifconfig -a | grep ^eth | head -1 | cut -f1 -d' '`
if [ -z "${ETH_X}" ] ; then
  echo Error, no ethernet devices detected.
  echo Cannot complete setup.
  exit 1
fi
${SUDO} /sbin/ethtool -i ${ETH_X} > /dev/null 2>&1
RV=${?}
if [ ${RV} -ne 0 ] ; then
  echo Failure: /sbin/ethtool -i ${ETH_X}
  ${SUDO} /sbin/ethtool -i ${ETH_X} 2>&1
  echo
  echo Cannot complete setup.
  exit 1
fi

ETH_ORDER_FILE=/tmp/eth-order-eth.out
PCI_ORDER_FILE=/tmp/eth-order-pci.out
NEW_ORDER_FILE=/tmp/eth-order-new.out
rm -f ${ETH_ORDER_FILE}
rm -f ${PCI_ORDER_FILE}

if [ -s /tmp/model-info.txt ] ; then
  . /tmp/model-info.txt
fi
if [ -z "${MODEL_NUMBER}" ] ; then
  if [ -x /sbin/dmidecode ] ; then
    D_EXE=/sbin/dmidecode
  elif [ -x /usr/sbin/dmidecode ] ; then
    D_EXE=/usr/sbin/dmidecode
  else
    D_EXE=dmidecode
  fi
  ${D_EXE} -s system-manufacturer > /dev/null 2>&1
  if [ ${?} -ne 0 ] ; then
     echo Unable to determine model number
     exit 1
  fi
  SMFG=`${D_EXE} -s system-manufacturer`
  case ${SMFG} in
  Juniper\ *) ;;
  *) 
    echo Setup is only for Juniper Networks hardware.
    exit 1
  ;;
  esac
  SERIAL_NUMBER=`${D_EXE} -s system-serial-number`
  MODEL_NUMBER=`echo "${SERIAL_NUMBER}" | cut -c1-4`
fi

case ${MODEL_NUMBER} in
0279) # VXA1001 : Network interfaces allowed 2x1be (built in)
  MODEL_NAME=VXA1001
  ;;
0280) # VXA1002: Network interfaces allowed: 4x1gbe (built in)
  MODEL_NAME=VXA1002
  ;;
0304) # VXA1100: Network interfaces allowed: 4x1gbe or 2x10Gbe (built in)
  MODEL_NAME=VXA1100
  ;;
0277) # VXA2002: Network interfaces allowed: 4x1gbe, 2x10Gbe,
  MODEL_NAME=VXA2002
  ;;
0278) # VXA2010: Network interfaces allowed: 4x1gbe, 2x10Gbe,
  MODEL_NAME=VXA2010
  ;;
0305) # VXA2100: Network interfaces allowed: 4x1gbe, 2x10Gbe,
  # Note: The VXA2100 is being renamed to VXA2000.
  MODEL_NAME=VXA2100
  ;;
0306) # VXA2200: Network interfaces allowed: 4x1gbe, 2x10Gbe,
  MODEL_NAME=VXA2200
  ;;
*)
  echo Installation is not supported on this model number: ${MODEL_NUMBER}
  echo Installation is only supported on the following:
  echo VXA1001 - model number 0279
  echo VXA1002 - model number 0280
  echo VXA1100 - model number 0304
  echo VXA2002 - model number 0277
  echo VXA2010 - model number 0278
  echo VXA2100 - model number 0305 '(aka VXA2000)'
  echo VXA2200 - model number 0306
  exit 1
esac

# Generate the files
for i in `/sbin/ifconfig -a | grep ^eth | cut -f1 -d' '`
do
  BUS_INFO=`${SUDO} /sbin/ethtool -i ${i} | grep "^bus-info: " | cut -c11-`
  HWADDR=`/sbin/ifconfig ${i} | grep HWaddr | cut -f2- -d: | sed -e "/Ethernet.*HWaddr /s///" | sed -e "/ /s///g"`
  echo ${BUS_INFO},${HWADDR},${i} >> ${ETH_ORDER_FILE}
done
sort ${ETH_ORDER_FILE} > ${PCI_ORDER_FILE}

SCHEME=use_pci_order
case ${MODEL_NUMBER} in
0279|0280)
  # VXA1001: Always two built-in ports.
  # VXA1002: Always four built-in ports.
  # VXA1100: Always two or four built-in ports.
  # The order to use is always the reverse of the pci address order.
  rm -f ${NEW_ORDER_FILE}
  ETH_COUNT=0
  for ITEM in `cat ${PCI_ORDER_FILE} | sort -r`
  do
    PCIA=`echo ${ITEM} | cut -f1-2 -d,`
    echo ${PCIA},eth${ETH_COUNT} >> ${NEW_ORDER_FILE}
    ETH_COUNT=$(( ${ETH_COUNT} + 1 ))
  done
  exit 0
  ;;

esac

# The rest of the script is to deal with the VXA20xx models.
# The VXA2002 and VXA2010 are the same chassis as far as ethernet is concerned.
# The VXA2100 and VXA2200 are supposted to be the same as the 2002 and 2010.

# Four built in ports, from LEFT TO RIGHT:
# 0: next-to-last pci device, use for eth0
# 1: last pci device, use for eth1
# 2: 2nd pci device = 03:00.0, use for eth2
# 3: 1st pci device = 02:00.0, use for eth3
#
# Top slot, when a two port card , from LEFT to RIGHT
# 0: 4th pci device - 07:00.1, use for eth20
# 1: 3th pci device - 07:00.0, use for eth21
#
# Top slot, when a four port card , from LEFT to RIGHT
# 0: 5th pci device - 0a:00.0, use for eth20
# 1: 6th pci device - 0a:00.1, use for eth21
# 2: 3th pci device - 09:00.0, use for eth22
# 3: 4th pci device - 09:00.1, use for eth23

# The pci devices in the middle slot vary depending on what is in the top slot.
# When nothing in the top slot:
# Middle slot, when a two port card , from LEFT to RIGHT
# 0: 4th pci device - 08:00.1, use for eth20
# 1: 3th pci device - 08:00.0, use for eth21
#
# When nothing in the top slot:
# Middle slot, when a four port card , from LEFT to RIGHT
# 0: 5th pci device - 0b:00.0, use for eth20
# 1: 6th pci device - 0b:00.1, use for eth21
# 2: 3th pci device - 0a:00.0, use for eth22
# 3: 4th pci device - 0a:00.1, use for eth23

# The pci devices in the middle slot vary depending on what is in the top slot.
# When two ports in the top slot:
# Middle slot, when a two port card , from LEFT to RIGHT
# 0: 6th pci device - 09:00.1, use for eth20
# 1: 5th pci device - 09:00.0, use for eth21
#
# When two ports in the top slot:
# Middle slot, when a four port card , from LEFT to RIGHT
# 0: 7th pci device - 0c:00.0, use for eth20
# 1: 8th pci device - 0c:00.1, use for eth21
# 2: 5th pci device - 0b:00.0, use for eth22
# 3: 6th pci device - 0b:00.1, use for eth23

# The pci devices in the middle slot vary depending on what is in the top slot.
# When four ports in the top slot:
# Middle slot, when a two port card , from LEFT to RIGHT
# 0: 8th pci device - 0b:00.1, use for eth20
# 1: 7th pci device - 0b:00.0, use for eth21
#
# When four ports in the top slot:
# Middle slot, when a four port card , from LEFT to RIGHT
# 0: 9th pci device - 0?:00.0, use for eth20
# 1:10th pci device - 0?:00.1, use for eth21
# 2: 7th pci device - 0?:00.0, use for eth22
# 3: 8th pci device - 0?:00.1, use for eth23

#  1 x  4 port scheme  ("400")
#  2 x  6 port schemes ("402", "420")
#  3 x  8 port schemes ("404", 422", "440")
#  2 x 10 port schemes ("424", "442")
#  1 x 12 port scheme  ("444")

TMP1_ORDER_FILE=/tmp/eth-order-tmp1.out
TMP2_ORDER_FILE=/tmp/eth-order-tmp2.out
TMP3_ORDER_FILE=/tmp/eth-order-tmp3.out
rm -f ${TMP1_ORDER_FILE}
rm -f ${TMP2_ORDER_FILE}
rm -f ${TMP3_ORDER_FILE}

rm -f ${NEW_ORDER_FILE}

PORT_COUNT=`wc -l ${PCI_ORDER_FILE} | awk '{ print $1 }'`

# First do the bottom row devices (eth0,1  or eth0,1,2,3)

# The last ports in the pci order are to be eth0 and eth1.
# When more that two ports, the first two in the pci order are to be eth2 and eth3
if [ ${PORT_COUNT} -gt 2 ] ; then
  # 1st pci device = 02:00.0, use for eth3
  # 2nd pci device = 03:00.0, use for eth2
  ETH_COUNT=3
  for ITEM in `cat ${PCI_ORDER_FILE}`
  do
    PCIA=`echo ${ITEM} | cut -f1-2 -d,`
    echo ${PCIA},eth${ETH_COUNT} >> ${TMP3_ORDER_FILE}
    ETH_COUNT=$(( ${ETH_COUNT} - 1 ))
    [ ${ETH_COUNT} -le 1 ] && break
  done
fi

Do_Last_Two()
{
  # Second to last device is eth0
  PCIA=`cat ${PCI_ORDER_FILE} | tail -2 | head -1 | cut -f1-2 -d,`
  echo ${PCIA},eth0 >> ${TMP3_ORDER_FILE}
  # Last device is eth1
  PCIA=`cat ${PCI_ORDER_FILE} | tail -1 | cut -f1-2 -d,`
  echo ${PCIA},eth1 >> ${TMP3_ORDER_FILE}
  # Now sort the tmp3 file by eth number and append to NEW_ORDER_FILE
  for i in 0 1 2 3 10 11 12 13 20 21 22 23
  do
     grep ,eth${i}'$' ${TMP3_ORDER_FILE} >> ${NEW_ORDER_FILE}
  done
}
if [ ${PORT_COUNT} -le 4 ] ; then
  Do_Last_Two
  exit 0
fi

# Now generate the middle devices.  This is the tricky part.
# First chop of the first two and last two.

# For the devices in the top and middle slot, create an input
# file with the first two and last two lines removed.

CHOP1=$(( ${PORT_COUNT} - 2 ))
CHOP2=$(( ${CHOP1} - 2 ))
cat ${PCI_ORDER_FILE} | tail -${CHOP1} | head -${CHOP2} > ${TMP1_ORDER_FILE}
PORT_COUNT=$(( ${PORT_COUNT} - 4 ))

# For a quick and dirty test for both if the device is in the
# top slot and if it is a two or four port device, use
# the known pci addresses for the known cards.
# A better way to see if a two or four port would be to
# check in ethernet hwaddr (mac addr).

# Generate the pci address pattern.
PATTERN=
for ITEM in `cat ${TMP1_ORDER_FILE}`
do
  PATTERN=${PATTERN},`echo ${ITEM} | cut -f1 -d,`
done
PATTERN=${PATTERN},
#echo PATTERN=${PATTERN}
case ${PATTERN} in
,0000:07:00.0,0000:07:00.1,*)
  # Top slot has a two port card.
  TOP_COUNT=2
  TOP_ORDER=0000:07:00.1,0000:07:00.0
  ;;
,0000:09:00.0,0000:09:00.1,0000:0a:00.0,0000:0a:00.1,*)
  TOP_COUNT=4
  TOP_ORDER=0000:0a:00.0,0000:0a:00.1,0000:09:00.0,0000:09:00.1
  ;;
*)
  TOP_COUNT=0
  TOP_ORDER=
  ;;
esac

if [ ${TOP_COUNT} -ne 0 ] ; then
  COUNT=0
  for ITEM in `cat ${TMP1_ORDER_FILE}`
  do
    PCIA=`echo ${ITEM} | cut -f1-2 -d,`
    case ${TOP_COUNT}${COUNT} in
    20) NAME=eth20 ;;
    21) NAME=eth21 ;;
    40) NAME=eth21 ;;
    41) NAME=eth20 ;;
    42) NAME=eth23 ;;
    43) NAME=eth22 ;;
    *) echo error, not expecting TOP_COUNT=${TOP_COUNT} COUNT=${COUNT} ;;
    esac
    echo ${PCIA},${NAME} >> ${TMP3_ORDER_FILE}
    COUNT=$(( ${COUNT} + 1 ))
    [ ${COUNT} -ge ${TOP_COUNT} ] && break
  done
fi

if [ ${PORT_COUNT} -eq ${TOP_COUNT} ] ; then
  Do_Last_Two
  exit 0
fi

MIDDLE_COUNT=$(( ${PORT_COUNT} - ${TOP_COUNT} ))
cat ${TMP1_ORDER_FILE} | tail -${MIDDLE_COUNT} > ${TMP2_ORDER_FILE}
COUNT=0
for ITEM in `cat ${TMP2_ORDER_FILE}`
do
  PCIA=`echo ${ITEM} | cut -f1-2 -d,`
  case "${MIDDLE_COUNT}${COUNT}" in
  20) NAME=eth10 ;;
  21) NAME=eth11 ;;
  40) NAME=eth11 ;;
  41) NAME=eth10 ;;
  42) NAME=eth13 ;;
  43) NAME=eth12 ;;
  *) echo Error, not expecting MIDDLE_COUNT=${MIDDLE_COUNT} COUNT=${COUNT} ;;
  esac
  echo ${PCIA},${NAME} >> ${TMP3_ORDER_FILE}
  COUNT=$(( ${COUNT} + 1 ))
done

Do_Last_Two
exit 0
