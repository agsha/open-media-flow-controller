:
VERBOSE=verbose
VERBOSE=quiet
TO_DIR=tmpdir
if [ ! -z ${1} ] ; then
  if [ -d ${1} ] ; then
    TO_DIR=${1}
    shift
  fi
fi
if [ ! -z ${1} ] ; then
  rm -f ${TO_DIR}/rpms.full.list
  for i in ${*}
  do basename ${i} >> ${TO_DIR}/rpms.full.list
  done
fi

rm -f ${TO_DIR}/srpms.list
for ITEM in `cat ${TO_DIR}/rpms.full.list`
do
  case ${ITEM} in
  compat-libstdc++-33-*)
    echo ${ITEM} | sed "/compat-libstdc++-33-[0-9].*/s//compat-libstdc++-33/" >> ${TO_DIR}/srpms.list
    ;;
  compat-libstdc++-296-*)
    echo ${ITEM} | sed "/compat-libstdc++-296-[0-9].*/s//compat-libstdc++-296/" >> ${TO_DIR}/srpms.list
    ;;
  java-*-openjdk-devel-*)
    echo ${ITEM} | sed "/\(java-.*-openjdk-devel\)-[0-9].*/s//\1/" >> ${TO_DIR}/srpms.list
    ;;
  java-*-openjdk-*)
    echo ${ITEM} | sed "/\(java-.*-openjdk\)-[0-9].*/s//\1/" >> ${TO_DIR}/srpms.list
    ;;
  *)
    echo ${ITEM} | sed "/-[0-9].*/s///" >> ${TO_DIR}/srpms.list
    ;;
  esac
done
wc -l ${TO_DIR}/srpms.list 

DIST_SCL_6=/volume/ssd-linux-storage01/repo/ssdlinux/scl6/release/6.6.R1.0
DIST_SCL_6_SRPMS=${DIST_SCL_6}/srpm
DIST_SCL_6_UPDATE1=${DIST_SCL_6}/SRs/6.6.R1.1
DIST_SCL_6_UPDATE1_SRPMS=${DIST_SCL_6_UPDATE1}/srpm
DIST_SCL_6_UPDATE2=${DIST_SCL_6}/SRs/6.6.R1.2
DIST_SCL_6_UPDATE2_SRPMS=${DIST_SCL_6_UPDATE2}/srpm
DIST_SCL_6_UPDATE3=${DIST_SCL_6}/SRs/6.6.R1.3
DIST_SCL_6_UPDATE3_SRPMS=${DIST_SCL_6_UPDATE3}/srpm
for L in DIST_SCL_6_SRPMS DIST_SCL_6_UPDATE1_SRPMS DIST_SCL_6_UPDATE2_SRPMS DIST_SCL_6_UPDATE3_SRPMS
do
  eval LOC=\$${L}
  [ ! -d ${LOC} ] && continue
  find ${LOC} -type f -name '*src.rpm' > ${TO_DIR}/srpms.${L}.list
done

# Find where the exact RPM file is located in the repo.
Find_RPM_File()
{
  THIS=${1}
  export V=n
  export SPECIAL=not
  case ${THIS} in 
  gdb-gdbserver-*) SPECIAL=special ;;
  gdb-*)           SPECIAL=special ;;
  ipsec-tools-*)   SPECIAL=special ;;
  openswan-*)      SPECIAL=special ;;
  esac
  if [ ${SPECIAL} = "special" ] ; then
    echo ${1} exact >> ${TO_DIR}/special.list
    continue
  fi
  [ ${V} = "v" ] && echo Find_RPM_File ${THIS}
  LOCATION=notfound
  for L in DIST_SCL_6_SRPMS DIST_SCL_6_UPDATE1_SRPMS DIST_SCL_6_UPDATE2_SRPMS DIST_SCL_6_UPDATE3_SRPMS
  do
    eval LOC=\$${L}
    [ ${V} = "v" ] && echo check ${TO_DIR}/rpms.${L}.list
    FULLPATH=`grep /${THIS}.src.rpm ${TO_DIR}/srpms.${L}.list`
    [ -z "${FULLPATH}" ] && continue
    [ ${V} = "v" ] && echo Found ${FULLPATH} 
    LOCATION=${L}
    break
  done
}

# Search the rpm files for the newest instance of a base rpm name
Search_RPMs()
{
  THIS=${1}
  export V=n
  export SPECIAL=not
  case ${THIS} in 
  gdb-gdbserver) SPECIAL=special ;;
  gdb)           SPECIAL=special ;;
  ipsec-tools)   SPECIAL=special ;;
  openswan)      SPECIAL=special ;;
  esac
  [ ${V} = "v" ] && echo Search_RPMs ${THIS}
  [ ${V} = "v" ] && set -x
  if [ ${SPECIAL} = "special" ] ; then
    echo ${1} ${2} >> ${TO_DIR}/special.list
    continue
  fi

  LOCATION=notfound
  for L in DIST_SCL_6_UPDATE3_SRPM DIST_SCL_6_UPDATE2_SRPMS DIST_SCL_6_UPDATE1_SRPMS DIST_SCL_6_SRPMS
  do
    eval LOC=\$${L}
    [ ${V} = "v" ] && echo check ${TO_DIR}/srpms.${L}.list
    [ ! -f ${TO_DIR}/srpms.${L}.list ] && continue
    FULLPATH=`grep "/${THIS}-[0-9]" ${TO_DIR}/srpms.${L}.list`
    [ -z "${FULLPATH}" ] && continue
    echo ${FULLPATH} | grep " " > /dev/null
    if [ $? -eq 0 ] ; then
      echo WARNING: Found two of ${THIS} in ${TO_DIR}/srpms.${L}.list
      FULLPATH=`echo ${FULLPATH} | cut -f1 -d' '`
    fi
    [ ${V} = "v" ] && echo Found ${FULLPATH} 
    LOCATION=${L}
    break
  done
  case ${THIS} in 
  compat-libstdc++*) set nolist ;;
  esac
  [ ${V} = "v" ] && set +x
}

rm -f ${TO_DIR}/change.list ;          touch ${TO_DIR}/change.list
rm -f ${TO_DIR}/change-notfound.list ; touch ${TO_DIR}/change-notfound.list
rm -f ${TO_DIR}/special.list ;         touch ${TO_DIR}/special.list
  for RPM in `cat ${TO_DIR}/srpms.list`
  do
    [ -z "${RPM}" ] && continue
    FULL=`grep "^${RPM}-[0-9]" ${TO_DIR}/rpms.full.list`
    [ ${VERBOSE} = "verbose" ] && \
      echo == RPM=${RPM} F=${FULL}
    if [ -z "${FULL}" ] ; then
      echo Error, Did not find FULL: ${RPM} in ${TO_DIR}/rpms.full.list
      continue
    fi
    [ ${VERBOSE} = "verbose" ] && \
      echo ==== I=${INST}
    # TBD do a Search or Find ?
    Search_RPMs ${RPM}
    #Find_RPM_File ${RPM}
    if [ ${LOCATION} = "notfound" ] ; then
      echo ${RPM} ${FULL} DIST_UNKNOWN /unknown/${RPM}-0.UNKNOWN.${TYPE}.rpm >> ${TO_DIR}/change-notfound.list
    else
      if [ -z "${FULLPATH}" ] ; then
        echo Error, FULLPATH is null
        echo ${RPM} ${FULL}
        exit 1
      fi
      echo ${RPM} ${FULL} ${LOCATION} ${FULLPATH} >> ${TO_DIR}/change.list
    fi
  done
[ ${VERBOSE} = "verbose" ] && \
  echo ===
