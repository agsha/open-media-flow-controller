:
# Fake mddbreq command.
# This script is only for use when developing the eth-reorder.sh script, both
# to make it possible to develop on a machine that cannot run a real mddbreq
# and to have the DB in text form, to make it easy to see the state of the DB.
# This script only has to simulate the subset of the real mddbreq command
# syntaxused by eth-reorder.sh:
# mddbreq -v ${DB_FILENAME} query get     "" ${ITEM}
# mddbreq -v ${DB_FILENAME} query iterate "" ${ITEM}
# mddbreq -c ${DB_FILENAME} set delete    "" ${ITEM}
# mddbreq -c ${DB_FILENAME} set modify    "" ${ITEM} ${TYPE} ${VALUE}

OPT1=${1}
DB_FILE=${2}
ACTION1=${3}
ACTION2=${4}
UNUSED=${5}
ITEM=${6}
TYPE=${7}
VALUE=${8}

[ ! -f ${DB_FILE} ] && touch ${DB_FILE}
TMP_FILE=/tmp/tmp.$$.tmp


case ${ACTION1}-${ACTION2} in
query-get) 
               grep "^${ITEM}=" ${DB_FILE} | cut -f2 -d=
               ;;
query-iterate) 
               grep "^${ITEM}/[0-9A-Za-z]*=" ${DB_FILE}  | cut -f2 -d=
               ;;
set-delete)
               grep -v "^${ITEM}=" ${DB_FILE} > ${TMP_FILE}
               cp ${TMP_FILE} ${DB_FILE}
               ;;
set-modify)    
               grep -q "^${ITEM}=" ${DB_FILE}
               if [ ${?} -eq 0 ] ; then
                 cat ${DB_FILE} | sed -e "s%^${ITEM}=.*%${ITEM}=${VALUE}%" > ${TMP_FILE}
                 cp ${TMP_FILE} ${DB_FILE}
               else
                 echo "${ITEM}=${VALUE}" >> ${DB_FILE}
               fi
               ;;
*)             echo unknown ${ACTION1}-${ACTION2}
               exit 1 ;;
esac
rm -f ${TMP_FILE}
