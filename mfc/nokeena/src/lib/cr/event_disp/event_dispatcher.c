#include "event_dispatcher.h"

static void* start(void*);
static void end(event_disp_t*);
static void delete(event_disp_t*);

static int32_t registerEntity(event_disp_t*, entity_data_t*);
static int32_t unregisterEntity(event_disp_t*, entity_data_t*);

static int32_t setRead(event_disp_t*, entity_data_t*);
static int32_t unsetRead(event_disp_t*, entity_data_t*);
static int32_t setWrite(event_disp_t*, entity_data_t*);
static int32_t unsetWrite(event_disp_t*, entity_data_t*);
static int32_t setError(event_disp_t*, entity_data_t*);
static int32_t unsetError(event_disp_t*, entity_data_t*);
static int32_t setHup(event_disp_t*, entity_data_t*);
static int32_t unsetHup(event_disp_t*, entity_data_t*);
static int32_t scheduleTimeoutWrap(event_disp_t*, entity_data_t*);

static int32_t addEvent(event_disp_t*, entity_data_t*, uint32_t);
static int32_t delEvent(event_disp_t*, entity_data_t*, uint32_t);
static int32_t delAllEvent(event_disp_t*, entity_data_t*);
static int32_t scheduleTimeout(event_disp_t*, entity_data_t*);
static struct timeval* newTimevalKey(struct timeval*);
static gboolean handleValidTimeout(gpointer key, gpointer value,
	gpointer data); 
static int32_t setNonBlocking(int32_t desc);
static int32_t diffTimevalToMs(struct timeval const*, struct timeval const *);
static gint timevalCompare(gconstpointer, gconstpointer, gpointer);
static void deleteKey(gpointer);

typedef struct local {

    int32_t poll_fds;
    uint32_t disp_id;
    uint32_t max_descs;
    uint32_t tmout_feature;
    int32_t tmout_fd;
    entity_data_t** con_ptr;
    pthread_mutex_t state;
    pthread_mutex_t* fd_state;
    pthread_t tid;

    GTree* registered_timeouts;
    struct timeval scheduled_timeout;
    int8_t stop_flag;
} local_t;

event_disp_t* newEventDispatcher(uint32_t id, 
	uint32_t max_descs, uint32_t tmout_feature) { 

    local_t* local = calloc(1, sizeof(local_t));
    if (local == NULL)
	return NULL;
    event_disp_t* disp = calloc(1, sizeof(event_disp_t));
    if (!disp) {
	free(local);
	return NULL;
    }
    local->disp_id = id;
    local->max_descs = max_descs;
    local->tmout_feature = tmout_feature;
    local->poll_fds = epoll_create(max_descs);
    local->stop_flag = 0;
    if (local->poll_fds == -1) {
	free(local);
	free(disp);
	return NULL;
    }
    local->con_ptr = calloc(max_descs, sizeof(entity_data_t*));
    if (!(local->con_ptr)) {
	free(local);
	free(disp);
	return NULL;
    }
    local->fd_state = calloc(max_descs, sizeof(pthread_mutex_t));
    if (local->fd_state == NULL) {

	free(local->con_ptr);
	free(local);
	free(disp);
	return NULL;
    }
    uint32_t i = 0;
    for (;i < max_descs; i++)
	pthread_mutex_init(&local->fd_state[i], NULL);
    pthread_mutex_init(&local->state, NULL);
    local->tid = 0;
    if (tmout_feature == 1) {
	GCompareDataFunc timeval_compare = timevalCompare; 
	GDestroyNotify delete_timeval = deleteKey;
	local->registered_timeouts = 
	    g_tree_new_full(timeval_compare, NULL, delete_timeval, NULL);
    }
    local->tmout_fd = -1;
    disp->__internal = local;

    disp->start_hdlr = start;
    disp->end_hdlr = end;
    disp->delete_hdlr = delete;

    disp->reg_entity_hdlr = registerEntity;
    disp->unreg_entity_hdlr = unregisterEntity;

    disp->set_read_hdlr = setRead;
    disp->unset_read_hdlr = unsetRead;
    disp->set_write_hdlr = setWrite;
    disp->unset_write_hdlr = unsetWrite;
    disp->set_error_hdlr = setError;
    disp->unset_error_hdlr = unsetError;
    disp->set_hup_hdlr = setHup;
    disp->unset_hup_hdlr = unsetHup;
    disp->sched_timeout_hdlr = scheduleTimeoutWrap;

    return disp;
}


static void delete(event_disp_t* disp) {

    local_t* local = (local_t*)disp->__internal;
    if (local->tmout_feature == 1)
	g_tree_destroy(local->registered_timeouts);
    uint32_t i = 0;
    for (;i < local->max_descs; i++) {
	local->con_ptr[i]->delete_hdlr(local->con_ptr[i]);
	pthread_mutex_destroy(&local->fd_state[i]);
    }
    pthread_mutex_destroy(&local->state);
    free(local->fd_state);
    free(local->con_ptr);
    close(local->poll_fds);
    free(local);
    free(disp);	
}


static int32_t registerEntity(event_disp_t* disp,
	entity_data_t* ent_data) {

    int32_t found_event = 0;
    int32_t found_timeout = 0;
    int32_t ret = 0;
    local_t* local = (local_t*)disp->__internal;
    if ((uint32_t)ent_data->fd >= local->max_descs)
	return -1;
    pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] != NULL) {
	ret = -2;
	goto reg_error;
    }
    if ((ent_data->fd >= 0) && (ent_data->event_flags)) {
	if (addEvent(disp, ent_data, ent_data->event_flags) < 0) {
	    ret = -3;
	    goto reg_error;
	}
	found_event = 1;
	setNonBlocking(ent_data->fd);
    }
    if ((ent_data->to_be_scheduled.tv_sec != 0) &&
	    (ent_data->to_be_scheduled.tv_usec != 0) && 
	    (local->tmout_feature == 1)) {
	pthread_mutex_lock(&local->state);
	if (scheduleTimeout(disp, ent_data) < 0) {
	    if (found_event == 1)
		delEvent(disp, ent_data, ent_data->event_flags);
	    ret = -4;
	    pthread_mutex_unlock(&local->state);
	    goto reg_error;
	}
	pthread_mutex_unlock(&local->state);
	found_timeout = 1;
    }
    if ((found_event != 1) && (found_timeout != 1)) {
	ret = -5;
	goto reg_error;
    }
    local->con_ptr[ent_data->fd] = ent_data;
reg_error:
    pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;
}


static void* start(void* args) {

    event_disp_t* disp = (event_disp_t*)args;
    local_t* local = (local_t*)disp->__internal;
    local->tid = pthread_self();
    struct epoll_event* events;
    events = calloc(local->max_descs, sizeof(struct epoll_event));
    if (!events)
	return NULL;
    int32_t rv, i, fd;
    entity_data_t* ent_data;

    prctl(PR_SET_NAME, "nw-event-disp", 0, 0, 0);
    while(1) {
	if (local->stop_flag == 1) {
	    break;
	}
	int32_t poll_fds = local->poll_fds;
	rv =  epoll_wait(poll_fds, events, local->max_descs, 1);

	if (rv == -1) {
	    if (errno == EINTR)
		continue;
	    break;
	}
	if(local->tmout_feature == 1) {
	    GTraverseFunc valid_tmout_hdlr = handleValidTimeout;
	    pthread_mutex_lock(&local->state);
	    g_tree_foreach(local->registered_timeouts, valid_tmout_hdlr, disp);
	    pthread_mutex_unlock(&local->state);
	    if (local->tmout_fd != -1) {
		pthread_mutex_lock(&local->fd_state[local->tmout_fd]);
		if (local->con_ptr[local->tmout_fd] != NULL) {
		    local->con_ptr[local->tmout_fd]->timeout_hdlr(ent_data, 
			    disp, NULL);
		}
		pthread_mutex_unlock(&local->fd_state[local->tmout_fd]);
	    }
	}
	if (rv == 0)
	    continue;

	for (i = 0; i < rv; i++) {
	    fd = events[i].data.fd;
	    pthread_mutex_lock(&local->fd_state[fd]);
	    ent_data = local->con_ptr[fd];
	    if (ent_data == NULL) {

		pthread_mutex_unlock(&local->fd_state[fd]);
		continue;
	    }
	    struct timeval now;
	    gettimeofday(&now, NULL);
	    ent_data->event_time.tv_sec = now.tv_sec;
	    ent_data->event_time.tv_usec = now.tv_usec;

	    if ((events[i].events & EPOLLIN) &&
		    (ent_data->event_flags & EPOLLIN))
		ent_data->read_hdlr(ent_data, disp, NULL);
	    else if ((events[i].events & EPOLLOUT) &&
		    (ent_data->event_flags & EPOLLOUT))
		ent_data->write_hdlr(ent_data, disp, NULL);
	    else if ((events[i].events & EPOLLERR) &&
		    (ent_data->event_flags & EPOLLERR))
		ent_data->error_hdlr(ent_data, disp, NULL);
	    else if ((events[i].events & EPOLLHUP) && 
		    (ent_data->event_flags & EPOLLHUP))
		ent_data->hup_hdlr(ent_data, disp, NULL);
	    pthread_mutex_unlock(&local->fd_state[fd]);
	}
    }
    free(events);
    disp->delete_hdlr(disp);
    return NULL;
}


void end(event_disp_t* disp) {

    local_t* local = (local_t*)disp->__internal;
    local->stop_flag = 1;	
}	


static int32_t unregisterEntity(event_disp_t* disp,
	entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    int32_t fd = ent_data->fd;
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;
    int ret = 0;
    if (ent_data->fd >= 0) { 
	if (local->tmout_feature == 1) {
	    if (pthread_equal(local->tid, c_tid) == 0)
		pthread_mutex_lock(&local->state);
	    g_tree_remove(local->registered_timeouts,
			  &(ent_data->scheduled));
	    if (pthread_equal(local->tid, c_tid) == 0)
		pthread_mutex_unlock(&local->state);
	}
	delAllEvent(disp, ent_data);
	local->con_ptr[ent_data->fd] = NULL;
	ent_data->delete_hdlr(ent_data);
	ret = 1;
    } else
	ret = -2;
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[fd]);
    return ret;
}


static int32_t setRead(event_disp_t* disp, entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();

    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    if (ent_data->read_hdlr) {
	ret = addEvent(disp, ent_data, EPOLLIN);
	if (ret > 0)
	    ent_data->event_flags  |= EPOLLIN;
    }
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t unsetRead(event_disp_t* disp,
	entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    ret = delEvent(disp, ent_data, EPOLLIN);	
    if (ret > 0)
	ent_data->event_flags ^= EPOLLIN;
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t setWrite(event_disp_t* disp, entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    if (ent_data->write_hdlr) {
	ret = addEvent(disp, ent_data, EPOLLOUT);
	if (ret > 0)
	    ent_data->event_flags |= EPOLLOUT;
    }
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t unsetWrite(event_disp_t* disp, 
	entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    ret = delEvent(disp, ent_data, EPOLLOUT);	
    if (ret > 0)
	ent_data->event_flags ^= EPOLLOUT;
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t setError(event_disp_t* disp, entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    if (ent_data->error_hdlr) {
	ret = addEvent(disp, ent_data, EPOLLERR);
	if (ret > 0)
	    ent_data->event_flags |= EPOLLERR;
    }
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t unsetError(event_disp_t* disp,
	entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    ret = delEvent(disp, ent_data, EPOLLERR);	
    if (ret > 0)
	ent_data->event_flags ^= EPOLLERR;
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t setHup(event_disp_t* disp, entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    int32_t ret = -1;
    if (ent_data->hup_hdlr) {
	ret = addEvent(disp, ent_data, EPOLLHUP);
	if (ret > 0)
	    ent_data->event_flags |= EPOLLHUP;
    }
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t scheduleTimeoutWrap(event_disp_t* disp, 
	entity_data_t* ent_data) {

    local_t* local = (local_t*)disp->__internal;
    if (local->tmout_feature != 1)
	return -1;
    int32_t ret = -2;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->state);
    ret = scheduleTimeout(disp, ent_data);
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->state);

check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;	
}


static int32_t unsetHup(event_disp_t* disp, entity_data_t* ent_data) {

    int32_t ret = -1;
    local_t* local = (local_t*)disp->__internal;
    pthread_t c_tid = pthread_self();
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_lock(&local->fd_state[ent_data->fd]);
    if (local->con_ptr[ent_data->fd] == NULL)
	goto check_return;

    ret = delEvent(disp, ent_data, EPOLLHUP);
    if (ret > 0)
	ent_data->event_flags ^= EPOLLHUP;
check_return:
    if (pthread_equal(local->tid, c_tid) == 0)
	pthread_mutex_unlock(&local->fd_state[ent_data->fd]);
    return ret;
}


static int32_t addEvent(event_disp_t* disp, 
	entity_data_t* ent_data, uint32_t value) {

    local_t* local = (local_t*)disp->__internal;
    struct epoll_event ev;
    memset((char *)&ev, 0, sizeof(struct epoll_event));

    ev.events = value;
    ev.data.fd = ent_data->fd;
    if (epoll_ctl(local->poll_fds, EPOLL_CTL_ADD, 
		ent_data->fd, &ev) < 0) {
	if (errno == EEXIST) {
	    ev.events |= ent_data->event_flags;
	    if (epoll_ctl(local->poll_fds, EPOLL_CTL_MOD,
			ent_data->fd, &ev) < 0) {
		perror("epoll_ctl 2");
		return -1;
	    } else
		return 1;
	}
	perror("epoll_ctl 1");
	return -2;
    }
    return 1;
}


static int32_t delAllEvent(event_disp_t* disp, entity_data_t* ent_data) { 

    local_t* local = (local_t*)disp->__internal;
    if (epoll_ctl(local->poll_fds, EPOLL_CTL_DEL,
		ent_data->fd, NULL) < 0) {
	perror("ctl del:");
	if(errno == EEXIST) 
	    return 0;
	return -1;
    }
    ent_data->event_flags = 0;
    return 1;
}


static int32_t delEvent(event_disp_t* disp, 
	entity_data_t* ent_data, uint32_t value) {

    struct epoll_event ev;
    memset((char *)&ev, 0, sizeof(struct epoll_event));
    local_t* local = (local_t*)disp->__internal;
    ev.events = ent_data->event_flags ^ value;
    ev.data.fd = ent_data->fd;
    if (epoll_ctl(local->poll_fds, EPOLL_CTL_MOD, 
		ent_data->fd, &ev) < 0) {
	perror("mod ctl: ");
	return -1;
    }
    return 1;
}


static int32_t scheduleTimeout(event_disp_t* disp, 
	entity_data_t* ent_data) {

    ent_data->scheduled.tv_sec = ent_data->to_be_scheduled.tv_sec;
    ent_data->scheduled.tv_usec = ent_data->to_be_scheduled.tv_usec;
    struct timeval* key = newTimevalKey(&(ent_data->scheduled));
    if (key) {
	local_t* local = (local_t*)disp->__internal;
	while (1) {
	    entity_data_t* tmp_ent_ctx = g_tree_lookup(
		    local->registered_timeouts, key);
	    if (!tmp_ent_ctx)
		break;
	    key->tv_usec += 1;
	}
	g_tree_insert(local->registered_timeouts, 
		key, ent_data);
	ent_data->to_be_scheduled.tv_sec = 0;
	ent_data->to_be_scheduled.tv_usec = 0;
	return 1;
    }
    return -1;
}


static int32_t setNonBlocking(int32_t fd) {

    int32_t flags;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
	return -1;
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
	return -1;
    return 1;
}


static gboolean handleValidTimeout(gpointer key, gpointer value,
	gpointer data) {

    struct timeval* val = (struct timeval*)key;
    event_disp_t* disp = (event_disp_t*)data;
    local_t* local = (local_t*)disp->__internal;
    struct timeval now;
    gettimeofday(&now, NULL);
    if (diffTimevalToMs(val, &now)== 0) {
	g_tree_remove(local->registered_timeouts, key);
	entity_data_t* ent_data = (entity_data_t*)value;
	if (ent_data->timeout_hdlr) {
	    ent_data->timeout_hdlr(ent_data, disp, NULL);
	}
    }
    return TRUE;
}

static int32_t diffTimevalToMs(struct timeval const* from, 
	struct timeval const * val) {
    //if -ve, return 0
    double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
    double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
    double diff = d_from - d_val;
    if (diff < 0)
	return 0;
    return (int32_t)(diff * 1000);
}


static gint timevalCompare(gconstpointer gtv1, gconstpointer gtv2, 
	gpointer user_data) {

    struct timeval* val1 = (struct timeval*)gtv1;
    struct timeval* val2 = (struct timeval*)gtv2;

    if (val1->tv_sec < val2->tv_sec)
	return -1;
    else if (val1->tv_sec > val2->tv_sec)
	return 1;
    else {
	if (val1->tv_usec < val2->tv_usec)
	    return -1;
	else if (val1->tv_usec > val2->tv_usec)
	    return 1;
	else
	    return 0;
    }
}


static struct timeval* newTimevalKey(struct timeval* val) {

    struct timeval* key = malloc(sizeof(struct timeval));
    if (key) {
	key->tv_sec = val->tv_sec;
	key->tv_usec = val->tv_usec;
    }
    return key;
}

static void deleteKey(gpointer ptr) {

    free(ptr);
}

