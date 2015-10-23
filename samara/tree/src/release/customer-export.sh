#!/bin/sh


#
# 
#
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

usage()
{
    echo "usage: $0 -o OUTPUT_DIR [-a CVS_TAG_TO_CREATE] " 
    echo "          [-d EXPORT_FROM_DATE | -r EXPORT_FORM_TAG_OR_REVISION]"
    echo "          [-w TMP_WORK_DIR] "
    echo ""
    exit 1
}

PARSE=`/usr/bin/getopt 'o:a:d:r:w:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

# Defaults
CVS=/usr/bin/cvs
TAR=/bin/tar

# Set CVSROOT to be the same as that of the directory we're called from
unset CVSROOT
CVSROOT=`cat CVS/Root`
export CVSROOT

ADD_CVS_TAG_NAME=
OUTPUT_DIR=
WORK_DIR=/tmp/customer-export-temp.$$
SELECT_DATE=
SELECT_REV=

while true ; do
    case "$1" in
        -o) OUTPUT_DIR=$2; shift 2 ;;
        -a) ADD_CVS_TAG_NAME=$2; shift 2 ;;
        -d) SELECT_DATE=$2; shift 2 ;;
        -r) SELECT_REV=$2; shift 2 ;;
        -w) WORK_DIR=$2/customer-export-temp.$$; shift 2 ;;
        --) shift ; break ;;
        *) echo "customer-export.sh: parse failure: '$1'" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

if [ -z "${OUTPUT_DIR}" ]; then
    echo "Error: Must specify '-o outputdir'"
    echo ""
    usage
fi

if [ -z "${WORK_DIR}" ]; then
    echo "Error: bad work directory '${WORK_DIR}' specified"
    echo ""
    usage
fi

if [ ! -z "${SELECT_DATE}" -a ! -z "${SELECT_REV}" ]; then
    echo "Error: cannot specify both a selection date and revision."
    echo ""
    usage
fi

if [ -z "${SELECT_DATE}" -a -z "${SELECT_REV}" ]; then
    echo "-- Note: assuming you want the HEAD revision"
    SELECT_REV=HEAD
fi

# Get a place to work in

rm -f ${WORK_DIR}
mkdir -p ${WORK_DIR}
mkdir -p ${OUTPUT_DIR}

select_string=""

# Convert 'HEAD' -> current date
if [ "${SELECT_REV}" = "HEAD" ]; then
    DATE_STR=`date '+%Y-%m-%d %H:%M:%S'`
    DATE_STR_FILE=`echo ${DATE_STR} | sed 's/-//g' | sed 's/://g' | sed 's/ /-/g'`
    SELECT_DATE=${DATE_STR}
    SELECT_REV=
    select_is_head=1
    echo "-- HEAD will be the timestamp: ${SELECT_DATE}"
fi

if [ ! -z "${SELECT_DATE}" ]; then
    select_string="date ${SELECT_DATE}"
    select_is_date=1
fi
if [ ! -z "${SELECT_REV}" ]; then
    select_string="rev ${SELECT_REV}"
    select_is_date=0
fi

# First, add any requested tag

if [ ! -z "${ADD_CVS_TAG_NAME}" ]; then
    echo "-- About to add a tag called '${ADD_CVS_TAG_NAME}' at ${select_string}"
    echo ""

    if [ ${select_is_date} ]; then
        ${CVS} rtag -D "${SELECT_DATE}" ${ADD_CVS_TAG_NAME} .
    else
        ${CVS} rtag -r "${SELECT_REV}" ${ADD_CVS_TAG_NAME} .
    fi

    # Update where we'll export from, as we have a tag now
    select_string="${ADD_CVS_TAG_NAME}"
    SELECT_REV=${ADD_CVS_TAG_NAME}
    select_is_date=0
fi


# Export the source tree
mkdir -p ${WORK_DIR}/tree
echo "-- About to export at ${select_string} ."

if [ ${select_is_date} ]; then
    (cd ${WORK_DIR}; ${CVS} -Q export -d tree -nN -D "${SELECT_DATE}" . )
else
    (cd ${WORK_DIR}; ${CVS} -Q export -d tree -nN -r "${SELECT_REV}" . )
fi

# Do a cvs rlog of what we just exported
mkdir -p ${WORK_DIR}/rlog

echo "-- About to do cvs rlog at ${select_string} ."


if [ ${select_is_date} ]; then
    ${CVS} -Q rlog -S -N -d "${SELECT_DATE}" . > ${WORK_DIR}/rlog/cvs-rlog-full.txt
else
    ${CVS} -Q rlog -S -N -r"${SELECT_REV}" . > ${WORK_DIR}/rlog/cvs-rlog-full.txt
fi
 
# Produce a stripped-down version which is easier to read
cat ${WORK_DIR}/rlog/cvs-rlog-full.txt | egrep '(^RCS file: |^revision [\.0-9]*$)' | sed 's/^RCS file/\nRCS file/' > ${WORK_DIR}/rlog/cvs-rlog.txt


cp ${WORK_DIR}/tree/README ${WORK_DIR}/README

echo "-- About to create md5s of all the files."
(cd ${WORK_DIR}; find . -type f -print0 |xargs -0 -r md5sum > /tmp/ce-md5sum.$$ )
mv /tmp/ce-md5sum.$$ ${WORK_DIR}/md5sums

# Ball up the tree, put it in the output dir 
echo "-- About to make a tarball of all the files."

GZIP="--rsyncable -9"
export GZIP

fds=NONE
if [ ${select_is_date} -eq 1 ]; then
    fds=`echo ${SELECT_DATE} | tr -d '[/\-:]' | sed 's/[^a-zA-Z0-9_-]/_/g'`
else
    if [ -z "${ADD_CVS_TAG_NAME}" ]; then
        fds=`echo ${SELECT_REV} | sed 's/[^a-zA-Z0-9_-]/_/g'`
    else
        fds=`echo ${ADD_CVS_TAG_NAME} | sed 's/[^a-zA-Z0-9_-]/_/g'`
    fi
fi
export_filename=tms-export-src-${fds}.tgz
exportinfo_filename=tms-export-info-${fds}.tgz


(cd ${WORK_DIR}; ${TAR} -czf ${OUTPUT_DIR}/${export_filename} .)
md5sum ${OUTPUT_DIR}/${export_filename} > ${OUTPUT_DIR}/tms-export-src-${fds}-md5sum

echo "-- About to make the info tarball of all the files."

mkdir -p ${WORK_DIR}/infostage
cp -p ${OUTPUT_DIR}/tms-export-src-${fds}-md5sum ${WORK_DIR}/infostage
cp -p ${WORK_DIR}/rlog/* ${WORK_DIR}/infostage
cp -pr ${WORK_DIR}/md5sums ${WORK_DIR}/infostage/exported-file-md5sums

(cd ${WORK_DIR}/infostage; ${TAR} -czf ${OUTPUT_DIR}/${exportinfo_filename} .)
md5sum ${OUTPUT_DIR}/${exportinfo_filename} > ${OUTPUT_DIR}/tms-export-info-${fds}-md5sum

echo "-- Cleaning up"
rm -rf ${WORK_DIR}

echo "-- Output in: ${OUTPUT_DIR}"
exit 0
