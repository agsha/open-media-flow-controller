#grep _RPM= Makefile | grep PROD_LINUX_ | grep _RPMS_DIR | grep rpm | cut -f2 -d/ > A

if [ ! -z "${1}" ] ; then
  PROD_LINUX_U_ROOT=${1}
fi
if [ -z "${PROD_LINUX_U_ROOT}" ] ; then
  if [ -f ${PROD_TREE_ROOT}/src/mk/common.mk ] ; then
    PROD_LINUX_U_ROOT=`grep PROD_LINUX_U_ROOT?=/ ${PROD_TREE_ROOT}/src/mk/common.mk | cut -f2 -d=`
  fi
fi
if [ -z "${PROD_LINUX_U_ROOT}" ] ; then
  echo Error, no PROD_LINUX_U_ROOT in env, and was not specified on the command line
  echo and, no valid PROD_TREE_ROOT for finding src/mk/common.mk
  exit 1
fi

rm -f found_newer.txt
for R in `cat Makefile | grep '_RPM=.*PROD_LINUX_.*_RPMS_DIR.*rpm$' | grep -v PROD_LINUX_U_ | cut -f2 -d/`
do
  echo Test: ${R} 
  find ${PROD_LINUX_U_ROOT} -name ${R} >> found_newer.txt
done
NUM=`cat found_newer.txt | wc -l`
echo Found newer versions: ${NUM}
if [ ${NUM} -ne 0 ] ; then
  cat found_newer.txt
fi
