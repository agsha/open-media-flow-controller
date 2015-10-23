#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>
#include <libelf.h>
#include <gelf.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <assert.h>


#include "server.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

// Separator used for instances
#define INST_SEP "."
#define INST_SEP_LEN strlen(INST_SEP)

// All counters

void init_counters(void);
void exit_counters(void);
void print_counters(void);
void reset_counters(char * var);
void copy_counters(void);

static glob_item_t * g_allcnts;
static nkn_shmdef_t * pshmdef = NULL;
static char * varname, *varend, *vartotal;
static char * shm;
static nkn_shmdef_t *dbg_pshmdef = NULL;	// local memory copy for debug
static pthread_mutex_t nkn_counts_lock;

static int
add_counter(const char *name, const char *instance, void *addr, uint32_t size)
{
	int namelen = strlen(name), instlen;

	pthread_mutex_lock(&nkn_counts_lock);
	if (instance) {
		instlen = strlen(instance);
		namelen += (instlen + INST_SEP_LEN);	// instance.name
	}
	if (namelen + varend > vartotal) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return ENOMEM;
	}

	/* Must be a coding bug 
	 * We only support sizes 1, 2, 4 and 8.
	 * any thing else is an error
	 */ 
	if((size != 1) && (size != 2) && (size != 4) && (size != 8)){
	    /* This should not happen. If this happens, we would miss
	     * some counters. glob_ should be used only for counters 
	     * Asserting if this fails.
	     */
		pthread_mutex_unlock(&nkn_counts_lock);
		assert(0);
		return ENOMEM;
	}

	if (pshmdef->tot_cnts >= (MAX_CNTS-1)) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return ENOMEM;
	}
	g_allcnts[pshmdef->tot_cnts].name = varend - varname;
	if (instance) {
		strcpy(varend, instance);
		strcpy(varend+instlen, INST_SEP);
		varend+=strlen(varend);
	}
	strcpy(varend, name);
	g_allcnts[pshmdef->tot_cnts].addr = addr;
	varend+=strlen(varend)+1;
	if (size > 8) {
		if (size + varend > vartotal) {
			pthread_mutex_unlock(&nkn_counts_lock);
			return ENOMEM;
		}
		g_allcnts[pshmdef->tot_cnts].value = varend - varname;
		varend+=size;
	}
	g_allcnts[pshmdef->tot_cnts].size = size;
	pshmdef->tot_cnts++;
	pshmdef->revision++;
	pthread_mutex_unlock(&nkn_counts_lock);
	return 0;
}

void init_counters(void)
{
	Elf         *elf;
	Elf_Scn     *scn = NULL;
	GElf_Shdr   shdr;
	Elf_Data    *data;
	int         fd, i, count;
	pid_t 	mypid;
	char 	filename[1024];
	int		shmid;

	/*
	 * Allocated shared memory
	 */

	// Create the segment.
	if ((shmid = shmget(NKN_SHMKEY, NKN_SHMSZ, IPC_CREAT | 0666)) < 0) {
		if ((shmid = shmget(NKN_SHMKEY, NKN_SHMSZ, 0666)) < 0) {
			DBG_LOG(ERROR, MOD_SYSTEM, "shmget error errno=%d", errno);
			return;
		}
	}

	/*
	 * Now we attach the segment to our data space.
	 * First structure is nkn_shmdef_t.
	 * Then followed by glob_item_t
	 */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
		DBG_LOG(ERROR, MOD_SYSTEM, "shmat error errno=%d", errno);
		return;
	}
	pshmdef = (nkn_shmdef_t *) shm;
	g_allcnts = (glob_item_t *)(shm+sizeof(nkn_shmdef_t));
	varname = (char *)(shm+sizeof(nkn_shmdef_t)+sizeof(glob_item_t)*MAX_CNTS);
	vartotal = (char *)shm+ NKN_SHMSZ;
	pthread_mutex_init(&nkn_counts_lock, NULL);

	dbg_pshmdef = (nkn_shmdef_t *)nkn_calloc_type(1, NKN_SHMSZ, mod_nkn_shmdef_t);
	if (dbg_pshmdef == NULL)
	    DBG_LOG(ERROR, MOD_SYSTEM, "counters allocation failed errno=%d", errno);

	pshmdef->tot_cnts=0;
	strcpy(pshmdef->version, NKN_VERSION);

	/*
	 * print out the compiler symbole for lookup
	 */
	mypid=getpid();
	snprintf(filename, sizeof(filename), "/proc/%d/exe", mypid);

	elf_version(EV_CURRENT);

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
	varend = varname;
	for (i = 0; i < count; ++i) {
		int ret;
		GElf_Sym sym;
		gelf_getsym(data, i, &sym);

		if (ELF64_ST_TYPE(sym.st_info) == STT_FUNC) continue;
		//if (ELF64_ST_BIND(sym.st_info) != STB_GLOBAL) continue;
		if(strncmp(elf_strptr(elf, shdr.sh_link, sym.st_name), "glob_", 5)==0) {
			ret = add_counter(elf_strptr(elf, shdr.sh_link, sym.st_name), 
					NULL, (void *)sym.st_value, sym.st_size);
			if (ret) break;

			// We only pick up the first MAX_CNTS.
			if(pshmdef->tot_cnts >= MAX_CNTS) break;
		}
	}
	pshmdef->static_cnts = pshmdef->tot_cnts;
	elf_end(elf);
	close(fd);

	return; // Done.
}

static
glob_item_t * find_counter(const char *name, const char *instance, int dyn)
{
	int i = 0, instlen;
	
	if (dyn)
		i = pshmdef->static_cnts;	
	if (instance)
		instlen = strlen(instance);
	for(; i < pshmdef->tot_cnts; i++) {
		glob_item_t *item;
		char *iname;

		item = &g_allcnts[i];
		iname = varname + item->name;

		if (item->size == 0)
			continue;
		if (instance) {
			if (strncmp(iname, instance, instlen))
				continue;
			if (strncmp(iname+instlen, INST_SEP, INST_SEP_LEN))
				continue;
			iname += (instlen+1);
		}
		if (strncmp(iname, name, strlen(name)))
			continue;
		return item;
	}
	return NULL;
}

int nkn_mon_add(const char *name, const char *instance, void *obj, uint32_t size)
{
    if (!pshmdef)
		return ENOMEM;
	return add_counter(name, instance, obj, size);
}

void nkn_mon_delete(const char *name, const char *instance)
{
	glob_item_t *item;

    if (!pshmdef)
		return;
	pthread_mutex_lock(&nkn_counts_lock);
	item = find_counter(name, instance, 1);
	if (!item) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return;
	}
	item->size = 0;		// indicates free entry
	item->value = 0;
	pshmdef->revision++;
	pthread_mutex_unlock(&nkn_counts_lock);
}


void nkn_mon_get(const char *name, const char *instance, void *obj, uint32_t *size)
{
	glob_item_t *item;

	if (obj == NULL)
		return;
	if (!pshmdef)
		return;
	pthread_mutex_lock(&nkn_counts_lock);
	item = find_counter(name, instance, 1);
	if (!item) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return;
	}

	switch(item->size)
	{
	case 1: *(uint8_t *)(obj) = *(uint8_t *)(item->addr); break;
	case 2: *(uint16_t *)(obj) = *(uint16_t *)(item->addr); break;
	case 4: *(uint32_t *)(obj) = *(uint32_t *)(item->addr); break;
	case 8: *(uint64_t *)(obj) = *(uint64_t *)(item->addr); break;
	default: break;
	}
	if (size)  
		*size = item->size;
	pthread_mutex_unlock(&nkn_counts_lock);
	return;
}

void nkn_mon_reset(const char *name, const char *instance)
{
	glob_item_t *item;
	if (!pshmdef)
		return;
	pthread_mutex_lock(&nkn_counts_lock);
	item = find_counter(name, instance, 1);
	if (!item) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return;
	}
	// Reset variable here. 
	switch(item->size)
	{
	case 1: *(uint8_t *)(item->addr) = 0; break;
	case 2: *(uint16_t *)(item->addr) = 0; break;
	case 4: *(uint32_t *)(item->addr) = 0; break;
	case 8: *(uint64_t *)(item->addr) = 0; break;
	default: break;
	}

	pthread_mutex_unlock(&nkn_counts_lock);
	return;
}

void exit_counters(void)
{
#if 0	// TBD - needs to synchronize with timer thread that calls copy
	if(shm) {
		shmdt(shm);
		shm=NULL;
	}
#endif
}

void print_counters(void)
{
    int i;
    unsigned char * pc;
    unsigned short * ps;
    unsigned int * pi;
    unsigned long * pl;


    if (!pshmdef)
		return;
    pthread_mutex_lock(&nkn_counts_lock);
    for(i=0;i<pshmdef->tot_cnts; i++) {
	//printf("%s: %d   %p\n", g_allcnts[i].name, g_allcnts[i].size, g_allcnts[i].addr);
	switch(g_allcnts[i].size) {
	case 1:
	    pc=(unsigned char *) g_allcnts[i].addr;
	    printf("%20s = %d\n", varname+g_allcnts[i].name, *pc);
	    break;
	case 2:
	    ps=(unsigned short *) g_allcnts[i].addr;
	    printf("%20s = %d\n", varname+g_allcnts[i].name, *ps);
	    break;
	case 4:
	    pi=(unsigned int *) g_allcnts[i].addr;
	    printf("%20s = %d\n", varname+g_allcnts[i].name, *pi);
	    break;
	case 8:
	    pl=(unsigned long *) g_allcnts[i].addr;
	    printf("%20s = %ld\n", varname+g_allcnts[i].name, *pl);
	    break;
	}
    }
    pthread_mutex_unlock(&nkn_counts_lock);
}

void reset_counters(char * var)
{
    int i;

    if (!pshmdef)
		return;
    pthread_mutex_lock(&nkn_counts_lock);
    for(i=0; i<pshmdef->tot_cnts; i++) {
	if((var==NULL) || (strstr(varname+g_allcnts[i].name, var)!=NULL) ) {
	     // reset this one
        switch(g_allcnts[i].size) {
        case 1:
            * (unsigned char *) g_allcnts[i].addr = 0;
            break;
        case 2:
            * (unsigned short *) g_allcnts[i].addr = 0;
            break;
        case 4:
            * (unsigned int *) g_allcnts[i].addr = 0;
            break;
        case 8:
            * (unsigned long *) g_allcnts[i].addr = 0;
            break;
        }
	}
    }
    pthread_mutex_unlock(&nkn_counts_lock);
}

void
copy_counters(void)
{
    int i;
    unsigned char *pc;
    unsigned short *ps;
    unsigned int *pi;
    unsigned long *pl;


    if (!pshmdef)
	return;

    pthread_mutex_lock(&nkn_counts_lock);

    // Make a local copy for debug since we do not dump shared memory segments
    if (dbg_pshmdef && (dbg_pshmdef->revision != pshmdef->revision))
	memcpy(dbg_pshmdef, pshmdef, NKN_SHMSZ);

    for (i=0; i<pshmdef->tot_cnts; i++) {
        //printf("%s: %d   %p\n", g_allcnts[i].name, g_allcnts[i].size, g_allcnts[i].addr);
#ifdef __ZVM__
	zvm_api_obj_zrace_disable(g_allcnts[i].addr);
#endif
        switch(g_allcnts[i].size) {
        case 1:
            pc = (unsigned char *) g_allcnts[i].addr;
	    g_allcnts[i].value = *pc;
            break;

        case 2:
            ps = (unsigned short *) g_allcnts[i].addr;
	    g_allcnts[i].value = *ps;
            break;

        case 4:
            pi = (unsigned int *) g_allcnts[i].addr;
	    g_allcnts[i].value = *pi;
            break;

        case 8:
            pl = (unsigned long *) g_allcnts[i].addr;
	    g_allcnts[i].value = *pl;
            break;

	default:
	    if (g_allcnts[i].size > 8) {
		memcpy((void *)(varname + g_allcnts[i].value),
		       g_allcnts[i].addr, g_allcnts[i].size);
		continue;
	    }
	    break;
        }
#ifdef __ZVM__
	zvm_api_obj_zrace_enable(g_allcnts[i].addr);
#endif
    }
    pthread_mutex_unlock(&nkn_counts_lock);
}


