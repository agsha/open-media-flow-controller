#!/bin/sh
#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2011 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

# Dumps DMI SMBIOS information (using dmidecode) in either sh or text
# form (use -t for text).

DMIDECODE=/usr/sbin/dmidecode
VAR_PREFIX="SMBIOS_"

# XXX #dep/parse: dmidecode
ALL_STRINGS_LIST='bios-vendor BIOS vendor
  bios-version BIOS version
  bios-release-date BIOS release date
  system-manufacturer System manufacturer
  system-product-name System product name
  system-version System version
  system-serial-number System serial number
  system-uuid System UUID
  baseboard-manufacturer Baseboard manufacturer
  baseboard-product-name Baseboard product name
  baseboard-version Baseboard version
  baseboard-serial-number Baseboard serial number
  baseboard-asset-tag Baseboard asset tag
  chassis-manufacturer Chassis manufacturer
  chassis-type Chassis type
  chassis-version Chassis version
  chassis-serial-number Chassis serial number
  chassis-asset-tag Chassis asset tag
  processor-family Processor family
  processor-manufacturer Processor manufacturer
  processor-version Processor version
  processor-frequency Processor frequency'

usage()
{
    echo "usage: $0    [-s] [-z] [-e FIELD-NAME] [-l]"
    echo "usage: $0 -t [-f] [-z] [-e FIELD-NAME] [-l]"
    echo ""
    echo "-t : text mode (default is -s , shell mode)"
    echo "-f : if in text mode, should we use friendly names"
    echo "-z : should names with an empty value be output"
    echo "-e FIELD-NAME : display only the named field (like \"system-uuid\")"
    echo "-l : print just the value, no name"
    echo ""
    exit 1
}

VERBOSE=0

GETOPT_OPTS='stfze:lvq'
PARSE=`/usr/bin/getopt -s sh $GETOPT_OPTS "$@"`
if [ $? != 0 ] ; then
    echo "Error from getopt"
    usage
fi

eval set -- "$PARSE"


# mode can be "sh" or "text"
MODE_SH=1
TEXT_FRIENDLY=0
SUPPRESS_EMPTY_VALUES=0
DUMP_ALL=1
DUMP_FIELDS=
PRINT_STYLE=name_value

while true ; do
    case "$1" in
        -s) MODE_SH=1; shift ;;
        -t) MODE_SH=0; shift ;;
        -f) TEXT_FRIENDLY=1; shift ;;
        -z) SUPPRESS_EMPTY_VALUES=1; shift ;;
        -e) DUMP_ALL=0; DUMP_FIELDS="${DUMP_FIELDS} `echo $2 | sed s/^${VAR_PREFIX}//`"; shift 2 ;;
        -l) PRINT_STYLE=value ; shift ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -q) VERBOSE=-1; shift ;;
        --) shift ; break ;;

        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

if [ "${MODE_SH}" = "1" -a "${TEXT_FRIENDLY}" = "1" ]; then
    TEXT_FRIENDLY=0
fi

if [ ! -x "${DMIDECODE}" ]; then
    echo "Could not find ${DMIDECODE}"
    echo "dmidump.sh not supported on this platform"
    exit 2
fi

# Get just the string names
STRINGS_LIST=`echo "${ALL_STRINGS_LIST}" | awk '{print $1}'`
# Gets the "friendly names" if we need them
if [ "${TEXT_FRIENDLY}" = "1" ]; then
    STRINGS_LIST_ASSOC=`echo "${ALL_STRINGS_LIST}" | awk '{gsub(/[^a-zA-Z0-9]/,"_",$1); printf "STRINGS_LIST_FRIENDLY_"  toupper($1) "=\""; for (i=2; i<=NF; i++) { printf $i ; if (i!=NF) printf " " } print "\""}'`
    eval "${STRINGS_LIST_ASSOC}"
fi

if [ "${MODE_SH}" = "0" ]; then
    # Get the max string length, so we can pad the names below
    pad_len=`echo "$STRINGS_LIST" | tr -s ' ' | sed 's/ /\n/g' | wc -L`
    pad_len="$((${pad_len}+10))"
fi

if [ "${DUMP_ALL}" = "1" ]; then
    PRINT_STRINGS="${STRINGS_LIST}"
else
    PRINT_STRINGS=`echo "${DUMP_FIELDS}" | tr '[A-Z]' '[a-z]' | sed 's/_/-/g'`
fi

for string in $PRINT_STRINGS ; do

    # The sed below is to escape: $ ` " \  using \ .  These 4
    # characters are what need escaping inside " in sh/bash .

    # XXX #dep/parse: dmidecode
    value="$(${DMIDECODE} --string $string | head -1 | sed 's/\([$\`"\\]\)/\\\1/g')"

    if [ "${value}" = "Not Specified" -o \
         "${value}" = "<OUT OF SPEC>" -o \
         \( "$string" = "system-uuid" -a \( "${value}" = "Not Settable" -o "${value}" = "Not Present" \) \) -o \
         "${value}" = " " ]; then
        value=""
    fi

    if [ "${SUPPRESS_EMPTY_VALUES}" = "1" -a -z "$value" ]; then
        continue
    fi

    if [ "${MODE_SH}" = "1" ]; then
        # Emit our name="value" , with the name in upper case
        val_name="$(echo ${VAR_PREFIX}$string | tr '[a-z]' '[A-Z]' | sed 's/[^A-Z0-9_]/_/g')"
        if [ "${PRINT_STYLE}" = "name_value" ]; then
            echo "${val_name}=\"${value}\""
        elif [ "${PRINT_STYLE}" = "value" ]; then
            echo "${value}"
        fi
    else
        if [ "${TEXT_FRIENDLY}" = "1" ]; then
            vns="$(echo STRINGS_LIST_FRIENDLY_$string | tr '[a-z]' '[A-Z]' | sed 's/[^A-Z0-9_]/_/g')"
            eval 'val_name="$'$vns'"'
        else
            val_name="$(echo ${VAR_PREFIX}$string | tr '[a-z]' '[A-Z]' | sed 's/[^A-Z0-9_]/_/g')"
        fi
        if [ "${PRINT_STYLE}" = "name_value" ]; then
            printf "%-${pad_len}s %s\n" "${val_name}:" "${value}"
        elif [ "${PRINT_STYLE}" = "value" ]; then
            echo "${value}"
        fi
    fi
done

exit 0
