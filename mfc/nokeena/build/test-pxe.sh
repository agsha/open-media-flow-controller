:
# Put test request in a que that a build user cron job on sv01
# reads to submit a sanity test.
# Parameters: [options] <pxe-name>
# Options:
#    -r string or --resultfile string: Specify the filename to pass back info.
#    -t string or --test string: Specify the names of the tests to run.

# Used env settings:
# PROD_OUTPUT_ROOT
# BUILD_LOG_DIR
[ "_${BUILD_LOG_DIR}"       = "_" ] && export BUILD_LOG_DIR=/tmp/${USER}

RESULT_FILENAME=/dev/null
FORCE=no
TEST_NAME=
while true ; do
  case "_${1}" in
  _--resultfile*|_-r)
    RESULT_FILENAME="${2}"
    if [ -z "${RESULT_FILENAME}" ] ; then
      echo Error, no file name specified for result file
      exit 1
    fi
    shift; shift
    continue
    ;;
  _--test*|_-t)
    TEST_NAME="${2}"
    if [ -z "${TEST_NAME}" ] ; then
      echo Error, no test name specified
      exit 1
    fi
    shift; shift
    continue
    ;;
  _--force|_-f)
    FORCE=force
    shift
    continue
    ;;
  *)  break
    ;;
  esac
done
case ${RESULT_FILENAME} in
/*) ;;
*) RESULT_FILENAME="`pwd`/${RESULT_FILENAME}"
esac
if [ -d "${RESULT_FILENAME}" ] ; then
  echo Error, specified a directory for the result file
  exit 1
fi
PXE_NAME="${1}"
case "_${PXE_NAME}" in
_)
  echo 'Syntax: [ options ] <pxe-name>'
  echo Options:
  echo '    --resultfile <filename>'
  echo '    --test <test-name>'
  echo '    --force'
  echo "ERROR=syntaxerror" >  ${RESULT_FILENAME}
  exit 1
  ;;
*' '*|*,*|*';'*)
  echo Illegal character in the pxe-name.
  exit 1
  ;;
esac 

# Verify that the specified pxe-name is valid by looking
# for a directory by that name on the pxe server.
PXE_ROOT=/home2/pxe-boot
[ ! -d ${PXE_ROOT} ] && PXE_ROOT=/home2/tftpboot/pxe-boot
if [ ! -d ${PXE_ROOT} ] ; then
  echo Cannot access pxe-boot dir to verfify pxe-name.
  echo Not submitting for test.
  exit 1
fi
if [ ! -d ${PXE_ROOT}/${PXE_NAME} ] ; then
  echo No such pxe name on server: ${PXE_NAME}
  if [ "${FORCE}" = "force" ] ; then
    echo Forced to continue
  else
    cd ${PXE_ROOT}
    pwd
    ls -ld *${PXE_NAME}*
    echo Not submitting for test.
    exit 1
  fi
else
  cd ${PXE_ROOT}/${PXE_NAME}
  ERR=0
  for i in rootflop- vmlinuz-bootflop- image-
  do
    COUNT=`ls ${i}* | wc -l`
    if [ ${COUNT} -eq 0 ] ; then
      echo Missing ${i}'*'
      ERR=1
    elif [ ${COUNT} -ne 1 ] ; then
      echo Too many files ${i}'*'
      ls ${i}*
      ERR=1
    fi
  done
  if [ ${ERR} -ne 0 ] ; then
    echo Error, invalid pxe-name directory on server.
    if [ "${FORCE}" = "force" ] ; then
      echo Forced to continue
    else
      echo "ERROR=improper-pxe-dir" >  ${RESULT_FILENAME}
      echo Not submitting for test.
      exit 1
    fi
  fi
fi

U_DIR=${PXE_ROOT}/test-submit-dir/${USER}
if [ ! -d "${U_DIR}" ] ; then
   mkdir "${U_DIR}"
fi
if [ ! -d "${U_DIR}" ] ; then
  echo Failed to create ${U_DIR}
  echo Not submitting for test.
  exit 1
fi
chmod 777 ${U_DIR} > /dev/null 2>&1

P_DIR=${U_DIR}/${PXE_NAME}
if [ ! -d ${P_DIR} ] ; then
  TO_DIR=${P_DIR}
else
  CNT=0
  while true
  do
    TO_DIR=${P_DIR}_${CNT}
    CNT=$((CNT + 1))
    [ ! -d "${TO_DIR}" ] && break
  done
fi
mkdir "${TO_DIR}"
if [ ! -d "${TO_DIR}" ] ; then
  echo Failed to create ${TO_DIR}
  echo Not submitting for test.
  exit 1
fi
chmod 777 ${TO_DIR} > /dev/null 2>&1
rm -f ${TO_DIR}/*

echo PXE_NAME=${PXE_NAME}    > ${TO_DIR}/settings.dot
echo TEST_NAME=${TEST_NAME} >> ${TO_DIR}/settings.dot
echo TEST_DIR=${TO_DIR}     >> ${TO_DIR}/settings.dot
echo SUBMITTER=${USER}      >> ${TO_DIR}/settings.dot

date > ${TO_DIR}/new

chmod 666 ${TO_DIR}/*
echo TEST_DIR=${TO_DIR} >  ${RESULT_FILENAME}
exit 0
