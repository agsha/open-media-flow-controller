:
# Called like this: ./install_pkg.sh ${EXTERNAL_PKG_DIR} ${TOP_INSTALL_DIR}/image
# Parameter 1: Directory where pkg subdirs are.
#   Each pkg subdir has a "pkg.info" file and the pkg file.
# Parameter 2: Where to extract the pkg file to.

PKG_DIR=${1}
INSTALL_ROOT=${2}
if [ -z ${PKG_DIR} ] ; then
  echo Error, input dir not specified.
  exit 1
fi
if [ ! -d ${PKG_DIR} ] ; then
  echo Error, input dir does not exist: ${PKG_DIR}
  exit 1
fi
if [ -z ${INSTALL_ROOT} ] ; then
  echo Error, output dir not specified.
  exit 1
fi
if [ ! -d ${INSTALL_ROOT} ] ; then
  echo Error, output dir does not exist: ${INSTALL_ROOT}
  exit 1
fi

case ${PKG_DIR} in
/*) ;;
*) PKG_DIR=`pwd`/${PKG_DIR} ;;
esac

COUNT=0
for INFO_FILE in "${PKG_DIR}"/*/pkg.info
do
  if [ ! -f ${INFO_FILE} ] ; then
    echo Error, no file ${INFO_FILE}
    exit 1
  fi
  COUNT=$((COUNT + 1))
  THIS_DIR=`dirname ${INFO_FILE}`

  PKG_NAME=
  PKG_FROM_URL=
  PKG_MD5SUM=
  TO_DIR=
  TP_LICENSE_URL=
  TP_ATTRIBUTES_URL=
  TP_SOURCE_URL=
  EXCLUDE=
  PKG_FILE=
  PKG_MD5SUM=
  CHECKSUM_FILE=
  LICENSE_FILE=
  ATTRIBUTES_FILE=
  SOURCE_FILE=
  . ${INFO_FILE}
  ERR=0
  [ -z "${PKG_FILE}" ] && ERR=1
  [ -z "${TO_DIR}" ] && ERR=1
  if [ ${ERR} -ne 0 ] ; then
    echo "Syntax error in ${INFO_FILE}"
    exit 1
  fi
  case ${TO_DIR} in
  /*) TOP_DIR="${INSTALL_ROOT}${TO_DIR}"  ;;
  *)  TOP_DIR="${INSTALL_ROOT}/${TO_DIR}" ;;
  esac
  # Now extract the pkg file where indicated. 
  [ ! -d "${TOP_DIR}" ] && mkdir -p "${TOP_DIR}"
  cd "${TOP_DIR}"
  case ${PKG_FILE} in
  *.tgz|*.tar.gz) tar xvfz ${THIS_DIR}/${PKG_FILE} ; RV=${?} ;;
  *.tar)          tar xvf  ${THIS_DIR}/${PKG_FILE} ; RV=${?} ;;
  *.cpio) cpio -idumv < ${THIS_DIR}/${PKG_FILE} ; RV=${?} ;;
  *.rpm) rpm2cpio ${THIS_DIR}/${PKG_FILE} | cpio -idumv ; RV=${?}  ;;
  *) echo Unsupported package type ${PKG_FILE} ; RV=1 ;;
  esac
  if [ ${RV} -ne 0 ] ; then
    echo Extraction failed
    exit 1
  fi
  # TBD ?
  # TBD, Install the license, attribute and source package file?
  # TBD ?
done
echo Processed ${COUNT}
if [ ${COUNT} -eq 0 ] ; then
  echo Error, no packages processed.
  exit 1
fi
exit 0

# Example contents of a pkg.info file:
PKG_NAME=jcf-gw
PKG_FROM_URL=http://ssd-git-01:8081/nexus/content/repositories/JVAE2-0/jcf-gw/D1.0.1-x86_64-linux/jcf-gw-D1.0.1-x86_64-linux.tgz
#PKG_MD5SUM=cadce42316cbd5b9563847326371dcfd
TO_DIR=/opt
PKG_FILE=jcf-gw-D1.0.1-x86_64-linux.tgz
[D@lab-2 external]$ cat /tmp/jfc-gw/*.info
PKG_NAME=jcf-gw
PKG_FROM_URL=http://ssd-git-01:8081/nexus/content/repositories/JVAE2-0/jcf-gw/D1.0.1-x86_64-linux/jcf-gw-D1.0.1-x86_64-linux.tgz
#PKG_MD5SUM=cadce42316cbd5b9563847326371dcfd
TO_DIR=/opt
TP_LICENSE_URL=
TP_ATTRIBUTES_URL=
TP_SOURCE_URL=
EXCLUDE=
PKG_FILE=jcf-gw-D1.0.1-x86_64-linux.tgz
PKG_MD5SUM=
CHECKSUM_FILE=
LICENSE_FILE=
ATTRIBUTES_FILE=
SOURCE_FILE=
