:
# Read each line in change.list and edit the Makefile to make the change.
# Six fields: BASE_RPM TYPE FROM_RPM INST LOCATION FULL_PATH_NEW_RPM
# 1: Base RPM name  (e.g. wget)
# 2: Type of rpm:  One of x86_64, i686, noarch, unknown
# 3. Full RPM filename in the Makefile
# 4. If rpm is currently installed, then "installed", else "notinstalled".
# 5. Location name of the replacement.  One of:
#            DIST_SCL_6_UPDATE3_X86_64 DIST_SCL_6_UPDATE3_I386
#            DIST_SCL_6_UPDATE2_X86_64 DIST_SCL_6_UPDATE2_I386
#            DIST_SCL_6_UPDATE1_X86_64 DIST_SCL_6_UPDATE1_I386
#            DIST_SCL_6_X86_64 DIST_SCL_6_I386
# 6. The full path to the replacement rpm file. 
# E.g.
# sysvinit-tools x86_64 sysvinit-tools-2.87-4.dsf.el6.x86_64.rpm installed DIST_SCL_6_X86_64 /volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0/x86_64/rpm/sysvinit-tools-2.87-5.dsf.SCLC6_6.R1.0.1.x86_64.rpm
# BASE_RPM       TYPE   FROM_RPM                                 INST      LOCATION          FULL_PATH_NEW_RPM

# The lines to update in the Makefile are like (without the leading "#")
#	${DIST_...}/sysvinit-tools-2.87-4.dsf.el6.x86_64.rpm \


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
  read BASE_RPM TYPE FROM_RPM INST LOCATION FULL_PATH_NEW_RPM
  [ -z ${BASE_RPM} ] && break
  if [ -z "${FULL_PATH_NEW_RPM}" ] ; then
    echo Error, problem with line:
    echo ${BASE_RPM} ${TYPE} ${FROM_RPM} ${INST} ${LOCATION} ${FULL_PATH_NEW_RPM}
    exit
  fi
  TO_NAME=`basename ${FULL_PATH_NEW_RPM}`
  cat ${T2} > ${T1}
  cat ${T1} | \
    sed "/DIST_.*}\/${FROM_RPM}/s//${LOCATION}}\/${TO_NAME}/g" \
    > ${T2}
  #diff ${T1} ${T2}
done
cat ${T2} > ${TMP_DIR}/Makefile.new