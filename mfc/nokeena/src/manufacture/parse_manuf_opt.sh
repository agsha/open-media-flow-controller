:
# This file is dotted in by any script that wants to parse
# the manufacture related options.  (pre-install, instal-mfc)

# Function to validate and parse manufacture related options
# When an invalid input, set:
#   INVALID_FLAG=invalid
#   INVALID_FLAG=unknown
# When valid, echo the setting on stdout and set matching global variables
# for each of the values or sub-values for each item.
OPT_DISK_LAYOUT=
OPT_RDISK_DEV=
OPT_RDISK_PDEV=
OPT_HDD2_DEV=
OPT_HDD2_PDEV=
OPT_EDISK_DEV=
OPT_EDISK_PDEV=
OPT_BOOT_FROM=
OPT_MAX_FS=
# BUF is for the /nkn filesystem partition 
OPT_BUF_ISIZE=
OPT_BUF_GW=
OPT_BUF_MAX=
# CACHE is for the cache raw partition + cache filesystem partition
OPT_CACHE_ISIZE=
OPT_CACHE_GW=
OPT_CACHE_MAX=
OPT_CACHEFS_PERCENT=
# LOG is for the /log filesystem partition
OPT_LOG_ISIZE=
OPT_LOG_GW=
OPT_LOG_MAX=
# COREDUMP is for the /coredump filesystem partition
OPT_COREDUMP_ISIZE=
OPT_COREDUMP_GW=
OPT_COREDUMP_MAX=
# ROOT is for the root filesystem size.
OPT_ROOT_ISIZE=
OPT_ROOT_GW=
OPT_ROOT_MAX=
# CONFIG is for the config filesystem size.
OPT_CONFIG_ISIZE=
OPT_CONFIG_GW=
OPT_CONFIG_MAX=
# VAR is for the var filesystem size.
OPT_VAR_ISIZE=
OPT_VAR_GW=
OPT_VAR_MAX=

Help_Manuf_Opt()
{
  echo Special options supported:
  if [ "_${1}" = "_all" ] ; then
    echo disk-layout='<disk_layout_name>'
    echo root-dev='sd*|hd*|cciss|c0d*|/dev/...'
    echo hdd2-dev='sd*|hd*|cciss|c0d*|/dev/...'
    echo eusb-dev='sd*|/dev/...'
    echo boot-from='eusb|mirror|root'
    echo 'mirror-boot-ok| root-boot-ok'
    echo max-fs='<max_filesystem_size_MB>'
    echo coredump-part='<initial_size_MB>[-<growth_weight>[-<max_size>]]'
    echo root-part='<initial_size_MB>[-<growth_weight>[-<max_size>]]'
    echo config-part='<initial_size_MB>[-<growth_weight>[-<max_size>]]'
    echo var-part='<initial_size_MB>[-<growth_weight>[-<max_size>]]'
  fi
  echo buf-part='<initial_size_MB>[-<growth_weight>[-<max_size>]]'
  echo cache-part='<initial_size_MB>[-<growth_weight>[-<max_size>[-<fs_percent>]]]'
  echo log-part='<initial_size_MB>[-<growth_weight>[-<max_size>]]'
}

Parse_Manuf_Opt()
{
  INVALID_FLAG=
  ERR_MSG=
  ERR2_MSG=
  PARSE_OUT=
  ITEM="${1}"
  case "_${ITEM}" in
  _)   INVALID_FLAG=invalid ; return ;;
  *\ *) INVALID_FLAG=invalid ; return ;;
  *\>*) INVALID_FLAG=invalid ; return ;;
  *\|*) INVALID_FLAG=invalid ; return ;;
  *\"*) INVALID_FLAG=invalid ; return ;;
  *\'*) INVALID_FLAG=invalid ; return ;;
  esac
  VAL=`echo ${ITEM} | cut -f2 -d=`
  case ${ITEM} in
  disk-layout=) 
    OPT_DISK_LAYOUT=
    PARSE_OUT=
    return
    ;;
  disk-layout=*) 
    Get_Model_Choices
    # Allow the "nocache" model, which in pre-install is changed to "normal".
    echo " nocache ${CFG_MODEL_CHOICES} " | grep " ${VAL} " > /dev/null
    if [ ${?} -ne 0 ] ; then
      INVALID_FLAG=invalid
      ERR_MSG="Choices are: ${CFG_MODEL_CHOICES}"
    else
      OPT_DISK_LAYOUT=${VAL}
    fi
    ;;
  root-dev=|root-dev=na)
    OPT_RDISK_DEV=
    OPT_RDISK_PDEV=
    PARSE_OUT=
    return
    ;;
  root-dev=*)
    case ${VAL} in 
    hd*)
      OPT_RDISK_DEV=/dev/${VAL}
      OPT_RDISK_PDEV=${OPT_RDISK_DEV}
      ;;
    sd*)
      OPT_RDISK_DEV=/dev/${VAL}
      OPT_RDISK_PDEV=${OPT_RDISK_DEV}
      ;;
    cciss)
      OPT_RDISK_DEV=/dev/cciss/c0d0
      OPT_RDISK_PDEV=${OPT_RDISK_DEV}p
      ;;
    c0d*)
      OPT_RDISK_DEV=/dev/cciss/${VAL}
      OPT_RDISK_PDEV=${OPT_RDISK_DEV}p
      ;;
    /dev/*\;*)
      # A semicolon can be used to specify the pdev when
      # it is not the same as the root device name.
      # After the command can be the full device name,
      # or the string to append to the root device name.
      OPT_RDISK_DEV=`echo  "${VAL}" | cut -f1 -d';'`
      OPT_RDISK_PDEV=`echo "${VAL}" | cut -f2 -d';'`
      case "_${OPT_RDISK_PDEV}" in
      _/*) ;;
      *) OPT_RDISK_PDEV="${OPT_RDISK_DEV}${OPT_RDISK_PDEV}" ;;
      esac
      echo "${OPT_RDISK_DEV};${OPT_RDISK_PDEV}"
      echo
      ;;
    /dev/*)
      OPT_RDISK_DEV="${VAL}"
      OPT_RDISK_PDEV=${OPT_RDISK_DEV}
      ;;
    *)
      INVALID_FLAG=invalid
      ERR_MSG="Choices are hda, sda, cciss, /dev/..."
      ;;
    esac
    ;;
  hdd2-dev=|hdd2-dev=na)
    OPT_HDD2_DEV=
    OPT_HDD2_PDEV=
    PARSE_OUT=
    return
    ;;
  hdd2-dev=*)
    case ${VAL} in 
    hd*)
      OPT_HDD2_DEV=/dev/${VAL}
      OPT_HDD2_PDEV=${OPT_HDD2_DEV}
      ;;
    sd*)
      OPT_HDD2_DEV=/dev/${VAL}
      OPT_HDD2_PDEV=${OPT_HDD2_DEV}
      ;;
    cciss)
      OPT_HDD2_DEV=/dev/cciss/c0d0
      OPT_HDD2_PDEV=${OPT_HDD2_DEV}p
      ;;
    c0d*)
      OPT_HDD2_DEV=/dev/cciss/${VAL}
      OPT_HDD2_PDEV=${OPT_HDD2_DEV}p
      ;;
    /dev/*\;*)
      # A semicolon can be used to specify the pdev when
      # it is not the same as the root device name.
      # After the command can be the full device name,
      # or the string to append to the root device name.
      OPT_HDD2_DEV=`echo  "${VAL}" | cut -f1 -d';'`
      OPT_HDD2_PDEV=`echo "${VAL}" | cut -f2 -d';'`
      case "_${OPT_HDD2_PDEV}" in
      _/*) ;;
      *) OPT_HDD2_PDEV="${OPT_HDD2_DEV}${OPT_HDD2_PDEV}" ;;
      esac
      echo "${OPT_HDD2_DEV};${OPT_HDD2_PDEV}"
      echo
      ;;
    /dev/*)
      OPT_HDD2_DEV="${VAL}"
      OPT_HDD2_PDEV=${OPT_HDD2_DEV}
      ;;
    *)
      INVALID_FLAG=invalid
      ERR_MSG="Choices are hda, sda, cciss, /dev/..."
      ;;
    esac
    ;;
  eusb-dev=|eusb-dev=na)
    OPT_EDISK_DEV=
    PARSE_OUT=
    return
    ;;
  eusb-dev=*)
    case ${VAL} in 
    sd*)    OPT_EDISK_DEV=/dev/${VAL} ;;
    /dev/*) OPT_EDISK_DEV="${VAL}" ;;
    *)
      INVALID_FLAG=invalid
      ERR_MSG="Choices are sd*, /dev/..."
      ;;
    esac
    OPT_EDISK_PDEV=${OPT_EDISK_DEV}
    ;;
  boot-from=)
    OPT_BOOT_FROM=
    PARSE_OUT=
    return
    ;;
  boot-from=*)
    case ${VAL} in
    eusb|root|mirror) OPT_BOOT_FROM=${VAL} ;;
    *)
      INVALID_FLAG=invalid
      ERR_MSG="Choices are: eusb, root, mirror"
      ;;
    esac
    ;;
  max-fs=)
    OPT_MAX_FS=
    PARSE_OUT=
    return
    ;;
  max-fs=*)
    ERR_MSG="Must specify a number greater than 100. (in MB)"
    case ${VAL} in
    *[A-Za-z]) INVALID_FLAG=invalid ;;
    [1-9][0-9][0-9]*)
      expr 1 + "${VAL}" > /dev/null 2>&1
      [ ${?} -ne 0 ] && INVALID_FLAG=invalid
      [ -z "${INVALID_FLAG}" ] && OPT_MAX_FS=${VAL}
      ;;
    *) INVALID_FLAG=invalid ;;
    esac
    ;;
  buf-part=)
    OPT_BUF_ISIZE=
    PARSE_OUT=
    return
    ;;
  cache-part=)
    OPT_CACHE_ISIZE=
    PARSE_OUT=
    return
    ;;
  log-part=)
    OPT_LOG_ISIZE=
    PARSE_OUT=
    return
    ;;
  coredump-part=)
    OPT_COREDUMP_ISIZE=
    PARSE_OUT=
    return
    ;;
  root-part=)
    OPT_ROOT_ISIZE=
    PARSE_OUT=
    return
    ;;
  config-part=)
    OPT_CONFIG_ISIZE=
    PARSE_OUT=
    return
    ;;
  var-part=)
    OPT_VAR_ISIZE=
    PARSE_OUT=
    return
    ;;
  cache-part=0)
    OPT_CACHE_ISIZE=0
    ;;
  buf-part=*|cache-part=*|log-part=*|coredump-part=*|root-part=*|config-part=*|var-part=*)
    # Syntax:  <initial_size_MB>-<growth_weight>-<max_size_MB>
    # The cache-part setting can also have appened "-<cachefs_percent>"
    case ${VAL} in
    *[A-Za-z])        INVALID_FLAG=invalid ;;
    [1-9][0-9][0-9]*) ;;
    *)                INVALID_FLAG=invalid ;;
    esac
    V1=
    V2=
    V3=
    V4=
    if [ -z "${INVALID_FLAG}" ] ; then
      # Now parse out the up to four values
      case _${VAL} in
      _[1-9]*-[1-9]*-[1-9]*-[1-9]*)
        V1=`echo ${VAL} | cut -f1 -d-`
        V2=`echo ${VAL} | cut -f2 -d-`
        V3=`echo ${VAL} | cut -f3 -d-`
        V4=`echo ${VAL} | cut -f4 -d-`
        [ ${V3} -le ${V1} ] && INVALID_FLAG=invalid
        # When the percent is specified, it must not be over 25.
        if [ ${V4} -gt 25 ] ; then
          INVALID_FLAG=invalid
          ERR2_MSG="Percent value must not be over 25"
        fi
        ;;
      _[1-9]*-[1-9]*-[1-9]*)
        V1=`echo ${VAL} | cut -f1 -d-`
        V2=`echo ${VAL} | cut -f2 -d-`
        V3=`echo ${VAL} | cut -f3 -d-`
        [ ${V3} -le ${V1} ] && INVALID_FLAG=invalid
        ;;
      _[1-9]*-[1-9]*)
        V1=`echo ${VAL} | cut -f1 -d-`
        V2=`echo ${VAL} | cut -f2 -d-`
        ;;
      _[1-9]*)
        V1=`echo ${VAL} | cut -f1 -d-`
        ;;
      _)
        ;;
      *)
        INVALID_FLAG=invalid
        ;;
      esac
    fi
    if [ ! -z "${V1}" ] ; then
      expr 1 + "${V1}" > /dev/null 2>&1
      [ ${?} -ne 0 ] && INVALID_FLAG=invalid
    fi
    if [ ! -z "${V2}" ] ; then
      expr 1 + "${V2}" > /dev/null 2>&1
      [ ${?} -ne 0 ] && INVALID_FLAG=invalid
    fi
    if [ ! -z "${V3}" ] ; then
      expr 1 + "${V3}" > /dev/null 2>&1
      [ ${?} -ne 0 ] && INVALID_FLAG=invalid
    fi
    if [ ! -z "${V4}" ] ; then
      expr 1 + "${V4}" > /dev/null 2>&1
      [ ${?} -ne 0 ] && INVALID_FLAG=invalid
    fi
    if [ -z "${INVALID_FLAG}" ] ; then
      case ${ITEM} in
      buf-part=*)
        OPT_BUF_ISIZE=${V1}
        OPT_BUF_GW=${V2}
        OPT_BUF_MAX=${V3}
        ;;
      cache-part=*)
        OPT_CACHE_ISIZE=${V1}
        OPT_CACHE_GW=${V2}
        OPT_CACHE_MAX=${V3}
        [ -z "${V4}" ] && V4=10
        OPT_CACHEFS_PERCENT=${V4}
        ;;
      log-part=*)
        OPT_LOG_ISIZE=${V1}
        OPT_LOG_GW=${V2}
        OPT_LOG_MAX=${V3}
        ;;
      coredump-part=*)
        OPT_COREDUMP_ISIZE=${V1}
        OPT_COREDUMP_GW=${V2}
        OPT_COREDUMP_MAX=${V3}
        ;;
      root-part=*)
        OPT_ROOT_ISIZE=${V1}
        OPT_ROOT_GW=${V2}
        OPT_ROOT_MAX=${V3}
        ;;
      config-part=*)
        OPT_CONFIG_ISIZE=${V1}
        OPT_CONFIG_GW=${V2}
        OPT_CONFIG_MAX=${V3}
        ;;
      var-part=*)
        OPT_VAR_ISIZE=${V1}
        OPT_VAR_GW=${V2}
        OPT_VAR_MAX=${V3}
        ;;
      esac
    else 
      ERR_MSG='Setting syntax:  <initial_size_MB>[-<growth_weight>[-<max_size_MB>]]'
      case ${ITEM} in
      cache-part=*)
        ERR_MSG='Setting syntax:  <initial_size_MB>[-<growth_weight>[-<max_size_MB>[-fs_percent]]]'
        ;;
      esac
    fi
    ;;
  *) INVALID_FLAG=unknown
  esac
  if [ -z "${INVALID_FLAG}" ] ; then
    PARSE_OUT=${ITEM}
  fi
}

Get_Model_Choices()
{
  if [ ! -f /etc/customer_rootflop.sh ] ; then
    CFG_MODEL_CHOICES=" normal cache vxa1 vxa2 cloudvm cloudrc "
  else
    eval `grep CFG_MODEL_CHOICES='"*"' /etc/customer_rootflop.sh`
  fi
}
