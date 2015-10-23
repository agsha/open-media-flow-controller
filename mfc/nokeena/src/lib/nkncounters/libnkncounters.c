
#define USE_TRIE_LOOKASIDE 1 // Enabled

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
#include <sys/syslog.h>
#include <fcntl.h>
#include <assert.h>
#include <alloca.h>

#include "nkn_defs.h"
#include <sys/prctl.h>

#ifdef USE_TRIE_LOOKASIDE
#include "cprops/collection.h"
#include "cprops/trie.h"
#endif /* USE_TRIE_LOOKASIDE */

#include "nkn_stat.h"

// All counters

void init_counters(int);
void exit_counters(void);
void print_counters(void);
void reset_counters(char * var);
void copy_counters(void);

static glob_item_t * g_allcnts;
static nkn_shmdef_t * pshmdef = NULL;
static char * varname, *varend, *vartotal;
static char * shm;
static nkn_shmdef_t *dbg_pshmdef = NULL;	// local memory copy for debug
static pthread_mutex_t nkn_counts_lock = PTHREAD_MUTEX_INITIALIZER;
key_t key;
static int64_t sh_size;
int max_cnt_space;
int32_t g_next_deleted_counter = -1;


static pthread_t id;
int copy_counter_interval=1;
void config_and_run_counter_update(void);
void countercopy_init(void);

long long glob_add_count_counter = 0;

/******************************************************************************/
#ifdef USE_TRIE_LOOKASIDE
/******************************************************************************/
/*
 * cp_trie interface routines.
 */
static cp_trie *nkn_cnt_trie;

static void *
trie_nkn_cnt_copy(void * nd)
{
	return nd;
}

static void
trie_nkn_cnt_destruct(void *tn)
{
	UNUSED_ARGUMENT(tn);
	return;
}

static
glob_item_t * int_find_counter(int delete_trie_entry, const char *name)
{
	int rv;
	char *iname;
	glob_item_t *item;
	glob_item_t  *removed_item;

	iname = (char *)name;

	item = (glob_item_t *)cp_trie_exact_match(nkn_cnt_trie, iname);
	if (item) {
		if (delete_trie_entry) {
			rv = cp_trie_remove(nkn_cnt_trie, iname, 
					    (void **) &removed_item);
			// Note: Success: rv >= 0
		}
		return item;
	} else {
		return NULL;
	}
}
/******************************************************************************/
#endif /* USE_TRIE_LOOKASIDE */
/******************************************************************************/

static int
add_counter(const char *name, void *addr, uint32_t size)
{
	int namelen = strlen(name);
#ifdef USE_TRIE_LOOKASIDE
	glob_item_t *item;
	int rv;
#endif

	if (varend + namelen > vartotal)
		return ENOMEM;

	/* Must be a coding bug 
	 * We only support sizes 1, 2, 4 and 8.
	 * any thing else is an error
	 */ 
	if((size != 1) && (size != 2) && (size != 4) && (size != 8)){
	    /* This should not happen. If this happens, we would miss
	     * some counters. glob_ should be used only for counters 
	     * Asserting if this fails.
	     */
		assert(0);
		return ENOMEM;
	}

	pthread_mutex_lock(&nkn_counts_lock);
	glob_add_count_counter++;
	if (pshmdef->tot_cnts > (max_cnt_space-2))
	{
		pthread_mutex_unlock(&nkn_counts_lock);
		return ENOMEM;
	}
	g_allcnts[pshmdef->tot_cnts].name = varend - varname;
	strcpy(varend, name);
	g_allcnts[pshmdef->tot_cnts].addr = addr;
	varend+=strlen(varend)+1;
#if 0
	if (size > 8) {
		if (size + varend > vartotal) {
			pthread_mutex_unlock(&nkn_counts_lock);
			return ENOMEM;
		}
		g_allcnts[pshmdef->tot_cnts].value = varend - varname;
		varend+=size;
	}
#endif // 0
	g_allcnts[pshmdef->tot_cnts].size = size;
#ifdef USE_TRIE_LOOKASIDE
	item = (glob_item_t *)
		cp_trie_exact_match(nkn_cnt_trie, 
				 varname + g_allcnts[pshmdef->tot_cnts].name);
	if (item) {
		/* Update */
		/* Note: Above check insures (size > 8) never occurs */
		glob_add_count_counter--;
		varend -= (namelen + 1);
		item->addr = addr;
		item->size = size;
		item->value = 0;
		pshmdef->revision++;
		pthread_mutex_unlock(&nkn_counts_lock);
		return 0;
	} else {
		/* Create */
		rv = cp_trie_add(nkn_cnt_trie, 
				varname + g_allcnts[pshmdef->tot_cnts].name,
				(void *)&g_allcnts[pshmdef->tot_cnts]);
		if (rv) {
			pthread_mutex_unlock(&nkn_counts_lock);
			return ENOMEM;
		}
	}
#endif
	pshmdef->tot_cnts++;
	pshmdef->revision++;
	pthread_mutex_unlock(&nkn_counts_lock);
	return 0;
}

void init_counters(int uniquekey)
{
	Elf         *elf;
	Elf_Scn     *scn = NULL;
	GElf_Shdr   shdr;
	Elf_Data    *data;
	int         fd, i, count;
	pid_t 	mypid;
	char 	filename[1024];
	int		shmid;

#ifdef USE_TRIE_LOOKASIDE
	nkn_cnt_trie = cp_trie_create_trie(
	    (COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|COLLECTION_MODE_DEEP),
	    trie_nkn_cnt_copy, trie_nkn_cnt_destruct);
	if (!nkn_cnt_trie) {
		syslog(LOG_ERR, "cp_trie_create_trie() failed");
		return;
	}
#endif

	/*
	 * Allocated shared memory
	 */

	// Create the segment.
	if ((shmid = shmget(uniquekey, sh_size, IPC_CREAT | 0666)) < 0) {
		if ((shmid = shmget(uniquekey, sh_size, 0666)) < 0) {
			syslog(LOG_ERR, "shmget error errno=%d", errno);
			return;
		}
	}

	/*
	 * Now we attach the segment to our data space.
	 * First structure is nkn_shmdef_t.
	 * Then followed by glob_item_t
	 */
	if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
		syslog(LOG_ERR, "shmat error errno=%d", errno);
		return;
	}
	pshmdef = (nkn_shmdef_t *) shm;
	g_allcnts = (glob_item_t *)(shm+sizeof(nkn_shmdef_t));
	varname = (char *)(shm+sizeof(nkn_shmdef_t)+sizeof(glob_item_t)*max_cnt_space);
	vartotal = (char *)shm+ sh_size;
	pthread_mutex_init(&nkn_counts_lock, NULL);

	dbg_pshmdef = (nkn_shmdef_t *)calloc(1, sh_size);
	if (dbg_pshmdef == NULL)
	    syslog(LOG_ERR, "counters allocation failed errno=%d", errno);

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
		if (!elf_strptr(elf, shdr.sh_link, sym.st_name)) continue;
		//if (ELF64_ST_BIND(sym.st_info) != STB_GLOBAL) continue;
		if(strncmp(elf_strptr(elf, shdr.sh_link, sym.st_name), "glob_", 5)==0) {
			ret = add_counter(elf_strptr(elf, shdr.sh_link, sym.st_name), 
					(void *)sym.st_value, sym.st_size);
			if (ret) break;

			// We only pick up the first max_cnt_space.
			if(pshmdef->tot_cnts >= max_cnt_space) break;
		}
	}
	pshmdef->static_cnts = pshmdef->tot_cnts;
	elf_end(elf);
	close(fd);

	

	return; // Done.
}

static
glob_item_t * find_next_deleted_counter(const char *name)
{
#ifdef USE_TRIE_LOOKASIDE
	glob_item_t *item;

	item = int_find_counter(0, name);
	if (item && (item->size == 0)) {
		return item;
	}
	return NULL;

#else /* !USE_TRIE_LOOKASIDE */
	int32_t i, nexti;
	char *iname;
	
	if (g_next_deleted_counter == -1) {
		return NULL;
	}

	/*
	 * Check out the first counter slot.
	 */
	i = g_next_deleted_counter;
	iname = varname + g_allcnts[i].name;
	if (strncmp(iname, name, MAX_CTR_NAME_LEN) == 0) {
		g_next_deleted_counter = g_allcnts[i].next_deleted_counter;
		return &g_allcnts[i];
	}

	/*
	 * Follow the link.
	 */
	nexti = g_allcnts[i].next_deleted_counter;
	while (nexti != -1) {

		iname = varname + g_allcnts[nexti].name;
		if ( (strcmp(iname, name) == 0) &&
		     (g_allcnts[nexti].size == 0) ) {
			// found it.
			g_allcnts[i].next_deleted_counter = g_allcnts[nexti].next_deleted_counter;
			return &g_allcnts[nexti];
		}
		i = nexti;
		nexti = g_allcnts[i].next_deleted_counter;
	}
	return NULL;
#endif /* !USE_TRIE_LOOKASIDE */
}

static
glob_item_t * find_counter(const char *name)
{
#ifdef USE_TRIE_LOOKASIDE
	UNUSED_ARGUMENT(name);
	return int_find_counter(0, name);

#else /* !USE_TRIE_LOOKASIDE */
	int i = 0;
	glob_item_t *item;
	char *iname;
	
	i = pshmdef->static_cnts;	
	for(; i < pshmdef->tot_cnts; i++) {

		item = &g_allcnts[i];
		iname = varname + item->name;

		if (strcmp(iname, name) == 0) {
			return item;
		}
	}
	return NULL;
#endif /* !USE_TRIE_LOOKASIDE */
}

static int  
make_counter_name(char *pname, const char *name, const char *instance)
{
    if ((pname == NULL) || (name == NULL))
	return 0;
    if (instance == NULL)
	return snprintf(pname, MAX_CTR_NAME_LEN, "%s", name);
    else
	return snprintf(pname, MAX_CTR_NAME_LEN, "%s.%s", instance, name);
#if 0
	static char instname[200]; // Max variable name size is 200 bytes.

	if (instance == NULL) {
		return (char *)name;
	}

	strcpy(instname, (char *)instance);
	strcat(instname, ".");
	strcat(instname, (char *)name);
	return instname;
#endif
}

int nkn_mon_add(const char *name, const char *instance, void *obj, uint32_t size)
{
	char pname[MAX_CTR_NAME_LEN];
	int l;
	glob_item_t *item;

	if (!pshmdef)
		return ENOMEM;

	if (strlen(name) >= MAX_CTR_NAME_LEN) {
		//We will not add this rogue counter
		//No one is catching this return value
		//So, even if I return 0, does not matter
		return -1;
	}

	l = make_counter_name(pname, name, instance);
	assert(l);

	/*
	 * if we found one with the same name. use it.
	 */
	pthread_mutex_lock(&nkn_counts_lock);
	item = find_next_deleted_counter(pname);
	if ((item != NULL) && (item->size == 0)) {
		item->addr = obj;
		item->size = size;
		pshmdef->revision++;
		pthread_mutex_unlock(&nkn_counts_lock);
		return 0;
	}
	pthread_mutex_unlock(&nkn_counts_lock);

	return add_counter(pname, obj, size);
}

void nkn_mon_delete(const char *name, const char *instance)
{
	glob_item_t *item;
	char pname[MAX_CTR_NAME_LEN];

	if (!pshmdef)
		return;
	pthread_mutex_lock(&nkn_counts_lock);
	make_counter_name(pname, name, instance);
#ifdef USE_TRIE_LOOKASIDE
	item = int_find_counter(0, pname);
#else
	item = find_counter(pname);
#endif
	if (!item) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return;
	}
#ifndef USE_TRIE_LOOKASIDE
	item->next_deleted_counter = g_next_deleted_counter;
	g_next_deleted_counter = ((char *)item - (char *)&g_allcnts[0]) / sizeof(glob_item_t);
#endif
	item->size = 0;		// indicates free entry 
	pshmdef->revision++;
	pthread_mutex_unlock(&nkn_counts_lock);
}


void nkn_mon_get(const char *name, const char *instance, void *obj, uint32_t *size)
{
	glob_item_t *item;
	char pname[MAX_CTR_NAME_LEN];

	if (obj == NULL)
		return;
	if (!pshmdef)
		return;
	pthread_mutex_lock(&nkn_counts_lock);
	make_counter_name(pname, name, instance);
	item = find_counter(pname);
	if (!item) {
		pthread_mutex_unlock(&nkn_counts_lock);
		return;
	}

	switch(item->size)
	{
	case 0: *(uint8_t *)(obj) = 0; *size = 0; break;
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
	char pname[MAX_CTR_NAME_LEN];
	if (!pshmdef)
		return;
	pthread_mutex_lock(&nkn_counts_lock);
	make_counter_name(pname, name, instance);
	item = find_counter(pname);
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

void copy_counters(void)
{
    int i;
    unsigned char * pc;
    unsigned short * ps;
    unsigned int * pi;
    unsigned long * pl;


    if (!pshmdef)
		return;
    pthread_mutex_lock(&nkn_counts_lock);
   
    // Make a local copy for debug since we do not dump shared memory segments
    if (dbg_pshmdef && (dbg_pshmdef->revision != pshmdef->revision)) {
	memcpy(dbg_pshmdef, pshmdef, sh_size);
    }

    for(i=0;i<pshmdef->tot_cnts; i++) {
        //printf("%s: %d   %p\n", g_allcnts[i].name, g_allcnts[i].size, g_allcnts[i].addr);
        switch(g_allcnts[i].size) {
        case 1:
            pc=(unsigned char *) g_allcnts[i].addr;
	    g_allcnts[i].value=*pc;
            break;
        case 2:
            ps=(unsigned short *) g_allcnts[i].addr;
	    g_allcnts[i].value=*ps;
            break;
        case 4:
            pi=(unsigned int *) g_allcnts[i].addr;
	    g_allcnts[i].value=*pi;
            break;
        case 8:
            pl=(unsigned long *) g_allcnts[i].addr;
	    g_allcnts[i].value=*pl;
            break;
	default:
	    break;
        }
    }
    pthread_mutex_unlock(&nkn_counts_lock);
}

static void *countercopy_func(void * arg)
{

        prctl(PR_SET_NAME, "nvsd-count", 0, 0, 0);
	UNUSED_ARGUMENT(arg);
        while(1) {
                sleep(copy_counter_interval); 
                copy_counters();
        }
        return NULL;
}

void countercopy_init(void)
{
        int rv;
        pthread_attr_t attr;
        int stacksize = 128 * KiBYTES;

        rv = pthread_attr_init(&attr);
        if (rv) 
	{
                syslog(LOG_ERR, "pthread_attr_init() failed in countercopy_init(), rv=%d", rv);
                return;
        }

        rv = pthread_attr_setstacksize(&attr, stacksize);
        if (rv)
	{
                syslog(LOG_ERR, "pthread_attr_setstacksize() failed, in countercopy_init(), rv=%d", rv);
                return;
        }

        if (pthread_create(&id, &attr,countercopy_func, NULL))
	{
                syslog(LOG_ERR, "Failed to create copycounter thread, rv=%d", rv);
                return;
        }
}

void config_and_run_counter_update(void)
{
        char name[256];
        int ret;

	ret=readlink("/proc/self/exe",name,sizeof(name));
        name[ret]='\0';

	if(strstr(name,"nvsd")!=NULL) {
		key=NKN_SHMKEY;
		max_cnt_space = MAX_CNTS_NVSD;
	}else if(strstr(name,"ssld")!=NULL) {
		key=NKN_SSL_SHMKEY;
		max_cnt_space = MAX_CNTS_SSLD;
	}else if( (strstr(name,"live_mfpd")) ||
		  (strstr(name, "file_mfpd"))) {
	        if ((key=ftok(name,NKN_SHMKEY))<0)
		{
			syslog(LOG_ERR, "ftok failed in config_and_run_counter_update() errno=%d", errno);
			return;
		}
		max_cnt_space = MAX_CNTS_MFP;
	}else if(strstr(name,"cb")!=NULL) {
		key=NKN_CB_SHMKEY;
		max_cnt_space = MAX_CNTS_CB;
	} else {
		if ((key=ftok(name,NKN_SHMKEY))<0)
		{
			syslog(LOG_ERR, "ftok failed in config_and_run_counter_update() errno=%d", errno);
			return;
		}
		max_cnt_space = MAX_CNTS_OTHERS;
	}	
	sh_size = (sizeof(nkn_shmdef_t)+max_cnt_space*(sizeof(glob_item_t)+30));

	init_counters(key);
	countercopy_init();
}
