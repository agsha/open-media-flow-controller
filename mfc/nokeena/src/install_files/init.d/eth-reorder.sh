#!/bin/sh
# Determine the eth interface ordering and write it to the manufacturing DB.
# Have this script run at boot up, before "rename_ifs" is run, which reads
# the manufacturing DB (/config/mfg/mfdb) and performs the mapping.
# In the source tree:
#   $PROD_CUSTOMER_ROOT/install_files/eth-reorder.sh
#   $PROD_TREE_ROOT/src/base_os/linux/el5/image_files/rename_ifs
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
umask 0022

[ -f /etc/build_version.sh ] && . /etc/build_version.sh

# ===========================================================================
# Catch signal and exit. Currently nothing to cleanup
# ===========================================================================
function trap_cleanup_exit()
{
    echo "*** Interrupted by signal"
    exit 1
}

# ===========================================================================
# Always Echo
# ===========================================================================
function Print() { echo "${@}" ; }

# ===========================================================================
# Echo or not based on OPT_VERBOSE_LEVEL setting
# ===========================================================================
function Vprint()
{
    echo "${@}" >> ${LOG_FILE}
    VPRINT_NUM=${1} ; shift
    [ ${OPT_VERBOSE_LEVEL} -ge ${VPRINT_NUM} ] && echo "${@}"
}

function Vcat()
{
    VPRINT_NUM=${1} ; shift
    cat "${@}" >> ${LOG_FILE}
    [ ${OPT_VERBOSE_LEVEL} -ge ${VPRINT_NUM} ] && cat "${@}"
}

function Mac_To_Dec()
{
  TMP=`echo ${1} | tr -d ':' | sed 's/\(..\)/ 0x\1/g'`
  printf "%03d%03d%03d%03d%03d%03d" ${TMP}
}

# ==================================================
# QueryDB -- Function to call mddbreq query --
# Parameter 1: "get" or "iterate".
# Parameter 2: the part of the DB item name after "/mfg/mfdb/interface/"
# The remaining parameters are appended to the mddbreq command line
function QueryDB()
{
    if [ ! -z "${OPT_OUTFILE}" ] ; then
       echo Internal error 1
       return
    fi
    SUBOP=${1} ; shift
    ITEM=${1} ; shift
    ${MDDBREQ} -v ${MFG_DB_PATH} query ${SUBOP} "" /mfg/mfdb/interface/${ITEM} ${@}
}

# ==================================================
# UpdateDB -- Function to call mddbreq set --
# Parameter 1: "modify" or "delete".
# Parameter 2: the part of the DB item name after "/mfg/mfdb/interface/"
# The remaining parameters are appended to the mddbreq command line
function UpdateDB()
{
    if [ ! -z "${OPT_OUTFILE}" ] ; then
       echo Internal error 2
       return
    fi
    SUBOP=${1} ; shift
    ITEM=${1} ; shift
    if [ "${SUBOP}" = "delete" ] ; then
       Vprint 7 UpdateDB ${SUBOP} ${ITEM} ${@}
    else
       Vprint 6 UpdateDB ${SUBOP} ${ITEM} ${@}
    fi
    if [ "${OPT_DRYRUN}" != "dryrun" ] ; then
       # Not using a DB when outputing to a file.
       ${MDDBREQ} -c ${MFG_DB_PATH} set ${SUBOP} "" /mfg/mfdb/interface/${ITEM} ${@}
    fi
}

# ==================================================
function Clear_DB()
{
    # Not using a DB when outputing to a file.
    [ ! -z "${OPT_OUTFILE}" ] && return
    Vprint 5 Deleting interface settings from the DB
    COUNT=`QueryDB get map/count`
    case "_${COUNT}" in
    _)        COUNT=32 ;;
    _*[A-z]*) echo WARNING: QueryDB failed ; COUNT=32 ;;
    esac
    [ ${COUNT} -le 0 ] && COUNT=32
    
    UpdateDB delete map/enable
    MAP_NUMS=`QueryDB iterate map/macifname | tr '\n' ' '`
    Vprint 4 MAP_NUMS=${MAP_NUMS}
    for I in ${MAP_NUMS}; do
        UpdateDB delete map/macifname/${I}/name
        UpdateDB delete map/macifname/${I}
        UpdateDB delete map/macifname/${I}/macaddr
    done
    UpdateDB delete map/count
}

# ==================================================
# Write out the list of mac -> interface name mappings in
#  /mfg/mfdb/interface/map/macifname/1/macaddr and
#  /mfg/mfdb/interface/map/macifname/1/name
function Write_Mapping()
{
    MAP_INDEX=1
    for THIS_MAC in ${IF_MACS_LIST}; do
        THIS_DEC=`Mac_To_Dec ${THIS_MAC}`

        eval 'K_NAME="${IF_MAC_'${THIS_DEC}'_KERNNAME}"'
        eval 'T_NAME="${IF_MAC_'${THIS_DEC}'_TARGETNAME}"'

        if [ "_${T_NAME}" = "_" ]; then
            if [ "_${K_NAME}" = "_" ]; then
                Vprint 2 "-- No mapping for ${THIS_MAC}"
            else
                Vprint 2 "-- No mapping for ${THIS_MAC} on ${K_NAME}"
            fi
            continue
        fi
        if [ "_${K_NAME}" != "_" ] ; then
            Vprint 3 "-- Writing mapping for ${THIS_MAC} from ${K_NAME} to ${T_NAME}" 
        else
            Vprint 3 "-- Writing mapping for ${THIS_MAC} to ${T_NAME}" 
        fi
        if [ ! -z "${OPT_OUTFILE}" ] ; then
            BUS_INFO=`${SUDO} /sbin/ethtool -i ${K_NAME} | grep "^bus-info: " | cut -c11-`
            echo ${BUS_INFO},${THIS_MAC},${T_NAME} >> "${OPT_OUTFILE}"
        else
            UpdateDB modify map/macifname/${MAP_INDEX} uint32 ${MAP_INDEX}
            UpdateDB modify map/macifname/${MAP_INDEX}/macaddr macaddr802 "${THIS_MAC}"
            UpdateDB modify map/macifname/${MAP_INDEX}/name string "${T_NAME}"
        fi
        MAP_INDEX=$((${MAP_INDEX} + 1))
    done
    MAP_INDEX=$((${MAP_INDEX} - 1))
    if [ -z "${OPT_OUTFILE}" ] ; then
        UpdateDB modify rename_type string mac-map
        UpdateDB modify map/enable bool true
        UpdateDB modify map/count uint32 ${MAP_INDEX}
    fi
}


#
# ===========================================================================
# Global variables and file locations
# ===========================================================================
SKIP_LIST=
MFG_DB_PATH=/config/mfg/mfdb
MDDBREQ=/opt/tms/bin/mddbreq

LOG_DIR=/log/init
if [ ! -d ${LOG_DIR} ] ; then
  LOG_DIR=/tmp
fi
PCI_SORTED_FILE=${LOG_DIR}/if-pci-sorted-list.txt
RAW_LIST_SAVE_FILE=${LOG_DIR}/if-raw-list.txt
PREV_MAPPING_SAVE_FILE=${LOG_DIR}/if-prev-mapping-list.txt
MY_DIR=`dirname "${0}"`
LOG_FILE=${LOG_DIR}/eth-reorder.log
if [ -f ${LOG_FILE} ] ; then
  COUNT=0
  while true
  do
    SAVE_LOG_FILE=${LOG_DIR}/eth-reorder.${COUNT}.log
    if [ ! -f ${SAVE_LOG_FILE} ] ; then
        mv ${LOG_FILE} ${SAVE_LOG_FILE}
        break
    fi
    COUNT=`expr ${COUNT} + 1`
  done
fi
NOW_DATE=`date`
echo Date:"${NOW_DATE}" > ${LOG_FILE}
grep BUILD_PROD_VERSION /etc/build_version.txt >> ${LOG_FILE}
grep BUILD_DATE         /etc/build_version.txt >> ${LOG_FILE}
grep BUILD_SCM_INFO     /etc/build_version.txt >> ${LOG_FILE}

# ==================================================
# Command line option variables
# ==================================================

export OPT_FORCE=noforce
export OPT_GROUPS=groups
export OPT_VERBOSE_LEVEL=1
export OPT_TESTING_LEVEL=0
export OPT_DRYRUN=nodryrun
export OPT_0=
export OPT_1=
export OPT_USE=none
export OPT_OUTFILE=

# Get the command line options
while true
do
    A=${1}
    [ "_${A}" = "_" ] && break
    shift
    case "_${A}" in
    _-f|_--force)   OPT_FORCE=force ;;
    _-g|_--group)   OPT_GROUPS=groups ;;
    _-G|_--nogroup|_nogroup) OPT_GROUPS=nogroups ;;
    _-q|_--quiet)   OPT_VERBOSE_LEVEL=0 ;;
    _-v|_--verbose) OPT_VERBOSE_LEVEL=$((${OPT_VERBOSE_LEVEL} + 1)) ;;
    _-d|_--dryrun)  OPT_DRYRUN=dryrun ;;
    _-D|_--nodryrun) OPT_DRYRUN=nodryrun ;;
    _-t|_--testing) OPT_TESTING_LEVEL=$((${OPT_TESTING_LEVEL} + 1)) ;;
    _-o|_--outfile) OPT_OUTFILE="${1}"
                    if [ -z "${OPT_OUTFILE}" ] ; then
                      echo No filename specified with the --outfile option.
                      exit 1
                    fi
                    shift
                    rm -f ${OPT_OUTFILE}
                    ;;
    _-u|_--use)     OPT_USE="${1}"
                    if [ -z "${OPT_USE}" ] ; then
                      echo No filename specified with the --use option.
                      exit 1
                    fi
                    if [ ! -f "${OPT_USE}" ] ; then
                      echo File "${OPT_USE}" does not exist.
                      exit 1
                    fi
                    if [ -d /config/nkn ] ; then
                      cp "${OPT_USE}" /config/nkn/ethreorder_use_done
                    fi
                    Vprint 3 "=== --use ${OPT_USE}"
                    Vcat 3 "${OPT_USE}"
                    Vprint 3 "==="
                    shift
                    ;;
    _-0=|_--eth0=*|_eth0=*) OPT_0=`echo ${A} | cut -f2 -d=` ;;
    _-1=|_--eth1=*|_eth1=*) OPT_1=`echo ${A} | cut -f2 -d=` ;;
    _-0|_--eth0|_eth0)    OPT_0="${1}" ; shift ;;
    _-1|_--eth1|_eth1)    OPT_1="${1}" ; shift ;;
    _--help_menu)
        Print "Reorder the eth device names to group the names by NIC card."
        Print "You can specify which device is to be used for eth0 and eth1."
        Print "To specify for eth0, type 'eth0' followed by one of the following:"
        Print "- HWaddr (aka MAC address), e.g. 00:30:48:B8:F8:22"
        Print "- The current ethN name, e.g. eth4"
        Print "- 'pxe' Use the device the machine was pxe-booted from when installing."
        Print "- An http or https URL. The device that can access the URL is used."
        Print "- 'url' Use the URL the system was installed from to detect the device."
        Print "- 'least-ports' To automatically have eth0 be set to the NIC device that"
        Print "      has the fewest ports."
        Print "- 'most-ports' To automatically have eth0 be set to the NIC device that"
        Print "      has the most ports."
        Print "E.g. eth0 least-ports"
        Print "After specifying eth0 with a ethN name or a HWaddr you can optionally"
        Print "specify for eth1 in the same way."
        Print "E.g. eth0 00:30:48:B8:F8:22 eth1 00:30:48:B8:F8:21"
        Print "Each NIC card starts at mulitiple of 10 (eth0, eth10, eth20...)"
        Print "To make it not do this grouping add 'nogroups' to the line."
        exit 0
        ;;
    *)
        if [ ! -z "${OPT_OUTFILE}" ] ; then
            Print Invalid parameter
            exit 1
        fi
        Print This command reorders the eth devices to group them by NIC card.
        Print "Each NIC card starts at mulitiple of 10 (eth0, eth10, eth20...)"
        Print Normally, once this command has set up the order for a device,
        Print it will not change it.
        Print " --force    (-f) Forces redoing the ordering."
        Print " --nogroups (-G) Do not do the grouping."
        Print " --quiet    (-q) Suppresses printing to stdout."
        Print " --verbose  (-v) Increases the amount of info printed to stdout."
        Print "                 Use more --verbose options for more detail."
        Print " --dryrun   (-d) Only print out what would be done, take no action."
        Print " --use X    (-u) Use the file X to specify the exact mapping."
        Print " --eth0 X   (-0) Use this device for eth0."
        Print " --eth1 X   (-1) Use this device for eth1"
        Print "The device specified by --eth0 and --eth1 can be:"
        Print "- HWaddr (aka MAC address), e.g. 00:30:48:B8:F8:22"
        Print "- The current ethN name, e.g. eth4"
        Print "- 'pxe' Use the device the machine was pxe-booted from when installing."
        Print "- An http or https URL. The device that can access the URL is used."
        Print "- 'url' Use the URL the system was installed from to detect the device."
        Print "- 'least-ports' To automatically have eth0 be set to the NIC device that"
        Print "      has the fewest ports."
        Print "- 'most-ports' To automatically have eth0 be set to the NIC device that"
        Print "      has the most ports."
        Print "Note: --eth1 can only be specified after --eth0 is specified as an"
        Print "      ethN name or HWaddr"
        #      1#############################################################################80
        # Undocumented options:
        #     --testing  (-t)
        # Undocumented because they specify the normal defaults:
        #     --group    (-g)
        #     --nodryrun (-D)
        # Undocumented: Certain command option overides can be specified
        # in the files:
        #   /config/nkn/ethreorder_options  'force' and/or 'nogroup'
        #   /config/nkn/ethreorder_eth0      the --eth0 value
        #   /config/nkn/ethreorder_eth1      the --eth1 value
        # Note: The /config/nkn directory does NOT exist in the manufacturing
        # environment, only in the fully installed MFC environment.
        Print "Existing ethernet devices:"
        ifconfig | grep -v RX | grep -v TX | grep -v collisions | grep -v -i loopback | grep -v 127.0.0.1 | grep -v "^$"
        exit 1 ;;
    esac
done
Vprint 3 Verbose level ${OPT_VERBOSE_LEVEL}
if [ ${OPT_TESTING_LEVEL} -gt 1 ] ; then
    MFG_DB_PATH=./mfdb
    [ -f ./mddbreq ] && MDDBREQ=./mddbreq
fi
if [ -d /config/nkn ] ; then
  # Get command line option overrides from configuration file.
  if [ -f /config/nkn/ethreorder_options ] ; then
    cat /config/nkn/ethreorder_options | grep -v "^#" | grep -q force
    if [ ${?} -eq 0 ] ; then
      OPT_FORCE=force
      Vprint 1 Override from /config/nkn/ethreorder_options: force
    fi
    cat /config/nkn/ethreorder_options | grep -v "^#" | grep -q nogroups
    if [ ${?} -eq 0 ] ; then
      OPT_GROUPS=nogroups
      Vprint 1 Override from /config/nkn/ethreorder_options: nogroups
    fi
    rm -f /config/nkn/ethreorder_options_done
    mv /config/nkn/ethreorder_options /config/nkn/ethreorder_options_done
  fi

  # Get command line option defaults from configuration files.
  if [ -z "${OPT_0}" -a -f /config/nkn/ethreorder_eth0 ] ; then
    OPT_0=`cat /config/nkn/ethreorder_eth0`
    Vprint 1 Eth 0 MAC specified from /config/nkn/ethreorder_eth0: ${OPT_0}
  fi
  if [ -z "${OPT_1}" -a -f /config/nkn/ethreorder_eth1 ] ; then
    OPT_1=`cat /config/nkn/ethreorder_eth1`
    Vprint 1 Eth 1 MAC specified from /config/nkn/ethreorder_eth1: ${OPT_1}
  fi
  if [ -f /config/nkn/ethreorder_eth0 ] ; then
    rm -f /config/nkn/ethreorder_eth0_done
    mv /config/nkn/ethreorder_eth0 /config/nkn/ethreorder_eth0_done
  fi
  if [ -f /config/nkn/ethreorder_eth1 ] ; then
    rm -f /config/nkn/ethreorder_eth1_done
    mv /config/nkn/ethreorder_eth1 /config/nkn/ethreorder_eth1_done
  fi
  # If the command line options were for eth0/eth1 setting save it.
  if [ ! -z "${OPT_0}" -a ! -f /config/nkn/ethreorder_eth0_done ] ; then
    echo "${OPT_0}" > /config/nkn/ethreorder_eth0_done
  fi
  if [ ! -z "${OPT_1}" -a ! -f /config/nkn/ethreorder_eth1_done ] ; then
    echo "${OPT_1}" > /config/nkn/ethreorder_eth1_done
  fi
  if [ ! -f /config/nkn/ethreorder_options_done -a \
       "${OPT_GROUPS}" = "nogroups" ] ; then
    echo nogroups >> /config/nkn/ethreorder_options_done
  fi
fi

ENABLED=false
if [ -z "${OPT_OUTFILE}" ] ; then
  # When not forced, get the previous mapping info
  if [ "${OPT_FORCE}" = "force" ] ; then
    WAS_ENABLED=`QueryDB get map/enable`
    if [ "_${WAS_ENABLED}" = "_true" ] ; then
      Vprint 1 Forced to ignore previous mapping in DB
    else
      Vprint 1 No previous mapping enabled in DB
    fi
  else
    Vprint 6 QueryDB get map/enable
    ENABLED=`QueryDB get map/enable`
    if [ -z "${ENABLED}" ] ; then
      Vprint1 WARNING: FAILED TO GET MAP/ENABLE STATE
      ENABLED=false
    fi
    Vprint 4 ENABLED=${ENABLED}
  fi
fi

if [ -d /config/nkn -a "${ENABLED}" != "true" ] ; then
  # There ALWAYS should be a mapping except for the first boot.
  # If this is not the first boot, and there is no mapping
  # then pull back the original configuration options.
  if [ -z "${OPT_USE}${OPT_0}${OPT_1}" ] ; then
    # This was not a first boot, or no option were used the first time.
    ${OPT_USE}=/config/nkn/ethreorder_use_done
    if [ -f ${OPT_USE} ] ; then
        Vprint 1 Mapping specified from ${OPT_USE}
        Vcat 1 ${OPT_USE}
    else
      OPT_USE=none
      if [ -f /config/nkn/ethreorder_eth0_done ] ; then
        OPT_0=`cat /config/nkn/ethreorder_eth0_done`
        Vprint 1 Eth 0 MAC specified from /config/nkn/ethreorder_eth0_done: ${OPT_0}
      fi
      if [ -f /config/nkn/ethreorder_eth1_done ] ; then
        OPT_1=`cat /config/nkn/ethreorder_eth1_done`
        Vprint 1 Eth 1 MAC specified from /config/nkn/ethreorder_eth1_done: ${OPT_1}
      fi
    fi
    if [ -f /config/nkn/ethreorder_options_done ] ; then
      cat /config/nkn/ethreorder_options_done | grep -v "^#" | grep -q nogroups
      if [ ${?} -eq 0 ] ; then
        OPT_GROUPS=nogroups
        Vprint 1 Override from /config/nkn/ethreorder_options_done: nogroups
      fi
    fi
  fi
fi
export AUTO_ETH0=off
case "${OPT_0}" in
na|default)
  OPT_0=
  ;;
least-ports)
  AUTO_ETH0=least-ports
  OPT_0=
  OPT_1=
  ;;
most-ports)
  AUTO_ETH0=most-ports
  OPT_0=
  OPT_1=
  ;;
esac
[ "_${OPT_1}" = "na" ] && OPT_1=

if [ -z "${OPT_0}" -a ! -z "${OPT_1}" ] ; then 
  Vprint 2 OPT_0=${OPT_0}
  Vprint 2 OPT_1=${OPT_1}
  Vprint 0 "Error, eth1 specified, but not eth0"
  Vprint 0 "When eth1 is specified, eth0 must also be specified."
  exit 1
fi

trap "trap_cleanup_exit" HUP INT QUIT PIPE TERM

if [ "${OPT_USE}" != "none" ] ; then
  if [ ! -z "${OPT_OUTFILE}" ] ; then
    echo Error, cannot specify --outfile and --use at the same time.
  fi
  # Use the mapping exactly as given.  The file is comma delimited
  # First field is PCI address (not needed here)
  # Second field is HWaddr
  # Third field is the eth name to assign.
  Vprint 2 "= Assigning interface names"
  MAP_INDEX=1
  for SET in `cat "${OPT_USE}"`
  do
    THIS_MAC=`echo "${SET}" | cut -f2 -d,`
    THIS_NAME=`echo "${SET}" | cut -f3 -d,`

    Vprint 3 "-- Writing mapping for ${THIS_MAC} to ${THIS_NAME}" 
    UpdateDB modify map/macifname/${MAP_INDEX} uint32 ${MAP_INDEX}
    UpdateDB modify map/macifname/${MAP_INDEX}/macaddr macaddr802 "${THIS_MAC}"
    UpdateDB modify map/macifname/${MAP_INDEX}/name string "${THIS_NAME}"

    MAP_INDEX=$((${MAP_INDEX} + 1))
  done
  MAP_INDEX=$((${MAP_INDEX} - 1))
  UpdateDB modify rename_type string mac-map
  UpdateDB modify map/enable bool true
  UpdateDB modify map/count uint32 ${MAP_INDEX}
  exit 0
fi

# Get the naming of all the interfaces
# ==================================================

# Note that we take out any alias interfaces, like eth0:1 , as they are 
# likely not a 'real' interface.

Get_List()
{
  for i in `/sbin/ifconfig -a | grep ^eth | cut -f1 -d' '`
  do
    case "${i}" in
    *:*) continue ;;
    esac
    BUS_INFO=`/sbin/ethtool -i ${i} | grep "^bus-info: " | cut -c11-`
    HWADDR=`/sbin/ifconfig ${i} | grep HWaddr | cut -f2- -d: | sed -e "/Ethernet.*HWaddr /s///"`
    echo ${BUS_INFO},${HWADDR} ${i}
  done
}

rm -f ${RAW_LIST_SAVE_FILE}
if [ ${OPT_TESTING_LEVEL} -eq 0 ] ; then
  Get_List | sort >  ${PCI_SORTED_FILE}
  cat ${PCI_SORTED_FILE} | sed -e "/^.*,/s///" > ${RAW_LIST_SAVE_FILE}
else
  cat ./if-raw-list${OPT_TESTING_LEVEL}.txt | grep -v '#' \
    > ${RAW_LIST_SAVE_FILE}
fi
Vprint 3 === RAW_LIST_SAVE_FILE
Vcat 3 ${RAW_LIST_SAVE_FILE}
Vprint 3 ===
#################################################################################

IF_NAME_MAC_RAW=`cat ${RAW_LIST_SAVE_FILE} | grep -v "#"`
IF_RAW_LIST=`echo "${IF_NAME_MAC_RAW}" | tr ' ' '-' | tr '\n' ' '`
Vprint 2 == Current devices:
Vprint 2 "${IF_NAME_MAC_RAW}"
IF_KERNNAMES_LIST=`echo "${IF_NAME_MAC_RAW}" | awk '{print $2}' | tr '\n' ' '`
IF_MACS_LIST=`     echo "${IF_NAME_MAC_RAW}" | awk '{print $1}' | tr '\n' ' '`
Vprint 4 "==== IF_KERNNAMES_LIST=${IF_KERNNAMES_LIST}"
Vprint 4 "==== IF_MACS_LIST=${IF_MACS_LIST}"
ETH0_RAW=`echo "${IF_NAME_MAC_RAW}" | grep 'eth0$'`
Vprint 4 ETH0_RAW=${ETH0_RAW}

function MAC_Pattern_From_URL()
{
  TMP=`${MY_DIR}/eth-device-from-http.sh "${1}"`
  RV=${?}
  if [ ${RV} -ne 0 -o -z "${TMP}" ] ; then
    Vprint 2 "No device can access ${1}, ${RV}"
    PATTERN=
    return
  fi
  Vprint 2 URL ${1} via ${TMP}
  # The MAC address is after the dash
  PATTERN=`echo ${TMP} | cut -f2 -d-`
}

FORCE_0_MAC=
FORCE_0_DEC_MAC=
FORCE_1_MAC=
FORCE_1_DEC_MAC=
Vprint 2 OPT_0=${OPT_0}
Vprint 2 OPT_1=${OPT_1}
if [ "_${OPT_0}" != "_" ] ; then
  if [ "${OPT_0}" = "url" ] ; then
    IMG_URL=
    [ -f ${LOG_DIR}/mfg.url ] && IMG_URL=`cat ${LOG_DIR}/mfg.url`
    if [ -z "${IMG_URL}" ] ; then
      Print "No image file URL was used to install this system."
      exit 1
    fi
    OPT_0="${IMG_URL}"
  fi
  if [ "${OPT_0}" = "pxe" ] ; then
    IOP_MAC_ADDR=
    [ -f ${LOG_DIR}/install-options.txt ] && . ${LOG_DIR}/install-options.txt
    if [ -z "${IOP_MAC_ADDR}" ] ; then
      Print "System was not installed via PXE."
      exit 1
    fi
    OPT_0="${IOP_MAC_ADDR}"
  fi
  case "${OPT_0}" in
  http*)
    Vprint 3 MAC_Pattern_From_URL "${OPT_0}"
    MAC_Pattern_From_URL "${OPT_0}"
    if  [ -z "${PATTERN}" ] ; then
      Print "No device can access ${OPT_0}"
      exit 1
    fi
    ;;
  *:*) PATTERN="${OPT_0} " ;;
  *)   PATTERN="${OPT_0}\$" ;;
  esac
  TMP=`echo "${IF_NAME_MAC_RAW}" | grep -i "${PATTERN}" | tr ' ' '-' | tr '\n' ' '`
  FORCE_0_MAC=`echo ${TMP} | cut -f1 -d-`
  if [ -z "${FORCE_0_MAC}" ] ; then
    Print "No device matching: ${OPT_0}"
    exit 1
  fi
  FORCE_0_DEC_MAC=`Mac_To_Dec ${FORCE_0_MAC}`
  FORCE_0_KNAME=`echo ${TMP} | cut -f2 -d-`
  Vprint 3 FORCE_0_MAC=${FORCE_0_MAC}
  Vprint 3 FORCE_0_DEC_MAC=${FORCE_0_DEC_MAC}
  Vprint 3 FORCE_0_KNAME=${FORCE_0_KNAME}
fi
if [ "_${OPT_1}" != "_" ] ; then
  case "${OPT_1}" in
  http*)
    Vprint 3 MAC_Pattern_From_URL "${OPT_1}"
    MAC_Pattern_From_URL "${OPT_1}"
    if  [ -z "${PATTERN}" ] ; then
      Print "No device can access ${OPT_1}"
      exit 1
    fi
    ;;
  *:*) PATTERN="${OPT_1} " ;;
  *)   PATTERN="${OPT_1}\$" ;;
  esac
  TMP=`echo "${IF_NAME_MAC_RAW}" | grep -i "${PATTERN}" | tr ' ' '-' | tr '\n' ' '`
  FORCE_1_MAC=`echo ${TMP} | cut -f1 -d-`
  if [ -z "${FORCE_1_MAC}" ] ; then
    Print "No device matching: ${OPT_1}"
    exit 1
  fi
  if [ "${FORCE_0_MAC}" = "${FORCE_1_MAC}" ] ; then
    Print "Error, same address specfied for eth0 and eth1"
    exit 1
  fi
  FORCE_1_DEC_MAC=`Mac_To_Dec ${FORCE_1_MAC}`
  FORCE_1_KNAME=`echo ${TMP} | cut -f2 -d-`
  Vprint 3 FORCE_1_MAC=${FORCE_1_MAC}
  Vprint 3 FORCE_1_DEC_MAC=${FORCE_1_DEC_MAC}
  Vprint 3 FORCE_1_KNAME=${FORCE_1_KNAME}
fi

# Get the list of mac -> interface name mappings in
#  /mfg/mfdb/interface/map/macifname/1/macaddr and
#  /mfg/mfdb/interface/map/macifname/1/name
rm -f ${PREV_MAPPING_SAVE_FILE}
touch ${PREV_MAPPING_SAVE_FILE}
SPECIAL_ADD=
function Get_Mapping()
{
    Vprint 6 QueryDB iterate map/macifname
    MAP_NUMS=`QueryDB iterate map/macifname | tr '\n' ' '`
    Vprint 4 MAP_NUMS=${MAP_NUMS}
    ETH0_PREV_MAC=
    for if_num in ${MAP_NUMS}; do
        Vprint 6 QueryDB get map/macifname/${if_num}/macaddr
        Vprint 6 QueryDB get map/macifname/${if_num}/name
        MAC_ADDR=`QueryDB get map/macifname/${if_num}/macaddr`
        DEV_NAME=`QueryDB get map/macifname/${if_num}/name`
        [ "${DEV_NAME}" = "eth0" ] && ETH0_PREV_MAC=${MAC_ADDR}
        Vprint 4 ${MAC_ADDR} ${DEV_NAME}
        echo ${MAC_ADDR} ${DEV_NAME} >> ${PREV_MAPPING_SAVE_FILE}
        
    done
    # See if upgrading from a previous MFD version that did not save the
    # eth0 setting.  When this is the case, add it to the PREV_NAME_MAC_RAW
    # to make it work as if it had been saved.
    if [ -z "${ETH0_PREV_MAC}" ] ; then
      # Not found.  Add it.
      Vprint 2 == Add ${ETH0_RAW}
      echo ${ETH0_RAW} >> ${PREV_MAPPING_SAVE_FILE}
      SPECIAL_ADD=eth0
    fi
    PREV_NAME_MAC_RAW=`cat ${PREV_MAPPING_SAVE_FILE} | grep -v "#"`
}

PRESERVE_DEVICE_LIST=
GONE_DEVICE_LIST=
NEW_DEVICE_LIST=

if [ "_${ENABLED}" = "_true" ] ; then
    Vprint 2 == eth reorder previously calculated
    # See if any change in the interfaces and remove mappings of all
    # MAC addressess that are not found anymore, and add new mappings
    # for any new MAC addresses.

    Get_Mapping

    Vprint 2 == Previous mapping:
    Vprint 2 "${PREV_NAME_MAC_RAW}"

    # First find any devices that have gone away.
    IF_PREV_LIST=" "`echo "${PREV_NAME_MAC_RAW}" | tr ' ' '-' | tr '\n' ' '`" "
    Vprint 4 == IF_PREV_LIST=${IF_PREV_LIST}
    for THIS_ITEM in ${IF_PREV_LIST} ; do
        THIS_MAC=`echo ${THIS_ITEM} | cut -f1 -d-`
        MATCH_LINE=`echo "${IF_NAME_MAC_RAW}" | grep ${THIS_MAC}`
        if [ "_${MATCH_LINE}" = "_" ] ; then
            GONE_DEVICE_LIST="${GONE_DEVICE_LIST}${THIS_ITEM} "
        else
            PRESERVE_DEVICE_LIST="${PRESERVE_DEVICE_LIST}${THIS_ITEM} "
            THIS_DEC=`Mac_To_Dec ${THIS_MAC}`
            K_NAME=`echo "${IF_NAME_MAC_RAW}" | grep ${THIS_MAC} | awk '{print $2}'`
            eval 'IF_MAC_'${THIS_DEC}'_KERNNAME="'${K_NAME}'"'
        fi
    done

    # Next find any devices that are new.
    Vprint 4 == IF_RAW_LIST=${IF_RAW_LIST}
    for THIS_ITEM in ${IF_RAW_LIST} ; do
        # If a new device, add to NEW_DEVICE_LIST
        THIS_MAC=`echo ${THIS_ITEM} | cut -f1 -d-`
        MATCH_LINE=`echo "${IF_PREV_LIST}" | grep ${THIS_MAC}`
        Vprint 5 === MATCH_LINE=${MATCH_LINE}
        if [ "_${MATCH_LINE}" = "_" ] ; then
            NEW_DEVICE_LIST="${NEW_DEVICE_LIST}${THIS_ITEM} "
        fi
    done
    Vprint 4 GONE_DEVICE_LIST=${GONE_DEVICE_LIST}
    Vprint 4 NEW_DEVICE_LIST=${NEW_DEVICE_LIST}
    if [ "_${GONE_DEVICE_LIST}" = "_" -a "_${NEW_DEVICE_LIST}" = "_" ] ; then
        Vprint 3 No ethernet device change.
        [ -z "${SPECIAL_ADD}" ] && exit 0
        Vprint 3 Need to update db with eth0 setting
    fi
    Vprint 4 PRESERVE_DEVICE_LIST=${PRESERVE_DEVICE_LIST}
else
    Vprint 2 == NO Previous mapping
    Vprint 4 ==== IF_RAW_LIST=${IF_RAW_LIST}
    for THIS_ITEM in ${IF_RAW_LIST} ; do
        NEW_DEVICE_LIST="${NEW_DEVICE_LIST}${THIS_ITEM} "
    done
    Vprint 4 NEW_DEVICE_LIST=${NEW_DEVICE_LIST}
fi

##############################################################
# At this point, something changed.
# (When no change, then it did the exit above.)
##############################################################
# Processing continues where "Save_Previous_Files" is called.
##############################################################

function Save_Previous_Files()
{
  # Save the previous mapping file.
  COUNT=0
  while true
  do
    SAVE_FILE_NAME=${LOG_DIR}/if-prev_mapping-list.${COUNT}.txt
    if [ ! -f ${SAVE_FILE_NAME} ] ; then
        mv ${PREV_MAPPING_SAVE_FILE} ${SAVE_FILE_NAME}
        break
    fi
    COUNT=`expr ${COUNT} + 1`
  done
}

function Verify_Stuff()
{
  ALREADY_PRESERVED_eth0=
  ALREADY_PRESERVED_eth1=
  # The format for PRESERVE_DEVICE_LIST a single line of item with form:
  #   <MAC>-eth<number>
  # separated by space characters.  E.g.: 
  # PRESERVE_DEVICE_LIST=00:15:17:AB:C5:D4-eth0 00:15:17:AB:C5:D5-eth1 00:30:48:B8:F8:22-eth10 00:30:48:B8:F8:23-eth11
  [ -z "${PRESERVE_DEVICE_LIST}" ] && return
  echo "${PRESERVE_DEVICE_LIST} " | grep -q "eth0 "
  if [ ${?} -eq 0 ] ; then
    ALREADY_PRESERVED_0=yes
    # eth0 is already preserved.  If it is the same as specified, then ignore.
    if [ ! -z "${FORCE_0_MAC}" ] ; then
      echo "${PRESERVE_DEVICE_LIST} " | grep -q "${FORCE_0_MAC}-eth0 "
      if [ ${?} -eq 0 ] ; then
        FORCE_0_MAC=
        FORCE_0_DEC_MAC=
        FORCE_1_MAC=
        FORCE_1_DEC_MAC=
      fi
    fi
  fi
  echo "${PRESERVE_DEVICE_LIST} " | grep -q "eth1 "
  if [ ${?} -eq 0 ] ; then
    ALREADY_PRESERVED_1=yes
    # eth1 is already preserved.  If it is the same as specified, then ignore.
    if [ ! -z "${FORCE_1_MAC}" ] ; then
      echo "${PRESERVE_DEVICE_LIST} " | grep -q "${FORCE_1_MAC}-eth1 "
      if [ ${?} -eq 0 ] ; then
        FORCE_1_MAC=
        FORCE_1_DEC_MAC=
      fi
    fi
  fi
  if [ ! -z "${FORCE_0_MAC}" ] ; then
    echo "${PRESERVE_DEVICE_LIST} " | grep -q "${FORCE_0_MAC}- "
    if [ ${?} -eq 0 ] ; then
      echo "Error, eth0 specified on command line as ${FORCE_0_MAC}, but is already fixed at another address."
      exit 1
    fi
  fi
  if [ ! -z "${FORCE_1_MAC}" ] ; then
    echo "${PRESERVE_DEVICE_LIST} " | grep -q "${FORCE_1_MAC}- "
    if [ ${?} -eq 0 ] ; then
      echo "Error, eth1 specified on command line as ${FORCE_1_MAC}, but is already fixed at another address."
      exit 1
    fi
  fi
}

function Determine_Sets()
{
  PREV_UPPER_DEC=0
  PREV_LOWER_DEC=0
  SET_0_INUM=0
  SET_0_TOTAL=0

  SMALLEST_SET_SIZE=99
  SMALLEST_SET_ID=0
  SMALLEST_SET_LIST=
  LARGEST_SET_SIZE=0
  LARGEST_SET_ID=0
  LARGEST_SET_LIST=
  SET_LIST=
  SET_DEC_LIST=

  SET_COUNT=0
  IF_INDEX=0
  FIRST_DEV=yes

  SET_ID_NUM=0
  SET_1_INUM=1
  SET_1_TOTAL=0
  Vprint 4 Determine the sets
  for THIS_ITEM in ${NEW_DEVICE_LIST}; do
    IF_INDEX=$((${IF_INDEX} + 1))

    K_NAME=`echo ${THIS_ITEM} | cut -f2 -d-`
    THIS_MAC=`echo ${THIS_ITEM} | cut -f1 -d-`
    THIS_DEC=`Mac_To_Dec ${THIS_MAC}`
    THIS_UPPER_DEC=`echo ${THIS_DEC} | cut -c1-9`
    THIS_LOWER_DEC=`echo ${THIS_DEC} | cut -c10-18`
    Vprint 4 "=------------MAC=${THIS_MAC}=${THIS_DEC}"

    DIFF_DEC=0
    if [ "${THIS_UPPER_DEC}" = "${PREV_UPPER_DEC}" ] ; then
        Vprint 6 "==== ${THIS_UPPER_DEC} = ${PREV_UPPER_DEC}" 
        DIFF_DEC=`expr ${THIS_LOWER_DEC} - ${PREV_LOWER_DEC}`
        Vprint 5 "=== DIFF_DEC=${DIFF_DEC}" 
    else
        Vprint 6 "==== ${THIS_UPPER_DEC} != ${PREV_UPPER_DEC}" 
        DIFF_DEC=9999
    fi
    if [ ${DIFF_DEC} -gt 15 -o ${DIFF_DEC} -lt -15 ] ; then
      if [ "${FIRST_DEV}" = "yes" ] ; then
        Vprint 4 "=== First device"
        FIRST_DEV=no
      else
        Vprint 4 "=== New device"
        # If this is the smallest set so far, save that info.
        if [ ${SET_COUNT} -lt ${SMALLEST_SET_SIZE} ] ; then
          SMALLEST_SET_SIZE=${SET_COUNT}
          SMALLEST_SET_ID=${SET_ID_NUM}
          SMALLEST_SET_LIST="${SET_LIST}"
          Vprint 5 SMALLEST_SET_SIZE=${SMALLEST_SET_SIZE}
          Vprint 5 SMALLEST_SET_ID=${SMALLEST_SET_ID}
          Vprint 5 SMALLEST_SET_LIST=${SMALLEST_SET_LIST}
        fi
        if [ ${SET_COUNT} -gt ${LARGEST_SET_SIZE} ] ; then
          LARGEST_SET_SIZE=${SET_COUNT}
          LARGEST_SET_ID=${SET_ID_NUM}
          LARGEST_SET_LIST="${SET_LIST}"
          Vprint 5 LARGEST_SET_SIZE=${LARGEST_SET_SIZE}
          Vprint 5 LARGEST_SET_ID=${LARGEST_SET_ID}
          Vprint 5 LARGEST_SET_LIST=${LARGEST_SET_LIST}
        fi
      fi
      SET_DEC_LIST=
      SET_LIST=
      SET_ID_NUM=$((${SET_ID_NUM} + 1))
      eval 'SET_'${SET_ID_NUM}'_INUM="'${IF_INDEX}'"'
      SET_COUNT=1
    else
      SET_COUNT=$((${SET_COUNT} + 1))
      Vprint 5 "=== Same device, cnt ${SET_COUNT}"
    fi
    PREV_UPPER_DEC="${THIS_UPPER_DEC}"
    PREV_LOWER_DEC="${THIS_LOWER_DEC}"
    SET_LIST="${SET_LIST} ${THIS_ITEM}"
    SET_DEC_LIST="${SET_DEC_LIST} ${THIS_DEC}"

    eval 'IF_MAC_'${THIS_DEC}'_MAC="'${THIS_MAC}'"'
    eval 'IF_MAC_'${THIS_DEC}'_KERNNAME="'${K_NAME}'"'

    Vprint 4 "=== IF_INDEX=${IF_INDEX} K_NAME= ${K_NAME}"
    Vprint 5 "==== SET_COUNT=${SET_COUNT} SET_ID_NUM=${SET_ID_NUM}"

    eval 'IF_MAC_'${THIS_DEC}'_IN_SET="'${SET_ID_NUM}'"'
    Vprint 4 'IF_MAC_'${THIS_DEC}'_IN_SET="'${SET_ID_NUM}'"'
    eval 'SET_'${SET_ID_NUM}'_TOTAL="'${SET_COUNT}'"'
    Vprint 4 'SET_'${SET_ID_NUM}'_TOTAL="'${SET_COUNT}'"'
  done
  Vprint 4 "SMALLEST_SET_SIZE prior to last device set:${SMALLEST_SET_SIZE}"
  Vprint 4 "LARGEST_SET_SIZE prior to last device set:${LARGEST_SET_SIZE}"
  Vprint 4 "Last device set count: SET_COUNT=${SET_COUNT}"
  # If this is the smallest set so far, save that info.
  if [ ${SET_COUNT} -lt ${SMALLEST_SET_SIZE} ] ; then
    SMALLEST_SET_SIZE=${SET_COUNT}
    SMALLEST_SET_ID=${SET_ID_NUM}
    SMALLEST_SET_LIST="${SET_LIST}"
    Vprint 4 'Set SMALLEST_SET_*'
  fi
  if [ ${SET_COUNT} -gt ${LARGEST_SET_SIZE} ] ; then
    LARGEST_SET_SIZE=${SET_COUNT}
    LARGEST_SET_ID=${SET_ID_NUM}
    LARGEST_SET_LIST="${SET_LIST}"
    Vprint 4 'Set LARGEST_SET_*'
  fi
  Vprint 5 SMALLEST_SET_SIZE=${SMALLEST_SET_SIZE}
  Vprint 5 SMALLEST_SET_ID=${SMALLEST_SET_ID}
  Vprint 5 SMALLEST_SET_LIST=${SMALLEST_SET_LIST}
  Vprint 5 LARGEST_SET_SIZE=${LARGEST_SET_SIZE}
  Vprint 5 LARGEST_SET_ID=${LARGEST_SET_ID}
  Vprint 5 LARGEST_SET_LIST=${LARGEST_SET_LIST}
  SET_LAST_ID_NUM=${SET_ID_NUM}
  IF_LAST_INDEX=${IF_INDEX}
}

# Put_In_Set_Zero() is only called by Set_Zero_Setup().
function Put_In_Set_Zero()
{
  Vprint 4 ==== "Put_In_Set_Zero ${*}"
  SET_COUNT=1
  SETZERO_LIST=
  for THIS_ITEM in ${*}
  do
    K_NAME=`echo ${THIS_ITEM} | cut -f2 -d-`
    THIS_MAC=`echo ${THIS_ITEM} | cut -f1 -d-`
    THIS_DEC_MAC=`Mac_To_Dec ${THIS_MAC}`
    # Put it in set 0 and remove from existing set.
    #eval 'OLD_SET="${IF_MAC'${THIS_DEC_MAC}'_IN_SET}"'
    eval 'IF_MAC_'${THIS_DEC_MAC}'_IN_SET=0'
    eval 'SET_0_TOTAL="'${SET_COUNT}'"'
    SET_COUNT=$((${SET_COUNT} + 1))
    SETZERO_LIST="${SETZERO_LIST} ${THIS_ITEM}"
    #
  done
  # Move these set zero items to the first of the list.
  CHECK_LIST="${NEW_DEVICE_LIST}"
  NEW_DEVICE_LIST="${SETZERO_LIST}"
  for THIS_ITEM in ${CHECK_LIST}; do
    echo "${SETZERO_LIST}" | grep -q ${THIS_ITEM}
    [ ${?} -ne 0 ] && NEW_DEVICE_LIST="${NEW_DEVICE_LIST} ${THIS_ITEM}"
  done
  Vprint 4 Updated NEW_DEVICE_LIST="${NEW_DEVICE_LIST}"
}

function Set_Zero_Setup()
{
  Vprint 3 === Set_Zero_Setup ${AUTO_ETH0} ${FORCE_0_MAC} ${FORCE_1_MAC}
  ############################################################################
  # When forced eth0/eth1, see which set they are in, so they can be taken out
  # and put in set 0.
  Vprint 4 FORCE_0_DEC_MAC=${FORCE_0_DEC_MAC}
  Vprint 4 FORCE_1_DEC_MAC=${FORCE_1_DEC_MAC}
  eval 'ETH0_SET="${IF_MAC_'${FORCE_0_DEC_MAC}'_IN_SET}"'
  eval 'ETH1_SET="${IF_MAC_'${FORCE_1_DEC_MAC}'_IN_SET}"'
  FORCING=
  [ ! -z "${ETH0_SET}" ] && FORCING="${FORCE_0_MAC}-${FORCE_0_KNAME} "
  [ ! -z "${ETH1_SET}" ] && FORCING="${FORCING}${FORCE_1_MAC}-${FORCE_1_KNAME}"
  if [ ! -z "${FORCING}" ] ; then
    Vprint 4 ETH0_SET=${ETH0_SET}
    Vprint 4 ETH1_SET=${ETH1_SET}
    Put_In_Set_Zero ${FORCING}
  else
    # There is no forced eth0 or eth1, so when no preserved eth0 or eth1
    # find the smallest group and force that to be group 0.
    Vprint 4 ALREADY_PRESERVED_eth0=${ALREADY_PRESERVED_eth0}
    Vprint 4 ALREADY_PRESERVED_eth1=${ALREADY_PRESERVED_eth1}
    if [ -z "${ALREADY_PRESERVED_eth0}" -a -z "${ALREADY_PRESERVED_eth1}" ]
    then
      if [ "${AUTO_ETH0}" = "least-ports" ] ; then
        Vprint 5 SMALLEST_SET_ID=${SMALLEST_SET_ID}
        Vprint 4 Put these in the zero set: SMALLEST_SET_LIST=${SMALLEST_SET_LIST}
        Put_In_Set_Zero ${SMALLEST_SET_LIST}
        eval 'SET_'${SMALLEST_SET_ID}'_TOTAL=0'
      elif [ "${AUTO_ETH0}" = "most-ports" ] ; then
        Vprint 5 LARGEST_SET_ID=${LARGEST_SET_ID}
        Vprint 4 Put these in the zero set: LARGEST_SET_LIST=${LARGEST_SET_LIST}
        Put_In_Set_Zero ${LARGEST_SET_LIST}
        eval 'SET_'${LARGEST_SET_ID}'_TOTAL=0'
      fi
    fi
  fi
  INDEX=0
  while true ; do
      eval 'I="${SET_'${INDEX}'_INUM}"'
      eval 'T="${SET_'${INDEX}'_TOTAL}"'
      [ -z "${T}" -a -z "${I}" ] && break
      Vprint 4 SET_${INDEX}_INUM=${I}
      Vprint 4 SET_${INDEX}_TOTAL=${T}
      INDEX=$((${INDEX} + 1))
  done
}

# Write the preserved mappings to the DB
# If eth0 and/or eth1 are preserved, then save that special info.
function Do_Preserved()
{
    Vprint 3 === Do_Preserved
    for THIS_ITEM in ${PRESERVE_DEVICE_LIST}; do
        THIS_MAC=`echo ${THIS_ITEM} | cut -f1 -d-`
        THIS_DEC=`Mac_To_Dec ${THIS_MAC}`
        THIS_NAME=`echo ${THIS_ITEM} | cut -f2 -d-`
        eval 'K_NAME=${IF_MAC_'${THIS_DEC}'_KERNNAME}'
        if [ "_${K_NAME}" = "_" ] ; then
            Vprint 1 "-- MAC: ${THIS_MAC} Preserving Mapping to: ${THIS_NAME}"
        else
            Vprint 1 "-- MAC: ${THIS_MAC} Preserving Mapping ${K_NAME} to: ${THIS_NAME}"
        fi
        eval 'IF_MAC_'${THIS_DEC}'_TARGETNAME="'${THIS_NAME}'"'
        Vprint 4 IF_MAC_'${THIS_DEC}'_TARGETNAME=${THIS_NAME}
    done
}

# Group the ***NEW*** devices from the NEW_DEVICE_LIST string.
function Do_Grouping()
{
    Vprint 3 == Do_Grouping
    # Assign first from the first set, then the second, and so on.
    Vprint 3 Start assigning at eth0
    ETH_INDEX=0
    SET_INDEX=-1
    Vprint 5 OPT_GROUPS=${OPT_GROUPS}
    Vprint 4 === Startloop: SET_INDEX=${SET_INDEX} ETH_INDEX=${ETH_INDEX}
    while [ ${SET_INDEX} -le ${SET_LAST_ID_NUM} ] ; do
        SET_INDEX=$((${SET_INDEX} + 1))
        Vprint 4 === Toploop: SET_INDEX=${SET_INDEX} ETH_INDEX=${ETH_INDEX}
        eval 'RANGE="${SET_'${SET_INDEX}'_TOTAL}"'
        [ -z "${RANGE}" ] && continue
        [ "${RANGE}" -eq 0 ] && continue
        # Find ethN index so all ethN thru ethN+T are free.
        # When doing groupings, reserve 8.
        if [ "${OPT_GROUPS}" = "groups" ] ; then
            RANGE=8
            TT=`expr ${ETH_INDEX} / 10`
            ETH_INDEX=${TT}0
            [ "${ETH_INDEX}" = "00" ] && ETH_INDEX=0
        else
            eval 'RANGE="${SET_'${SET_INDEX}'_TOTAL}"'
        fi
        Vprint 4 === ETH_INDEX=${ETH_INDEX} RANGE=${RANGE}
        while true ; do
            EI=${ETH_INDEX}
            LIMIT=`expr ${EI} + ${RANGE}`
            OK=yes
            Vprint 5 ==== EI=${EI} LIMIT=${LIMIT}
            while [ ${EI} -lt ${LIMIT} ] ; do
                T_NAME=eth${EI}
                EI=$((${EI} + 1))
                echo "${PRESERVE_DEVICE_LIST} " | grep "${T_NAME} " > /dev/null
                RV=${?}
                if [ ${RV} -eq 0 ] ; then
                    Vprint 5 ==== In use: ${T_NAME}
                    OK=no
                    if [ "${OPT_GROUPS}" = "groups" ] ; then
                        ETH_INDEX=`expr ${ETH_INDEX} + 10`
                    else
                        ETH_INDEX=${EI}
                    fi
                    Vprint 4 === New ETH_INDEX=${ETH_INDEX}
                    break
                fi
            done
            [ "${OK}" = "yes" ] && break
        done
        Vprint 4 === ETH_INDEX=${ETH_INDEX}
        Vprint 3 Start assigning at eth${ETH_INDEX}
        for THIS_ITEM in ${NEW_DEVICE_LIST}; do
            THIS_MAC=`echo ${THIS_ITEM} | cut -f1 -d-`
            THIS_DEC=`Mac_To_Dec ${THIS_MAC}`
            eval 'SET_ID=${IF_MAC_'${THIS_DEC}'_IN_SET}'

            # Skip devices not in this group.
            [ ${SET_ID} -ne ${SET_INDEX} ] && continue
            Vprint 4 SET_ID=${SET_ID} THIS_DEC=${THIS_DEC}

            eval 'K_NAME=${IF_MAC_'${THIS_DEC}'_KERNNAME}'
            echo " ${SKIP_LIST} " | grep " ${K_NAME} "  > /dev/null
            if [ ${?} -eq 0 ] ; then
                eval 'MAC_ADDR="${IF_MAC_'${THIS_DEC}'_MAC}"'
                Vprint 1 "-- MAC: ${MAC_ADDR} Not mapping: ${K_NAME}"
                continue
            fi
            eval 'MAC_ADDR="${IF_MAC_'${THIS_DEC}'_MAC}"'
            # Set IF_MAC_*_TARGETNAME
            T_NAME=eth${ETH_INDEX}
            Vprint 1 "-- MAC: ${MAC_ADDR} Mapping from: ${K_NAME} to: ${T_NAME}"
            eval 'IF_MAC_'${THIS_DEC}'_TARGETNAME="'${T_NAME}'"'
            ETH_INDEX=$((${ETH_INDEX} + 1))
        done
        if [ "${OPT_GROUPS}" = "groups" ] ; then
            ETH_INDEX=`expr ${ETH_INDEX} + 10`
        fi
    done
}


#######################################################################
Save_Previous_Files
Verify_Stuff
Determine_Sets
Set_Zero_Setup
Vprint 1 "= Assigning interface names"
Do_Preserved
Do_Grouping
Clear_DB
Write_Mapping

if [ ${OPT_VERBOSE_LEVEL} -ge 5 ] ; then
  echo Env Settings----------------------------------------------------
  echo ^IF_=============================
  set | grep "^IF_"
  echo ^SET_=============================
  set | grep "^SET_"
  echo ETH=============================
  set | grep "ETH" | grep -v "^_" | grep -v "^ "
  echo _LIST=============================
  set | grep "_LIST" | grep -v "^_" | grep -v "^ "
fi
