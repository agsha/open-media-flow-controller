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

#include <i386/include/stdarg.h>

#include <nknshim/nkns_osl.h>
#include <nknshim/nkns_api.h>

     
#if 0
/* Obtain a backtrace and print it to stdout. */
void net_print_hex(char * name, unsigned char * buf, int size)
{
	int i, j;

	printf("name=%s", name);

	for(i=0;i<size/16;i++) {
		printf("\n%08x ", *(int *)&buf[i*16]);
		for(j=0; j<16;j++) {
			printf("%02x ", buf[i*16+j]);
		}
		printf("    ");
		for(j=0; j<16;j++) {
			if(isascii(buf[i*16+j])) printf("%c", buf[i*16+j]);
			else printf(".");
		}
	}
	printf("\n%08x ", *(int *)&buf[i*16]);
	for(j=0; j<size-i*16;j++) {
		printf("%02x ", buf[i*16+j]);
	}
	printf("    ");
	for(j=0; j<size-i*16;j++) {
		if(isascii(buf[i*16+j])) printf("%c", buf[i*16+j]);
		else printf(".");
	}
	printf("\n");
}
#endif

void net_assert(char * file, int line, char * failedexpr)
{
	char * p=NULL;

	(void)printf("sp_assert \"%s\" failed: file \"%s\", line %d\n",
		     failedexpr, file, line);
	*p='c';
	/* NOTREACHED */
}

void net_printf(char * sFmt, ...)
{
	va_list tAp;
	char buf[2048];
	int len;

	//getmicrotime(&pslf->tv);
	va_start(tAp, sFmt);
	len = vsnprintf(buf, 2048, sFmt, tAp);
	va_end(tAp);

	buf[len]=0;
	printf( buf);
}
