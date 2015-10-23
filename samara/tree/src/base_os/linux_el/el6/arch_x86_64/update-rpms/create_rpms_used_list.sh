:
MY_DIR=`dirname ${0}`
OUT_DIR=.
if [ -z "${1}" ] ; then
  echo "Parameters [-d outdir ] Makefile"
  exit 0
fi 
M=${1}
if [ "${1}" = "-d" ] ; then
  OUT_DIR=${2}
  if [ -z "${OUT_DIR}" ] ; then
    echo "Parameters [-d outdir ] Makefile"
    exit
  fi
  M=${3}
fi
 if [ -z "${M}" ] ; then
   echo "Parameters [-d outdir ] Makefile"
   exit
 fi
if [ ! -f ${M} ] ; then
  echo Not a file: ${M}
  exit
fi
[ ! -d ${OUT_DIR} ] && mkdir ${OUT_DIR}
cat ${M} \
 | grep -v '^#' \
 | grep -v ^include \
 | grep -v ^-include \
 | grep -v DEFAULT_GOAL \
 | grep -v build_version \
 | grep -v "MAKE" \
 | sed /TOOLS_REQUIRED_RPMS_HOST_LINUX_EL_EL6/s//PACKAGES_BINARY_RPMS/ \
 > ${OUT_DIR}/M.inc

# Now create the list of used rpms from the M.inc file created above.
# This uses the current settings in
#    ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/customer.mk
# so that it lists only the rpms use in the product.

cat ${MY_DIR}/create_rpms_used_list.mk \
 | sed "/outputdir/s##${OUT_DIR}#" \
 | sed "/bindir/s##${MY_DIR}#" \
 > ${OUT_DIR}/M.mk
make -f ${OUT_DIR}/M.mk
if [ ! -f ${OUT_DIR}/rpms.used.list ] ; then
  echo Failed to create ${OUT_DIR}/rpms.used.list
  exit 1
fi
cat ${OUT_DIR}/rpms.used.list | sort | uniq > ${OUT_DIR}/r
mv ${OUT_DIR}/r ${OUT_DIR}/rpms.used.list
wc -l ${OUT_DIR}/rpms.used.list

rpm -qa > ${OUT_DIR}/rpms.installed.list
for i in ${OUT_DIR}/pkg.used.installed.list ${OUT_DIR}/pkg.used.notinstalled.list ${OUT_DIR}/pkg.used.unknown.list
do
  rm -f ${i}
  touch ${i}
done
for ITEM in `cat ${OUT_DIR}/rpms.used.list | grep -v UNKNOWN`
do
  grep ${ITEM} ${OUT_DIR}/rpms.installed.list >> ${OUT_DIR}/pkg.used.installed.list
  [ ${?} -ne 0 ] && echo ${ITEM} >> ${OUT_DIR}/pkg.used.notinstalled.list
done
grep UNKNOWN ${OUT_DIR}/rpms.used.list  >> ${OUT_DIR}/pkg.used.unknown.list
wc -l ${OUT_DIR}/pkg.used.installed.list ${OUT_DIR}/pkg.used.notinstalled.list ${OUT_DIR}/pkg.used.unknown.list

