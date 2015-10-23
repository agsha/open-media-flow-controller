:

function Look_For_Customer_Root()
{
  # Look for known directory pattern that shows that we are at the root of
  # a MFx source tree.
  # When found set PROD_PRODUCT and PROD_CUSTOMER_ROOT to match and return 0.
  if [ -z "${PROD_PRODUCT}" ] ; then
    for i in */src/mk/build_settings.dot
    do
      [ ! -f ${i} ] && continue
      export PROD_PRODUCT=`echo ${i} | cut -f1 -d/`
      export PROD_PRODUCT_LC=${PROD_PRODUCT}
      export PROD_CUSTOMER_ROOT=`pwd`
      echo Setting PROD_PRODUCT="${PROD_PRODUCT}" 1>&2
      echo Setting PROD_CUSTOMER_ROOT="${PROD_CUSTOMER_ROOT}" 1>&2
      return 0
    done
  elif [ -f ${PROD_PRODUCT}/src/mk/build_settings.dot ] ; then
    export PROD_PRODUCT
    export PROD_PRODUCT_LC=${PROD_PRODUCT}
    export PROD_CUSTOMER_ROOT=`pwd`
    echo Setting PROD_CUSTOMER_ROOT="${PROD_CUSTOMER_ROOT}" 1>&2
    return 0
  fi
  # At this point we know that the curdir is not PROD_CUSTOMER_ROOT.

  # Look for known directory pattern that shows that we are at the root of
  # the MFx source tree as specified by the first parameter.
  #  When found set PROD_CUSTOMER_ROOT to it and return 0.
  # We are either in or under the actual PROD_PRODUCT dir.
  SAVE1_DIR=`pwd`
  if [ -z "${PROD_PRODUCT}" ] ; then
    COUNT=0
    while true; do
      COUNT=$(( COUNT + 1 ))
      if [ ${COUNT} -gt 10 ] ; then
        echo Source tree not found 1>&2
        return 1
      fi
      if [ -f src/mk/build_settings.dot ] ; then
        export PROD_PRODUCT=`basename ${PWD}`
        export PROD_PRODUCT_LC=${PROD_PRODUCT}
        cd ..
        export PROD_CUSTOMER_ROOT=`pwd`
        cd ${SAVE1_DIR}
        echo Setting PROD_PRODUCT="${PROD_PRODUCT}" 1>&2
        echo Setting PROD_CUSTOMER_ROOT="${PROD_CUSTOMER_ROOT}" 1>&2
        return 0
      fi
      cd ..
    done
  else
    COUNT=0
    while true; do
      COUNT=$(( COUNT + 1 ))
      if [ ${COUNT} -gt 10 ] ; then
        echo Source tree not found 1>&2
        return 1
      fi
      if [ -f ${PROD_PRODUCT}/src/mk/build_settings.dot ] ; then
        export PROD_PRODUCT
        export PROD_PRODUCT_LC=${PROD_PRODUCT}
        export PROD_CUSTOMER_ROOT=`pwd`
        echo Setting PROD_CUSTOMER_ROOT="${PROD_CUSTOMER_ROOT}" 1>&2
        return 0
      fi
    done
  fi
  return 1
}
Look_For_Customer_Root
