#!/bin/sh

PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin
export PATH

usage()
{
    echo "usage: $0 [-k] [-m] [-a] [-O] [-r]"
    echo "  -k  Also copy kernel and modules"
    echo "  -m  Also copy MO files (compiled gettext string catalogs)"
    echo "  -o  remount / read-only after copying (default)"
    echo "  -O  do NOT remount / read-only after copying"
    echo "  -r  Restart pm after the binaries are copied"
    echo "  -R  do NOT restart pm after the binaries are copied (default)"
    exit 1
}

PARSE=`/usr/bin/getopt 'kmaor' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

DO_KERNEL_CP=no
DO_MO_CP=no
DO_REMOUNT_RO=yes
DO_PM_RESTART=no

while true ; do
    case "$1" in
        -k) DO_KERNEL_CP=yes; shift 1 ;;
        -m) DO_MO_CP=yes; shift 1 ;;
        -o) DO_REMOUNT_RO=yes; shift 1 ;;
        -O) DO_REMOUNT_RO=no; shift 1 ;;
        -r) DO_PM_RESTART=yes; shift 1 ;;
        -R) DO_PM_RESTART=no; shift 1 ;;
        --) shift ; break ;;
        *) echo "dobincp.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

# The account 'root' is used because otherwise you could have trouble
# replacing parts of the mgmt system itself (as admin's shell is the cli).
# To make a root account on your box do: "username root nopassword"

if [ -z "${TARGET_HOST}" -a -z "${TARGET_HOSTS}" ]; then
    echo "ERROR: first set one of the TARGET_HOST or TARGET_HOSTS environment variables"
    exit 1
fi

if [ ! -z "${TARGET_HOSTS}" ]; then
    all_targets=${TARGET_HOSTS}
else
    all_targets=${TARGET_HOST}
fi

if [ -z "${PROD_PRODUCT}" ]; then
    # export PROD_PRODUCT=juniper
    export PROD_PRODUCT=nokeena
fi
export PROD_PRODUCT_LC=`echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]'`

if [ -z "${PROD_TARGET_ARCH}" ]; then
    PROD_TARGET_ARCH=X86_64
fi
PROD_TARGET_ARCH_LC=`echo ${PROD_TARGET_ARCH} | tr '[A-Z]' '[a-z]'`

if [ -z "${IMAGE_DIR}" ]; then
    if [ -z "${PROD_OUTPUT_ROOT}" ]; then
        echo "ERROR: IMAGE_DIR and PROD_OUTPUT_ROOT environment variables are NOT set."
        echo Either set IMAGE_DIR to the full path to the product-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}/install/image dir
        echo or set PROD_OUTPUT_ROOT to the directory where the dir product-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC} is.
        exit 1
    fi
    if [ ! -d ${PROD_OUTPUT_ROOT} ] ; then
        echo Error, PROD_OUTPUT_ROOT directory does not exist: ${PROD_OUTPUT_ROOT}
        exit 1
    fi
    IMAGE_DIR=${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}/install/image
fi
if [ ! -d ${IMAGE_DIR} ] ; then
    echo Error, image directory does not exist: ${IMAGE_DIR}
    echo Either set IMAGE_DIR to the full path to the product-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}/install/image dir
    echo or set PROD_OUTPUT_ROOT to the directory where the dir product-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC} is.
    exit 1
fi

REMOTE_USER=root

echo "== Installing binaries for ${PROD_PRODUCT} to targets: ${all_targets}"

for target_host in ${all_targets}; do

    # If your box is behind a tunnel / firewall, you may need to set this
    if [ -z "${TARGET_SSH_PORT}" -o "${TARGET_SSH_PORT}" = "22" ]; then
        TARGET_SSH_PORT_STRING=""
        friendly_target_name="${target_host}"
    else
        TARGET_SSH_PORT_STRING="-p ${TARGET_SSH_PORT}"
        friendly_target_name="${target_host} port ${TARGET_SSH_PORT}"
    fi

    SYNC_TARGET=${REMOTE_USER}@${target_host}

    echo "-- Copying binaries to ${friendly_target_name}"

    ssh ${SYNC_TARGET} "mount -o remount,rw /"

    echo "-- Copying from ${IMAGE_DIR}/opt to ${friendly_target_name}"
    rsync      -e "ssh ${TARGET_SSH_PORT_STRING}" -cavz ${IMAGE_DIR}/opt/ ${SYNC_TARGET}:/opt
    echo "-- Copying from ${IMAGE_DIR}/lib to ${friendly_target_name}"
    rsync      -e "ssh ${TARGET_SSH_PORT_STRING}" -cavz ${IMAGE_DIR}/lib/ ${SYNC_TARGET}:/lib
    echo "-- Copying from ${IMAGE_DIR}/nkn/plugins to ${friendly_target_name}"
    rsync      -e "ssh ${TARGET_SSH_PORT_STRING}" -cavz ${IMAGE_DIR}/nkn/plugins/ ${SYNC_TARGET}:/nkn/plugins

    if [ "${DO_KERNEL_CP}" = "yes" ]; then
        echo "-- Copying kernel and modules to ${friendly_target_name}"
        rsync -e "ssh ${TARGET_SSH_PORT_STRING}" -cavz ${IMAGE_DIR}/lib/modules/ ${SYNC_TARGET}:/lib/modules

        ssh ${SYNC_TARGET} "mount -o remount,rw /boot"
        rsync -e "ssh ${TARGET_SSH_PORT_STRING}" -cavz ${IMAGE_DIR}/image/boot/ ${SYNC_TARGET}:/boot
    fi

    if [ "${DO_MO_CP}" = "yes" ]; then
        echo "-- Copying MO files to ${friendly_target_name}"
        rsync -e "ssh ${TARGET_SSH_PORT_STRING}" -cavz ${IMAGE_DIR}/usr/share/locale/ ${SYNC_TARGET}:/usr/share/locale
    fi

    echo "-- Fixing file permissions on ${friendly_target_name}"
    ssh ${SYNC_TARGET} "chown 0.0 /opt/tms/lib/web/cgi-bin/launch; chmod 4755 /opt/tms/lib/web/cgi-bin/launch"

    if [ "${DO_REMOUNT_RO}" = "yes" ]; then
        ssh ${SYNC_TARGET} "mount -o remount,ro /"

        if [ "${DO_KERNEL_CP}" = "yes" ]; then
            ssh ${SYNC_TARGET} "mount -o remount,ro /boot"
        fi
    fi

    if [ "${DO_PM_RESTART}" = "yes" ]; then
        ssh ${SYNC_TARGET} "service pm restart"
    fi

done

echo "-- Done"
