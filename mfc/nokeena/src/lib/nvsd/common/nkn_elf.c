#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <gelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libelf.h>


#include "nkn_elf.h"
#include "nkn_debug.h"
#include "nkn_util.h"

static char * pelf_relaplt=NULL;
static char * pelf_gotplt=NULL;
static char * pelf_plt=NULL;
static int gotplt_offset=0;
static int plt_offset=0;
static char filename[100];

void init_elf(void);

int nkn_get_symbol_addr(char * symbolname, void ** ptr, int * size)
{
	Elf    		* elf = NULL;
	Elf_Scn 	* scn = NULL;
	GElf_Shdr  	shdr;
	Elf_Data   	* data;
	int     	fd = -1;
	int		i, count;

	fd = open(filename, O_RDONLY);
	elf = elf_begin(fd, ELF_C_READ, NULL);

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		if (shdr.sh_type == SHT_SYMTAB) {
			// found a symbol table, go collect
			break;
		}
	}

	data = elf_getdata(scn, NULL);
	count = shdr.sh_size / shdr.sh_entsize;

	// print the symbol names
	for (i = 0; i < count; ++i) {
		GElf_Sym sym;
		gelf_getsym(data, i, &sym);

		//if(ELF64_ST_TYPE(sym.st_info) == STT_FUNC) continue;
		//printf("%s\n", elf_strptr(elf, shdr.sh_link, sym.st_name));
		if(strstr(elf_strptr(elf, shdr.sh_link, sym.st_name), symbolname) != NULL) {
			elf_end(elf);
			close(fd);
			*ptr = (void *)sym.st_value;
			*size = sym.st_size;
			return 1;
        	}
	}
	elf_end(elf);
	close(fd);
	return 0;
}


/* 
 * Symbol Table Handling
 */
#define MAX_SYMBOLS	6000
static char st_buf[300*1024];
static int st_end = 0;

static struct symtable {
	uint64_t addr;
	char * name;
} st_list[MAX_SYMBOLS];
static int max_st_list = 0;

static void st_buildup_table(char * exename);
static void st_buildup_table(char * exename)
{
#define BUF_SIZE 1024
	FILE * fp;
	char * ps, * pd;
	char buf[BUF_SIZE];
	const char * symlist = "/config/nkn/.symlist";
	struct stat stbuf;

	if( stat((char *)"/usr/bin/objdump", &stbuf) == -1) {
		return;
	}
	buf[BUF_SIZE-1]='\0';
	snprintf(buf, BUF_SIZE-1, "/usr/bin/objdump -d %s | grep ^0 > %s",
		 exename, symlist);
	system(buf);

	fp = fopen(symlist, "r");
	if(!fp) return;
	while( !feof(fp) ) {
		if( fgets(buf, 1024, fp) == NULL) break;
		st_list[max_st_list].addr = (uint64_t)convert_hexstr_2_int64(buf);
		if( st_list[max_st_list].addr == 0) break;
		ps = &buf[18];
		pd = st_list[max_st_list].name = &st_buf[st_end];
		while(*ps != '>' && *ps != '@' && *ps != 0) {
			*pd ++ = *ps ++;
			st_end++;
		}
		*pd = 0;
		st_end ++;

		max_st_list ++;
		if(max_st_list >= MAX_SYMBOLS) break;
	}
	fclose(fp);
}

char * st_lookup_by_addr(uint64_t curaddr, uint64_t * funcaddr)
{
	int i;

	for(i=0; i<max_st_list; i++) {
		if(st_list[i].addr > curaddr) {
			*funcaddr = st_list[i-1].addr;
			return st_list[i-1].name;
		}
	}
	*funcaddr = 0;
	return NULL;
}

uint64_t st_lookup_by_name(const char * name);
uint64_t st_lookup_by_name(const char * name)
{
	int i;

	for(i=0; i<max_st_list; i++) {
		if(strcmp(st_list[i].name, name) == 0) {
			return st_list[i].addr;
		}
	}
	return 0;
}

void st_display(void);
void st_display(void)
{
	int i;
	for(i=0; i<max_st_list; i++) {
		printf("add=0x%lu\t%s\n", st_list[i].addr, st_list[i].name);
	}
}

#if 0
int main(int argc, char **argv)
{
	st_buildup_table();
	st_display();
	printf("name=close addr=0x%x\n",
		st_lookup_by_name("close"));
}
#endif // 0

void * nkn_replace_func(const char * api_name, void * pmyapi)
{
	void * p_sys_api;
	char * addr;
	int offset;
	uint64_t psysapi;

	if(!pmyapi) return NULL;

	psysapi = st_lookup_by_name(api_name);
	if( psysapi == 0 ) {
		DBG_LOG(ERROR, MOD_SYSTEM, "psysapi = %p is not a PLT entry.", (void *)psysapi);
		return NULL;
	}

	/*
	 * From section .plt to get index of this function pointer
	 */
	offset = (psysapi - (uint64_t)pelf_plt) / 16;
	offset--;
	//printf("offset=%d psysapi=%p\n", offset, psysapi);
	if(offset > plt_offset) 
	{
		return NULL;
	}

	/*
	 * Based on section rela.plt to calculation offset in table .got.plt
	 */
	addr = (char *)*(unsigned long *)(pelf_relaplt + sizeof(Elf64_Rela) * offset);
	if(addr < pelf_gotplt || addr > pelf_gotplt + gotplt_offset)
	{
		return NULL;
	}

	/*
	 * backup old function pointer and replace by new function pointer
	 * Here we are working on 64bit machine only.
	 * so the sizeof pointer or functions are 8 bytes.
	 */
	memcpy(&p_sys_api, addr, 8);
	memcpy(addr, &pmyapi, 8);

	//printf("p_sys_api=%p\n", p_sys_api);
	return p_sys_api;
}

void init_elf(void)
{
	Elf    		* e = NULL;
	Elf_Scn 	* scn = NULL;
	GElf_Shdr  	shdr;
	GElf_Ehdr 	ehdr;
	int     	fd = -1;
	pid_t 		mypid;
	char		* name;

	mypid=getpid();
	snprintf(filename, 100, "/proc/%d/exe", mypid);

	st_buildup_table(filename);
	//st_display();

	if (elf_version(EV_CURRENT) == EV_NONE) {
		DBG_LOG(ERROR, MOD_SYSTEM, "ELF library initialization failed: %s\n", elf_errmsg(-1));
		return;
	}

	if ((fd = open(filename, O_RDONLY, 0)) < 0) {
		DBG_LOG(ERROR, MOD_SYSTEM, "failed to open %s\n", filename);
		goto done;
	}

	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		DBG_LOG(ERROR, MOD_SYSTEM, "elf_begin() failed: %s.\n", elf_errmsg(-1));
		goto done;
	}

	if (elf_kind(e) != ELF_K_ELF) {
		DBG_LOG(ERROR, MOD_SYSTEM, "%s is not an ELF object.\n", filename);
		goto done;
	}

	if (gelf_getehdr(e, &ehdr) == NULL) {
		DBG_LOG(ERROR, MOD_SYSTEM, "%s is not an ELF object.\n", filename);
		goto done;
	}

	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			DBG_LOG(ERROR, MOD_SYSTEM, "getshdr() failed: %s.\n", elf_errmsg(-1));
			goto done;
		}

		if ((name = elf_strptr(e, ehdr.e_shstrndx/*shstrndx*/, shdr.sh_name)) == NULL) {
			DBG_LOG(ERROR, MOD_SYSTEM, "elf_strptr() failed: %s.\n", elf_errmsg(-1));
			goto done;
		}

		if(strcmp(name, ".plt")==0) {
			pelf_plt = (char *)shdr.sh_addr;
			plt_offset = shdr.sh_offset;
		}
		else if(strcmp(name, ".rela.plt")==0) {
			pelf_relaplt = (char *)shdr.sh_addr;
		}
		else if(strcmp(name, ".got.plt")==0) {
			pelf_gotplt = (char *)shdr.sh_addr;
			gotplt_offset = shdr.sh_offset;
		}
	}

done:
	if(e) elf_end(e);
	if(fd!=-1) close(fd);
	//printf("pelf_plt=%p\n", pelf_plt);
	//printf("pelf_relaplt=%p\n", pelf_relaplt);
	//printf("gotplt_offset=%p\n", gotplt_offset);
}

