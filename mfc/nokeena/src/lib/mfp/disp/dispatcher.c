#include "disp/dispatcher.h"

// functions that have direct access from the disp struct {
static void* startDispatcher(void*);
static void endDispatcher(dispatcher_t*);
static void deleteDispatcher(dispatcher_t*);

static int8_t registerEntity(dispatcher_t*, entity_context_t*);
static int8_t unregisterEntity(dispatcher_t*, entity_context_t*);

static int8_t setReadEvent(dispatcher_t*, entity_context_t*);
static int8_t unsetReadEvent(dispatcher_t*, entity_context_t*);
static int8_t setWriteEvent(dispatcher_t*, entity_context_t*);
static int8_t unsetWriteEvent(dispatcher_t*, entity_context_t*);
static int8_t setErrorEvent(dispatcher_t*, entity_context_t*);
static int8_t unsetErrorEvent(dispatcher_t*, entity_context_t*);
static int8_t setHupEvent(dispatcher_t*, entity_context_t*);
static int8_t unsetHupEvent(dispatcher_t*, entity_context_t*);

static int8_t scheduleTimeoutEvent(dispatcher_t*, entity_context_t*);
static int8_t delAllEvent(dispatcher_t*, entity_context_t*);
//}


// functions that are purely local {
static int8_t addEvent(dispatcher_t*, entity_context_t*, uint32_t);
static int8_t delEvent(dispatcher_t*, entity_context_t*, uint32_t);
static struct timeval* newTimevalKey(struct timeval*);
static int32_t getTimeoutFd(dispatcher_t*);
static void handleTimeoutFd(dispatcher_t*);
static int8_t scheduleTimeoutEventLockHeld(dispatcher_t*,entity_context_t*);

static int8_t setNonBlocking(int32_t fd);
static int32_t diffTimevalToMs(struct timeval const*, struct timeval const *);
//}


// Callbacks registered with glib structs
static gint timevalCompare(gconstpointer, gconstpointer, gpointer);
static void deleteKey(gpointer);
static gboolean updateEpollTimeout(gpointer, gpointer, gpointer);


dispatcher_t* newDispatcher(uint32_t id, fd_lock_t* fd_lk) {

	if (!fd_lk)
		return NULL;

	dispatcher_t* disp = disp_calloc(1, sizeof(dispatcher_t));
	if (!disp)
		return NULL;
	disp->fd_lk = fd_lk;
	disp->disp_id = id;
	disp->poll_fds = epoll_create(fd_lk->max_fds);
	if (disp->poll_fds == -1) {
		free(disp);
		return NULL;
	}
	disp->fd_flag = disp_calloc(fd_lk->max_fds ,sizeof(int32_t));
	if (!(disp->fd_flag)) {
		free(disp);
		return NULL;
	}
	disp->con_ptr = disp_calloc(fd_lk->max_fds, sizeof(entity_context_t*));
	if (!(disp->con_ptr)) {
		free(disp->fd_flag);
		free(disp);
		return NULL;
	}

	int i = 0;
	for (i = 0; i < fd_lk->max_fds; i++) {
		disp->fd_flag[i] = -1;
		disp->con_ptr[i] = NULL;
	}

	disp->epoll_timeout = -1;

	GCompareDataFunc timeval_compare = timevalCompare; 
	GDestroyNotify delete_timeval = deleteKey;
	disp->registered_timeouts = 
		g_tree_new_full(timeval_compare, NULL, delete_timeval, NULL);
	pthread_mutex_init(&(disp->disp_state), NULL);
	disp->stop_flag = 0;

	disp->start_dispatcher = startDispatcher;
	disp->end_dispatcher = endDispatcher;
	disp->delete_dispatcher = deleteDispatcher;

	disp->register_entity = registerEntity;
	disp->unregister_entity = unregisterEntity;

	disp->set_read_event = setReadEvent;
	disp->unset_read_event = unsetReadEvent;
	disp->set_write_event = setWriteEvent;
	disp->unset_write_event = unsetWriteEvent;
	disp->set_error_event = setErrorEvent;
	disp->unset_error_event = unsetErrorEvent;
	disp->set_hup_event = setHupEvent;
	disp->unset_hup_event = unsetHupEvent;
	disp->del_all_event = delAllEvent;

	disp->sched_timeout_event = scheduleTimeoutEvent;

	DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id, "Created new dispatcher");
	return disp;
}


void deleteDispatcher(dispatcher_t* disp) {

	g_tree_destroy(disp->registered_timeouts);
	int i = 0;
	for (;i < disp->fd_lk->max_fds; i++) {
		if (disp->fd_flag[i] != -1)
			disp->con_ptr[i]->delete_entity(disp->con_ptr[i]);
	}
	free(disp->con_ptr);
	free(disp->fd_flag);
	pthread_mutex_destroy(&(disp->disp_state));
	close(disp->poll_fds);
	free(disp);	
	DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id, "Delete dispatcher");
}


static int8_t registerEntity(dispatcher_t* disp,
		entity_context_t* ent_context) {

	pthread_mutex_lock(&(disp->disp_state));
	int32_t ret = 0;
	if (ent_context->fd >= disp->fd_lk->max_fds) {
		ret = -1;
		goto disp_unlock;
	}
	if (disp->fd_flag[ent_context->fd] == -1) {
		int8_t found_event = 0;
		int8_t found_timeout = 0;
		disp->fd_flag[ent_context->fd] = 1;
		disp->con_ptr[ent_context->fd] = ent_context;
		if ((ent_context->fd >= 0) && (ent_context->event_flags)) {
			if (addEvent(disp, ent_context, 
						ent_context->event_flags) < 0) {
				ret = -2;
				goto reg_error;
			}
			found_event = 1;
			setNonBlocking(ent_context->fd);
		}
		if ((ent_context->to_be_scheduled.tv_sec != 0) &&
				(ent_context->to_be_scheduled.tv_usec != 0)) {
			if (scheduleTimeoutEventLockHeld(disp,
						ent_context) < 0) {
				if (found_event == 1)
					delEvent(disp, ent_context, 
							ent_context->event_flags);
				ret = -2;
				goto reg_error;
			}
			found_timeout = 1;
		}
		if ((found_event == 1) || (found_timeout == 1)) {
			DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id, "Register success");
			ret = 1;
			goto disp_unlock;
		}
		ret = -3;
		goto reg_error;
	}
	ret = -4;
reg_error:
	disp->fd_flag[ent_context->fd] = -1;
	disp->con_ptr[ent_context->fd] = NULL;
disp_unlock:
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;
}


static int8_t unregisterEntity(dispatcher_t* disp,
		entity_context_t* ent_context) {

	pthread_mutex_lock(&(disp->disp_state));
	int ret = 0;
	if ((ent_context->fd >= 0) && disp->fd_flag[ent_context->fd]) {
		g_tree_remove(disp->registered_timeouts, 
				&(ent_context->scheduled));
		delAllEvent(disp, ent_context);
		disp->fd_flag[ent_context->fd] = -1;
		disp->con_ptr[ent_context->fd] = NULL;
		ent_context->delete_entity(ent_context);
		DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id, "Unregister success");
		ret = 1;
	} else
		ret = -1;
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;
}


static void* startDispatcher(void* args) {

	dispatcher_t* disp = (dispatcher_t*)args;
	if (disp_thread_conf_hdlr != NULL)
		disp_thread_conf_hdlr(disp->disp_id);
	struct epoll_event* events;
	events = disp_calloc(disp->fd_lk->max_fds, sizeof(struct epoll_event));
	if (!events)
		return NULL;
	int32_t rv, i, fd;
	entity_context_t* ent_context;
	while(1) {
		//printf("waiting\n");
		pthread_mutex_lock(&(disp->disp_state));
		if (disp->stop_flag == 1) {
			pthread_mutex_unlock(&(disp->disp_state));
			break;
		}
		int32_t poll_fds = disp->poll_fds;
		int32_t max_fds = disp->fd_lk->max_fds;
		int32_t epoll_timeout = disp->epoll_timeout;
		pthread_mutex_unlock(&(disp->disp_state));

		rv =  epoll_wait(poll_fds, events, max_fds, epoll_timeout);
		struct timeval now;
		gettimeofday(&now, NULL);
		//printf("Got event\n");
		if (rv == -1) {
			if (errno == EINTR)
				continue;
			break;
		} else if (rv == 0) {
			handleTimeoutFd(disp);
			continue;
		}

		int8_t flag = 0;
		pthread_mutex_lock(&(disp->disp_state));
		if (disp->epoll_timeout != -1) {
			//recalc. the timeout and handle expired timeouts
			disp->epoll_timeout = timevalCompare(
					&(disp->scheduled_timeout), &now, NULL);
			if (disp->epoll_timeout <= 0)
				flag = 1;
		}
		pthread_mutex_unlock(&(disp->disp_state));
		if (flag)
			handleTimeoutFd(disp);

		for (i = 0; i < rv; i++) {
			fd = events[i].data.fd;

			if (disp->fd_lk->acquire_lock(disp->fd_lk, fd) != 1)
				continue;

			/* Why Check for !=-1?
			   Reason: handleTimeoutFd() could have cleaned ent_context */
			if (disp->fd_flag[fd] != -1) {
				ent_context = disp->con_ptr[fd];
				ent_context->event_time.tv_sec = now.tv_sec;
				ent_context->event_time.tv_usec = now.tv_usec;
			}
			/* 1. We check if fd_flag status is true. Reason: While
			   handling an event, the handler can unregister the fd.
			   2. In the Below IFs, we check also if the context's
			   event flags are set. Reason: When handling an event 
			   another event could be UNSET by the handler */

			if (events[i].events & EPOLLIN)
				if ((disp->fd_flag[fd] != -1) && (ent_context->
							event_flags & EPOLLIN))
					ent_context->event_read(ent_context);
			if (events[i].events & EPOLLOUT)
				if ((disp->fd_flag[fd] != -1) && (ent_context->
							event_flags & EPOLLOUT))
					ent_context->event_write(ent_context);
			if (events[i].events & EPOLLERR)
				if ((disp->fd_flag[fd] != -1) && (ent_context->
							event_flags & EPOLLERR))
					ent_context->event_error(ent_context);
			if (events[i].events & EPOLLHUP)
				if ((disp->fd_flag[fd] != -1) && (ent_context->
							event_flags & EPOLLHUP))
					ent_context->event_hup(ent_context);

			disp->fd_lk->release_lock(disp->fd_lk, fd);
		}
	}
	free(events);
	disp->delete_dispatcher(disp);
	return NULL;
}


void endDispatcher(dispatcher_t* disp) {

	pthread_mutex_lock(&(disp->disp_state));
	disp->stop_flag = 1;	
	pthread_mutex_unlock(&(disp->disp_state));
}	


static int8_t scheduleTimeoutEvent(dispatcher_t* disp, 
		entity_context_t* ent_context) {
	int8_t ret = -1;
	pthread_mutex_lock(&(disp->disp_state));
	ret = scheduleTimeoutEventLockHeld(disp, ent_context);
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;	
}


static int8_t setReadEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	if (ent_context->event_read) {
		pthread_mutex_lock(&(disp->disp_state));
		ret = addEvent(disp, ent_context, EPOLLIN);
		if (ret > 0)
			ent_context->event_flags  |= EPOLLIN;
		pthread_mutex_unlock(&(disp->disp_state));
	}
	return ret;	
}


static int8_t unsetReadEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	pthread_mutex_lock(&(disp->disp_state));
	ret = delEvent(disp, ent_context, EPOLLIN);	
	if (ret > 0)
		ent_context->event_flags ^= EPOLLIN;
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;	
}


static int8_t setWriteEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	if (ent_context->event_write) {
		pthread_mutex_lock(&(disp->disp_state));
		ret = addEvent(disp, ent_context, EPOLLOUT);
		if (ret > 0)
			ent_context->event_flags |= EPOLLOUT;
		pthread_mutex_unlock(&(disp->disp_state));
	}
	return ret;	
}


static int8_t unsetWriteEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	pthread_mutex_lock(&(disp->disp_state));
	ret = delEvent(disp, ent_context, EPOLLOUT);	
	if (ret > 0)
		ent_context->event_flags ^= EPOLLOUT;
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;	
}


static int8_t setErrorEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	if (ent_context->event_error) {
		pthread_mutex_lock(&(disp->disp_state));
		ret = addEvent(disp, ent_context, EPOLLERR);
		if (ret > 0)
			ent_context->event_flags |= EPOLLERR;
		pthread_mutex_unlock(&(disp->disp_state));
	}
	return ret;	
}


static int8_t unsetErrorEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	pthread_mutex_lock(&(disp->disp_state));
	ret = delEvent(disp, ent_context, EPOLLERR);	
	if (ret > 0)
		ent_context->event_flags ^= EPOLLERR;
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;	
}


static int8_t setHupEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	if (ent_context->event_hup) {
		pthread_mutex_lock(&(disp->disp_state));
		ret = addEvent(disp, ent_context, EPOLLHUP);
		if (ret > 0)
			ent_context->event_flags |= EPOLLHUP;
		pthread_mutex_unlock(&(disp->disp_state));
	}
	return ret;	
}


static int8_t unsetHupEvent(dispatcher_t* disp, entity_context_t* ent_context) {

	int8_t ret = -1;
	pthread_mutex_lock(&(disp->disp_state));
	ret = delEvent(disp, ent_context, EPOLLHUP);
	if (ret > 0)
		ent_context->event_flags ^= EPOLLHUP;
	pthread_mutex_unlock(&(disp->disp_state));
	return ret;
}


static int8_t setNonBlocking(int32_t fd) {

	int32_t flags;
	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return -1;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;
	return 1;
}


static int8_t addEvent(dispatcher_t* disp, 
		entity_context_t* ent_context, uint32_t value) {

	DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id, "Adding event: %d", value);
	if (disp->fd_flag[ent_context->fd] >= 0) {
		struct epoll_event ev;
		memset((char *)&ev, 0, sizeof(struct epoll_event));

		ev.events = value;
		ev.data.fd = ent_context->fd;
		if (epoll_ctl(disp->poll_fds, EPOLL_CTL_ADD, 
					ent_context->fd, &ev) < 0) {
			if (errno == EEXIST) {
				ev.events |= ent_context->event_flags;
				if (epoll_ctl(disp->poll_fds, EPOLL_CTL_MOD,
							ent_context->fd, &ev) < 0) {
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
	return -3;
}


static int8_t delAllEvent(dispatcher_t* disp, entity_context_t* ent_context) { 

	if (disp->fd_flag[ent_context->fd] >= 0) {
		if (epoll_ctl(disp->poll_fds, EPOLL_CTL_DEL,
					ent_context->fd, NULL) < 0) {
			perror("ctl del:");
			if(errno == EEXIST) 
				return 0;
			return -1;
		}
		ent_context->event_flags = 0;
		return 1;
	}
	return -2;
}


static int8_t delEvent(dispatcher_t* disp, 
		entity_context_t* ent_context, uint32_t value) {

	DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id, "Delete event: %d",
			value);
	if (disp->fd_flag[ent_context->fd] >= 0) {
		struct epoll_event ev;
		memset((char *)&ev, 0, sizeof(struct epoll_event));

		ev.events = ent_context->event_flags ^ value;
		ev.data.fd = ent_context->fd;
		if (epoll_ctl(disp->poll_fds, EPOLL_CTL_MOD, 
					ent_context->fd, &ev) < 0) {
			perror("mod ctl: ");
			return -1;
		}
		return 1;
	}
	return -2;
}


static int8_t scheduleTimeoutEventLockHeld(dispatcher_t* disp, 
		entity_context_t* ent_context) {

	if (disp->fd_flag[ent_context->fd] != 1)
		return -1;

	gboolean rc = g_tree_remove(disp->registered_timeouts, 
			&(ent_context->scheduled));
	ent_context->scheduled.tv_sec = 
		ent_context->to_be_scheduled.tv_sec;
	ent_context->scheduled.tv_usec = 
		ent_context->to_be_scheduled.tv_usec;
	struct timeval* key = newTimevalKey(&(ent_context->scheduled));
	if (key) {
		while (1) {
			entity_context_t* tmp_ent_ctx = g_tree_lookup(
					disp->registered_timeouts, key);
			if (!tmp_ent_ctx)
				break;
			key->tv_usec += 1;
		}
		g_tree_insert(disp->registered_timeouts, 
				key, ent_context);
		ent_context->to_be_scheduled.tv_sec = 0;
		ent_context->to_be_scheduled.tv_usec = 0;
		//recalculate the epoll_timeout
		GTraverseFunc update_epoll_timeout = updateEpollTimeout;
		g_tree_foreach(disp->registered_timeouts, 
				update_epoll_timeout, disp);
		//traverse only one element.require MIN(timeout)
		return 1;
	}
	return -1;
}


static void handleTimeoutFd(dispatcher_t* disp) {

	int32_t fd = getTimeoutFd(disp);
	if (fd == -1)
		return;
	if (disp->fd_lk->acquire_lock(disp->fd_lk, fd) > 0) {
		/*
		   int8_t ret = -1;
		   pthread_mutex_lock(&(disp->disp_state));
		   if (disp->fd_flag[fd] != -1)
		   ret = 1;
		   pthread_mutex_unlock(&(disp->disp_state));
		   if (ret == 1)
		 */
		if (disp->fd_flag[fd] != -1)
			if (disp->con_ptr[fd]->event_timeout != NULL)
				disp->con_ptr[fd]->event_timeout(disp->con_ptr[fd]);
		disp->fd_lk->release_lock(disp->fd_lk, fd);
	}
}


static int32_t getTimeoutFd(dispatcher_t* disp) {

	pthread_mutex_lock(&(disp->disp_state));
	int32_t fd = -1;
	entity_context_t* ent_context = g_tree_lookup(
			disp->registered_timeouts, &(disp->scheduled_timeout));
	if (ent_context) {
		fd = ent_context->fd;
		g_tree_remove(disp->registered_timeouts, 
				&(disp->scheduled_timeout));
	}
	disp->epoll_timeout = -1;
	GTraverseFunc update_epoll_timeout = updateEpollTimeout;
	g_tree_foreach(disp->registered_timeouts, update_epoll_timeout, disp);
	pthread_mutex_unlock(&(disp->disp_state));
	return fd;
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


static gboolean updateEpollTimeout(gpointer key, 
		gpointer value, gpointer data) {

	struct timeval* val = (struct timeval*)key;
	dispatcher_t* disp = (dispatcher_t*)data;
	if ((disp->epoll_timeout == -1) || //firstTime or moveToNext 
			/* Checking if this new timeout value would be 
			   before the current epoll_timeout */
			(diffTimevalToMs(val, &(disp->scheduled_timeout)) == 0)) {
		struct timeval now;
		gettimeofday(&now, NULL);
		disp->epoll_timeout = diffTimevalToMs(val, &now);
		disp->scheduled_timeout.tv_sec = val->tv_sec;
		disp->scheduled_timeout.tv_usec = val->tv_usec;
	}
	return TRUE;
}



