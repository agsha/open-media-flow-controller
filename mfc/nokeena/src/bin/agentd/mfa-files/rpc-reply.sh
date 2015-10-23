#!/bin/sh
SB_NAME=pacifica
SRC_BASE=/volume/bng-nfsbuild6/dgautam/vjx-12-1
TOOL_BASE=/volume/bng-nfsbuild6/dgautam

SANDBOX_BASE=${SRC_BASE}/${SB_NAME}
SANDBOX_SRC=${SANDBOX_BASE}/src
SANDBOX_OBJ=${SANDBOX_BASE}/obj-i386
SANDBOX_FB_OBJ=${SANDBOX_BASE}/obj-freebsd7-i386
DMI_TOOLS=${TOOL_BASE}/dmi-tools
MY_LD_LIB_PATH=${SANDBOX_OBJ}/bsd/gnu/lib/libgcc

export LD_LIBRARY_PATH=${MY_LD_LIB_PATH}:${LD_LIBRARY_PATH}
#export LD_LIBRARY_PATH=/volume/bng-nfsbuild6/dgautam/vjx-12-1/pacifica/obj-i386/bsd/gnu/lib/libgcc:$LD_LIBRARY_PATH
ODL_XML_TAGS=${SANDBOX_OBJ}/junos/lib/odl/xmltags

RPC_NAMES="mfc-system-details interfaces-detail interface-list interface-configured interface-brief interface-stats error-info resource-pool-list resource-pool-detail get-cmd-response op-cmd-response mfc-vm-details mfc-vm-volume-details bond-detail bond-list mfc-crawler-list mfc-crawler-detail mfc-service-info "

XSD_FILE_NAMES=

for name in ${RPC_NAMES}
do
    echo ${name};
    cp  ${ODL_XML_TAGS}/media-flow-controller.MANIFEST ${ODL_XML_TAGS}/${name}.MANIFEST
    XSD_FILE_NAMES="${XSD_FILE_NAMES} ${ODL_XML_TAGS}/${name}.xsd"
done

echo ${XSD_FILE_NAMES};

../../dmi-convert-schema.pl -d 1 -p -f junos -s ${SANDBOX_BASE} -m tmp/xmltag_mapping.txt -o dmi/junos/releases/pacifica/rpc-reply.xsd ${XSD_FILE_NAMES} ${ODL_XML_TAGS}/xnm\:error.xsd m7i
