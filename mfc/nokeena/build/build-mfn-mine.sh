:
# Build a Samara based product.
# Do a full build from the existing checked out tree, optionally updated to current.

MY_DIR=`dirname "${0}"`
case "${MY_DIR}" in
/*)   ;;
../*) MY_DIR=`pwd`/.. ;;
.|./) MY_DIR=`pwd` ;;
*) MY_DIR=`pwd`/"${MY_DIR}" ;;
esac

# The mf_env_funcs.dot files defines the functions:
#   Get_Env_Settings_From_File()
#   Validate_PROD()
#   Set_Defaults()
#   Validate_PROD_TREE_ROOT_From_Source_Tree()
#   Set_NKN_Env_From_Source_Tree()
#   Make_ENVS()
#   efVprint() -- used in Set_NKN_Env_From_Source_Tree()
#      When VERBOSE_EFST=yes, then it logs extra info
#      to stdout and the ${LOG_FILE}.
#
EF=${MY_DIR}/mf_env_funcs.dot
if [ ! -f "${EF}" ] ; then
    echo Error, no ${EF}
    exit 1
fi
. ${EF}
VERBOSE_EFST=no

# Get settings from file :env.sh
Get_Env_Settings_From_File

# Set some default PROD_*, NKN_BUILD_*, BUILD_* and REAL_USER settings.
Validate_PROD
Set_Defaults
Validate_PROD_TREE_ROOT_From_Source_Tree
export PROD_PRODUCT_LC=`echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]'`

# Environment variables used:
# PROD_CUSTOMER_ROOT
# PROD_OUTPUT_ROOT
# PROD_PRODUCT_LC - Specifies the subdir of PROD_CUSTOMER_ROOT = nokeena or juniper
#    This is the lower case translation of ${PROD_PRODUCT}
# PROD_TARGET_ARCH
# PROD_TARGET_HOST
# PROD_TREE_ROOT
# BUILD_LOG_DIR
# REAL_USER
# NKN_BUILD_PROD_NAME
# NKN_BUILD_ID
# NKN_BUILD_NUMBER
# NKN_BUILD_SCM_INFO
# NKN_BUILD_SCM_INFO_SHORT
# NKN_BUILD_PROD_RELEASE

export LOG_FILE="${BUILD_LOG_DIR}"/build-log.txt
export LOG2_FILE="${BUILD_LOG_DIR}"/build-log2.txt

rm -f ${LOG_FILE}
touch "${LOG_FILE}"

Text ()   { echo "${@}" ; } ;
Print ()  { echo "${@}" >> ${LOG_FILE} ; echo "${@}" ; } ;
Vprint () { echo "${@}" >> ${LOG_FILE} ; [ ${VERBOSE_LEVEL} -ne 0 ] && echo "${@}" ; } ;
Log ()    { echo "${@}" >> ${LOG_FILE} ; } ;
Cat_Log () { cat "${@}" >> ${LOG_FILE} ; } ;

BUILD_SAMARA_EXE=${MY_DIR}/build-samara.sh
if [ ! -f "${BUILD_SAMARA_EXE}" ] ; then
    echo Error, no ${BUILD_SAMARA_EXE}
    exit 1
fi

Syntax()
{
    echo This command does a build using the script ${BUILD_SAMARA_EXE}
    echo This builds from the ${PROD_PRODUCT} source in ${PROD_CUSTOMER_ROOT}
    echo Command line parameters:
    #echo "[options] [generic|vxa] [fresh|clean] [all|default|none|[[basic][install][release][mfgcd]]"
    echo "[options] [fresh|clean] [all|default|none|[[basic][install][release][mfgcd]]"
    echo "When no parameters are specified, it is the same as specifying 'default'"
    echo "'fresh' does: rm -rf" ${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT}-'['ARCH']'
    echo "'clean' does: make clean"
    echo "'all' is the same as specifying '${ALL_LIST}'"
    echo "'default' is the same as specifying '${DEFAULT_LIST}'"
    echo "'none' causes no compiling to be done (cancels doing the 'default')"
    #echo "Specify the OEM version to be built: generic or vxa.  (default=generic)"
    echo "'basic' does: make"
    echo "'install' does: make install"
    echo "'release' does: make release"
    echo "'mfgcd' makes the CD ISO image file of the release"
    echo "'mfgsrc' makes the required GPL source release"
    echo Options:
    echo "--img-suffix-dated : Use the build date in the filenames for the release images."
    echo "--img-suffix-id : Use the build id in the filenames for the release images."
    echo "-q      : quiet mode. Do not print to stdout or stderr"
    echo "-v      : verbose mode. Print all the make output"
    echo "        : When -q and -v are not used then print limited make output"
    echo "--bg    : Force the build to be run in the background."
    echo "--fg    : Force the build to be run in the foreground. (default)"
    echo "-s      : before building do a svn update of PROD_CUSTOMER_ROOT: ${PROD_CUSTOMER_ROOT}"
    echo "-failsvnstatus|-f : Exit when 'svn status' printed anything out."
    echo "-u name : svn user name"
    echo "-p pw   : svn password"
    echo "--O         : Compile with '-O'"
    echo "--noO       : Do NOT compile with '-O'"
    echo "            : O option default: ${USE_O_OPT}"
    echo "--autosamara : Automatically set PROD_TREE_ROOT for this build."
    #echo "--legacy100 : Set environment varibles for legacy 1.0.0 builds."
    #echo "--legacy101 : Set environment varibles for legacy 1.0.1 builds."
    echo "--sums      : Calucluate sums of all the files when the build is done."
    echo "--shellx|-x : Run build-samara.sh with 'sh -x'"
    echo "Uses BUILD_LOG_DIR settings in your environment for location of log files: ${BUILD_LOG_DIR}"
    echo "Uses all the PROD_* NKN_BUILD_* and BUILD_* settings in your environment:"
    Make_ENVS
    echo "${ENVS}"
    echo You can set these in your environment:
    echo BUILD_RELEASE_APPEND
    echo BUILD_ID_PREPEND and/or BUILD_ID_APPEND
}

# Options for svn update:
SVN_USERNAME_OPT=
SVN_PASSWORD_OPT=
[ "_${SUDO_USER}" != "_" ] && SVN_USERNAME_OPT="--username ${SUDO_USER}"

ALL_LIST="basic install release mfgcd"
DEFAULT_LIST="basic install release"
DO_LIST=default
CLEAN_OPT=
SUFFIX_OPT=unset
FRESH_OPT=
X_OPT=

# Get the default for the USE_O_OPT from the evironment var BUILD_USE_O.
case ${BUILD_USE_O} in
O) USE_O_OPT=O   ;;
*) USE_O_OPT=noO ;;
esac

LEGACY_OPT=none

VERBOSE_LEVEL=1
VERBOSE=no
SVN_UPDATE=no
SVN_STATUS_FAIL=no
SUMS_OPT=nosums
FOREGROUND=foreground
DRYRUN=nodryurun
Parse_Params()
{
  while true do
  do
    A=${1}
    [ "_${A}" = "_" ] && break
    shift
    case "${A}" in
    #generic)            BUILD_OEM=generic
    #                    ;;
    #vxa)                BUILD_OEM=vxa
    #                    ;;
    #ankeena)            BUILD_OEM=ankeena
    #                    ;;
    #jnpr|juniper)       BUILD_OEM=juniper
    #                    ;;
    fresh)              FRESH_OPT=fresh
                        ;;
    clean)              CLEAN_OPT=clean
                        ;;
    all)                DO_LIST=all
                        ;;
    default)            DO_LIST=default
                        ;;
    none)               DO_LIST=
                        ;;
    basic|install|release|mfgcd|mfgusb)
                        [ "_${DO_LIST}" = "_default" ] && DO_LIST=
                        [ "_${DO_LIST}" = "_all"     ] && DO_LIST=
                        DO_LIST="${DO_LIST} ${A}"
                        ;;
    mfgsrc)             export DO_BUILD_FREEDIST=1 ;;
    --autosamara)       PROD_TREE_ROOT= ;;
    --img-suffix-dated) SUFFIX_OPT=dated ;;
    --img-suffix-id)    SUFFIX_OPT=id ;;
    --dryrun)           DRYRUN=dryurun ;;
    #--legacy101)       LEGACY_OPT=legacy101 ;;
    --O)                USE_O_OPT=O ;;
    --noO)              USE_O_OPT=noO ;;
    -q|--quiet)         VERBOSE_LEVEL=0 ;;
    -v|--verbose)       VERBOSE_LEVEL=2 ;;
    -s|--svnupdate)     SVN_UPDATE=yes ;;
    -f|--failsvnstatus) SVN_STATUS_FAIL=yes ;;
    -u|--user)          SVN_USERNAME_OPT="--username ${1}" ; shift ;;
    -p|--password)      SVN_PASSWORD_OPT="--password ${1}" ; shift ;;
    -x|--shellx)        X_OPT=shellx ;;
    --sums)             SUMS_OPT=sums ;;
    --bg)               FOREGROUND=noforeground ;;
    --fg)               FOREGROUND=foreground ;;
    *)  
      echo "Syntax error, no such parameter: ${A}"
      Syntax
      exit 1
      ;;
    esac
  done
}

#if [ -z "${BUILD_OEM}" ] ; then
#  # Set default
#  if [ -d "${PROD_CUSTOMER_ROOT}"/${PROD_PRODUCT}/src/oem/generic ] ; then
#    BUILD_OEM=generic
#  elif [ -d "${PROD_CUSTOMER_ROOT}"/${PROD_PRODUCT}/src/oem/ankeena ] ; then
#    BUILD_OEM=ankeena
#  fi
#fi
## Convert older to newer oem setting automatically.
#if [ "${BUILD_OEM}" = "ankeena" -a -d "${PROD_CUSTOMER_ROOT}"/${PROD_PRODUCT}/src/oem/generic ] ; then
#    BUILD_OEM=generic
#fi
#if [ "${BUILD_OEM}" = "juniper" -a -d "${PROD_CUSTOMER_ROOT}"/${PROD_PRODUCT}/src/oem/vxa ] ; then
#    BUILD_OEM=vxa
#fi
#export BUILD_OEM

echo Parse_Params ${BUILD_MINE_DEFAULTS} ${@}
Parse_Params ${BUILD_MINE_DEFAULTS} ${@}
[ "_${DO_LIST}" = "_default" ] && DO_LIST="${DEFAULT_LIST}"
[ "_${DO_LIST}" = "_all"     ] && DO_LIST="${ALL_LIST}"
[ "_${CLEAN_OPT}" = "_clean" ] && DO_LIST="clean ${DO_LIST}"
[ "_${FRESH_OPT}" = "_fresh" ] && DO_LIST="fresh ${DO_LIST}"
[ "_${FRESH_OPT}" = "_fresh" -a "_${CLEAN_OPT}" = "_clean" ] && DO_LIST="fresh clean ${DO_LIST}"

[ ! -d ${PROD_CUSTOMER_ROOT} ] && mkdir -p ${PROD_CUSTOMER_ROOT}
[ ! -d ${PROD_OUTPUT_ROOT} ]   && mkdir -p ${PROD_OUTPUT_ROOT}
[ ! -d ${BUILD_LOG_DIR} ]      && mkdir -p ${BUILD_LOG_DIR}

if [ ! -d ${PROD_CUSTOMER_ROOT} ] ; then
    echo Cannot create PROD_CUSTOMER_ROOT=${PROD_CUSTOMER_ROOT}
    exit 1
fi
if [ ! -d ${PROD_OUTPUT_ROOT} ] ; then
    echo Cannot create PROD_OUTPUT_ROOT=${PROD_OUTPUT_ROOT}
    exit 1
fi
# When not printing anything out, don't do any backgrounding.
if [ ${VERBOSE_LEVEL} -eq 0 ] ; then
  FOREGROUND=forground
  VERBOSE=no
#else
#  VERBOSE=yes
fi

case ${USE_O_OPT} in
O)   export BUILD_USE_O=O
     BUILD_ID_APPEND="${BUILD_ID_APPEND}-O"
     ;;
noO) export BUILD_USE_O=noO
     ;;
esac
export BUILD_ID_APPEND
export BUILD_EXTRA_CFLAGS

Print Start: `date`
for P in `set | grep ^PROD_`
do
    V=`echo "${P}" | cut -f2 -d=`
    if [ -z ${V} ] ; then
        Print 'Empty:' ${P}
    else
        Print "${P}"
    fi
done

cd ${PROD_CUSTOMER_ROOT}
Print `pwd`

#### TBD ####
# Need to use a loop like this when accessing svn, because
# sometimes it fails, probably when there is a brief
# network outage or if the SVN server refuses connections
# temporarily (because of load to high???)
#  while true ; do
#    svn log -r ${THIS_REV} ${SVN_URL_FULL} > ${OUT_FILE1}
#    RV=${?}
#    LINES=`cat ${OUT_FILE1} | wc -l`
#    [ ${LINES} -ne 0 ] && break
#    RETRY=`expr ${RETRY} + 1`
#    if [ ${RETRY} -gt 10 ] ; then
#      echo Failed ${RV} >&2
#      break
#    fi
#    echo Problem ${RV} >&2
#    sleep ${RETRY}
#    echo Retry svn log -r ${THIS_REV} >&2
#  done

if [ "${SVN_UPDATE}" = "yes" ] ; then
    Print svn ${SVN_USERNAME_OPT} --no-auth-cache --non-interactive update
    if [ ${VERBOSE_LEVEL} -ne 0 ] ; then
        svn ${SVN_USERNAME_OPT} ${SVN_PASSWORD_OPT} --no-auth-cache --non-interactive update 2>&1 | tee -a ${LOG_FILE}
    else
        svn ${SVN_USERNAME_OPT} ${SVN_PASSWORD_OPT} --no-auth-cache --non-interactive update >> ${LOG_FILE} 2>&1
    fi
fi
if [ ! -f ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot ] ; then
    echo Error: There is no file ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot
    exit 1
fi

# The function Validate_PROD_TREE_ROOT_From_Source_Tree (in mf_env_funcs.dot)
# sets PROD_TREE_ROOT if it is not already set, and then makes sure that the
# svn rev there matches.
# To do this, PROD_CUSTOMER_ROOT must be set and the build_settings.dot file
# must be in place.
Validate_PROD_TREE_ROOT_From_Source_Tree

cd ${PROD_CUSTOMER_ROOT}

# When NKN_BUILD_PROD_RELEASE is not set, it is set from src/mk/customer.mk
#  BUILD_RELEASE_APPEND is appended to NKN_BUILD_PROD_RELEASE

#  NKN_BUILD_TYPE determines the format of the NKN_BUILD_ID.
#  It is made from BUILD_ID_PREPEND, NKN_BUILD_NUMBER, BUILD_ID_APPEND and other info.
#  See mf_env_funcs.dot
#export NKN_BUILD_TYPE=numbered
#export NKN_BUILD_TYPE=dev      - The builder's userid is appended.
#export NKN_BUILD_TYPE=QABUILD  - This string is appended.

#  NKN_BUILD_NUMBER when not set is set to the svn revision number.
#  When it is already set, then the svn revision number is appended with an "_".

#  BUILD_ID_PREPEND, BUILD_ID_APPEND are prepended and appended to the ID string.

##########################################################################################

# Now that we have the source trees we are building from, set some NKN_* environment variables that depend on that.
# Always sets:
#   NKN_BUILD_SCM_INFO_SHORT
#   NKN_BUILD_SCM_INFO
# Sets if not already set:
#   NKN_BUILD_PROD_RELEASE
#   NKN_BUILD_ID
#   NKN_BUILD_NUMBER
# When VERBOSE_LEVEL is not zero, prints to stdout the output from "svn status".
# When SVN_STATUS_FAIL=yes, will exit when "svn status" printed anything out.
# Set BUILD_ID_SVN_EXTRA to "+" to have Subversion info about how many
# modified files there are, added to NKN_BUILD_ID.
BUILD_ID_SVN_EXTRA=+
NKN_BUILD_NUMBER=1
echo Before call to Set_NKN_Env_From_Source_Tree
echo NKN_BUILD_SCM_INFO_SHORT="${NKN_BUILD_SCM_INFO_SHORT}"
echo NKN_BUILD_ID="${NKN_BUILD_ID}"
# Note that Set_NKN_Env_From_Source_Tree requires the PROD_TREE_ROOT to be set properly.
echo Call Set_NKN_Env_From_Source_Tree
Set_NKN_Env_From_Source_Tree
echo NKN_BUILD_SCM_INFO_SHORT="${NKN_BUILD_SCM_INFO_SHORT}"
echo NKN_BUILD_ID="${NKN_BUILD_ID}"
echo BUILD_ID_MADE="${BUILD_ID_MADE}"

# Default to "id".
[ -z ${BUILD_CUSTOMER_SUFFIX} ] && BUILD_CUSTOMER_SUFFIX=id
[ "${SUFFIX_OPT}" = "dated" ] && BUILD_CUSTOMER_SUFFIX=dated
[ "${SUFFIX_OPT}" = "id"    ] && BUILD_CUSTOMER_SUFFIX=id
case _${BUILD_CUSTOMER_SUFFIX} in
_id)
  # When not already set, then set to our default.
  # E.g. mfd-2.0.0-qa-94_8169_157
  #      NKN_BUILD_PROD_RELEASE=mfd-2.0.0-qa
  #      NKN_BUILD_ID=94_8169_157
  BUILD_CUSTOMER_SUFFIX=${NKN_BUILD_PROD_RELEASE}-${NKN_BUILD_ID}
  ;;
_dated)
  # Unset it so that the system default will be used.
  # The system default is ${BUILD_PROD_NAME}-${BUILD_TARGET_ARCH_LC}-${BUILD_DATE}
  unset BUILD_CUSTOMER_SUFFIX
  ;;
esac
echo BUILD_CUSTOMER_SUFFIX="${BUILD_CUSTOMER_SUFFIX}"

# Save some of the env settings used in a file in the log dir
# for other scripts to use if they want to.
echo Remove ${BUILD_LOG_DIR}/envs.txt
rm -f ${BUILD_LOG_DIR}/envs.txt
echo NKN_BUILD_SCM_INFO_SHORT="${NKN_BUILD_SCM_INFO_SHORT}" >> ${BUILD_LOG_DIR}/envs.txt
echo NKN_BUILD_SCM_INFO="${NKN_BUILD_SCM_INFO}"             >> ${BUILD_LOG_DIR}/envs.txt
echo NKN_BUILD_PROD_RELEASE="${NKN_BUILD_PROD_RELEASE}"     >> ${BUILD_LOG_DIR}/envs.txt
echo NKN_BUILD_ID="${NKN_BUILD_ID}"                         >> ${BUILD_LOG_DIR}/envs.txt
echo NKN_BUILD_NUMBER="${NKN_BUILD_NUMBER}"                 >> ${BUILD_LOG_DIR}/envs.txt
echo NKN_BUILD_TYPE="${NKN_BUILD_TYPE}"                     >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_CUSTOMER_ROOT="${PROD_CUSTOMER_ROOT}"             >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_OUTPUT_ROOT="${PROD_OUTPUT_ROOT}"                 >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_TREE_ROOT="${PROD_TREE_ROOT}"                     >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_PRODUCT="${PROD_PRODUCT}"                         >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_PRODUCT_LC="${PROD_PRODUCT_LC}"                   >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_TARGET_ARCH="${PROD_TARGET_ARCH}"                 >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_TARGET_HOST="${PROD_TARGET_HOST}"                 >> ${BUILD_LOG_DIR}/envs.txt
echo PROD_TREE_ROOT="${PROD_TREE_ROOT}"                     >> ${BUILD_LOG_DIR}/envs.txt
echo BUILD_LOG_DIR="${BUILD_LOG_DIR}"                       >> ${BUILD_LOG_DIR}/envs.txt
echo BUILD_EXTRA_CFLAGS="${BUILD_EXTRA_CFLAGS}"             >> ${BUILD_LOG_DIR}/envs.txt
echo BUILD_CUSTOMER_SUFFIX="${BUILD_CUSTOMER_SUFFIX}"       >> ${BUILD_LOG_DIR}/envs.txt
echo DMI_PKG="${DMI_PKG}"                                   >> ${BUILD_LOG_DIR}/envs.txt

#case ${LEGACY_OPT} in
#legacy101|legacy100) export BUILD_ASSERT="-DNKN_ASSERT_ENABLE" ;;
#esac

# Now ready to do the build.

#rm -f ${BUILD_LOG_DIR}/makebasic.log
#date > ${BUILD_LOG_DIR}/makebasic.log 2> /dev/null
Print Build ouput to ${LOG_FILE}
if [ "_${X_OPT}" = "_shellx" ] ; then
    COMMAND_LINE=" sh -x ${BUILD_SAMARA_EXE} "
else
    COMMAND_LINE="${BUILD_SAMARA_EXE} "
fi
# Create a string with all the PROD_*, NKN_BUILD_*, BUILD_* and REAL_USER settings for the command line.
# Create a string with all the PROD_*, NKN_BUILD_*, BUILD_* and REAL_USER settings for the command line.
Make_ENVS
COMMAND_LINE="${COMMAND_LINE} VERBOSE_LEVEL=${VERBOSE_LEVEL} FOREGROUND=${FOREGROUND} ${ENVS}"
echo "${COMMAND_LINE}" > ${BUILD_LOG_DIR}/build-samara-command-line.txt
echo "${DO_LIST}" > ${BUILD_LOG_DIR}/build-samara-do-list.txt
COMMAND_LINE="${COMMAND_LINE} ${DO_LIST}"
export COMMAND_LINE
Print "command line::${COMMAND_LINE}::"
if [ "${DRYRUN}" = "dryurun" ] ; then
  Print Dryrun - stop here
  date
  echo ${BUILD_LOG_DIR}/envs.txt
  cat ${BUILD_LOG_DIR}/envs.txt
  exit 0
fi
BG1_PID=
BG2_PID=
trap_cleanup_exit()
{
  echo "Trap - build-mine"
  for i in  ${BG1_PID} ${BG2_PID} 
  do
      Print build-mine kill ${i}
      ps -f ${i} | grep -v STIME
      sudo /bin/kill ${i}
  done
  exit 1
}
echo =====================================================
if [ "foreground" = "${FOREGROUND}" ]  ; then
  if [ "_${X_OPT}" = "_shellx" ] ; then
    sudo ${COMMAND_LINE} >>  ${LOG_FILE} 2> ${LOG2_FILE}
  else
    sudo ${COMMAND_LINE} >>  ${LOG_FILE} 2>&1
  fi
  RV=${?}
else
  trap trap_cleanup_exit HUP INT QUIT PIPE TERM
  if [ "_${X_OPT}" = "_shellx" ] ; then
    sudo ${COMMAND_LINE} >> ${LOG_FILE} 2> ${LOG2_FILE} &
  else
    sudo ${COMMAND_LINE} >> ${LOG_FILE} 2>&1 &
  fi
  BG1_PID=${!}
  Print BG1_PID=${BG1_PID}
  sleep 1
  if [ ${VERBOSE_LEVEL} -eq 1 ] ; then
    ( tail -f ${LOG_FILE} | grep " Entering directory " ) &
  else
    tail -f ${LOG_FILE} &
  fi
  BG2_PID=${!}
  Print BG2_PID=${BG2_PID}
  wait ${BG1_PID}
  RV=${?}
  echo build-mine.sh back from wait, RV=${RV}
  kill ${BG2_PID} > /dev/null 2>&1
fi
Print RV=${RV}
Print Finished: `date`
if [ ${RV} -eq 0 ] ; then
    # Build succeeded
    ls "${PROD_OUTPUT_ROOT}"/product-*/release/image/*.img
    exit 0
fi

Print Build failed
grep "makecleanRV=" ${LOG_FILE} > /dev/null
if [ ${?} -eq 0 ] ; then
    grep "makecleanRV=0" ${LOG_FILE} > /dev/null
    if [ ${?} -ne 0 ] ; then
            Print Tail of ${BUILD_LOG_DIR}/makeclean.log
        tail -40 ${BUILD_LOG_DIR}/makeclean.log >> ${LOG_FILE}
        [ ${VERBOSE_LEVEL} -le 1 ] && tail -40 ${BUILD_LOG_DIR}/makeclean.log
        exit ${RV}
    fi
fi
grep "makebasicRV=" ${LOG_FILE} > /dev/null
if [ ${?} -eq 0 ] ; then
    grep "makebasicRV=0" ${LOG_FILE} > /dev/null
    if [ ${?} -ne 0 ] ; then
        Print Tail of ${BUILD_LOG_DIR}/makebasic.log
        tail -40 ${BUILD_LOG_DIR}/makebasic.log >> ${LOG_FILE}
        [ ${VERBOSE_LEVEL} -le 1 ] && tail -40 ${BUILD_LOG_DIR}/makebasic.log
        exit ${RV}
    fi
fi
grep "makeinstallRV=" ${LOG_FILE} > /dev/null
if [ ${?} -eq 0 ] ; then
    grep "makeinstallRV=0" ${LOG_FILE} > /dev/null
    if [ ${?} -ne 0 ] ; then
        Print Tail of ${BUILD_LOG_DIR}/makeinstall.log
        tail -40 ${BUILD_LOG_DIR}/makeinstall.log >> ${LOG_FILE}
        [ ${VERBOSE_LEVEL} -le 1 ] && tail -40 ${BUILD_LOG_DIR}/makeinstall.log
        exit ${RV}
    fi
fi
grep "makereleaseRV=" ${LOG_FILE} > /dev/null
if [ ${?} -eq 0 ] ; then
    grep "makereleaseRV=0" ${LOG_FILE} > /dev/null
    if [ ${?} -ne 0 ] ; then
        Print Tail of ${BUILD_LOG_DIR}/makerelease.log
        tail -40 ${BUILD_LOG_DIR}/makerelease.log >> ${LOG_FILE}
        [ ${VERBOSE_LEVEL} -le 1 ] && tail -40 ${BUILD_LOG_DIR}/makerelease.log
        exit ${RV}
    fi
fi
grep "makemfgcdRV=" ${LOG_FILE} > /dev/null
if [ ${?} -eq 0 ] ; then
    grep "makemfgcdRV=0" ${LOG_FILE} > /dev/null
    if [ ${?} -ne 0 ] ; then
        Print Tail of ${BUILD_LOG_DIR}/makemfgcd.log
        tail -40 ${BUILD_LOG_DIR}/makemfgcd.log >> ${LOG_FILE}
        [ ${VERBOSE_LEVEL} -le 1 ] && tail -40 ${BUILD_LOG_DIR}/makemfgcd.log
        exit ${RV}
    fi
fi
#grep "makemfgusbRV=" ${LOG_FILE} > /dev/null
#if [ ${?} -eq 0 ] ; then
#    grep "makemfgusbRV=0" ${LOG_FILE} > /dev/null
#    if [ ${?} -ne 0 ] ; then
#        Print Tail of ${BUILD_LOG_DIR}/makemfgusb.log
#        tail -40 ${BUILD_LOG_DIR}/makemfgusb.log >> ${LOG_FILE}
#        [ ${VERBOSE_LEVEL} -le 1 ] && tail -40 ${BUILD_LOG_DIR}/makemfgusb.log
#        exit ${RV}
#    fi
#fi
exit ${RV}
