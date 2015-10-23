:
# Called like this: ./download_pkg.sh jcf-gw.info ${EXTERNAL_PKG_DIR}/jfc-gw
# Parameter 1 is the local info file.
# Parameter 2 is the root directory where to download the pkg to and create
#   the pkg.info file.

INFO_FILE=${1}
OUTPUT_DIR=${2}
if [ -z ${INFO_FILE} ] ; then
  echo Error, input file not specified.
  exit 1
fi
if [ ! -f ${INFO_FILE} ] ; then
  echo Error, input file does not exist: ${INFO_FILE}
  exit 1
fi
if [ -z ${OUTPUT_DIR} ] ; then
  echo Error, output dir not specified.
  exit 1
fi
if [ ! -d ${OUTPUT_DIR} ] ; then
  echo Error, output dir does not exist: ${OUTPUT_DIR}
  exit 1
fi

case ${INFO_FILE} in
/*) ;;
*) INFO_FILE=`pwd`/${INFO_FILE} ;;
esac

case ${OUTPUT_DIR} in
/*) ;;
*) OUTPUT_DIR=`pwd`/${OUTPUT_DIR} ;;
esac


PKG_NAME=
PKG_FROM_URL=
PKG_CHECKSUM_URL=
PKG_MD5SUM=
TO_DIR=
TP_LICENSE_URL=
TP_ATTRIBUTES_URL=
TP_SOURCE_URL=
EXCLUDE=

. ${INFO_FILE}

ERR=0
[ -z "${PKG_NAME}" ] && ERR=1
[ -z "${PKG_FROM_URL}" ] && ERR=1
if [ ${ERR} -ne 0 ] ; then
  echo Syntax error in ${INFO_FILE}
  exit 1
fi
[ -z "${TO_DIR}" ] && TO_DIR=/
rm -f ${OUTPUT_DIR}/pkg.info


PKG_FILE=`basename ${PKG_FROM_URL}`
CHECKSUM_FILE=
LICENSE_FILE=
ATTRIBUTES_FILE=
SOURCE_FILE=
[ ! -z "${PKG_CHECKSUM_URL}"  ] && CHECKSUM_FILE=`basename   ${PKG_CHECKSUM_URL}`
[ ! -z "${TP_LICENSE_URL}"    ] && LICENSE_FILE=`basename    ${TP_LICENSE_URL}`
[ ! -z "${TP_ATTRIBUTES_URL}" ] && ATTRIBUTES_FILE=`basename ${TP_ATTRIBUTES_URL}`
[ ! -z "${TP_SOURCE_URL}"     ] && SOURCE_FILE=`basename     ${TP_SOURCE_URL}`

for THIS_URL in ${PKG_FROM_URL} ${PKG_CHECKSUM_URL} ${TP_LICENSE_URL} ${TP_ATTRIBUTES_URL} ${TP_SOURCE_URL}
do
  THIS_FILE=`basename ${THIS_URL}`
  case ${THIS_URL} in
  http:*|ftp:*)
    curl ${THIS_URL} -o ${OUTPUT_DIR}/${THIS_FILE}
    RV=${?}
    if [ ${RV} -ne 0 ] ; then
      echo failed to download ${THIS_URL}
      exit 1
    fi
    ;;
  scp:*)
    scp ${THIS_URL} ${OUTPUT_DIR}/${THIS_FILE}
    RV=${?}
    if [ ${RV} -ne 0 ] ; then
      echo failed to download ${THIS_URL}
      exit 1
    fi
    ;;
  /*)
    cp ${THIS_URL} ${OUTPUT_DIR}/${THIS_FILE}
    RV=${?}
    if [ ${RV} -ne 0 ] ; then
      echo failed to download ${THIS_URL}
      exit 1
    fi
    ;;
  *)
    echo Unsupported URL type: ${THIS_URL}
    exit 1
    ;;
  esac
  echo Downloaded ${THIS_URL} to ${OUTPUT_DIR}
done

THIS_SUM=`md5sum ${OUTPUT_DIR}/${PKG_FILE} | cut -f1 -d' '`
if [ ! -z "${PKG_MD5SUM}" ] ; then
  if [ "${PKG_MD5SUM}" != "${THIS_SUM}" ] ; then
    echo Error, md5sum does not match PKG_MD5SUM=${PKG_MD5SUM}
    exit 1
  fi
fi
if [ ! -z "${CHECKSUM_FILE}" ] ; then
  THAT_SUM=`cat ${OUTPUT_DIR}/${CHECKSUM_FILE} | cut -f1 -d' '`
  if [ ${THAT_SUM} != ${THIS_SUM} ] ; then
    echo Error, md5sum does not match ${OUTPUT_DIR}/${CHECKSUM_FILE}
    exit 1
  fi
    [ -z "${PKG_MD5SUM}" ] && PKG_MD5SUM=${THIS_SUM}
fi
cat ${INFO_FILE} | grep -v ^PKG_MD5SUM= >  ${OUTPUT_DIR}/pkg.info
echo PKG_FILE=${PKG_FILE}               >> ${OUTPUT_DIR}/pkg.info
echo PKG_MD5SUM=${PKG_MD5SUM}           >> ${OUTPUT_DIR}/pkg.info
echo CHECKSUM_FILE=${CHECKSUM_FILE}     >> ${OUTPUT_DIR}/pkg.info
echo LICENSE_FILE=${LICENSE_FILE}       >> ${OUTPUT_DIR}/pkg.info
echo ATTRIBUTES_FILE=${ATTRIBUTES_FILE} >> ${OUTPUT_DIR}/pkg.info
echo SOURCE_FILE=${SOURCE_FILE}         >> ${OUTPUT_DIR}/pkg.info

[ -z "${PKG_MD5SUM}" ] && echo Note: No checksum to verify
cat ${OUTPUT_DIR}/pkg.info | grep -v '=$'

