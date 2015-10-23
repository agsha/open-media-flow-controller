#! /bin/bash
echo "${*}"
TO_IMG_FILE=${1}

MD5SUMS_FILE=md5sums
TPKG_MANIFEST=tpkg-manifest

if [ "_${1}" = "_" -o "_${2}" = "_" ] ; then
  echo Incorrect parameters. Two required parameters:
  echo ' image-file  file-to-add  [file-to-add ... ]'
  echo The image-file can be specified either full or relative path.
  echo Note: In the current directory these two files are created and removed:
  echo '  '${MD5SUMS_FILE} ${TPKG_MANIFEST}
  echo If each file-to-add is not in the current directory then a symlink
  echo to it is temporarily created in the current directory.
  exit 1
fi

if [ ! -f ${TO_IMG_FILE} ] ; then
  echo Error: No such file ${TO_IMG_FILE}
  exit 1
fi
shift
ERRS=0
for i in ${*}
do
  [ -f ${i} ] && continue
  echo Error: No such file ${i}
  ERRS=1
done
if [ ${ERRS} -ne 0 ] ; then
  exit 1
fi

# Check if the file is a Zip file
file ${TO_IMG_FILE} | grep "Zip archive data" > /dev/null
if [ ${?} -ne 0 ] ; then
  echo Error: ${TO_IMG_FILE} not a zip file
fi

PROD_OUTPUT_DIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64
INSTALL_DIR=${PROD_OUTPUT_DIR}/install
LFI_EXE=${INSTALL_DIR}/image/bin/lfi

Cleanup ()
{
  for i in ${MD5SUMS_FILE} ${MD5SUMS_FILE}.tmp ${TPKG_MANIFEST} ${TPKG_MANIFEST}.tmp
  do
    rm -rf ${i}
    [ ! -f ${i} ] && continue
    echo Error: Failed to remove ${i}
    exit 1
  done
}

CP_NAME=
Check_Err ()
{
  [ ${1} -eq 0 ] && return
  echo "Error: ${2}"
  echo
  Cleanup
  [ ! -z "${CP_NAME}" ] && rm -f ${CP_NAME}
  exit 1
}


Add_File_To_Zip ()
{
  # Replace md5sums file with an updated version that has the added file.
  Cleanup

  unzip ${TO_IMG_FILE} ${MD5SUMS_FILE}
  Check_Err $? "unzip failed to extract ${MD5SUMS_FILE} from ${TO_IMG_FILE}, exiting ..."

  grep -v ${ADD_THIS_FILE} ${MD5SUMS_FILE} > ${MD5SUMS_FILE}.tmp
  md5sum ./${ADD_THIS_FILE} >> ${MD5SUMS_FILE}.tmp
  Check_Err $? "md5sum for ${ADD_THIS_FILE} failed, exiting ..."
  mv ${MD5SUMS_FILE}.tmp ${MD5SUMS_FILE}

  zip -r ${TO_IMG_FILE} ${MD5SUMS_FILE}

  # Replace tpkg-manifest with updated md5sums and the added file.
  unzip ${TO_IMG_FILE} ${TPKG_MANIFEST}
  Check_Err $? "unzip failed to extract ${TPKG_MANIFEST} from ${TO_IMG_FILE}, exiting ..."

  grep -v ${MD5SUMS_FILE} ${TPKG_MANIFEST} | grep -v ${ADD_THIS_FILE}  > ${TPKG_MANIFEST}.tmp
  ${LFI_EXE} -tgunm ./${MD5SUMS_FILE} >> ${TPKG_MANIFEST}.tmp
  ${LFI_EXE} -tgunm -a sha256 ./${MD5SUMS_FILE} >> ${TPKG_MANIFEST}.tmp
  ${LFI_EXE} -tgunm ./${ADD_THIS_FILE} >> ${TPKG_MANIFEST}.tmp
  ${LFI_EXE} -tgunm -a sha256 ./${ADD_THIS_FILE} >> ${TPKG_MANIFEST}.tmp
  mv ${TPKG_MANIFEST}.tmp ${TPKG_MANIFEST}

  zip -r ${TO_IMG_FILE} ${TPKG_MANIFEST}

  # Easy step is to add to the zip
  zip -g ${TO_IMG_FILE} ${ADD_THIS_FILE}
  Check_Err $? "zip failed to add ${ADD_THIS_FILE} to ${TO_IMG_FILE}, exiting ..."

  Cleanup
}

S="`ls -l ${TO_IMG_FILE} | awk '{print $5}'`"
BEFORE_MB="$(( S / 1024 / 1024 ))"
for ADD_THIS_FILE in ${*}
do
  CP_NAME=
  if [ ! -f ${ADD_THIS_FILE} ] ; then
    echo "Warning, no such file ${ADD_THIS_FILE}"
    continue
  fi
  T=`basename ${ADD_THIS_FILE}`
  [ "${ADD_THIS_FILE}" = "./${T}" ] && ADD_THIS_FILE=${T}
  echo ${ADD_THIS_FILE} | grep -q /
  if [ ${?} -eq 0 ] ; then
    CP_NAME=`basename ${ADD_THIS_FILE}`
    if [ -f ${CP_NAME} ] ; then
       echo Error: ${ADD_THIS_FILE} is not in current directory and
       echo a file of the same name is in the current directory.
       echo This is not allowed because we need to copy it into this dir.
       exit 1
    fi
    cp -a ${ADD_THIS_FILE} ${CP_NAME}
    ADD_THIS_FILE=${CP_NAME}
  fi
  Add_File_To_Zip
  [ ! -z "${CP_NAME}" ] && rm -f ${CP_NAME}
done
S="`ls -l ${TO_IMG_FILE} | awk '{print $5}'`"
AFTER_MB="$(( S / 1024 / 1024 ))"
echo "Size MB before, change and after: ${BEFORE_MB} $(( AFTER_MB - BEFORE_MB )) ${AFTER_MB}"

exit 0
