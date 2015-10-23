
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <md_client.h>
#include <license.h>
#include <stdint.h>

#include "log_accesslog_pri.h"
#include "log_channel.h"



/*----------------------------------------------------------------------------
 * CHANNEL MAGIC FUNCTIONS
 *--------------------------------------------------------------------------*/

//static uint64_t chann_hash_code(void *foo);
//static int chann_hash_compare(void *k1, void *k2);
//cp_hashtable *channel_hash = NULL;
//nknlog_hash_t	*channel_hash = NULL;


struct channel g_channel[MAX_CHANNELS];
static pthread_mutex_t channel_hash = PTHREAD_MUTEX_INITIALIZER;

void isempty_name_channel(const char *name, struct channel **channel,
                          const char *func_name)
{
    if(name == NULL){
        complain_error_msg(1, "name is empty from %s!", func_name);
        exit(EXIT_FAILURE);
    }
    if(channel == NULL){
        complain_error_msg(1, "channel is empty from %s!", func_name);
        exit(EXIT_FAILURE);
    }
}

int log_channel_init(void)
{
    int i = 0;
    bzero(&g_channel, sizeof(struct channel) * MAX_CHANNELS);

    for (i = 0; i < MAX_CHANNELS; i++) {
	g_channel[i].index = i;
    }
    return 0;
}

int log_channel_add(const char *name, struct channel *channel)
{
    isempty_name_channel(name, &channel, "log_channel_add");
    return 0;
}

int log_channel_delete(const char *name, struct channel **channel)
{
    int i = 0;
    int err = -1;

    isempty_name_channel(name, channel, "log_channel_delete");
    i = (*channel)->index;

    /* Do nothing */
    return err;
}

int log_channel_get(const char *name, struct channel **channel)
{
    int err = -1;
    uint64_t keyhash = 0;
    int i = 0;

    isempty_name_channel(name, channel, "log_channel_get");
    pthread_mutex_lock(&channel_hash);
    for (i = 0; i < MAX_CHANNELS; i++) {
	if (g_channel[i].name && (strcmp(name, g_channel[i].name) == 0)) {
	    *channel = &g_channel[i];
	    pthread_mutex_unlock(&channel_hash);
	    return 0;
	}
    }

    *channel = NULL;
    pthread_mutex_unlock(&channel_hash);

    return err;
}


int log_channel_for_each(void (*iterator)(void *, void *, void *), void *user)
{
    int err = 0;
    int i = 0;

    pthread_mutex_lock(&channel_hash);
    for (i = 0; i < MAX_CHANNELS; i++) {
	if (g_channel[i].state == (cs_INUSE|cs_INEPOLL|cs_HELO)) {
	    iterator(NULL, &(g_channel[i]), user);
	}
    }
    pthread_mutex_unlock(&channel_hash);

    return 0;
}


struct channel *log_channel_new(const char *name)
{
    int i = 0;

    pthread_mutex_lock(&channel_hash);
    /* Get a free channel slot */
    for (i = 0; i < MAX_CHANNELS; i++) {
	if (g_channel[i].state == 0) {
	    g_channel[i].state |= cs_INUSE;

	    //printf("----------------> Creating channel with name: %s\n", name);

	    g_channel[i].name = strdup(name);

	    pthread_mutex_unlock(&channel_hash);
	    return &g_channel[i];
	}
    }

    pthread_mutex_unlock(&channel_hash);
    return NULL;
}

void log_channel_free(struct channel *c)
{
    if (c->name) {
	free(c->name);
	c->name = NULL;
    }
    c->state = 0;
}

