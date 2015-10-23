# Create the freedist archive and install into the release directory where
# it will get picked up to put on the release ISO and set of release files.

FD_OUT=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/freedist
COPY_TO_DIR=${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist

if [ ! -f ${FD_OUT}/freedistinfo.sh ] ; then
  cd ${PROD_TREE_ROOT}/src/release/freedist
  # Create the freedist*.tgz and freedistinfo.sh files
  make
else
  echo Already exists: ${FD_OUT}/freedistinfo.sh
fi
if [ ! -f ${FD_OUT}/freedistinfo.sh ] ; then
  echo Failed to create freedistinfo.sh
  exit 1
fi

# This sets FREEDIST_FILENAME
. ${FD_OUT}/freedistinfo.sh
if [ ! -f ${FD_OUT}/release/${FREEDIST_FILENAME} ] ; then
  echo Failed to create ${FD_OUT}/release/${FREEDIST_FILENAME}
  exit 1
fi

[ ! -d ${COPY_TO_DIR} ] && mkdir -p ${COPY_TO_DIR}
if [ ! -d ${COPY_TO_DIR} ] ; then
  echo Failed to create ${COPY_TO_DIR}
  exit 1
fi

echo Install ${FREEDIST_FILENAME} in ${COPY_TO_DIR}/
cp ${FD_OUT}/release/${FREEDIST_FILENAME} ${COPY_TO_DIR}/
ls -l ${COPY_TO_DIR}/${FREEDIST_FILENAME}
