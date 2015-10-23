#include <stdint.h>
#include <sys/shm.h>
#include "pxy_server.h"
#include "pxy_policy.h"

int  pxy_tunnel_all = 0;
int  pxy_cache_fail_reject = 0;
nvsd_health_td *glob_nvsd_health = NULL;

int
pxy_attach_nvsd_health_info(void)
{
	int ret = FALSE ;
        int shmid = -1 ;


        shmid = shmget(PXY_HEALTH_CHECK_KEY,
                       sizeof(nvsd_health_td),
                       0) ; /* No flags, only read if present */ 
        if (shmid == -1) { /*Error case*/
              DBG_LOG(MSG, MOD_PROXYD, "Failed to get shmem id. Error: %s\n",
                                       strerror(errno));
              return ret;
        }
                       
	/* Attach to the shared memory which holds the health info for reading */
        glob_nvsd_health = shmat(shmid, NULL, SHM_RDONLY);
        if (glob_nvsd_health == (void*)-1) { /*Error case*/
	      DBG_LOG(MSG, MOD_PROXYD, "Failed to get health info from nvsd. Error: %s\n",
                                       strerror(errno));
              return ret;
        }

        return TRUE;
}

static int
nvsd_in_bad_health(void)
{
	int ret = TRUE ;

        if (glob_nvsd_health == NULL) {
	      DBG_LOG(MSG, MOD_PROXYD, "health info not initialized in nvsd:") ; 
              return TRUE;
        }
        if(glob_nvsd_health->good) {
              ret = FALSE;
        }
	DBG_LOG(MSG, MOD_PROXYD, "health info from nvsd: %s\n", 
                                ((glob_nvsd_health->good==1)?"Good":"Bad")) ;

        return ret;
}

l4proxy_action 
get_l4proxy_action(uint32_t ip)
{
        l4proxy_action ret = L4PROXY_ACTION_WHITE;

       /*
        * When 'domain-filter tunnel-list all' is not configured,
        * send all the client connections from proxyd to nvsd.
        */
        if (pxy_tunnel_all) {
             ret = L4PROXY_ACTION_TUNNEL ;
        } else { /* L4PROXY_ACTION_WHITE */
             /* Do Health check for nvsd here, return TRUE on bad health */
             if (nvsd_in_bad_health()){
                  /* 'domain-filter cache-fail reject' enabled. */
                  if (pxy_cache_fail_reject) {
                      ret = L4PROXY_ACTION_REJECT;
                  } else {
                      ret = L4PROXY_ACTION_TUNNEL;
                  }
             }
        }

	return ret;
}

#if 0 /* For now DBL/DWL handling is not present and hence commenting out this code.*/

static int  
check_ip_list(uint32_t ip)
{
	PIL * plist;

	plist = p_global_ip_list;
	while(plist) {
		if (plist->ip == ip) return TRUE;
		plist = plist->next;
	}

	return FALSE;
}

static int 
add_into_ip_list(uint32_t ip, l4proxy_action action)
{
	PIL * plist;

	plist = (PIL *)malloc(sizeof(PIL));
	if (!plist) return FALSE;

	plist->next = p_global_ip_list;
	plist->ip = ip;
	plist->action = action;

	p_global_ip_list = plist;
	return TRUE;
}

static void 
delete_from_ip_list(uint32_t ip)
{
	PIL * plist, * plist_next;

	if (!p_global_ip_list) return;

	if (p_global_ip_list->ip == ip) {
		plist = p_global_ip_list->next;
		free(p_global_ip_list);
		p_global_ip_list = plist;
		return;
	}

	plist = p_global_ip_list;
	while (plist->next) {
		plist_next = plist->next;
		if (plist_next->ip == ip) {
			plist->next = plist_next->next;
			free(plist_next);
			return ;
		}
		plist = plist_next;
	}
	return;
}


/* ************************************
 * block black ip list
 * ************************************ */
void 
free_whole_ip_list(char * p)
{
	PIL * plist;

	while(p_global_ip_list) {
		plist = p_global_ip_list->next;
		free(p_global_ip_list);
		p_global_ip_list = plist;
	}
}

static void 
add_ip_to_ip_list(char * p, l4proxy_action act)
{
	uint32_t ip;
	PIL * plist;

	ip = inet_addr(p);
	if(ip == INADDR_NONE) return;

	if (check_ip_list(ip) == TRUE) {
		delete_from_ip_list(ip);
	}

	add_into_ip_list(ip, act);
}

void 
add_block_ip_list(char * p)
{
	DBG_LOG(MSG, MOD_PROXYD, "add %s for block IP list", p);
	add_ip_to_ip_list(p, L4PROXY_ACTION_BLACK);
}

void  
add_cacheable_ip_list(char * p)
{
	DBG_LOG(MSG, MOD_PROXYD, "add %s for cacheable IP list", p);
	add_ip_to_ip_list(p, L4PROXY_ACTION_WHITE);
}

void 
del_ip_list(char * p)
{
	uint32_t ip;

	ip = inet_addr(p);
	if(ip == INADDR_NONE) return;

	DBG_LOG(MSG, MOD_PROXYD, "del %s for IP list", p);
	delete_from_ip_list(ip);
}

#endif


