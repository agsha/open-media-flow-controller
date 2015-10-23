#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <execinfo.h>
#include <stdint.h>

#include "nkn_elf.h"
#include "nkn_debug.h"
     
void print_trace (void);
void print_hex(char * name, unsigned char * buf, int size);

/* Obtain a backtrace and print it to stdout. */
void print_trace (void)
{
	void * t_array[20];
	size_t size;
	size_t i;
	uint64_t curaddr;
	uint64_t funcaddr;
	char * pfunname;

	size = backtrace (t_array, 20);
	DBG_LOG(SEVERE, MOD_SYSTEM, "Back trace :");
	for (i = 1; i < size; i++) {
		curaddr = (uint64_t)t_array[i];
		pfunname = st_lookup_by_addr(curaddr, &funcaddr);
		if(pfunname == NULL) break;
		DBG_LOG(SEVERE, MOD_SYSTEM, "%s [0x%lx + 0x%lx]", pfunname, funcaddr, curaddr - funcaddr);
		DBG_ERR(SEVERE,  "%s [0x%lx + 0x%lx]", pfunname, funcaddr, curaddr - funcaddr);
	}
}
     
void print_hex(char * name, unsigned char * buf, int size)
{
	int i, j;
	char ch;

	printf("name=%s", name);

	for(i=0;i<size/16;i++) {
		printf("\n%p ", &buf[i*16]);
		for(j=0; j<16;j++) {
			printf("%02x ", buf[i*16+j]);
		}
		printf("    ");
		for(j=0; j<16;j++) {
			ch=buf[i*16+j];
			if(isascii(ch) &&
			   (ch!='\r') &&
			   (ch!='\n') &&
			   (ch!=0x40) )
		 	   printf("%c", ch);
			else printf(".");
		}
	}
	printf("\n%p ", &buf[i*16]);
	for(j=0; j<size-i*16;j++) {
		printf("%02x ", buf[i*16+j]);
	}
	printf("    ");
	for(j=0; j<size-i*16;j++) {
		ch=buf[i*16+j];
		if(isascii(ch) &&
		   (ch!='\r') &&
		   (ch!='\n') &&
		   (ch!=0x40) )
		   printf("%c", ch);
		else printf(".");
	}
	printf("\n");
}


