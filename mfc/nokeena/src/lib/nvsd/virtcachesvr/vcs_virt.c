/*============================================================================
 *
 *
 * Purpose: This file implements vcs_virt functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "vcs_virt.h"
#include "vcs_private.h"

static char * vcs_channel_xml(
    const char *path,
    const char *drive,
    const char *name)
{
    static const char *fmt =
"<disk type='file' device='disk'>\n"
"  <driver name='qemu' type='raw' io='native'/>\n"
"  <source file='%s'/>\n"
"  <target dev='%s' bus='virtio'/>\n"
"  <readonly/>\n"
"  <serial>VCVD-%s</serial>\n"
"  <address type='pci' domain='0x0000' bus='0x00' slot='0x10' function='0x0'/>\n"
"</disk>\n";

    char *xml = nkn_malloc_type(strlen(fmt)+strlen(path)+strlen(drive)+strlen(name)+1,
				mod_vcs_channel_fmt_str);
    if (xml == NULL)
	return NULL;

    sprintf(xml, fmt, path, drive, name);
    return xml;
}

typedef enum {
    NKN_VIRT_DEVICE_ATTACH,
    NKN_VIRT_DEVICE_DETACH,
} vcs_virt_device_action_t;


static int vcs_virt_manage_device(
    vcs_virt_device_action_t action,
    const char *name,
    const char *path,
    const char *drive)
{
    virConnectPtr conn;
    virDomainPtr dom;
    unsigned int flags;
    char *xml;
    int rc = -1;

    conn = virConnectOpen("qemu:///system");
    if (conn == NULL) {
	DBG_VCE("Failed to open connection to qemu:///system\n");
        return 1;
    }

    dom = virDomainLookupByName(conn, name);
    if (dom == NULL) {
	DBG_VCE("Failed to lookup domain '%s'\n", name);
	goto connection;
    }

     flags = VIR_DOMAIN_DEVICE_MODIFY_CURRENT;
     if (virDomainIsActive(dom) == 1)
	flags |= VIR_DOMAIN_DEVICE_MODIFY_LIVE;

    xml = vcs_channel_xml(path, drive, name);
    if (xml == NULL)
	return -1;

    switch(action) {
    case NKN_VIRT_DEVICE_ATTACH:
	rc = virDomainAttachDeviceFlags(dom, xml, flags);
	break;
    case NKN_VIRT_DEVICE_DETACH:
	rc = virDomainDetachDeviceFlags(dom, xml, flags);
	break;
    default:
	errno = EINVAL;
    }

    free(xml);
    virDomainFree(dom);
connection:
    virConnectClose(conn);
    return rc;
}

int vcs_virt_attach_device(
    const char *dom, 
    const char *path, 
    const char *drive)
{
    DBG_VCM3("%s:%s as '%s'\n", dom, path, drive);
    return vcs_virt_manage_device(NKN_VIRT_DEVICE_ATTACH, dom, path, drive);
}

int vcs_virt_detach_device(
    const char *dom,
    const char *path,
    const char *drive)
{
    DBG_VCM3("%s from %s\n", drive, dom);
    return vcs_virt_manage_device(NKN_VIRT_DEVICE_DETACH, dom, path, drive);
}

static void vcs_virt_cleanup_devices(void)
{
    virConnectPtr conn;
    virDomainPtr dom;
    int i, n, numDomains, *activeDomains = NULL;
    char d, *xml;
    unsigned int flags;
    char disk_name[VCS_MAX_DISK_NAME_STR];

    conn = virConnectOpen("qemu:///system");
    if (conn == NULL) {
        DBG_VCE("Failed to open connection to qemu:///system\n");
        return;
    }

    numDomains = virConnectNumOfDomains(conn);
    if (!numDomains)
	goto connection;
    
    activeDomains = alloca(numDomains * sizeof(int));
    numDomains = virConnectListDomains(conn, activeDomains, numDomains);
    if (numDomains == -1) {
        DBG_VCE("Failed to get list of numDomains'\n");
        goto connection;
    }
    
    strcpy(disk_name, "vdv");
    for (n = 0; n < numDomains; n++) {
	dom = virDomainLookupByID(conn, activeDomains[n]);
	if (dom == NULL) {
	    DBG_VCE("Failed to lookup domain '%d'\n", activeDomains[n]);
	    continue;
	}
	
	flags = VIR_DOMAIN_DEVICE_MODIFY_CURRENT;
	if (virDomainIsActive(dom) == 1)
	    flags |= VIR_DOMAIN_DEVICE_MODIFY_LIVE;
	
	for (i = 0; i < VCS_N_CHANNELS; i++) {
	    for (d='l'; d<='z'; d++) {
		disk_name[2] = d;
		xml = vcs_channel_xml(vc->vcs_vc_channels[i].path, 
				      disk_name,
				      virDomainGetName(dom));
		if (xml == NULL) {
		    DBG_OOM();
		    break;
		    virDomainDetachDeviceFlags(dom, xml, flags);
		    free(xml);
		}
	    }
	}
	virDomainFree(dom);
    }

connection:
    if (activeDomains)
        free(activeDomains);
    virConnectClose(conn);
}

static void vcs_virt_errfunc(void *userdata, virErrorPtr err)
{
    UNUSED_ARGUMENT(userdata);
    DBG_VCE("%s\n", err->message);
}

void vcs_virt_init(void)
{
    virSetErrorFunc(NULL, vcs_virt_errfunc);
    vcs_virt_cleanup_devices();
}
