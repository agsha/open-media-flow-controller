:
# Script to build the Samara based product.
echo PROD_OUTPUT_ROOT=${PROD_OUTPUT_ROOT}
Syntax()
{
    echo What this does: Change to '$'PROD_TREE_ROOT and do a make
    echo Command line parameters:
    echo "[optional environment settings] [fresh] [clean] [all|default|[[basic][install][release][release-opensource][mfgcd][mfgusb]][sums]"
    echo "When no parameters are specified, it is the same as specifying 'default'"
    echo "'all' is the same as specifying '${ALL_LIST}'"
    echo "'default' is the same as specifying '${DEFAULT_LIST}'"
    echo "'fresh' does: rm -rf" ${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT}-'['ARCH']'
    echo "'clean' does: make clean"
    echo "'basic' does: make"
    echo "'install' does: make install"
    echo "'release' does: make release"
    echo "'release-opensource' does: make release-opensource"
    echo "'mfgcd' makes the CD ISO image file of the release"
    echo "'mfgusb' makes the USB drive zip fle of the release"
    echo Optionally specify environment settings to use
    echo for the make, in the form NAME=VALUE.
    echo The following are the important ones, with the default values:
    echo PROD_CUSTOMER_ROOT=${PROD_CUSTOMER_ROOT}
    echo PROD_OUTPUT_ROOT=${PROD_OUTPUT_ROOT}
    echo PROD_TREE_ROOT=${PROD_TREE_ROOT}
    echo PROD_PRODUCT=${PROD_PRODUCT}
    echo PROD_TARGET_ARCH=${PROD_TARGET_ARCH}
    echo PROD_TARGET_HOST=${PROD_TARGET_HOST}
    echo BUILD_LOG_DIR='${PROD_OUTPUT_ROOT}'
 
}

MY_DIR=`dirname "${0}"`
case "${MY_DIR}" in
/*)   ;;
../*) MY_DIR=`pwd`/.. ;;
.|./) MY_DIR=`pwd` ;;
*) MY_DIR=`pwd`/"${MY_DIR}" ;;
esac    

ALL_LIST="basic install release mfgcd"
DEFAULT_LIST="basic install release"
DO_LIST=default
CLEAN_OPT=
FRESH_OPT=
SUMS_OPT=
L_DIR_DEFAULT=${BUILD_LOG_DIR}
VERBOSE_LEVEL=0
FOREGROUND=foreground
for ARG in ${*}
do
    A="${ARG//[]/ }"
    VAL="${A#*=}"
    case ${A} in 
    PROD_OUTPUT_ROOT=*) export PROD_OUTPUT_ROOT="${VAL}"
                        echo "${A}"
                        [ "_${L_DIR_DEFAULT}" = "_" ]  && export L_DIR_DEFAULT=${PROD_OUTPUT_ROOT}
                        ;;
    BUILD_LOG_DIR=*)    L_DIR_DEFAULT="${VAL}"
                        ;;
    *=*)                export "${A}"
                        echo "${A}"
                        ;;
    fresh)              FRESH_OPT=fresh
                        ;;
    clean)              CLEAN_OPT=clean
                        ;;
    all)                DO_LIST="${ALL_LIST}"
                        ;;
    default)            DO_LIST="${DEFAULT_LIST}"
                        ;;
    basic|install|release|release-opensource|mfgcd|mfgusb|sums)
                        [ "_${DO_LIST}" = "_default" ] && DO_LIST=
                        [ "_${DO_LIST}" = "_all" ] && DO_LIST=
                        DO_LIST="${DO_LIST} ${A}"
                        ;;
    *)  Syntax ; exit 1 ;;
    esac
done
echo PROD_OUTPUT_ROOT=${PROD_OUTPUT_ROOT}
echo DO_LIST="${DO_LIST}"
echo "${DO_LIST}" | grep -q "${DEAULT_LIST}" 
[ ${?} -eq 0 ] && echo Doing the default build

[ "_${CLEAN_OPT}" = "_clean" ] && DO_LIST="clean ${DO_LIST}"

echo Doing: ${FRESH_OPT} ${DO_LIST}
echo BUILD_EXTRA_CFLAGS=${BUILD_EXTRA_CFLAGS}
if [ ! -z "${BUILD_ASSERT}" ] ; then
  # This is a legacy setting for MFD 1.0.1 and earlier
  echo BUILD_ASSERT=${BUILD_ASSERT}
fi

[ "_${L_DIR_DEFAULT}" != "_" ] && export BUILD_LOG_DIR=${L_DIR_DEFAULT}
[ "_${BUILD_LOG_DIR}" = "_" ]  && export BUILD_LOG_DIR=${PROD_OUTPUT_ROOT}
echo BUILD_LOG_DIR=${BUILD_LOG_DIR}
if [ ! -d "${BUILD_LOG_DIR}" ] ; then
  echo mkdir -p "${BUILD_LOG_DIR}"
  mkdir -p "${BUILD_LOG_DIR}"
  if [ ! -d "${BUILD_LOG_DIR}" ] ; then
    echo Error, cannot created directory "${BUILD_LOG_DIR}"
    exit 1
  fi
fi

if [ "${USER}" != "root" ] ; then
    echo You must run this script from a "sudo bash" prompt.
    echo If you do not have sudo permission, add this line to /etc/sudoers:
    echo "${USER} ALL=(ALL) ALL"
    echo 'Then type "sudo bash", and at the # prompt run this script again.'
    echo "or if you specify all the parameters on the command line, do sudo ${0}"
    exit 1
fi
if [ -z "${REAL_USER}" ] ; then
    REAL_USER=build
fi
# Make sure the required environment is set and correct.
if [ "_${PROD_CUSTOMER_ROOT}" = "_" ] ; then
    echo PROD_CUSTOMER_ROOT not set
    exit 1
fi
if [ "_${PROD_OUTPUT_ROOT}" = "_" ] ; then
    echo PROD_OUTPUT_ROOT not set
    exit 1
fi
if [ "_${PROD_TREE_ROOT}" = "_" ] ; then
    echo PROD_TREE_ROOT not set
    exit 1
fi
if [ "_${PROD_PRODUCT}" = "_" ] ; then
    echo PROD_PRODUCT not set
    exit 1
fi
export PROD_PRODUCT_LC=`echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]'`
if [ "_${PROD_TARGET_ARCH}" = "_" ] ; then
    echo PROD_TARGET_ARCH not set
    exit 1
fi
if [ "_${PROD_TARGET_HOST}" = "_" ] ; then
    export PROD_TARGET_HOST=X86_64
fi
# ----------------------------------------------------------
if [ ! -d ${PROD_TREE_ROOT} ] ; then
    echo PROD_TREE_ROOT=${PROD_TREE_ROOT} not set to valid directory
    exit 1
fi
if [ ! -d ${PROD_CUSTOMER_ROOT} ] ; then
    echo PROD_CUSTOMER_ROOT=${PROD_CUSTOMER_ROOT} not set to valid directory
    exit 1
fi
if [ ! -d ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT} ] ; then
    echo PROD_PRODUCT=${PROD_PRODUCT} not set to valid subdirectory
    exit 1
fi

if [ ! -d ${PROD_OUTPUT_ROOT} ] ; then
    mkdir -p ${PROD_OUTPUT_ROOT}
    chown ${REAL_USER} ${PROD_OUTPUT_ROOT}
fi
if [ ! -d ${BUILD_LOG_DIR} ] ; then
    mkdir -p ${BUILD_LOG_DIR}
    chown ${REAL_USER} ${BUILD_LOG_DIR}
fi
PROD_TARGET_ARCH_LC=`echo ${PROD_TARGET_ARCH} | tr '[A-Z]' '[a-z]'`
PRODUCT_DIR=${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT}-${PROD_TARGET_ARCH_LC}
if [ "_${FRESH_OPT}" = "_fresh" ] ; then
    for i in build install release ; do
        echo rm -rf ${PRODUCT_DIR}/${i}
        rm      -rf ${PRODUCT_DIR}/${i}
        if [ -d ${PRODUCT_DIR}/${i} ] ; then
            echo Failed to remove ${PRODUCT_DIR}/${i}
            echo ls -l  ${PRODUCT_DIR}
            ls      -l  ${PRODUCT_DIR}
            exit 1
        fi
    done
    if [ "_${DO_LIST}" = "_" ] ; then
        echo Finished removing ${PRODUCT_DIR} build output directories
        exit 0
    fi
fi
export LC_ALL=C
# Clean out the build_version.* files, so version string changes will be re-done.
for THIS in ${DO_LIST}
do
    case "${THIS}" in 
    clean|basic)
        rm -f ${PRODUCT_DIR}/build/release/build_version.*
        rm -f ${BUILD_LOG_DIR}/sums.list
        ;;
     esac
    case "${THIS}" in 
    mfgcd)
        echo rm -rf ${PRODUCT_DIR}/release/mfgcd
        rm      -rf ${PRODUCT_DIR}/release/mfgcd
        ;;
    mfgusb)
        echo rm -rf ${PRODUCT_DIR}/release/mfgusb
        rm      -rf ${PRODUCT_DIR}/release/mfgusb
        ;;
    release-opensource)
        echo rm -rf ${PRODUCT_DIR}/release/opensource
        rm      -rf ${PRODUCT_DIR}/release/opensource
        ;;
    *)
        # Always clean out the release directory, except when doing an mfg* only build,
        # so a failed build will not have the old files there.
        echo rm -rf ${PRODUCT_DIR}/release
        rm      -rf ${PRODUCT_DIR}/release
        ;;
     esac
done
# ----------------------------------------------------------
BG_PID1=
BG_PID2=
trap_cleanup_exit()
{
  echo "Trap - build-samara"
  for i in  ${BG_PID1} ${BG_PID2} 
  do
      echo "build-samara - kill ${i}"
      ps -f ${i} | grep -v STIME
      /bin/kill ${i}
  done
  exit 1
}

for THIS in ${DO_LIST}
do
    echo Doing ${THIS} ++++++++++++++++++++++++++++++++++++++++++++++++
    case "${THIS}" in
    sums)
        echo Putting sums into ${BUILD_LOG_DIR}/sums.list
        ${MY_DIR}/sumtree.sh ${PRODUCT_DIR}/build   >  ${BUILD_LOG_DIR}/sums.list 2>&1
        ${MY_DIR}/sumtree.sh ${PRODUCT_DIR}/install >> ${BUILD_LOG_DIR}/sums.list 2>&1
        ${MY_DIR}/sumtree.sh ${PRODUCT_DIR}/release >> ${BUILD_LOG_DIR}/sums.list 2>&1
        continue
        ;;
    mfgcd|mfgusb)
        cd ${PROD_TREE_ROOT}/src/release
        ;;
    *)
        cd ${PROD_TREE_ROOT}
        ;;
    esac
    TARGET=${THIS}
    [ "${THIS}" = "basic" ] && TARGET=
    LF="${BUILD_LOG_DIR}"/make${THIS}.log
    rm -f ${LF}
    echo "make${THIS}Start:  "`date` | tee ${LF}
    chmod 666 ${LF}
    chown ${REAL_USER} ${LF}
    if [ "_${FOREGROUND}" = "_forground" ] ; then
        make ${TARGET} >> ${LF} 2>&1
        RV=$?
    else
        trap trap_cleanup_exit INT QUIT TERM
        make ${TARGET} >> ${LF} 2>&1 &
        BG_PID1=${!}
        echo BG_PID1=${BG_PID1}
        tail -f ${LF} &
        BG_PID2=${!}
        echo BG_PID2=${BG_PID2}
        wait ${BG_PID1}
        RV=${?}
        echo Back from wait, RV=${RV}
        kill ${BG_PID2} > /dev/null 2>&1
    fi
    echo make${THIS}Finish: `date` | tee -a ${LF}
    echo make${THIS}RV=${RV}       | tee -a ${LF}
    chown ${REAL_USER} ${PROD_OUTPUT_ROOT}
    chown ${REAL_USER} ${PROD_OUTPUT_ROOT}/*
    chown ${REAL_USER} ${PRODUCT_DIR}
    chown ${REAL_USER} ${PRODUCT_DIR}/*
    chown -R ${REAL_USER} ${PRODUCT_DIR}/release
    if [ -d ${PRODUCT_DIR}/log ] ; then
      chown -R ${REAL_USER} ${PRODUCT_DIR}/log
    fi
    if [ ${RV} -ne 0 ] ; then
       echo Abort exit ${RV} build-samara.sh
       exit ${RV}
    fi
done
#############################################
echo Normal exit ${RV} build-samara.sh
exit 0
