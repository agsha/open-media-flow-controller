/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Quite extensively rewritten by Poul-Henning Kamp of the FreeBSD
 * project, to make these variables more userfriendly.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/kern_sysctl.c,v 1.177 2007/09/02 09:59:33 rwatson Exp $");

#include "opt_compat.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/sysproto.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

/*
 * Initialization of the MIB tree.
 *
 * Order by number in each list.
 */

void
sysctl_register_oid(struct sysctl_oid *oidp)
{
	printf("ERROR: sysctl_register_oid\n");
}

void
sysctl_unregister_oid(struct sysctl_oid *oidp)
{
	printf("ERROR: sysctl_unregister_oid\n");
}

/* Initialize a new context to keep track of dynamically added sysctls. */
int
sysctl_ctx_init(struct sysctl_ctx_list *c)
{
	printf("ERROR: sysctl_ctx_init\n");
	return (0);
}

/* Free the context, and destroy all dynamic oids registered in this context */
int
sysctl_ctx_free(struct sysctl_ctx_list *clist)
{
	printf("ERROR: sysctl_ctx_free\n");
	return (0);
}

/* Add an entry to the context */
struct sysctl_ctx_entry *
sysctl_ctx_entry_add(struct sysctl_ctx_list *clist, struct sysctl_oid *oidp)
{
	printf("ERROR: sysctl_ctx_entry_add\n");
	return (NULL);
}

/* Find an entry in the context */
struct sysctl_ctx_entry *
sysctl_ctx_entry_find(struct sysctl_ctx_list *clist, struct sysctl_oid *oidp)
{
	printf("ERROR: sysctl_ctx_entry_find\n");
	return (NULL);
}

/*
 * Delete an entry from the context.
 * NOTE: this function doesn't free oidp! You have to remove it
 * with sysctl_remove_oid().
 */
int
sysctl_ctx_entry_del(struct sysctl_ctx_list *clist, struct sysctl_oid *oidp)
{
	printf("ERROR: sysctl_ctx_entry_del\n");
	return (0);
}

/*
 * Remove dynamically created sysctl trees.
 * oidp - top of the tree to be removed
 * del - if 0 - just deregister, otherwise free up entries as well
 * recurse - if != 0 traverse the subtree to be deleted
 */
int
sysctl_remove_oid(struct sysctl_oid *oidp, int del, int recurse)
{
	printf("ERROR: sysctl_remove_oid\n");
	return (0);
}

/*
 * Create new sysctls at run time.
 * clist may point to a valid context initialized with sysctl_ctx_init().
 */
struct sysctl_oid *
sysctl_add_oid(struct sysctl_ctx_list *clist, struct sysctl_oid_list *parent,
	int number, const char *name, int kind, void *arg1, int arg2,
	int (*handler)(SYSCTL_HANDLER_ARGS), const char *fmt, const char *descr)
{
	printf("ERROR: sysctl_add_oid\n");
	return NULL;
}

/*
 * Reparent an existing oid.
 */
int
sysctl_move_oid(struct sysctl_oid *oid, struct sysctl_oid_list *parent)
{
	printf("ERROR: sysctl_move_oid\n");
	return 0;
}

/*
 * Default "handler" functions.
 */

/*
 * Handle an int, signed or unsigned.
 * Two cases:
 *     a variable:  point arg1 at it.
 *     a constant:  pass it in arg2.
 */

int
sysctl_handle_int(SYSCTL_HANDLER_ARGS)
{
	printf("ERROR: sysctl_handle_int\n");
	return (0);
}


/*
 * Based on on sysctl_handle_int() convert milliseconds into ticks.
 */

int
sysctl_msec_to_ticks(SYSCTL_HANDLER_ARGS)
{
	printf("ERROR: sysctl_msec_to_ticks\n");
	return (0);
}


/*
 * Handle a long, signed or unsigned.  arg1 points to it.
 */

int
sysctl_handle_long(SYSCTL_HANDLER_ARGS)
{
	printf("ERROR: sysctl_handle_long\n");
	return (0);
}

/*
 * Handle a 64 bit int, signed or unsigned.  arg1 points to it.
 */

int
sysctl_handle_quad(SYSCTL_HANDLER_ARGS)
{
	printf("ERROR: sysctl_handle_quad\n");
	return (0);
}

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 * 	a variable string:  point arg1 at it, arg2 is max length.
 * 	a constant string:  point arg1 at it, arg2 is zero.
 */

int
sysctl_handle_string(SYSCTL_HANDLER_ARGS)
{
	printf("ERROR: sysctl_handle_string\n");
	return (0);
}

/*
 * Handle any kind of opaque data.
 * arg1 points to it, arg2 is the size.
 */

int
sysctl_handle_opaque(SYSCTL_HANDLER_ARGS)
{
	printf("ERROR: sysctl_handle_opaque\n");
	return (0);
}

int
kernel_sysctl(struct thread *td, int *name, u_int namelen, void *old,
    size_t *oldlenp, void *new, size_t newlen, size_t *retval, int flags)
{
	printf("ERROR: kernel_sysctl\n");
	return (0);
}

int
kernel_sysctlbyname(struct thread *td, char *name, void *old, size_t *oldlenp,
    void *new, size_t newlen, size_t *retval, int flags)
{
	printf("ERROR: kernel_sysctlbyname\n");
	return (0);
}

/*
 * Wire the user space destination buffer.  If set to a value greater than
 * zero, the len parameter limits the maximum amount of wired memory.
 */
int
sysctl_wire_old_buffer(struct sysctl_req *req, size_t len)
{
	printf("ERROR: sysctl_wire_old_buffer\n");
	return (0);
}

int
sysctl_find_oid(int *name, u_int namelen, struct sysctl_oid **noid,
    int *nindx, struct sysctl_req *req)
{
	printf("ERROR: sysctl_find_oid\n");
	return (0);
}


#ifndef _SYS_SYSPROTO_H_
struct sysctl_args {
	int	*name;
	u_int	namelen;
	void	*old;
	size_t	*oldlenp;
	void	*new;
	size_t	newlen;
};
#endif
int
__sysctl(struct thread *td, struct sysctl_args *uap)
{
	printf("ERROR: __sysctl\n");
	return (0);
}

/*
 * This is used from various compatibility syscalls too.  That's why name
 * must be in kernel space.
 */
int
userland_sysctl(struct thread *td, int *name, u_int namelen, void *old,
    size_t *oldlenp, int inkernel, void *new, size_t newlen, size_t *retval,
    int flags)
{
        printf("ERROR: userland_sysctl\n");
        return (0);
}

