/*-
 * Copyright (c) 1995 Terrence R. Lambert
 * All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)init_main.c	8.9 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/init_main.c,v 1.283.2.2 2007/12/14 13:41:08 rrs Exp $");

#include "opt_ddb.h"
#include "opt_init_path.h"
#include "opt_mac.h"
#include "opt_printf.h"


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/sysent.h>
#include <sys/reboot.h>
//#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/domain.h>
#include <sys/time.h>
#include <sys/eventhandler.h>
#include <sys/random.h>

#include <machine/cpu.h>

#include <sys/msgbuf.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/syslog.h>
#include <sys/cons.h>
#include <sys/ctype.h>
#include <sys/protosw.h>
#include <sys/sx.h>


#include <nknshim/nkns_osl.h>

void * (* nkn_linux_malloc)(size_t size) = NULL;
void   (* nkn_linux_free)(void *ptr) = NULL;
long int (* nkn_random)(void) = NULL;
void (* nkn_exit)(int __status) __attribute__ ((__noreturn__)) = NULL;
int (* nkn_usleep)(useconds_t usec) = NULL;
int (* nkn_pthread_mutex_lock)(pthread_mutex_t *mutex) = NULL;
int (* nkn_pthread_mutex_trylock)(pthread_mutex_t *mutex) = NULL;
int (* nkn_pthread_mutex_unlock)(pthread_mutex_t *mutex) = NULL;
void (* nkn_print_trace) (void) = NULL;
int (* nkn_gettimeofday)(struct timeval *tv, struct timezone *tz) = NULL;
int (* nkn_send_packet)(char * phead, int lhead, char * pbuf, int lbuf) = NULL;


int nkn_verbose=0;

//#define UMA_BOOT_PAGES	48
//static void * bootmem;

//extern void ip_init(void);
//extern void net_add_domain(void *data);
//extern struct domain inetdomain;
//extern void uma_startup(void *bootmem, int boot_pages);

//extern int ncallout;
//extern caddr_t kern_timeout_callwheel_alloc(caddr_t v);
//extern void kern_timeout_callwheel_init(void);

extern void mi_startup(void);
extern void nkn_init_api_mutex(void);
extern int  nkn_set_memory(void * pmem, long size);

void nkn_init_funcs(struct nkn_osl_funcs_t * posl, void * pmem, long lmem);
void nkn_init_funcs(struct nkn_osl_funcs_t * posl, void * pmem, long lmem)
{
	char * v;

	//
	// Setup Linux functins so it can be called by FreeBSD TCP/IP stack
	//
	nkn_linux_malloc = posl->nkn_linux_malloc;
	nkn_linux_free = posl->nkn_linux_free;
	nkn_random = posl->nkn_random;
	nkn_exit = posl->nkn_exit;
	nkn_usleep = posl->nkn_usleep;
	nkn_pthread_mutex_lock = posl->nkn_pthread_mutex_lock;
	nkn_pthread_mutex_trylock = posl->nkn_pthread_mutex_trylock;
	nkn_pthread_mutex_unlock = posl->nkn_pthread_mutex_unlock;
	nkn_print_trace = posl->nkn_print_trace;
	nkn_gettimeofday=posl->nkn_gettimeofday;
	nkn_send_packet=posl->nkn_send_packet;

        nkn_set_memory(pmem, lmem);

	// currently set the time resolutin to 100ms.
	// it means hz=10 only.
	// Setup clocks
	hz=10; 		// unit of hz
	tick=1;
	ticks=0;

	nkn_init_api_mutex();

	//
	// After setup the linux functions, time to do freebsd SYSINIT
	//
	v=nkn_netmalloc(sizeof(char *)*ncallout);
	if(!v) {
		printf("cannot allocate 3k buffer\n");
		nkn_exit(1);
	}
	kern_timeout_callwheel_alloc(v);
	kern_timeout_callwheel_init();
	printf("nkns_init_func: v=%ld\n", *(long *)(v+1024));
	/*
	bootmem=(void *)nkn_netmalloc(1024*1024*UMA_BOOT_PAGES);
	if(!bootmem) {
		printf("cannot allocate 48M buffer\n");
		nkn_exit(1);
	}
	uma_startup(bootmem, UMA_BOOT_PAGES);
	*/
	uma_startup(NULL, 0);
	printf("%s calls mi_start\n", __FUNCTION__);
	mi_startup();

	//printf("domains=0x%x\n", (int)domains);
	//net_add_domain(&inetdomain);
	//printf("domains=0x%x\n", (int)domains);
	//ip_init();
}

