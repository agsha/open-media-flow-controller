# Encode a file for uploading via 'wget --post-file=...'.
# This generates the text that follows "--post-file=" on the wget command line.
# First param is the name of the file to upload. (required)
# Second param is a filename to use for output.
#  When not specified, or if "." is specified, an output file is created
#  with the same name as the input filename plus ".post" appended.
#  When "-" is specified, the generated text is printed to stdout.
#  When "-" is NOT used, then the text is written to the output
#     file and the name of the output file is printed to stdout.
# Any following command line args are prepended to the text output line
# with "%0A&" in between as the delimiter.

FILE="${1}"
if [ -z "${FILE}" ] ; then
  echo Specify a file
  exit 1
fi
if [ ! -f "${FILE}" ] ; then
  echo File does not exist: "${FILE}"
  exit 1
fi
shift
OUTF="${1}"
[ ! -z "${OUTF}" ] && shift

EXTRA_ARGS=
for I in ${*} ;
do
  EXTRA_ARGS="${EXTRA_ARGS}${I}"'%0A&'
done

case "_${OUTF}" in
_|_.)  OUTF=${FILE}.post ;;
_-)    OUTF= ;;
esac
if [ ! -z "${OUTF}" ] ; then
  rm -f "${OUTF}"
  touch "${OUTF}"
  if [ ! -f "${OUTF}" ] ; then
    echo Cannot create "${OUTF}"
    exit 1
  fi
  rm -f "${OUTF}"
  echo "${EXTRA_ARGS}"filedata=`base64 -w 0 ${FILE}`'%0A&filename='`basename ${FILE}` > ${OUTF}
  echo ${OUTF}
else
  echo "${EXTRA_ARGS}"filedata=`base64 -w 0 ${FILE}`'%0A&filename='`basename ${FILE}`
fi
