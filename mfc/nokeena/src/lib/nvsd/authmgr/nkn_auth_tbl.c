#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "nkn_am_api.h"
#include "ssp.h"
#include "nkn_namespace.h"
#include "nkn_vpemgr_defs.h"
#include "nkn_cl.h"
#include "nkn_mediamgr_api.h"
#include "nkn_rtsched_api.h"
#include "network_hres_timer.h"
//#include "nkn_encmgr.h"
#include "nkn_http.h"

#include "nkn_auth_tbl.h"
#include "nkn_authmgr.h"

/* Hashtable and local table structures and mutex */
static GHashTable *auth_hash_table = NULL;
TAILQ_HEAD(auth_local_list, auth_loc_tbl_t) auth_local_list_q;
nkn_mutex_t auth_htbl_list_mutex;

void auth_htbl_list_init(void)
{
    	TAILQ_INIT(&auth_local_list_q);
    	auth_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                    (GDestroyNotify)auth_free_loc_tbl_obj);
	NKN_MUTEX_INIT(&auth_htbl_list_mutex, NULL,
                            "auth_htbl_list_mutex");


}

void auth_htbl_destroy(void)
{
	g_hash_table_destroy(auth_hash_table);
}

auth_loc_tbl_t* auth_create_loc_tbl_obj(auth_msg_t *data)
{
        auth_loc_tbl_t* objp = (auth_loc_tbl_t*) nkn_calloc_type (1,
                                        sizeof(auth_loc_tbl_t),
                                         mod_auth_loc_tbl_t);
        if(!objp) 
                return NULL;

	memcpy(&(objp->auth_msg_loc),data, sizeof(auth_msg_t));
        
	/* TODO Additional authtypes to be added to hashtable */
	 
        return objp;
}

int auth_fetch_local(auth_msg_t* data)
{
	UNUSED_ARGUMENT(data);
 	/* TODO based on the authtype proper lookup needs to be implemented
	This is currently a place holder to add further cases 	

	auth_loc_tbl_t* objp =g_hash_table_lookup(auth_hash_table,data->uniqueid);
	if(!objp)
		return -1;
	//preserve the con pointer, very important!!
	con_t* _temp=data->con;
	memcpy(data,&(objp->authinfo),sizeof(auth_msg_t));
	data->con=_temp;
	*/
	return -1;

}


void auth_free_loc_tbl_obj(auth_msg_t* obj)
{
        if(obj)
        {
                free(obj);
                obj=NULL;
        }

}

int auth_update_local(auth_msg_t *data)
{
	UNUSED_ARGUMENT(data);
	/* TODO based on the authtype proper update needs to be implemented
	This is currently a place holder to add further cases

        auth_loc_tbl_t* objp = auth_create_loc_tbl_obj(data);

        if(!objp)
        {
                DBG_LOG(ERROR, MOD_AUTHMGR,
                                 "COuld not create entry into local tbl");
                return -1;
        }
        NKN_MUTEX_LOCK(&auth_htbl_list_mutex);
        TAILQ_INSERT_TAIL(&auth_local_list_q, objp, p_tbl_entry_p);
        g_hash_table_insert(auth_hash_table, objp->authinfo.uniqueid, objp);
        NKN_MUTEX_UNLOCK(&auth_htbl_list_mutex);
	*/
        return 0;
}

int auth_remove_local(unsigned char *remkey)
{
	
	UNUSED_ARGUMENT(remkey);
	/* TODO based on the authtype proper update needs to be implemented
	This is currently a place holder to add further cases

        NKN_MUTEX_LOCK(&auth_htbl_list_mutex);
        auth_loc_tbl_t* objp = g_hash_table_lookup(auth_hash_table, remkey);
        if(objp == NULL)
        {
                DBG_LOG(WARNING, MOD_AUTHMGR, "AuthMgr cannot find the specified obj: "
                        "key = %s", objp->authinfo.uniqueid);
                NKN_MUTEX_UNLOCK(&auth_htbl_list_mutex);
                return -1;
        }

        TAILQ_REMOVE(&auth_local_list_q, objp, p_tbl_entry_p);
        g_hash_table_remove(auth_hash_table, objp->authinfo.uniqueid);
        NKN_MUTEX_UNLOCK(&auth_htbl_list_mutex);
	*/
        return 0;
}

int auth_htbl_sz(void)
{
	return g_hash_table_size(auth_hash_table);
}
