#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <pthread.h>
#include "nkn_debug.h"
#include "nkn_hash.h"
#include "om_port_mapper.h"
#include "nkn_memalloc.h"
#include "nkn_stat.h"
#include "nkn_lockstat.h"

///////////////////////////////////////////////////////////////////////////////
#ifdef NDEBUG

#define ASSERT(expr, n)		assert(expr)

#else /* Not NDEBUG */
#define ASSERT(expr, n)								\
    do{										\
	if ((expr)) {								\
	    __ASSERT_VOID_CAST(0);						\
	} else {								\
	    __assert_fail (__STRING(expr), __FILE__, __LINE__, __ASSERT_FUNCTION);  \
	    return (n);								\
	}									\
    } while(0)

#endif /* NDEBUG */


#if 0
#define log_dbg(sev, fmt,...)							\
    do {									\
	fprintf(stderr, "%s(%5d) : "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
    } while(0)


#endif

#define log_dbg(sev, fmt,...)	DBG_LOG(sev, MOD_OM, fmt, ##__VA_ARGS__)


#define fprintf(x,p,...)    do{}while(0)
///////////////////////////////////////////////////////////////////////////////
#define PM_MAGIC	    0x50414D50	// "PMAP"


struct pmapper {
    uint32_t	    magic;
    NHashTable	    *table;
    pthread_mutex_t mutex;  // Lock on the table, for insert/remove operations
    uint32_t	    clients;
    uint32_t	    port_base;
    uint16_t	    pcount;
};


struct pmapper_stats {
    AO_t	    ports_alloced;
    AO_t	    ports_dealloced;
    AO_t	    no_ports;
    AO_t	    bad_ports;
    AO_t	    no_context;
    uint64_t	    collisions;
    uint64_t	    alloc_retries;
    nkn_lockstat_t  lockstat;
    nkn_lockstat_t  tbl_lockstat;
};

struct pmapper_stats	pmap_stat;
struct om_portmap_conf om_pmap_config;

#define INC_STAT(stat)	    AO_fetch_and_add1(&(pmap_stat.stat))
#define DEC_STAT(stat)	    AO_fetch_and_sub1(&(pmap_stat.stat))

#define __ADD_COUNTER(name, var) \
    nkn_mon_add(name, "portmapper", (void *)&(pmap_stat.var), sizeof(pmap_stat.var))

/* For us a WORD = 32 bit.
 */
typedef uint64_t    WORD;

//static uint32_t pcount = MAX_PORTS;


struct tuple {
    uint32_t	dip;
    uint16_t	dport;
    uint16_t	cport;
};

struct conn_entry {
    union {
	struct tuple	tuple;
	uint64_t	tuple_word;
    } u;

    LIST_ENTRY(conn_entry)  entries;
};

LIST_HEAD(connhead, conn_entry);


struct portmap {
    uint32_t	    client_ip;
    pthread_mutex_t mutex;
    uint32_t	    current_used; // refcount for this entry.
    //uint32_t	    max_used;
    uint32_t	    pcount;
    uint16_t	    base;
    uint16_t	    curr_port; // Incrementing number to allocate ports.
    uint64_t	    last_used;

    struct connhead conns;
};

uint32_t    port_alloc = 0;

static int conn_unique(struct conn_entry *c1, struct conn_entry *c2);
static int om_pmap_client_remove(pmapper_t context, unsigned int client_ip);

static inline struct pmapper*
pmapper(pmapper_t context)
{
    return (struct pmapper*)(context);
}

/*
 * Given 2 keys, check if they are identical or not.
 * Called by the hash table engine.
 */
static inline int
om_port_do_equal(const void *key1, const void *key2)
{
    const uint32_t addr1 = *(const uint32_t*)(key1);
    const uint32_t addr2 = *(const uint32_t*)(key2);

    return (addr1 == addr2);
}

/*
 * Given a key, return the hash for it.
 * Called by the hash table engine.
 */
static inline uint64_t 
om_port_do_hash(const void *key)
{
    return ((*(const uint64_t *)(key)) & 0x00000000FFFFFFFF);
}


/*
 * Allocate a client-IP entry.
 */
static inline struct portmap *
om_pmap_client_alloc(uint32_t client_ip,
		     uint16_t pcount, uint16_t base)
{
    struct portmap *p = nkn_malloc_type(sizeof(struct portmap), mod_portmap_t);
    p->client_ip = client_ip;
    p->current_used = 0;
    p->pcount = pcount;
    p->base = base;
    //p->curr_port = base + (int) (32768.0 * (rand() / (RAND_MAX + 1.0))); // base;
    p->curr_port = base + (int) (pcount * (rand() / (RAND_MAX + 1.0))); // base;
    LIST_INIT(&(p->conns));
    pthread_mutex_init(&(p->mutex), NULL);
    return p;
}

/*
 * Free a client-IP entry
 */
static inline int
om_pmap_client_free(struct portmap *p)
{
    if (p->current_used > 0)
	return -1;

    free(p);
    return 0;
}

/*
 * Given a portmap entry for a client-ip, get a free port
 */
static int
om_pmap_get_port(struct portmap *pmap, uint32_t dest_ip, uint16_t dport, uint16_t *port)
{
    int ret = -ENOPORT;
    struct conn_entry *c1;
    struct conn_entry c2;
    uint16_t	tmp, curr;

    *port = 0;
    c2.u.tuple.dip = dest_ip;
    c2.u.tuple.dport = dport;

    // BEGIN: Critical section
    NKN_MUTEX_LOCKSTAT((&(pmap->mutex)), &(pmap_stat.lockstat));

    curr = pmap->curr_port;

not_unique:
    // Allocate the next port.
    tmp = pmap->curr_port++;
    // protect rollovers
    if (pmap->curr_port > (pmap->base + pmap->pcount)) {
        pmap->curr_port = pmap->base;
    }

    if (curr == pmap->curr_port) {
	// Full scan done.. and still we cant allocate a port.
	goto no_ports;
    }

    c2.u.tuple.cport = tmp;

    // Check for collisions
    LIST_FOREACH(c1, &(pmap->conns), entries) {

	if (conn_unique(c1, &c2)) {
	    INC_STAT(collisions);
	    goto not_unique;
	}
	else
	    goto unique;
    }

unique:
    c1 = nkn_malloc_type(sizeof(struct conn_entry), mod_conn_entry_t);
    c1->u.tuple_word = c2.u.tuple_word;
    LIST_INSERT_HEAD(&(pmap->conns), c1, entries);
    pmap->current_used++; // ref cnt

    *port = tmp;
    ret = 0;

no_ports:
    pthread_mutex_unlock(&(pmap->mutex));
    // END: Critical Section

    return ret;
}

/*
 * Given a portmap entry for a client-ip and a port, free the port
 * back to pool.
 */
static int
om_pmap_put_port(struct portmap *pmap, unsigned short port)
{
    int ret = -EBADPORT;
    struct conn_entry *c1;

    // BEGIN: Critical section
    NKN_MUTEX_LOCKSTAT((&(pmap->mutex)), &(pmap_stat.lockstat));

    // TODO: Must check if the port was allocated by us or not
    // Right now it is good faith!!

    LIST_FOREACH(c1, &(pmap->conns), entries) {
	if (c1->u.tuple.cport == port) {
	    // Free this entry.
	    LIST_REMOVE(c1, entries);
	    pmap->current_used--;
	    free(c1);
	    ret = 0;
	    break;
	}
    }

    pthread_mutex_unlock(&(pmap->mutex));
    // END: Critical Section

    return ret;
}

/* Given a client-ip, check and return a context.
 * If not found, create the context, insert and
 * return.
 */
static struct portmap *
om_pmap_client_get(pmapper_t context,
		   unsigned int client_ip, int insertable)
{
    struct pmapper *pm = pmapper(context);
    struct portmap *p = NULL;

    p = nhash_lookup(pm->table,
		om_port_do_hash(&client_ip),
		(void *) &(client_ip));
    if ((NULL == p) && (insertable)) {
	// First entry for this Client IP.
	p = om_pmap_client_alloc(client_ip, pm->pcount, pm->port_base);

	// BEGIN: CRITICAL Section
	NKN_MUTEX_LOCKSTAT((&(pm->mutex)), &(pmap_stat.tbl_lockstat));
	//pthread_mutex_lock(&pm->mutex);
	nhash_insert(pm->table,
		om_port_do_hash(&client_ip),
		(void *) &(p->client_ip),
		(void *) p);
	pthread_mutex_unlock(&pm->mutex);
	// END: CRITICAL Section

    } else {
	//log_dbg(DEBUG, "Client IP found..\n");
    }

    return p;
}

int
om_pmapper_get_port(pmapper_t context, uint32_t client_ip,
		    uint32_t dest_ip, uint16_t dport, uint16_t *port)
{
    struct pmapper *pm = pmapper(context);
    struct portmap *p = NULL;

    /* Check if feature disabled */
    if (om_pmap_config.pmapper_disable) {
	if (*port) {
	    /* returning a port = 0 will cause bind(0), which will
	     * tell kernel to auto-allocate port for us. This is the
	     * old behavior.
	     */
	    *port = 0;
	}
	return 0;
    }

    /* Basic sanity */
    ASSERT(pm != NULL, -EINVAL);
    ASSERT(pm->magic == PM_MAGIC, -EBADCONTEXT);
    ASSERT((client_ip != 0) || (client_ip != 0xFFFFFFFF), -EINVAL);
    ASSERT(port != NULL, -EINVAL);
    ASSERT(pm->table != NULL, -EINVAL);

    /* Get a client-ip context. If not found, create one
     * and add to the hash-table.
     */
    p = om_pmap_client_get(context, client_ip, 1);

    /* Oops.. we failed to find and/or allocate one.
     * bail!!
     */
    if (NULL == p) {
	return -ENOMEM;
    }

    /* Get a free port. */
    if (om_pmap_get_port(p, dest_ip, dport, port) < 0) {
	/* No free ports?? bail.
	 */
	INC_STAT(no_ports);
	return -ENOPORT;
    } else {
	INC_STAT(ports_alloced);
    }

    return 0;
}

int
om_pmapper_put_port(pmapper_t context, uint32_t client_ip, uint16_t port)
{
    struct pmapper *pm = pmapper(context);
    struct portmap *p = NULL;
    int ret = 0;

    /* Check if feature disabled */
    if (om_pmap_config.pmapper_disable) {
	return 0;
    }

    /* Basic sanity */
    ASSERT(pm != NULL, -EINVAL);
    ASSERT(pm->magic == PM_MAGIC, -EBADCONTEXT);
    ASSERT((client_ip != 0) && (client_ip != 0xFFFFFFFF), -1);
    ASSERT(port != 0, -EINVAL);
    ASSERT(pm->table != NULL, -EINVAL);

    /* Get a client-ip context. Ip not found.. something wrong
     * We couldnt have deleted the IP entry till some port
     * was allocated.
     */
    p = om_pmap_client_get(context, client_ip, 0);

    if (NULL == p) {
	log_dbg(WARNING, "Hmm... client IP give with no entry in my table!!\n");
	return -ENOCTXT;
    }

    /* Free the port */
    if ((ret = om_pmap_put_port(p, port)) < 0) {
	log_dbg(WARNING, "Duh... put port failed.. Insane setup\n");
	if (ret == -EBADPORT) {
	    INC_STAT(bad_ports);
	}
	return ret;
    } else {
	INC_STAT(ports_dealloced);

	if (p->current_used == 0) {
	    /* TODO: need to dealloc here. but then the tuple in stack,
	     * if moved to death_row, will throw an error when we reuse
	     * the same port from the same source to the same dest.
	     * the death_row tuple is un-reusable for at least 1s from
	     * the time the connection is closed and moves to TIME_WAIT.
	     */
	    return 0;
	    (void) om_pmap_client_remove(context, client_ip);

	    om_pmap_client_free(p);
	}

    }

    return 0;
}


static int
om_pmap_client_remove(pmapper_t context, unsigned int client_ip)
{
    struct pmapper *pm = pmapper(context);

    NKN_MUTEX_LOCKSTAT((&(pm->mutex)), &(pmap_stat.tbl_lockstat));

    nhash_remove(pm->table,
	    om_port_do_hash(&client_ip),
	    (void *) (&client_ip));
    pthread_mutex_unlock(&pm->mutex);
    return 0;
}


/*
 * Initialize the mapper table. Create a hash table, init
 * a mutex, etc.
 */
int
om_pmapper_init(pmapper_t context)
{
    struct pmapper *pm = pmapper(context);

    ASSERT(pm->magic == PM_MAGIC, -EBADCONTEXT);
    DBG_LOG(MSG3, MOD_OM, "MAX Ports per client: %d\n", pm->pcount);

    pm->table = nhash_table_new(
		om_port_do_hash,
		om_port_do_equal,
		512*1024, mod_portmap_table_t);

    ASSERT(pm->table != NULL, -ENOMEM);

    pthread_mutex_init(&pm->mutex, NULL);

    // Round off/up pcount to something thats aligned to a WORD
   // pm->pcount = roundup(pm->pcount,
	//	    (sizeof(WORD)*NBBY))/(sizeof(WORD)*NBBY);

    //printf("(round-up) pcount = %d words\n", pm->pcount);
    //printf("ports: %lu\n", pm->pcount*sizeof(WORD)*NBBY);


    __ADD_COUNTER("ports_alloced", ports_alloced);
    __ADD_COUNTER("ports_dealloced", ports_dealloced);
    __ADD_COUNTER("no_ports", no_ports);
    __ADD_COUNTER("bad_ports", bad_ports);
    __ADD_COUNTER("no_context", no_context);
    __ADD_COUNTER("collisions", collisions);
    __ADD_COUNTER("alloc_retries", alloc_retries);

    NKN_LOCKSTAT_INIT(&(pmap_stat.tbl_lockstat), "portmapper.lockstat.tbl");
    NKN_LOCKSTAT_INIT(&(pmap_stat.lockstat), "portmapper.lockstat");

    return 0;
}

int
om_pmapper_deinit(pmapper_t context)
{
    (void)context;
    return 0;
}


/* Always called before anything is done.
 * Create/allocate a context and return opaque
 * handle to caller. Handle should be used in
 * all calls to the pmapper module.
 */
pmapper_t
om_pmapper_new(void)
{
    struct pmapper *p = nkn_calloc_type(1, sizeof(struct pmapper), mod_pmapper_t);
    p->magic = PM_MAGIC;
    return (pmapper_t)(p);
}

int
om_pmapper_delete(pmapper_t context)
{
    struct pmapper *p = pmapper(context);

    ASSERT(p != NULL, -EINVAL);
    ASSERT(p->magic == PM_MAGIC, -EBADCONTEXT);

    if (p->clients > 0) {
	// Cant free this..
	return -1;
    }

    // Delete the table
    if (p->table) {
	nhash_destroy(p->table);
	p->table = NULL;
    }

    free(p);

    return 0;
}


static int
conn_unique(struct conn_entry *c1, struct conn_entry *c2)
{
    return (c1->u.tuple_word == c2->u.tuple_word);
}

/*----------------------------------------------------------------------------
 *			------- CONFIG PLANE APIs ---------
 *--------------------------------------------------------------------------*/

/*
 * Set the port base on which to allocate from for
 * a mapper context.
 * Applies only to new clients after this change
 * is done. For client-IPs that are already in the
 * table, this change is not effected.
 */
int
om_pmapper_set_port_base(pmapper_t context, unsigned short port)
{
    struct pmapper *pm = pmapper(context);

    ASSERT(pm != NULL, -EINVAL);
    ASSERT(pm->magic == PM_MAGIC, -EBADCONTEXT);
    ASSERT(port != 0, -EINVAL);
    pm->port_base = port;
	DBG_LOG(MSG, MOD_OM, "Port base is set to %d\n", pm->port_base);
    return 0;
}


/*
 * Configuration plane hook to set the port count.
 * Applies only to new clients after this change
 * is done. For client-IPs that are already in the
 * table, this change is not effected.
 */
int
om_pmapper_set_port_count(pmapper_t context, unsigned int count)
{
    struct pmapper *pm = pmapper(context);
    ASSERT(pm->magic == PM_MAGIC, -EBADCONTEXT);
    pm->pcount = count;
	DBG_LOG(MSG, MOD_OM, "Port count is set to %d\n", pm->pcount);
    return 0;
}

#undef __ADD_COUNTER
#undef INC_STAT
