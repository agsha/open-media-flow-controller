#!/bin/bash
# Set up environment to run host-utils/scripts/mk-mfc-pkg.sh
# Parameters:
# 1: The release/image directory, where the input as-mlc-*.img file is
#    and the output files are put.
# 2: (Optional) A temporary directory to use, which must already exist.
# 3: (Optional) Value passed to host-utils/scripts/mk-mfc-pkg.sh as third param.
#           Use "default", "local", the host:port of the signing server.

ME=`basename ${0}`

Usage () {
  echo "${ME} <release/image dir> [<tmp dir>]"
}
#http://bclary.com/blog/2006/07/20/pipefail-testing-pipeline-exit-codes/
#set -o pipefail

if [ ${#} -lt 1 -a ${#} -gt 3 ] ; then
  Usage
  exit 1
fi
if [ ! -d ${1} ] ; then
  echo First parameter is not a directory name.
  Usage
  exit 1
fi
RELEASEDIR=${1}
PAC_IMG=`ls "${1}"/as-mlc-*.img`
if [ ! -f "${PAC_IMG}" ] ; then
  echo "Error, No as-mlc-*.img in ${1}"
  Usage
  exit 1
fi
REMOVE_TMP_DIR=no
if [ ${#} -ge 2 -a "${2}" != "_-" ] ; then
  if [ ! -d "${2}" ] ; then
    echo "Second parameter is not a directory name. ${2}"
    Usage
    exit 1
  fi
  TMP_DIR="${2}"
else
  TMP_DIR=`pwd`/tmp.$$.tmp
  if [ ! -d "${TMP_DIR}" ] ; then
    REMOVE_TMP_DIR=yes
    mkdir "${TMP_DIR}"
    if [ ! -d "${TMP_DIR}" ] ; then
      echo "Error, failed to create ${TMP_DIR}"
      exit 1
    fi
  fi
fi
SIGN_OPTION=
if [ ${#} -ge 3 ] ; then
  SIGN_OPTION=${3}
fi


export TMP_DIR
export JUNOS_PACKAGING_TOP_DIR=`pwd`
# Get the minimum JunOS version that we can install on.
export JUNOS_RELMAJOR=`grep MAJORVERSION JUNOS-RELEASE |awk -F = '{print $2}'`
export JUNOS_RELMINOR=`grep MINORVERSION JUNOS-RELEASE |awk -F = '{print $2}'`

echo ${ME}:
echo RELEASEDIR=${RELEASEDIR}
echo JUNOS_PACKAGING_TOP_DIR=${JUNOS_PACKAGING_TOP_DIR}

MY_DIR=`pwd`

cd "${TMP_DIR}"
# Extract the mx-mfc.img file from as-mlc-*.img 
unzip ${PAC_IMG} mx-mfc.img
# Then extract all the files from mx-mfc.img into a tftpboot dir.
mkdir tftpboot
unzip -d tftpboot mx-mfc.img
rm mx-mfc.img
# Update the elilo.conf file
cd tftpboot
cp elilo.conf tmp.file
cat tmp.file \
  | sed -e "s#/var/db/ext/juniper/mfc/images/#/var/tftpboot/#" \
  > elilo.conf
diff tmp.file elilo.conf
rm tmp.file
echo Contents of tftpboot:
ls -l 

cd ${MY_DIR}
echo ===================mk-mfc-pkg.sh=================================
echo host-utils/scripts/mk-mfc-pkg.sh ${TMP_DIR}/tftpboot ${RELEASEDIR} ${SIGN_OPTION}
host-utils/scripts/mk-mfc-pkg.sh ${TMP_DIR}/tftpboot ${RELEASEDIR} ${SIGN_OPTION}
RV=${?}
[ ${RV} -ne 0 ] && echo Error, mk-mfc-pkg.sh returned ${RV}

rm -rf "${TMP_DIR}"/tftpboot
if [ "${REMOVE_TMP_DIR}" = "yes" ] ; then
  cd "${MY_DIR}"
  rm -rf "${TMP_DIR}"
fi
exit ${RV}
