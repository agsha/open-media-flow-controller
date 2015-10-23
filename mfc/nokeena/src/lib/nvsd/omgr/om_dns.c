#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <netdb.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"
#include "nvsd_mgmt_api.h"

#define SLIST_FOREACH_SAFE(var, head, field, tvar)                      \
        for ((var) = SLIST_FIRST((head));                               \
                (var) && ((tvar) = SLIST_NEXT((var), field), 1);            \
                (var) = (tvar))

#define NKN_STRLCPY(s1, s2, n)		\
	strncpy(s1,s2,n);		\
	s1[n-1]='\0'


NKNCNT_DEF(dns_tot_cache_entry, AO_t, "", "Tot cached dns entry")
NKNCNT_DEF(dns_tot_cache_entry_ipv6, AO_t, "", "Tot cached dns entry for ipv6")
NKNCNT_DEF(dns_cache_hit, int64_t, "", "Tot cached dns hit")
NKNCNT_DEF(dns_cache_miss, int64_t, "", "Tot cached dns hit")

#define DNS_IP_ENTRY_TTL_VALID          0x01
#define DNS_IP_ENTRY_TTL_STALE          0x02
#define DNS_IP_ENTRY_STATUS_ALIVE       0x04
#define DNS_IP_ENTRY_STATUS_DEAD        0x08
#define DNS_EVICT_TIME_IN_SECS		5

#define SET_IP_ENTRY_FLAG(x, f)  (x)->flag |= (f)
#define CLEAR_IP_ENTRY_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_IP_ENTRY_FLAG(x, f) ( ((x)->flag & (f)) == (f) )

#define DEFAULT_DNS_BASE_TTL	1024
#define DEFAULT_DNS_ERR_TTL	60
#define MAX_DNSHASH	409600

SLIST_HEAD(ip_list_head, ip_addr_entry_t);

struct ip_addr_entry_t {
        SLIST_ENTRY(ip_addr_entry_t) entries;
        ip_addr_t ip;
        uint32_t ttl;
        uint8_t flag;
};

/*
 * TBD: 1. to support DNS round robin.
 *      2. to support DNS TTL
 *      3. to add timer to expire the DNS entry.
 */
struct dns_cache_t {
        struct dns_cache_t * next;
        char * domain;
	uint8_t resolved;
	struct ip_list_head *ip_list;
	uint8_t family;
        uint64_t ts;
        int32_t hitcnt;
};

struct dns_ip_entry_t {
	ip_addr_t ip;
	uint8_t resolved;
};

static struct dns_cache_t * dnshash[MAX_DNSHASH]; 
static pthread_mutex_t dns_mutex[MAX_DNSHASH];

int is_loopback(ip_addr_t *ip_ptr);
char *get_ip_str(ip_addr_t *ip_ptr);
static int dns_lookup(uint32_t hash, char *domain, int32_t family, int *found, struct dns_ip_entry_t *ip_entry);
static inline void dns_delete(uint32_t hash);
static int dns_delete_domain(uint32_t hash, char * domain)  __attribute__ ((unused));
static int dns_mark_ip_entry(uint32_t hash, char * domain, int *found, ip_addr_t ip);
static void dns_insert(uint32_t hash, char * domain, ip_addr_t ip_ptr[], 
	int ttl[], uint8_t resolved, int num);

static pthread_t dns_timerid;

extern int nkn_max_domain_ips;

/* TODO: move get_ip_str() and is_loopback() to some common file */
static __thread char ip_str[INET6_ADDRSTRLEN + 1];
char *get_ip_str(ip_addr_t *ip_ptr)
{
    ip_str[INET6_ADDRSTRLEN] = 0;

    if (ip_ptr == NULL || 
        (ip_ptr->family != AF_INET && ip_ptr->family != AF_INET6)){
        strncpy(ip_str, "invalid IP", INET6_ADDRSTRLEN);
    } else if (ip_ptr->family == AF_INET) {
        inet_ntop(AF_INET, &(ip_ptr->addr.v4),
                  ip_str, INET_ADDRSTRLEN);
    } else {
        inet_ntop(AF_INET6, &(ip_ptr->addr.v6),
                  ip_str, INET6_ADDRSTRLEN);
    }

    return ip_str;
}


int is_loopback(ip_addr_t *ip_ptr)
{
    if (ip_ptr->family == AF_INET) {
        if (ip_ptr->addr.v4.s_addr == htonl(INADDR_LOOPBACK)) {
            return 1;
        }
    } else if (ip_ptr->family == AF_INET6) {
        if (IN6_IS_ADDR_LOOPBACK(&(ip_ptr->addr.v6))) {
            return 1;
        }
    }

    return 0;
}

static inline uint32_t
DNS_DOMAIN_HASH(char *domain)
{
    unsigned int hash=0, i=0;
    while(domain[i] != '\0')
    {
        hash += domain[i] | 0x20 ;
        hash += (hash << 10);
        hash ^= (hash >> 6);
        i++ ;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % MAX_DNSHASH;
}

static int
dns_lookup(uint32_t hash, char * domain, int32_t family, int *found, struct dns_ip_entry_t *ip_entry)
{
        struct dns_cache_t * pdns;
	struct ip_list_head *head;
	struct ip_addr_entry_t *node, *next_node;
	int ret = 0;

        pdns = dnshash[hash];

	while(pdns) {
		if(strcasecmp(pdns->domain, domain)==0 &&
		   pdns->family == family) {
			*found = 1;
			head = pdns->ip_list;

			SLIST_FOREACH(node, head, entries) {
				//Note that INADDR_NONE with AF_INET family is manually set to 
				//indicate DNS error scenarios. Hence IPv4() check alone done below.
				if (IPv4(node->ip) == INADDR_NONE) {
					*found = 0;
					CLEAR_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_VALID);
					SET_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_STALE);
					dns_delete_domain(hash, domain);
					IPv4(ip_entry->ip) = INADDR_NONE;
					ip_entry->resolved = 0;
					ret = 1;
					goto done;
				}
				if (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_ALIVE) &&
				   (nkn_cur_ts - pdns->ts <= node->ttl)) {
					pdns->hitcnt++;
					ip_entry->ip = node->ip;
					ip_entry->resolved = pdns->resolved;
					ret = 1;
					goto done;
				} else {
					int is_expired_entry = 0;
					CLEAR_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_VALID);
					SET_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_STALE);
					next_node = SLIST_NEXT(node, entries);
					is_expired_entry = (nkn_cur_ts - pdns->ts > node->ttl)? 1 : 0;
					if (next_node == NULL) {
						if (is_expired_entry) {
							dns_delete_domain(hash, domain);
							*found = 0;
							goto done;
						} else if (!is_expired_entry &&
							   CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_DEAD)) {
							dns_delete_domain(hash, domain);
							goto done;
						}
					}
				}
			}
		}
		pdns = pdns->next;
	}
	glob_dns_cache_miss++;
done:
	return ret;
}

static int dns_mark_ip_entry(uint32_t hash, char * domain, int *found, ip_addr_t ip) 
{
        struct dns_cache_t * pdns;
        struct ip_addr_entry_t *node;
        struct ip_list_head *head;
	char ip_string[INET6_ADDRSTRLEN + 1];
	char node_ip_string[INET6_ADDRSTRLEN+1];

        *found = 0;
        pdns = dnshash[hash];
        while(pdns) {
                if(strcasecmp(pdns->domain, domain)==0) {
                        NKN_STRLCPY(ip_string, get_ip_str(&ip), sizeof(ip_string));
                        head = pdns->ip_list;
                        SLIST_FOREACH(node, head, entries) {
                                NKN_STRLCPY(node_ip_string, get_ip_str(&node->ip), sizeof(node_ip_string));
                                if (strcmp(node_ip_string, ip_string) == 0) {
                                        CLEAR_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_ALIVE);
                                        SET_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_DEAD);
                                        DBG_LOG(MSG, MOD_OM,
                                                "dns_mark_ip_entry: Marked ip:%s as DEAD for domain:%s\n",
                                                pdns->resolved ? node_ip_string : "<unresolved>",
						domain);
                                        *found = 1;
                                        return 0;
                                }
                        }
                }
                pdns = pdns->next;
        }
        return 1;
}

static inline void dns_delete(uint32_t hash)
{
        struct dns_cache_t * pdns, * pdnsnext, * pdnstemp;
	struct ip_list_head *head;
	struct ip_addr_entry_t *node, *temp;
	int first_node_flag=0;

        pdns = dnshash[hash];
        if( !pdns ) return;

        /*
         * the link list is sorted by time. latest time on the top
         */
	head = pdns->ip_list;
	SLIST_FOREACH(node, head, entries) {
		if (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_STALE) ||
		   (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_DEAD)) ||
		   (nkn_cur_ts - pdns->ts > node->ttl)) {
			first_node_flag = 1;
			goto delete_chain;
		}
	}
	while(pdns->next) {
		head = pdns->next->ip_list;
		SLIST_FOREACH(node, head, entries) {
			if (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_STALE) ||
			   (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_DEAD)) ||
			   (nkn_cur_ts - pdns->next->ts > node->ttl)) {
				pdnsnext = pdns->next;
				pdnstemp = pdns;
				pdns = pdnsnext;
				goto delete_chain;
			}
		}
		pdns = pdns->next;
	}
	return;

delete_chain:

	/*
	 * Time to delete the expired dns entry.
	 */
	while(pdns) {
		pdnsnext = pdns->next;
		head = pdns->ip_list;

		SLIST_FOREACH_SAFE(node, head, entries, temp) {
			if (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_STALE) ||
			   (CHECK_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_DEAD)) ||
			   (nkn_cur_ts - pdns->ts > node->ttl)) {
				DBG_LOG(MSG, MOD_OM, "dns_delete: domain %s with ip %s\n",
				pdns->domain, pdns->resolved ? get_ip_str(&node->ip) : "<unresolved>");
				SLIST_REMOVE(head, node, ip_addr_entry_t, entries);
				free(node);
			}
		}
		if(SLIST_EMPTY(head)) {
			if (first_node_flag) {
				dnshash[hash] = pdnsnext;
			} else {
				pdnstemp->next = pdnsnext;
			}
			if (pdns->family == AF_INET6) {
				AO_fetch_and_sub1(&glob_dns_tot_cache_entry_ipv6);
			} else {
				AO_fetch_and_sub1(&glob_dns_tot_cache_entry);
			}
			free(head);
			free(pdns->domain);
			free(pdns);
		}
		pdns = pdnsnext;
	}
}

static void
dns_insert(uint32_t hash, char * domain, ip_addr_t ip_ptr[], int ttl[],
           uint8_t resolved, int num)
{
	struct dns_cache_t * pdns = NULL;
	struct ip_list_head *head;
	struct ip_addr_entry_t *node;
	struct dns_ip_entry_t ip_entry;
	int i, found, ret;

	if (ip_ptr == NULL) {
		DBG_LOG(ERROR, MOD_OM, "ip_ptr is NULL");
		return;
	}

	/*
	 * Generate a random ttl between 1024 and 2047
	 */
	int32_t rand_ttl=((rand()%DEFAULT_DNS_BASE_TTL)+DEFAULT_DNS_BASE_TTL);

	head = (struct ip_list_head *)nkn_calloc_type(1, sizeof(struct ip_list_head), mod_ip_list_head);
	if(!head) return;

	SLIST_INIT(head);
//      UNUSED_ARGUMENT(ttl);
	ret = dns_lookup(hash, domain, ip_ptr[0].family, &found, &ip_entry);
	if(ret) {
		DBG_LOG(MSG, MOD_OM, "dns_insert: domain %s already exists in DNS cache!",
			domain);
		goto bail_error;
	}

	pdns = (struct dns_cache_t *)nkn_malloc_type(sizeof(struct dns_cache_t), mod_dns_entry);
	if(!pdns) goto bail_error;
	pdns->domain = nkn_strdup_type(domain, mod_dns_entry);
	for(i=0; i<num; i++) {
		node = (struct ip_addr_entry_t *)nkn_calloc_type(1, sizeof(struct ip_addr_entry_t), mod_dns_ip_entry);
		if (!node) {
			if (i==0) {
				free(pdns);
				goto bail_error;
			} else {
				goto done;
			}
		}

		pdns->resolved = resolved;
		pdns->family = ip_ptr[i].family;
		node->ip = ip_ptr[i];

		/* glob_adns_cache_timeout:
		 *  -100    Use TTL from DNS server     [auto]
		 *  -1      (NYI) Use a random generated number
		 *          between 1024 and 2400 [random]
		 *   0      expire immediate
		 *  > 0     Actual TTL in seconds with 60 being minimum
		 *          resolution
		 *
		 *  TODO: set a random number for TTL is glob_adns_cache_timeout < 0
		 */
		node->ttl = (glob_adns_cache_timeout == -100) ?
			(ttl[i]) :
			((glob_adns_cache_timeout == -1) ?
			rand_ttl :
			glob_adns_cache_timeout);
		/* For failed domains, always evict after DEFAULT_DNS_ERR_TTL secs */
		if (pdns->resolved == 0) {
			node->ttl = DEFAULT_DNS_ERR_TTL;
		}

		SET_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_TTL_VALID);
		SET_IP_ENTRY_FLAG(node, DNS_IP_ENTRY_STATUS_ALIVE);

		SLIST_INSERT_HEAD(head, node, entries);

		DBG_LOG(MSG, MOD_OM, "dns_insert: domain %s with ip %s hash %d resolved %d family %d\n",
			domain, resolved ? get_ip_str(&node->ip) : "<unresolved>",
			hash, pdns->resolved, pdns->family);
	}
done:
	pdns->ip_list = head;
	pdns->ts = nkn_cur_ts;
	pdns->hitcnt = 1;
	pdns->next = dnshash[hash];
	dnshash[hash] = pdns;
	if (pdns->family == AF_INET6) {
		AO_fetch_and_add1(&glob_dns_tot_cache_entry_ipv6);
	} else {
		AO_fetch_and_add1(&glob_dns_tot_cache_entry);
	}
	return;

bail_error:
	free(head);
	return;
}

/*
 * This function is called once every 5 seconds.
 */
static void * dns_timer_func(void * arg);
static void * dns_timer_func(void * arg)
{
	int i;

        UNUSED_ARGUMENT(arg);
        prctl(PR_SET_NAME, "nvsd-dns-timer", 0, 0, 0);

	while(1) {
		sleep(DNS_EVICT_TIME_IN_SECS); // sleep 5 seconds

		for (i=0; i<MAX_DNSHASH; i++) {
			if (dnshash[i]) {
				pthread_mutex_lock(&dns_mutex[i]);
				dns_delete(i);
				pthread_mutex_unlock(&dns_mutex[i]);
			}
		}
	}
	return NULL;
}

/* Return 0 if dns cache entry is not found, or the entry is found but
   unresolved. ip_ptr is allocated by caller, and filled by this function */
extern int dns_domain_to_ip46(char * domain, int32_t family, ip_addr_t *ip_ptr);
int dns_domain_to_ip46(char * domain, int32_t family, ip_addr_t *ip_ptr)
{
	uint32_t hash;
	struct addrinfo hints, *res = NULL;
	int rv, ret, found, ret_ip_entry;
	int try_again=1;
	struct dns_ip_entry_t pdns;

	if (!domain || (family != AF_INET && family != AF_INET6) ||
	   ip_ptr == NULL) {
		return 0;
	}
	ret = inet_pton(family, domain, &(ip_ptr->addr));
	/* it is dot-format string: xxx.xxx.xxx.xxx */
	if (ret == -1) {
		DBG_LOG(MSG, MOD_OM, 
			"inet_pton failed for family:%d with errno:%d for domain:%s", 
			family, errno, domain);
		return 0;
	}
 
	if (ret > 0) {
		ip_ptr->family = family;
		DBG_LOG(MSG, MOD_OM, "returns %s for domain %s",
			get_ip_str(ip_ptr), domain);
		return 1;
	}
	if ((family == AF_INET) && ret == 0) {
		ret=inet_addr(domain);
		if (ret == 0) {
			DBG_LOG(MSG, MOD_OM, 
				"address translation resulted in zero as ip, for domain %s",
				domain);
			ip_ptr->family = family;
			IPv4((*ip_ptr)) = ret;
			return 1;
		}
	}

	/* get hash value for this domain */
	hash = DNS_DOMAIN_HASH(domain);

	/* look up domain from hash table */
	pthread_mutex_lock(&dns_mutex[hash]);
	ret_ip_entry = dns_lookup(hash, domain, family, &found, &pdns);
	if (ret_ip_entry == 0 || pdns.resolved == 0) {
		ret = 0;
	} else {
		ret = 1;
		*ip_ptr = pdns.ip;
		glob_dns_cache_hit++;
		DBG_LOG(MSG, MOD_OM, "returns %s for domain %s",
			get_ip_str(ip_ptr), domain);
	}
	pthread_mutex_unlock(&dns_mutex[hash]);

	/* adns changes, we do not want getaddrinfo at all because we
	 * already did a DNS lookup by adns and pushed to hash by now, so just
	 * return from here.
	 */
	if(adns_enabled || ret == 1) {
		return ret;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = family;
	ret = 0;
again:
	/* TODO: need to test blocking path */
	rv = getaddrinfo(domain, NULL, &hints, &res);
	if (rv != 0) {
		if (res != NULL) {
			/* Sanity check to avoid memory leak */
			DBG_LOG(ERROR, MOD_OM, "res should be NULL in error condition!");
		}

		/* if getaddrinfo returns EAI_AGAIN. try one more time */
		if((rv == EAI_AGAIN) && (try_again == 1)) {
			try_again = 0;
			goto again;
		}

		DBG_LOG(WARNING, MOD_OM, "getaddrinfo(%s) failed, return=%d",
			domain, rv);
	} else {
		ip_ptr->family = family;
		if (family == AF_INET) {
			ip_ptr->addr.v4 = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;
		} else {
			ip_ptr->addr.v6 =
				((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr;
		}

		if (is_loopback(ip_ptr) == 0) {
			ret = 1;
			int ttl;
			/* cache it into hash table */
			pthread_mutex_lock(&dns_mutex[hash]);
			ttl = DEFAULT_DNS_BASE_TTL;
			dns_insert(hash, domain, ip_ptr, &ttl, 1, 1);
			pthread_mutex_unlock(&dns_mutex[hash]);

			DBG_LOG(MSG, MOD_OM, "returns %s for domain %s",
				get_ip_str(ip_ptr), domain);
		} else {
			DBG_LOG(MSG, MOD_OM, "loopback address %s for domain %s",
				get_ip_str(ip_ptr), domain);
		}

		freeaddrinfo(res);
	}

	return ret;
}

/* TODO: this function should be removed after migrating all callers to dns_domain_to_ip46 */
uint32_t dns_domain_to_ip(char * domain);
uint32_t dns_domain_to_ip(char * domain)
{
    ip_addr_t ret;
    int valid;

    valid = dns_domain_to_ip46(domain, AF_INET, &ret);
    if (valid == 0) {
        return INADDR_NONE;
    }

    return IPv4(ret);
}

extern void
dns_hash_and_insert(char *domain, ip_addr_t ip_ptr[], int32_t ttl[],
                    uint8_t resolved, int num);
void
dns_hash_and_insert(char *domain, ip_addr_t ip_ptr[], int32_t ttl[],
                    uint8_t resolved, int num)
{
        uint32_t hash;
        /* get hash value for this domain */
        hash = DNS_DOMAIN_HASH(domain);
        pthread_mutex_lock(&dns_mutex[hash]);
        dns_insert(hash, domain, &ip_ptr[0],&ttl[0], resolved, num);
        /* We would have incremented cache_miss unnecessarily
           in dns_insert() when we do a lookup, so decrement */
        glob_dns_cache_miss--;
        pthread_mutex_unlock(&dns_mutex[hash]);
}

/* Return 0 if dns cache entry is not found */
extern int dns_hash_and_lookup(char *domain, int32_t family, uint8_t *resolved_ptr);
int dns_hash_and_lookup(char *domain, int32_t family, uint8_t *resolved_ptr)
{
        uint32_t hash;
        struct dns_ip_entry_t pdns;
        int found = 0, ret, ret_ip_entry;

        /* get hash value for this domain */
        hash = DNS_DOMAIN_HASH(domain);
        pthread_mutex_lock(&dns_mutex[hash]);
        ret_ip_entry = dns_lookup(hash, domain, family, &found, &pdns);
        if (ret_ip_entry != 0) {
            ret = found;
            *resolved_ptr = pdns.resolved;
        } else {
            ret = found;
            *resolved_ptr = 0;
        }
        pthread_mutex_unlock(&dns_mutex[hash]);
        return ret;
}

extern int dns_hash_and_mark(char *domain, ip_addr_t ip, int *found);
int dns_hash_and_mark(char *domain, ip_addr_t ip, int *found) {
        uint32_t hash;
        int ret = 0;

        hash = DNS_DOMAIN_HASH(domain);
        pthread_mutex_lock(&dns_mutex[hash]);
        ret=dns_mark_ip_entry(hash, domain, found, ip);
        pthread_mutex_unlock(&dns_mutex[hash]);
        return ret;
}

/************************************************************************
* CLI API to provide the dns cache list. Its the callers responsibility 
* to malloc and free the params. Returns the number of entries.
************************************************************************/

void dns_get_cache_list(char *dns_list, int32_t max_entries);
void dns_get_cache_list(char *dns_list, int32_t max_entries)
{
	int i,j=0;
	struct dns_cache_t * pdns;
	struct ip_list_head *head;
	struct ip_addr_entry_t *node;

	assert(dns_list);

	for (i=0; i<MAX_DNSHASH; i++)
	{
		pthread_mutex_lock(&dns_mutex[i]);
                if (dnshash[i])
		{
			pdns = dnshash[i];
			while (pdns)
			{
				if(j>max_entries)
				{
					pthread_mutex_unlock(&dns_mutex[i]);
					*dns_list = '\0';
					return;
				}
				/*************************************************
				 * Need the string as \n terminated for each
				 * entry and \0 at the end, when done. Below case
				 * the \0 appended is over written everytime, 
				 * except the last time.
				 ***********************************************/
				head = pdns->ip_list;
				dns_list+=snprintf(dns_list, MAX_DNS_INFO*4,
/*					 "domain:%s ip:%s ttl:%d hitcnt:%d\n",*/
					 "%s;",
					 pdns->domain);

				SLIST_FOREACH(node, head, entries) {
					dns_list+=snprintf(dns_list, MAX_DNS_INFO*4,
						"%s(%d), ",
						pdns->resolved ? get_ip_str(&node->ip) : "<unresolved>", node->ttl);
				}
				dns_list+=snprintf(dns_list, MAX_DNS_INFO*4,
						";%d\n",
						pdns->hitcnt);
				j++;
				pdns = pdns->next;
			}
                }
        	pthread_mutex_unlock(&dns_mutex[i]);
        }
 	return;
}

/************************************************************************
* CLI API to delete all the dns cache list. Returns the number of
* entries deleted
************************************************************************/

int32_t dns_delete_cache_all(void);
int32_t dns_delete_cache_all(void)
{
	int i,cnt=0;
	struct dns_cache_t *pdns, *pdnsnext;
	struct ip_addr_entry_t *node;
	struct ip_list_head *head;

        for (i=0; i<MAX_DNSHASH; i++)
	{
        	pthread_mutex_lock(&dns_mutex[i]);
                if (dnshash[i])
		{
			pdns = dnshash[i];
			while (pdns)
			{
				head = pdns->ip_list;
				while (!SLIST_EMPTY(head)) {         // List Deletion.
					node = SLIST_FIRST(head);
					SLIST_REMOVE_HEAD(head, entries);
					free(node);
				}
				pdnsnext = pdns->next;
				free(pdns->domain);
				free(pdns->ip_list);
				if (pdns->family == AF_INET6) {
					AO_fetch_and_sub1(&glob_dns_tot_cache_entry_ipv6);
				} else {
					AO_fetch_and_sub1(&glob_dns_tot_cache_entry);
				}
				free(pdns);
				cnt++;
				pdns = pdnsnext;
			}
			dnshash[i] = NULL;
                }
        	pthread_mutex_unlock(&dns_mutex[i]);
        }
	return cnt;
}


/************************************************************************
* CLI API to delete a particular fqdn from dns cache list.
* return 0 => found and deleted
* return 1 => not found
************************************************************************/

static int32_t dns_delete_cache_fqdn64(char *domain, int32_t family);
static int32_t dns_delete_cache_fqdn64(char *domain, int32_t family)
{
        uint32_t hash;
	struct dns_cache_t *pdns = NULL, *pdnsprev = NULL;
	struct ip_addr_entry_t *node;
	struct ip_list_head *head;

        /* get hash value for this domain */
        hash = DNS_DOMAIN_HASH(domain);
        pthread_mutex_lock(&dns_mutex[hash]);

        pdns = dnshash[hash];

        while(pdns)
 	{
                if(strcasecmp(pdns->domain, domain)==0 &&
                   pdns->family == family)
		{
			if (pdnsprev==NULL)/*First entry*/
			{
				dnshash[hash] = pdns->next;
				
			}
			else
			{
				pdnsprev->next = pdns->next;
				
			}
			head = pdns->ip_list;
			while (!SLIST_EMPTY(head)) {         // List Deletion.
				node = SLIST_FIRST(head);
				SLIST_REMOVE_HEAD(head, entries);
				free(node);
			}

			free(pdns->domain);
			free(pdns->ip_list);
			if (pdns->family == AF_INET6) {
				AO_fetch_and_sub1(&glob_dns_tot_cache_entry_ipv6);
			} else {
				AO_fetch_and_sub1(&glob_dns_tot_cache_entry);
			}
			free(pdns);
        		pthread_mutex_unlock(&dns_mutex[hash]);
			return 0;
                }
		pdnsprev = pdns;
                pdns = pdns->next;
        }
	
        pthread_mutex_unlock(&dns_mutex[hash]);
	return 1;
}

int32_t dns_delete_cache_fqdn(char *domain);
int32_t dns_delete_cache_fqdn(char *domain)
{
    return dns_delete_cache_fqdn64(domain, AF_INET);
}

int32_t is_dns_mismatch(char *host_name, ip_addr_t client_ip, int32_t family);
int32_t is_dns_mismatch(char *host_name, ip_addr_t client_ip, int32_t family)
{
        uint32_t hash;
        uint32_t len;
        struct dns_cache_t *pdns = NULL;
        struct ip_addr_entry_t *node;
        struct ip_list_head *head;

        /* get hash value for this domain */
        hash = DNS_DOMAIN_HASH(host_name);
        pthread_mutex_lock(&dns_mutex[hash]);
        pdns = dnshash[hash];
        while (pdns)
        {
                if (pdns->resolved && (strcasecmp(pdns->domain, host_name)==0) && (pdns->family==family)) {
                        if (pdns->family == AF_INET) {
                                head = pdns->ip_list;
                                SLIST_FOREACH(node, head, entries) {
                                        if (memcmp(IPv4(client_ip),
                                                IPv4(node->ip), sizeof(in_addr_t)) == 0) {
                                                pthread_mutex_unlock(&dns_mutex[hash]);
                                                return 0;
                                        }
                                }
                        } else {
                                head = pdns->ip_list;
                                SLIST_FOREACH(node, head, entries) {
                                        if (memcmp(IPv6(client_ip),
                                                IPv6(node->ip), sizeof(struct in6_addr)) == 0) {
                                                pthread_mutex_unlock(&dns_mutex[hash]);
                                                return 0;
                                        }
                                }
                        }
                }
                pdns = pdns->next;
        }
        pthread_mutex_unlock(&dns_mutex[hash]);

        return 1;
}

/************************************************************************
* API to give a particular fqdn info from dns cache list.
* domain [in], ip[out], ttl[out]
* return 0 => found and returned
* return 1 => not found
************************************************************************/
int32_t dns_get_cache_fqdn46(char *dn, char *domain, int32_t family);
int32_t dns_get_cache_fqdn46(char *dn, char *domain, int32_t family)
{
	uint32_t hash;
	struct dns_cache_t *pdns = NULL;
	struct ip_addr_entry_t *node;
	struct ip_list_head *head;
	assert(domain);
	/* get hash value for this domain */
	hash = DNS_DOMAIN_HASH(dn);
	pthread_mutex_lock(&dns_mutex[hash]);

	pdns = dnshash[hash];

	while(pdns)
	{
		if(strcasecmp(pdns->domain, dn)==0 &&
               	   pdns->family == family) {

			domain+=snprintf(domain, MAX_DNS_INFO, 
				"%s;",
				pdns->domain);
			head = pdns->ip_list;
			SLIST_FOREACH(node, head, entries) {
				domain+=snprintf(domain, MAX_DNS_INFO,
						"%s(%d), ",
						pdns->resolved ? get_ip_str(&node->ip) : "<unresolved>", 
						node->ttl);
			}
			domain+=snprintf(domain, MAX_DNS_INFO,
					";%d\n",
					pdns->hitcnt);

			pthread_mutex_unlock(&dns_mutex[hash]);
			return 0;
		}
		pdns = pdns->next;
	}
	pthread_mutex_unlock(&dns_mutex[hash]);
	return 1;
}

int32_t dns_get_cache_fqdn(char *dn, char *domain);
int32_t dns_get_cache_fqdn(char *dn, char *domain)
{
    return dns_get_cache_fqdn46(dn, domain, AF_INET);
}

static int dns_delete_domain(uint32_t hash, char * domain) {
	struct dns_cache_t * pdns, *pdnsprev;
	struct ip_addr_entry_t *node, *temp;
	struct ip_list_head *head;

	pdns = dnshash[hash];

	// First node in the hash list.
	if (strcasecmp(pdns->domain, domain)==0) {
		head = pdns->ip_list;
		SLIST_FOREACH_SAFE(node, head, entries, temp) {
			SLIST_REMOVE(head, node, ip_addr_entry_t, entries);
			DBG_LOG(MSG, MOD_OM,
				"dns_delete_domain: Delete IP list (%s) for domain:%s\n",
				pdns->resolved ? get_ip_str(&node->ip) : "<unresolved>", domain);
			free(node);
		}
		dnshash[hash] = pdns->next;
		goto complete;
	}

	pdnsprev = pdns;
	while(pdns->next) {
		head = pdns->next->ip_list;
		if(strcasecmp(pdns->next->domain, domain)==0) {
			SLIST_FOREACH_SAFE(node, head, entries, temp) {
				SLIST_REMOVE(head, node, ip_addr_entry_t, entries);
				free(node);
				DBG_LOG(MSG, MOD_OM,
					"dns_delete_domain: Delete IP list (%s) for domain:%s\n",
					pdns->resolved ? get_ip_str(&node->ip) : "<unresolved>", domain);
			}
			pdnsprev = pdns;
			pdns = pdns->next;
			pdnsprev->next = pdns->next;
			goto complete;
		}
		pdnsprev = pdns;
		pdns = pdns->next;
	}
	return 0;

complete:
	free(head);
	free(pdns->domain);
	if (pdns->family == AF_INET6) {
		AO_fetch_and_sub1(&glob_dns_tot_cache_entry_ipv6);
	} else {
		AO_fetch_and_sub1(&glob_dns_tot_cache_entry);
	}
	free(pdns);
	return 1;
}

void dns_init(void);
void dns_init(void)
{
	int i;
        int rv;
        pthread_attr_t attr;
        int stacksize = 128 * KiBYTES;

        rv = pthread_attr_init(&attr);
        if (rv) {
                DBG_LOG(MSG, MOD_NETWORK, "pthread_attr_init() failed, rv=%d",
                        rv);
                return;
        }

        rv = pthread_attr_setstacksize(&attr, stacksize);
        if (rv) {
                DBG_LOG(MSG, MOD_NETWORK,
                        "pthread_attr_setstacksize() failed, rv=%d", rv);
                return;
        }

        if(pthread_create(&dns_timerid, &attr, dns_timer_func, NULL)) {
                DBG_LOG(MSG, MOD_NETWORK, "Failed to create timer thread.%s", "");
                return;
        }


	for(i=0;i<MAX_DNSHASH;i++) {
		dnshash[i] = NULL;
		pthread_mutex_init(&dns_mutex[i], NULL);
	}

	return;
}

