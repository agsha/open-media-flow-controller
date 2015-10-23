#!/bin/bash
##Following 5 variable should be set by caller
## this script should be put into TOOL_BASE/dmi-tools/debug

RPC_NAMES="mfc-system-details interfaces-detail interface-list interface-configured interface-brief interface-stats error-info resource-pool-list resource-pool-detail get-cmd-response op-cmd-response mfc-vm-details mfc-vm-volume-details bond-detail bond-list mfc-crawler-list mfc-crawler-detail mfc-service-info mfc-schema-version mfc-config-version mfc-backup-file mfc-all-stats"
SB_NAME=pacifica
SVN_ID=487792
RE_APP_SRC=/homes/dgautam/re-app-12.x/src
SRC_BASE=/volume/bng-nfsbuild6/dgautam/vjx-12-1
TOOL_BASE=/volume/bng-nfsbuild6/dgautam

SANDBOX_BASE=${SRC_BASE}/${SB_NAME}
SANDBOX_BASE_ESCAPED='\/volume\/bng-nfsbuild6\/dgautam\/vjx-12-1\/pacifica'
SANDBOX_SRC=${SANDBOX_BASE}/src
SANDBOX_OBJ=${SANDBOX_BASE}/obj-i386
SANDBOX_FB_OBJ=${SANDBOX_BASE}/obj-freebsd7-i386
DMI_TOOLS=${TOOL_BASE}/dmi-tools
#MY_LD_LIB_PATH=${SANDBOX_OBJ}/bsd/gnu/lib/libgcc
MY_LD_LIB_PATH=/homes/dgautam/local/lib:${SANDBOX_OBJ}/bsd/gnu/lib/libgcc
#MY_PERL_LIB=${DMI_TOOLS}/sw-tools/buildtools/dmi-tools:${DMI_TOOLS}/local.lib/perl
MY_PERL_LIB=/volume/bng-nfsbuild6/dgautam/perl/lib/5.18.1:/volume/bng-nfsbuild6/dgautam/perl/lib/5.18.1/i386-freebsd/:${DMI_TOOLS}/sw-tools/buildtools/dmi-tools:${DMI_TOOLS}/local.lib/perl

MY_CURR_DIR=`pwd`
DATE_STR=`date '+%Y-%m-%d %H:%M:%S'`
DATE_STR_FILE=`echo ${DATE_STR} | sed 's/-//g' | sed 's/://g' | sed 's/ /-/g'`
SCHEMA_DIR=${DMI_TOOLS}/debug/schema-${DATE_STR_FILE}


DDL_DIR=${SANDBOX_SRC}/junos/lib/ddl/input
ODL_DIR=${SANDBOX_SRC}/junos/lib/odl/input

RE_DDL_DIR=${RE_APP_SRC}/lib/ddl/input
RE_ODL_DIR=${RE_APP_SRC}/lib/odl/input
ODL_XML_TAGS=${SANDBOX_OBJ}/junos/lib/odl/xmltags

export PATH=/volume/bng-nfsbuild6/dgautam/perl/bin:$PATH
export LD_LIBRARY_PATH=${MY_LD_LIB_PATH}:${LD_LIBRARY_PATH}
export PERL5LIB=${MY_PERL_LIB}

echo ${PERL5LIB}
echo ${LD_LIBRARY_PATH}
echo ${DDL_DIR}

function clean_ddl()
{
    cd ${DDL_DIR}
    files=`svn status | cut -d ' '  -f 8 | xargs rm -fr`;
    echo ${files};
    cd ${MY_CURR_DIR}
}
function svn_update_ddl()
{
    cd ${DDL_DIR}
    files=`svn update -r ${SVN_ID}`;
    echo ${files};
    cd ${MY_CURR_DIR}
}
function copy_re_app_ddl()
{
    cp ${RE_DDL_DIR}/*.dd ${DDL_DIR};
    cd ${DDL_DIR}
    svn status
    echo "" >> input.mk
    echo "" >> input.mk
    echo "INPUT_FEATURES+= media-flow" >> input.mk
    echo "media-flow_INPUT_FILES += \\" >> input.mk
    cat ${RE_DDL_DIR}/media-flow.input.mk | grep ".dd" | grep -v "#" | grep -v "templates\/" >> input.mk
    tail -30 input.mk
    cd ..
    mk
    cd ${MY_CURR_DIR}
}

function clean_odl()
{
    cd ${ODL_DIR}
    files=`svn status | cut -d ' '  -f 8 | xargs rm `;
    echo ${files};
    cd ${MY_CURR_DIR}
}
function svn_update_odl()
{
    cd ${ODL_DIR}
    files=`svn update -r ${SVN_ID}`;
    echo ${files};
    cd ${MY_CURR_DIR}
}
function copy_re_app_odl()
{
    cp ${RE_ODL_DIR}/*.odl ${ODL_DIR};
    cd ${ODL_DIR}
    svn status
    echo "" >> input.mk
    echo "" >> input.mk
    echo "INPUT_FEATURES += media-flow-controller" >> input.mk
    echo "media-flow-controller_INPUT_FILES += \\" >> input.mk
    cat ${RE_ODL_DIR}/media-flow-controller.input.mk | grep ".odl" | grep -v "#" >> input.mk
    tail -30 input.mk
    cd ..
    mk
    cd ${MY_CURR_DIR}
}
function make_setup()
{
    mkdir -p ${SCHEMA_DIR}
    cd ${SCHEMA_DIR}
    mkdir -p dmi/junos/releases/${SB_NAME}
    mkdir -p tmp/ddl_so
    ln -s ${SANDBOX_OBJ}/junos/lib/ddl/feature/libmedia-flow-dd.so tmp/ddl_so/libmedia-flow-dd.so
    ln -s ${SANDBOX_OBJ}/junos/lib/ddl/feature/libmedia-flow-actions-dd.so tmp/ddl_so/libmedia-flow-actions-dd.so
    cd ${MY_CURR_DIR}
}

function generate_config_xsd()
{
    cd ${SCHEMA_DIR}
    ${SANDBOX_FB_OBJ}/junos/lib/ddl/gram/gxsl -x -t ./tmp/ddl_so/ -j c -F ./tmp/ddl_so/ > tmp/config.xsd.error
    sed 's/http:\/\/www.w3.org\/2001\/[ 	]*XMLSchema/http:\/\/www.w3.org\/2001\/XMLSchema/g' tmp/config.xsd.error > tmp/config.xsd
    ../../dmi-convert-schema.pl -c -f junos -s ${SANDBOX_BASE} -x ../junos-invalid-paths.txt -o dmi/junos/releases/${SB_NAME}/config.xsd ./tmp/config.xsd m7i
    cd ${MY_CURR_DIR}
}

function generate_rpcs_xsd()
{
    cd ${SCHEMA_DIR}
    touch tmp/xmltag_mapping.txt
    ${SANDBOX_FB_OBJ}/junos/lib/ddl/gram/gxsl -x -t ./tmp/ddl_so -j o -F./tmp/ddl_so/ > tmp/operational-command.xsd.error
    sed 's/http:\/\/www.w3.org\/2001\/[ 	]*XMLSchema/http:\/\/www.w3.org\/2001\/XMLSchema/g' tmp/operational-command.xsd.error > tmp/operational-command.xsd
    ../../dmi-convert-schema.pl -r -f junos -s ${SANDBOX_BASE} -m tmp/xmltag_mapping.txt -o dmi/junos/releases/${SB_NAME}/rpcs.xsd ./tmp/operational-command.xsd m7i
    cd ${MY_CURR_DIR}
}

function generate_rpcs_reply_xsd()
{
    cd ${SCHEMA_DIR}
    touch tmp/xmltag_mapping.txt
    for rpc_name in ${RPC_NAMES}
    do
	echo ${rpc_name};
	cp  ${ODL_XML_TAGS}/media-flow-controller.MANIFEST ${ODL_XML_TAGS}/${rpc_name}.MANIFEST
	XSD_FILE_NAMES="${XSD_FILE_NAMES} ${ODL_XML_TAGS}/${rpc_name}.xsd"
    done
    echo ${XSD_FILE_NAMES};
    sed -i .error 's/##any\"processContents/##any\" processContents/g' ${XSD_FILE_NAMES}

    ../../dmi-convert-schema.pl -d 1 -p -f junos -s ${SANDBOX_BASE} -m tmp/xmltag_mapping.txt -o dmi/junos/releases/${SB_NAME}/rpc-reply.xsd ${XSD_FILE_NAMES} ${ODL_XML_TAGS}/xnm\:error.xsd m7i

    cd ${MY_CURR_DIR}
}
clean_ddl
svn_update_ddl
copy_re_app_ddl

clean_odl
svn_update_odl
copy_re_app_odl

make_setup
echo "---------------------------------------"
echo "-------- Generating CONFIG.xsd --------"
echo "---------------------------------------"
generate_config_xsd
echo "---------------------------------------"
echo "--------- Generating RPC.xsd ----------"
echo "---------------------------------------"
generate_rpcs_xsd
XSD_FILE_NAMES=""
echo "---------------------------------------"
echo "------ Generating RPC-REPLY.xsd -------"
echo "---------------------------------------"
generate_rpcs_reply_xsd
echo "------- Generated ALL the files @ -------"

echo ${SCHEMA_DIR} ${PWD}
ls -l ${SCHEMA_DIR}/dmi/junos/releases/${SB_NAME}/*.xsd

XSD_FILE_NAME=mfa-xsd-file-`date +%Y-%m-%d`.tgz
echo ${XSD_FILE_NAME}
if [ -f  ${XSD_FILE_NAME} ];
then
    mv ${XSD_FILE_NAME} ${XSD_FILE_NAME}.1 
fi
tar zcvf ${XSD_FILE_NAME} -C ${SCHEMA_DIR}/dmi/junos/releases/${SB_NAME}/ config.xsd rpc-reply.xsd rpcs.xsd

echo "$? : status of tar";
mutt -a ${XSD_FILE_NAME}   -s  "xsd file ${XSD_FILE_NAME}" dgautam@juniper.net  < /volume/bng-nfsbuild6/dgautam/dmi-tools/debug/mutt-file
