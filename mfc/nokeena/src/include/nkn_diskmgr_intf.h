#ifndef NKN_DISKMGR_INTF_H
#define NKN_DISKMGR_INTF_H
#include <assert.h>
#include <string.h>
#include "glib.h"
#include "cache_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"

void disk_mgr_cleanup(nkn_task_id_t id);
void disk_mgr_input(nkn_task_id_t id);
void disk_mgr_output(nkn_task_id_t id);
void disk_io_req_hdl(gpointer data, gpointer user_data);

int DM_get(MM_get_resp_t *in_ptr_resp);
#endif	/* NKN_DISKMGR_INTF_H */
