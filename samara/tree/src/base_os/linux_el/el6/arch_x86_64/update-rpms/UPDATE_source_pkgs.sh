:
# Read each line in change.list and edit the build_required.inc to make the change.
# Four fields: BASE_RPM TYPE FROM_RPM LOCATION FULL_PATH_NEW_RPM
# 1: Base RPM name  (e.g. wget)
# 2. RPM filename in source_pkgs.inc (with "src.rpm" extention added)
# 3. Location name of the replacement.  One of:
#            DIST_SCL_6_UPDATE3_SRPMS
#            DIST_SCL_6_UPDATE2_SRPMS
#            DIST_SCL_6_UPDATE1_SRPMS
#            DIST_SCL_6_SRPMS
# 4. The full path to the replacement rpm file. 
# E.g.
# binutils binutils-2.20.51.0.2-5.42.SCLC6_6.R1.0.1.src.rpm DIST_SCL_6_SRPMS /volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0/srpm/binutils-2.20.51.0.2-5.42.SCLC6_6.R1.0.1.src.rpm 
# BASE_RPM FROM_RPM                                         LOCATION         FULL_PATH_NEW_RPM

# The lines to update in build_required.inc are like (without the leading "#")
#	binutils-2.20.51.0.2-5.36.el6.x86_64 \
#	binutils-devel-2.20.51.0.2-5.36.el6.x86_64 \


TMP_DIR=${1}
if [ -z "${TMP_DIR}" ] ; then
  echo Need to specify tmp dir
  exit 1
fi
if [ ! -d ${TMP_DIR} ] ; then
  echo Not a dir: ${TMP_DIR}
  exit 1
fi
shift
ORIG=${1}
if [ ! -d ${TMP_DIR} ] ; then
  echo Did not specify input makefile filename
  exit 1
fi
shift
TO=${1}
shift
if [ -z "${1}" ] ; then
  echo Need to specify change list file
  exit 1
fi
if [ ! -f ${1} ] ; then
  echo Not a file: ${1}
  exit 1
fi

T1=${TMP_DIR}/M1
T2=${TMP_DIR}/M2
cat ${ORIG} > ${T2}

cat ${*} | while true
do
  read BASE_RPM FROM_RPM LOCATION FULL_PATH_NEW_RPM
  [ -z ${BASE_RPM} ] && break
  if [ -z "${FULL_PATH_NEW_RPM}" ] ; then
    echo Error, problem with line:
    echo ${BASE_RPM} ${FROM_RPM} ${LOCATION} ${FULL_PATH_NEW_RPM}
    exit
  fi
  FROM_NAME=`basename ${FROM_RPM} .src.rpm`
  TO_NAME=`basename ${FULL_PATH_NEW_RPM} .src.rpm`
  #echo  sed "/DIST_.*}\/${FROM_NAME}/s//${LOCATION}}\/${TO_NAME}/"
  cat ${T2} > ${T1}
  cat ${T1} | \
    sed "/DIST_.*}\/${FROM_NAME}/s//${LOCATION}}\/${TO_NAME}/" \
    > ${T2}
  #diff ${T1} ${T2}
done
cat ${T2} > ${TMP_DIR}/${TO}
