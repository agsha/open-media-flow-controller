#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>
#include <fcntl.h>
#include <pthread.h>
#include "network_int.h"
#include "nkn_stat.h"

/*
 * Note: Ideally, we should be able to use the weak symbol attribute for
 * 	 the nkn_mon_XXX functions, but weak doesn't work where the
 *  	 weak reference resides in a static library.
 */
static pthread_mutex_t proc_init_lock = PTHREAD_MUTEX_INITIALIZER;
static int proc_init_done = 0;

static int (*proc_nkn_mon_add)(const char *name, const char *instance, 
			       void *obj, uint32_t size);
static void (*proc_nkn_mon_delete)(const char *name, const char *instance);
static void (*proc_nkn_mon_get)(const char *name, const char *instance,
				void *obj, uint32_t *size);
static void (*proc_nkn_mon_reset)(const char *name, const char *instance);

static void init_procs(void)
{
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, i, count;
    pid_t 	mypid;
    char 	filename[1024];

    pthread_mutex_lock(&proc_init_lock);

    while (1) {

    if (proc_init_done) {
    	break;
    }

    mypid = getpid();
    snprintf(filename, sizeof(filename), "/proc/%d/exe", mypid);
    elf_version(EV_CURRENT);

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
    	break;
    }
    elf = elf_begin(fd, ELF_C_READ, NULL);

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	gelf_getshdr(scn, &shdr);
	if (shdr.sh_type == SHT_SYMTAB) {
	    // found the symbol table, go process it
	    break;
	}
    }

    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    for (i = 0; i < count; ++i) {
	GElf_Sym sym;
	char *p;
	gelf_getsym(data, i, &sym);

	if (ELF64_ST_TYPE(sym.st_info) == STT_FUNC) {
	    p = elf_strptr(elf, shdr.sh_link, sym.st_name);
	    if (!p) {
	    	continue;
	    }
	    if (strncmp(p, "nkn_mon_add", 11) == 0) {
		proc_nkn_mon_add = (int (*)(const char *, const char *, 
		                            void *, uint32_t)) sym.st_value;
	    } else if (strncmp(p, "nkn_mon_delete", 14) == 0) {
		proc_nkn_mon_delete = 
		    (void (*)(const char *, const char *)) sym.st_value;
	    } else if (strncmp(p, "nkn_mon_get", 11) == 0) {
		proc_nkn_mon_get = (void (*)(const char *, const char *,
					     void *, uint32_t *)) sym.st_value;
	    } else if (strncmp(p, "nkn_mon_reset", 13) == 0) {
		proc_nkn_mon_reset =  
		    (void (*)(const char *, const char *)) sym.st_value;
	    }
	}
    }
    elf_end(elf);
    close(fd);
    proc_init_done = 1;
    break;

    } // End while

    pthread_mutex_unlock(&proc_init_lock);
}

int loc_nkn_mon_add(const char *name, const char *instance, void *obj, 
		    uint32_t size)
{
    if (!proc_init_done) {
    	init_procs();
    }

    if (proc_nkn_mon_add) {
    	return (*proc_nkn_mon_add)(name, instance, obj, size);
    } else {
    	return 0;
    }
}

void loc_nkn_mon_delete(const char *name, const char *instance)
{
    if (!proc_init_done) {
    	init_procs();
    }

    if (proc_nkn_mon_delete) {
    	return (*proc_nkn_mon_delete)(name, instance);
    } else {
    	return;
    }
}

void loc_nkn_mon_get(const char *name, const char *instance, void *obj,
		     uint32_t *size)
{
    if (!proc_init_done) {
    	init_procs();
    }

    if (proc_nkn_mon_get) {
    	return (*proc_nkn_mon_get)(name, instance, obj, size);
    } else {
    	return;
    }
}

void loc_nkn_mon_reset(const char *name, const char *instance)
{
    if (!proc_init_done) {
    	init_procs();
    }

    if (proc_nkn_mon_reset) {
    	return (*proc_nkn_mon_reset)(name, instance);
    } else {
    	return;
    }
}

/*
 * End of nkn_counts.c
 */
