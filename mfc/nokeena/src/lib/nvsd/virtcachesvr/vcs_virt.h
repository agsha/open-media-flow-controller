/*============================================================================
 *
 *
 * Purpose: This file defines the public vcs_virt functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#ifndef __VCS_VIRT_H__
#define __VCS_VIRT_H__

void vcs_virt_init(void);

int vcs_virt_attach_device(
    const char *dom,
    const char *path,
    const char *drive);

int vcs_virt_detach_device(
    const char *dom,
    const char *path,
    const char *drive);

#endif /* __VCS_VIRT_H__ */
