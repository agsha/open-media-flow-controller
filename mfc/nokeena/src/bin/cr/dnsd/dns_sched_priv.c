/*****************************************************
 * implements stub functions required by NKN SCHEDULER
 *****************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "nkn_defs.h"
#include "nkn_sched_api.h"
#include "nkn_hash_map.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "nkn_slotapi.h"
#include "nkn_authmgr.h"
#include "nkn_encmgr.h"
#include "../../../lib/nvsd/rtsched/nkn_rtsched_task.h"

void media_mgr_cleanup(nkn_task_id_t id); 
void media_mgr_cleanup(nkn_task_id_t id) { }
void media_mgr_input(nkn_task_id_t id);
void media_mgr_input(nkn_task_id_t id) { }
void media_mgr_output(nkn_task_id_t id);
void media_mgr_output(nkn_task_id_t id) { }

void move_mgr_input(nkn_task_id_t id); 
void move_mgr_input(nkn_task_id_t id) { }
void move_mgr_output(nkn_task_id_t id);
void move_mgr_output(nkn_task_id_t id) { }
void move_mgr_cleanup(nkn_task_id_t id);
void move_mgr_cleanup(nkn_task_id_t id) { }

void chunk_mgr_input(nkn_task_id_t id) { }
void chunk_mgr_output(nkn_task_id_t id) { }
void chunk_mgr_cleanup(nkn_task_id_t id) { }

void http_mgr_input(nkn_task_id_t id) { }
void http_mgr_output(nkn_task_id_t id) { }
void http_mgr_cleanup(nkn_task_id_t id) { }

void om_mgr_input(nkn_task_id_t id) { }
void om_mgr_output(nkn_task_id_t id) { }
void om_mgr_cleanup(nkn_task_id_t id) { }

void nfs_tunnel_mgr_input(nkn_task_id_t id);
void nfs_tunnel_mgr_input(nkn_task_id_t id) { }
void nfs_tunnel_mgr_output(nkn_task_id_t id);
void nfs_tunnel_mgr_output(nkn_task_id_t id) { }
void nfs_tunnel_mgr_cleanup(nkn_task_id_t id);
void nfs_tunnel_mgr_cleanup(nkn_task_id_t id) { }

//void enc_mgr_input(nkn_task_id_t id); 
void enc_mgr_input(nkn_task_id_t id) { }
//void enc_mgr_output(nkn_task_id_t id);
void enc_mgr_output(nkn_task_id_t id) { }
//void enc_mgr_cleanup(nkn_task_id_t id);
void enc_mgr_cleanup(nkn_task_id_t id) { }

//void auth_mgr_input(nkn_task_id_t id);
void auth_mgr_input(nkn_task_id_t id) {}
//void auth_mgr_output(nkn_task_id_t id);
void auth_mgr_output(nkn_task_id_t id) {}
//void auth_mgr_cleanup(nkn_task_id_t id);
void auth_mgr_cleanup(nkn_task_id_t id) {}

void auth_helper_input(nkn_task_id_t id);
void auth_helper_input(nkn_task_id_t id) {}
void auth_helper_output(nkn_task_id_t id);
void auth_helper_output(nkn_task_id_t id) {}
void auth_helper_cleanup(nkn_task_id_t id);
void auth_helper_cleanup(nkn_task_id_t id) {}

void pe_geo_mgr_input(nkn_task_id_t id);
void pe_geo_mgr_input(nkn_task_id_t id) {}
void pe_geo_mgr_output(nkn_task_id_t id);
void pe_geo_mgr_output(nkn_task_id_t id) {}
void pe_geo_mgr_cleanup(nkn_task_id_t id);
void pe_geo_mgr_cleanup(nkn_task_id_t id) {}

void pe_helper_input(nkn_task_id_t id);
void pe_helper_input(nkn_task_id_t id) {}
void pe_helper_output(nkn_task_id_t id);
void pe_helper_output(nkn_task_id_t id) {}
void pe_helper_cleanup(nkn_task_id_t id);
void pe_helper_cleanup(nkn_task_id_t id) {}

void pe_ucflt_mgr_input(nkn_task_id_t id);
void pe_ucflt_mgr_input(nkn_task_id_t id) {}
void pe_ucflt_mgr_output(nkn_task_id_t id);
void pe_ucflt_mgr_output(nkn_task_id_t id) {}
void pe_ucflt_mgr_cleanup(nkn_task_id_t id);
void pe_ucflt_mgr_cleanup(nkn_task_id_t id) {}

void mm_async_thread_hdl(void);
void mm_async_thread_hdl() { }

void cache_update_debug_data(struct nkn_task *ntask) {}
void dns_hash_and_insert(char *domain, ip_addr_t *ip, int32_t *ttl, int res, int num);
void dns_hash_and_insert(char *domain, ip_addr_t *ip, int32_t *ttl, int res, int num) {}

