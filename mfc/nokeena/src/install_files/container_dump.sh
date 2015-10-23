#! /bin/bash 

RMDIR="rm -rf"
MKDIR=mkdir
MKDIR_ARGS="-p"
TAR=tar
TAR_CREATE_ARGS="-czf"


DATE_STR=`date '+%Y-%m-%d %H:%M:%S'`
DATE_STR_FILE=`echo ${DATE_STR} | sed 's/-//g' | sed 's/://g' | sed 's/ /-/g'`
OUTPUT_PREFIX=cachedump-`uname -n`-${DATE_STR_FILE}
FINAL_DIR=/nkn/tmp
WORK_DIR=/nkn/tmp/${OUTPUT_PREFIX}-$$
STAGE_DIR_REL=${OUTPUT_PREFIX}
STAGE_DIR=${WORK_DIR}/${STAGE_DIR_REL}
DUMP_FILENAME=${WORK_DIR}/${OUTPUT_PREFIX}.tgz

# since there could be spaces in the directory name, set the
# IFS to ignore space as a delimiter.
# See cache_ls.sh for how this is done.
IFS=$'\t\n'
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

	find ${dm_mount} -type f | grep -e "freeblks" -e "container" -e "attribute" > ${prefix}/tmp_file

	cat ${prefix}/tmp_file | \
	while read line
	do
        ${MKDIR} ${MKDIR_ARGS} ${prefix}/`dirname ${line}`
		dd if=${line} of=${prefix}/${line} iflag=direct status=noxfer > /dev/null 2>&1
	done
	rm -f ${prefix}/tmp_file
}


${MKDIR} ${MKDIR_ARGS} ${WORK_DIR}
${MKDIR} ${MKDIR_ARGS} ${STAGE_DIR}

dirs=`ls -d /nkn/mnt/*`
for dir in $dirs
do
	dump_contents ${STAGE_DIR} ${dir}
done

IFS=$' \t\n'


# The file is dumped as 
# dcontainer-<hostname>-<date>-<time>.tgz
#
${TAR} -C ${WORK_DIR} ${TAR_CREATE_ARGS} ${DUMP_FILENAME} ${STAGE_DIR_REL}
mv ${DUMP_FILENAME} ${FINAL_DIR}


${RMDIR} ${WORK_DIR}

echo "CONTAINER=`echo ${DUMP_FILENAME} | sed s,${WORK_DIR},${FINAL_DIR},`"
# Reset the IFS back
