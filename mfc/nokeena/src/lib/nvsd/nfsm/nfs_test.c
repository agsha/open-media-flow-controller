#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <glib.h>
#include "nkn_nfs_api.h"
#include "nkn_nfs.h"
#include "nkn_mediamgr_api.h"
#include "nkn_defs.h"
#include "nkn_trace.h"
#include "http_header.h"
#include "fqueue.h"
#include "nkn_namespace.h"

int s_file_no = 1;
GThreadPool *nfs_test_threads;
char dummy1[24];
char dummy2[24];

static int
s_get_new_fileno(void)
{
	return (++s_file_no);
}

void
test_nfs_multi_thread_init(void)
{

#if 0
	nfs_test_threads = g_thread_pool_new(&s_test_nfs_put,
					     NULL,
					     2,
					     TRUE,
					     NULL);
#endif
	s_test_nfs_put(NULL, NULL);
}

void
test_nfs_multi_thread_start(void)
{
	int i;

	for(i = 0; i < 11; i++) {
		g_thread_pool_push(nfs_test_threads, dummy1, NULL);
	}
}

void
test_nfs_mount(void)
{
    namespace_config_t cfg;
    http_config_t      ht;
    //char prefix[] = "www.break.com\0";
    char prefix[] = "/\0";
    char mntpath[] = "172.19.172.52/home/vikram/tmp\0";
    origin_server_NFS_t server;
    origin_server_NFS_t *sp = &server;
    namespace_token_t dummy;

    cfg.uri_prefix = strdup(prefix);
    cfg.http_config = &ht;
    cfg.http_config->origin.u.nfs.entries = 1;
    cfg.http_config->origin.u.nfs.server[0] = sp;
    cfg.http_config->origin.u.nfs.server[0]->hostname_path = mntpath;

    NFS_configure(dummy, &cfg);
}

static void
test_bug378(MM_put_data_t *map, char *uri)
{
        map->uol.uri = (char *) nkn_malloc_type (160 * sizeof(char), 
		mod_nfs_test);
	memcpy(map->uol.uri, uri, 35);
        map->attr = (nkn_attr_t *) nkn_malloc_type (sizeof(nkn_attr_t), 
		mod_nfs_test);
        map->total_length = 100;
	map->iov_data = (nkn_iovec_t *) nkn_malloc_type (sizeof(nkn_iovec_t), 
		mod_nfs_test);
        map->iov_data->iov_base = (char *) nkn_malloc_type (32768, 
		mod_nfs_test);
        map->iov_data_len = 1;
        map->uol.length = 9; /* no need for this*/

#if 0
        map->uol.offset = 0;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 12;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 25;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 11;
        map->iov_data->iov_len = 2;
        TFM_put(map, 0);

        map->uol.offset = 36;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 47;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 58;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 69;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);

        map->uol.offset = 79;
        map->iov_data->iov_len = 10;
        TFM_put(map, 0);
#endif

        map->uol.offset = 0;
        map->iov_data->iov_len = 100;
        //TFM_put(map, 0);
}

#if 1
void
test_nfs_config_file(void)
{
    namespace_token_t dummy;
    NFS_configure(dummy, NULL);
}
#endif

void
s_test_nfs_put(gpointer data,
               gpointer user_data)
{
	int i = 0;
        MM_put_data_t *map;
	int total = 8 * 1024 *1024;
	int br1 = 2 * 1024 * 1024;
	int br2 = 2 * 1024 * 1024;
	int br3 = 2 * 1024 * 1024;
	int chunk = 32768;
	int num_chunks = br1/chunk;
	char uri[MAX_URI_SIZE];

	snprintf(uri, MAX_URI_SIZE, "%s_%d", "vikramvideo", s_get_new_fileno());
	printf("\n WORKER: %d", s_file_no);
        map = (MM_put_data_t *) nkn_malloc_type (sizeof(MM_put_data_t), 
		mod_nfs_test);
	test_bug378(map, uri);

#if 0
        map->uol.uri = (char *) nkn_malloc_type (160 * sizeof(char),
		mod_nfs_test);
	memcpy(map->uol.uri, uri, 35);
        map->attr = (nkn_attr_t *) nkn_malloc_type (sizeof(nkn_attr_t),
		mod_nfs_test);
        map->uol.offset = 0;
        map->uol.length = 32767;
        map->total_length = 2 * 1024 * 1024;
        map->iov_data_len = 1;
	map->iov_data = (nkn_iovec_t *) nkn_malloc_type (sizeof(nkn_iovec_t),
		mod_nfs_test);
        map->iov_data->iov_base = (char *) nkn_malloc_type (32768,
		mod_nfs_test);



	num_chunks = num_chunks / 4;
	i = 0;
	map->uol.offset = (0 * num_chunks * chunk);
	while(i < num_chunks) {
        	map->iov_data->iov_len = chunk;
        	TFM_put(map, 0);
		map->uol.offset += chunk;
		i++;
	}
	i = 0;
	map->uol.offset = (3 * num_chunks * chunk);
	while(i < num_chunks) {
        	map->iov_data->iov_len = chunk;
        	TFM_put(map, 0);
		map->uol.offset += chunk;
		i++;
	}
	i = 0;
	map->uol.offset = (2 * num_chunks * chunk);
	while(i < num_chunks) {
        	map->iov_data->iov_len = chunk;
        	TFM_put(map, 0);
		map->uol.offset += chunk;
		i++;
	}
	i = 0;
	map->uol.offset = (1 * num_chunks * chunk);
	while(i < num_chunks) {
        	map->iov_data->iov_len = chunk;
        	TFM_put(map, 0);
		map->uol.offset += chunk;
		i++;
	}
#endif
	printf("\n ------------------ TFM TEST DONE --------------------");
}

