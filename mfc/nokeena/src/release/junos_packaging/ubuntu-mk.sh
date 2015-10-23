:
# Set up environment to run host-utils/scripts/mk-mfc-pkg.sh
#
# First param is the installation image file to use as input for
# creating the JunOS style installation package.
#  E.g.  /data/archive/releases/pacifica/Ubuntu-hypervisor-12.04.3.img
# Second parm (optional): Value passed to host-utils/scripts/mk-mfc-pkg.sh
#   as third param. Use "prod", "dev", "default" or the host:port of the signing server.

SOURCE=${1}
SIGN_OPTION=${2}
TMP_ROOT=/build/tmp/build_root
TMP_DIR=${TMP_ROOT}/work_dir
RELEASEDIR=${TMP_ROOT}/releasedir

if [ -z "${SOURCE}" ] ; then
  echo Specify the source Ubuntu-hypervisor img file
  echo Optional second param: prod, dev, default, or the host:port of the signing server
  exit 1
fi
if [ ! -f "${SOURCE}" ] ; then
  echo Error, No such file ${SOURCE}
  exit 1
fi
if [ -z ${SIGN_OPTION} ] ; then
  SIGN_OPTION=default
fi

JUNOS_PACKAGING_TOP_DIR=`pwd`
if [ ! -f ${JUNOS_PACKAGING_TOP_DIR}/package-scripts/basic/DESC ] ; then
  echo Error, missing ${JUNOS_PACKAGING_TOP_DIR}/package-scripts/basic/DESC
  exit 1
fi
#
rm -rf ${TMP_ROOT}
mkdir ${TMP_ROOT}
if [ ! -d ${TMP_ROOT} ] ; then
  echo Failed to created ${TMP_ROOT}
  exit 1
fi
mkdir ${TMP_DIR}
if [ ! -d ${TMP_DIR} ] ; then
  echo Failed to created ${TMP_DIR}
  exit 1
fi

if [ ! -d ${RELEASEDIR} ] ; then
  mkdir ${RELEASEDIR}
fi
if [ ! -d ${RELEASEDIR} ] ; then
  echo Failed to created ${RELEASEDIR}
  exit 1
fi
THIS=`basename ${SOURCE} .img`
THAT=`echo ${THIS} | sed "/\(-[1-9]\)/s//-\1/"`
rm -f ${RELEASEDIR}/${THAT}*-signed*.tgz*
echo ${THIS} ${THAT}
#


export TMP_DIR
export JUNOS_PACKAGING_TOP_DIR
# Get the minimum JunOS version that we can install on.
export JUNOS_RELMAJOR=`grep MAJORVERSION JUNOS-RELEASE |awk -F = '{print $2}'`
export JUNOS_RELMINOR=`grep MINORVERSION JUNOS-RELEASE |awk -F = '{print $2}'`

cd ${TMP_ROOT}

# Extract the mx-mfc.img file from the .img file
unzip ${SOURCE}
pwd
ls -l
# Then extract all the files from mx-mfc.img into a tftpboot dir.
cd ${TMP_DIR}
mkdir tftpboot
unzip -d tftpboot ${TMP_ROOT}/mx-*.img
rm ${TMP_ROOT}/mx-*.img

# Update the elilo.conf file
# Change /var/db/ext/juniper/mfc/images/Ubuntu*/
#     to /packages/mnt/<pkgname>/boot/

cd tftpboot
cp elilo.conf /tmp/elilo.conf.before
cp elilo.conf tmp.file
cat tmp.file \
  | sed -e "s#contrail#ubuntu#" \
  | sed -e "s#Contrail#Ubuntu#" \
  | sed -e "s#/var/db/ext/juniper/.*/images/Ubuntu.*/#/packages/mnt/${THIS}/boot/#" \
  > elilo.conf
echo elilo.conf change:
diff tmp.file elilo.conf
echo elilo.conf contents:
cat elilo.conf
cp elilo.conf /tmp/elilo.conf.after
grep -q /var/db/ext/juniper elilo.conf
if [ ${?} -eq 0 ] ; then
  echo Error, did not properly update elilo.conf
  echo Fail
  exit 1
fi
rm tmp.file
echo Files in tftpboot:
ls -l

cd ${JUNOS_PACKAGING_TOP_DIR}

echo ===================mk-mfc-pkg.sh=================================
echo host-utils/scripts/mk-mfc-pkg.sh ${TMP_DIR}/tftpboot ${RELEASEDIR} ${SIGN_OPTION} UbuntuKVM
host-utils/scripts/mk-mfc-pkg.sh ${TMP_DIR}/tftpboot ${RELEASEDIR} ${SIGN_OPTION} UbuntuKVM
