#! /bin/bash 

RMDIR="rm -rf"
MKDIR=mkdir
MKDIR_ARGS="-p"
TAR=tar
TAR_CREATE_ARGS="-czf"

MDREQ=/opt/tms/bin/mdreq

DATE_STR=`date '+%Y-%m-%d %H:%M:%S'`
DATE_STR_FILE=`echo ${DATE_STR} | sed 's/-//g' | sed 's/://g' | sed 's/ /-/g'`
OUTPUT_PREFIX=amrank-`uname -n`-${DATE_STR_FILE}
FINAL_DIR=/nkn/tmp
WORK_DIR=/nkn/tmp/${OUTPUT_PREFIX}-$$
STAGE_DIR_REL=${OUTPUT_PREFIX}
STAGE_DIR=${WORK_DIR}/${STAGE_DIR_REL}
DUMP_FILENAME=${WORK_DIR}/${OUTPUT_PREFIX}.tgz


function dump_contents()
{
	prefix=$1
	dm_mount=$2

	#---------------------------------------------
    #--- This was original code which created
    #--- the tree. We do this now in a single
    #--- loop.
	#find ${dm_mount} -type d | grep -v "lost+found" > ${prefix}/tmp_file
	#cat ${prefix}/tmp_file | \
	#while read line
	#do
		#mkdir -p ${prefix}/${line}
	#done
	#rm -f ${prefix}/tmp_file
	#---------------------------------------------
    ${MDREQ} action /nkn/nvsd/am/actions/rank rank int32 -1 > ${prefix}/amrank_file

}


${MKDIR} ${MKDIR_ARGS} ${WORK_DIR}
${MKDIR} ${MKDIR_ARGS} ${STAGE_DIR}
dump_contents ${STAGE_DIR} ${dir}

# The file is dumped as 
# dcontainer-<hostname>-<date>-<time>.tgz
#
${TAR} -C ${WORK_DIR} ${TAR_CREATE_ARGS} ${DUMP_FILENAME} ${STAGE_DIR_REL}
mv ${DUMP_FILENAME} ${FINAL_DIR}


${RMDIR} ${WORK_DIR}

echo "AMDUMP=`echo ${DUMP_FILENAME} | sed s,${WORK_DIR},${FINAL_DIR},`"
