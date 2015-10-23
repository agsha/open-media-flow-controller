#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2012 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 -s \"Subject\" -t \"addr@example.com\" -c \"cc@example.com\" [-i inline.txt] [-S sendmail_opts] [-P preamble.txt] [-m <mime type>] [-o outputfile] [--stdin] [[attach1.txt] ...]"
    echo "    -s: subject"
    echo "    -t: address for \"To\" header"
    echo "    -c: address for \"Cc\" header"
    echo "    -i: filename for text of email body"
    echo "    -S: options to pass on to sendmail"
    echo "    -P: preamble to replace \"This is a multi-part message in MIME format.\""
    echo "    -m: MIME type to be used for ALL attachments"
    echo "    -o: instead of sending mail, save mail in a file"
    echo "    --stdin: in lieu of -s, -t, -c, and -i, take headers from "
    echo "             stdin until first blank line, then email body from "
    echo "             what remains."
    echo "    Rest of parameters: names of files to attach to email"
    echo ""
    exit 1
}

PARSE=`/usr/bin/getopt -s sh -l 'stdin' 's:t:c:i:S:P:m:o:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"
    
MAIL_SUBJECT=
ADDR_TO=
ADDR_CC=
INLINE_FILE=
SENDMAIL_OPTS=
PREAMBLE_FILE=
PREAMBLE_STRING="This is a multi-part message in MIME format."
MIME_TYPE=application/octet-stream
DO_MAIL=1
DO_STDOUT=0
OUTPUT_FILE=
STDIN=0

# Customer.sh should set the EMAIL_HEADER_BRANDING variable for us.
# But set it ourselves first in case it doesn't.
EMAIL_HEADER_BRANDING=Samara
if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

while true ; do
    case "$1" in
        -s) MAIL_SUBJECT=$2; shift 2 ;;
        -t) ADDR_TO=$2; shift 2 ;;
        -c) ADDR_CC=$2; shift 2 ;;
        -i) INLINE_FILE=$2; shift 2 ;;
        -S) SENDMAIL_OPTS=$2; shift 2 ;;
        -P) PREAMBLE_FILE=$2; shift 2 ;;
        -m) MIME_TYPE=$2; shift 2 ;;
        -o) DO_MAIL=0; OUTPUT_FILE=$2; shift 2 ;;
        --stdin) STDIN=1; shift ;;
        --) shift ; break ;;
        *) echo "makemail.sh: parse failure: $1" >&2 ; usage ;;
    esac
done

ATTACH_FILES="$*"

if [ ! -z "${MAIL_SUBJECT}" -o ! -z "${ADDR_TO}" -o \
     ! -z "${ADDR_CC}" -o ! -z "${INLINE_FILE}" ] ; then
    if [ ${STDIN} -eq 1 ]; then
        echo "Cannot specify -s, -t, -c, or -i if you specify --stdin"
        usage
    fi
fi

if [ -z "${MAIL_SUBJECT}" ] ; then
    MAIL_SUBJECT="No subject"
fi

if [ -z "${ATTACH_FILES}" -a -z "${INLINE_FILE}" -a ${STDIN} -eq 0 ] ; then
    echo "Must specify attachments, or an inline file, or --stdin"
    usage
fi

if [ -z "${ADDR_TO}" -a ${STDIN} -eq 0 ]; then
    echo "Must specify a To address with -t, or use --stdin"
    usage
fi

if [ ${DO_MAIL} -eq 0 ]; then
    if [ "${OUTPUT_FILE}" = "-" ]; then
        DO_STDOUT=1
        OUTPUT_FILE=
    fi
fi

if [ -z "${OUTPUT_FILE}" ]; then
    OUTPUT_FILE=/tmp/mm-temp-$$
    rm -f ${OUTPUT_FILE}
    touch ${OUTPUT_FILE}
    chmod 600 ${OUTPUT_FILE}
fi

if [ -f /opt/tms/release/build_version.sh ]; then
    . /opt/tms/release/build_version.sh
fi

# gnu uuencode wants to print a header and footer we don't want
B64CONV_1="uuencode -m -"
B64CONV_2="sed -e 1d -e \$d"

MIME_BOUNDARY="`dd if=/dev/urandom bs=15 count=1 2> /dev/null | ${B64CONV_1} | ${B64CONV_2}`"

if [ ${STDIN} -eq 0 ]; then
    echo "To: ${ADDR_TO}" >> ${OUTPUT_FILE}
    if [ ! -z "${ADDR_CC}" ]; then
        echo "Cc: ${ADDR_CC}"  >> ${OUTPUT_FILE}
    fi
    echo "Subject: ${MAIL_SUBJECT}"  >> ${OUTPUT_FILE}
else
    while read line ; do
        if [ -z "${line}" ]; then
            break
        fi
        echo "$line" >> ${OUTPUT_FILE}
    done
fi

echo "Mime-Version: 1.0"  >> ${OUTPUT_FILE}
echo "Content-Type: multipart/mixed; boundary=\"${MIME_BOUNDARY}\""  >> ${OUTPUT_FILE}
if [ ! -z "${INLINE_FILE}" -o ${STDIN} -eq 1 ]; then
    echo "Content-Disposition: inline"  >> ${OUTPUT_FILE}
fi
echo "User-Agent: tmsmakemail/1.0"  >> ${OUTPUT_FILE}

# It is assumed these would come in on stdin
if [ ${STDIN} -eq 0 ]; then
    echo "X-TMS-${EMAIL_HEADER_BRANDING}-build_tms_srcs_id: ${BUILD_TMS_SRCS_ID}"  >> ${OUTPUT_FILE}
    echo "X-TMS-${EMAIL_HEADER_BRANDING}-build_prod_customer: ${BUILD_PROD_CUSTOMER}"  >> ${OUTPUT_FILE}
    echo "X-TMS-${EMAIL_HEADER_BRANDING}-build_prod_product: ${BUILD_PROD_PRODUCT}"  >> ${OUTPUT_FILE}
    echo "X-TMS-${EMAIL_HEADER_BRANDING}-build_prod_id: ${BUILD_PROD_ID}"  >> ${OUTPUT_FILE}
    echo "X-TMS-${EMAIL_HEADER_BRANDING}-build_prod_release: ${BUILD_PROD_RELEASE}"  >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
fi

if [ ! -z "${PREAMBLE_FILE}" ]; then
    cat ${PREAMBLE_FILE} >> ${OUTPUT_FILE}
elif [ ! -z "${PREAMBLE_STRING}" ]; then
    echo "${PREAMBLE_STRING}" >> ${OUTPUT_FILE}
fi

# Inline text
if [ ! -z "${INLINE_FILE}" -o ${STDIN} -eq 1 ]; then
    echo "" >> ${OUTPUT_FILE}
    echo "--${MIME_BOUNDARY}" >> ${OUTPUT_FILE}
    echo "Content-Type: text/plain; charset=us-ascii" >> ${OUTPUT_FILE}
    echo "Content-Disposition: inline" >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
    if [ ${STDIN} -eq 0 ]; then
        cat ${INLINE_FILE} >> ${OUTPUT_FILE}
    else
        cat >> ${OUTPUT_FILE}
    fi
fi

# Attachments
for attach in ${ATTACH_FILES}; do
    attach_base=`basename $attach`
    echo "" >> ${OUTPUT_FILE}
    echo "--${MIME_BOUNDARY}" >> ${OUTPUT_FILE}
    echo "Content-Type: ${MIME_TYPE}" >> ${OUTPUT_FILE}
    echo "Content-Disposition: attachment; filename=\"$attach_base\"" >> ${OUTPUT_FILE}
    echo "Content-Transfer-Encoding: base64" >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
    cat ${attach} | ${B64CONV_1} | ${B64CONV_2} >> ${OUTPUT_FILE}
done

echo "" >> ${OUTPUT_FILE}
echo "--${MIME_BOUNDARY}--" >> ${OUTPUT_FILE}
echo "" >> ${OUTPUT_FILE}

if [ ${DO_MAIL} -eq 1 ]; then
    cat ${OUTPUT_FILE} | /usr/lib/sendmail ${SENDMAIL_OPTS} -t
    rm ${OUTPUT_FILE}
fi

if [ ${DO_STDOUT} -eq 1 ]; then
    cat ${OUTPUT_FILE}
    rm ${OUTPUT_FILE}
fi

exit 0
