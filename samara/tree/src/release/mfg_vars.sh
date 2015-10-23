#!/bin/sh

#
# 
#
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

usage()
{
    echo "Usage: $0 [-v] [-M MFG_ROOT]"

    exit 1
}

# In case we need to do more manufacturing stuff here, do not conflict 
# these options with manufacture.sh 's .

PARSE=`/usr/bin/getopt 'vM:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
if [ ! -z "${PROD_INSTALL_OUTPUT_DIR}" ]; then 
    MROOT=${PROD_INSTALL_OUTPUT_DIR}/image
else
    MROOT=
fi

while true ; do
    case "$1" in
        -v) VERBOSE=$((${VERBOSE}+1))
            shift ;;
        -M) MROOT=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

if [ -z "${MROOT}" ]; then
    usage
fi

if [ -f ${MROOT}/etc/build_version.sh ]; then
    . ${MROOT}/etc/build_version.sh
fi

# Define customer-specific graft functions
if [ -f ${MROOT}/etc/customer_rootflop.sh ]; then
    . ${MROOT}/etc/customer_rootflop.sh
fi

# Define optional feature-specific graft functions
if [ -f ${MROOT}/sbin/opt_feature_rootflop.sh ]; then
    . ${MROOT}/sbin/opt_feature_rootflop.sh
fi

# Graft point for models: append to/reset CFG_MODEL_CHOICES, CFG_MODEL_DEF
if [ "$HAVE_MANUFACTURE_GRAFT_1" = "y" ]; then
    manufacture_graft_1
fi
if [ "$HAVE_OPT_MANUFACTURE_GRAFT_1" = "y" ]; then
    opt_manufacture_graft_1
fi

## IL_LAYOUT=${CFG_LAYOUT}
## export IL_LAYOUT
##if [ -r ${MROOT}/etc/layout_settings.sh ]; then
##    . ${MROOT}/etc/layout_settings.sh
##fi

enabled_model_choices=
for model in ${CFG_MODEL_CHOICES}; do
    eval 'enabled="${MODEL_'${model}'_ENABLE}"'
    if [ "${enabled:-0}" -ne 1 ]; then
        continue
    fi
    enabled_model_choices="${enabled_model_choices}${enabled_model_choices:+ }${model}"
done

# Print the list of model choices, and their descriptions
echo "MFG_MODELS=\"${enabled_model_choices}\""
echo "export MFG_MODELS"
if [ ! -z "${CFG_MODEL_DEF}" ]; then
    echo "MFG_MODELS_DEFAULT=\"${CFG_MODEL_DEF}\""
    echo "export MFG_MODELS_DEFAULT"
fi
for model in ${enabled_model_choices}; do
    eval 'descr="${MODEL_'${model}'_DESCR}"'
    if [ ! -z "${descr}" ]; then
        echo "MFG_MODEL_${model}_DESCR=\"${descr}\""
        echo "export MFG_MODEL_${model}"
    fi
done

exit 0
