#include "disp/dispatcher_manager.h"

static void* startDispManager(void* args);
static void endDispManager(dispatcher_mngr_t*);
static void deleteDispManager(dispatcher_mngr_t* disp_mngr);
static int8_t dispManagerTimeout(entity_context_t* ent_context);

static int8_t registEntity(entity_context_t* ent_context);
static int8_t selfUnregister(entity_context_t* ent_context);
static int8_t unregistEntity(entity_context_t* ent_context);

static int8_t setRead(entity_context_t*);
static int8_t unsetRead(entity_context_t*);
static int8_t setWrite(entity_context_t*);
static int8_t unsetWrite(entity_context_t*);
static int8_t setError(entity_context_t*);
static int8_t unsetError(entity_context_t*);
static int8_t setHup(entity_context_t*);
static int8_t unsetHup(entity_context_t*);
static int8_t delAllEvent(entity_context_t*);

static int8_t scheduleTimeout(entity_context_t*);


void initDispLib(disp_malloc_fptr malloc_hdlr, disp_calloc_fptr calloc_hdlr, 
		disp_thread_conf_fptr thread_conf, disp_log_hdlr_fptr log_hdlr,
		uint64_t log_id) {

	disp_thread_conf_hdlr = thread_conf;
	if (malloc_hdlr != NULL)
		disp_malloc_hdlr = malloc_hdlr;
	if (calloc_hdlr != NULL)
		disp_calloc_hdlr = calloc_hdlr;
	if (log_hdlr != NULL)
		disp_log_hdlr = log_hdlr;
	disp_log_id = log_id;
}


dispatcher_mngr_t* newDispManager(int32_t num_disps, int max_fds) {

	if (num_disps <=0)
		return NULL;
	max_fds += num_disps; // extra allocated fds are used by disp manager

	dispatcher_mngr_t* disp_mngr = disp_calloc_hdlr(1, 
			sizeof(dispatcher_mngr_t));
	if (!disp_mngr)
		return NULL;
	disp_mngr->num_disps = num_disps;
	disp_mngr->max_fds = max_fds;
	disp_mngr->next_disp_to_use = 0;

	disp_mngr->disps = NULL;
	disp_mngr->fd_flag = NULL;
	disp_mngr->fd_lk = NULL;
	disp_mngr->disp_thrds = NULL;
	disp_mngr->mngr_cnxt = NULL;

	disp_mngr->fd_lk = newFdState(max_fds);
	if (!disp_mngr->fd_lk)
		goto del_disp_mngr;

	disp_mngr->disps = disp_calloc_hdlr(num_disps, sizeof(dispatcher_t*));
	int i = 0;
	for (;i < num_disps; i++)
		disp_mngr->disps[i] = NULL; 
	if (!disp_mngr->disps)
		goto del_disp_mngr;
	for (i = 0;i < num_disps; i++) {
		disp_mngr->disps[i] = newDispatcher(i + 1, disp_mngr->fd_lk);
		if (!disp_mngr->disps[i])
			goto del_disp_mngr;
	}

	disp_mngr->fd_flag = disp_calloc_hdlr(max_fds, sizeof(int32_t));
	if (!disp_mngr->fd_flag)
		goto del_disp_mngr;
	for (i = 0; i < max_fds; i++)
		disp_mngr->fd_flag[i] = -1;

	disp_mngr->disp_thrds =
		disp_calloc_hdlr(disp_mngr->num_disps, sizeof(pthread_t));
	if (!disp_mngr->disp_thrds)
		goto del_disp_mngr;

	disp_mngr->mngr_cnxt = 
		disp_calloc_hdlr(disp_mngr->num_disps, sizeof(entity_context_t*));
	if (!disp_mngr->mngr_cnxt)
		goto del_disp_mngr;
	dispManagerIntf timeout_handler = dispManagerTimeout;
	for (i = 0;i < num_disps; i++) {
		disp_mngr->mngr_cnxt[i] = newEntityContext(max_fds-1-i, NULL,
			NULL, NULL, NULL, NULL, NULL, 
			timeout_handler, disp_mngr);
		if (!disp_mngr->mngr_cnxt[i])
			goto del_disp_mngr;
	}

        disp_mngr->register_entity = registEntity;
        disp_mngr->unregister_entity = unregistEntity;
        disp_mngr->self_unregister = selfUnregister;
	disp_mngr->start = startDispManager;
	disp_mngr->end = endDispManager;

	disp_mngr->self_set_read = setRead;
	disp_mngr->self_set_write = setWrite;
	disp_mngr->self_set_error = setError;
	disp_mngr->self_set_hup = setHup;
	disp_mngr->self_unset_read = unsetRead;
	disp_mngr->self_unset_write = unsetWrite;
	disp_mngr->self_unset_error = unsetError;
	disp_mngr->self_unset_hup = unsetHup;

        disp_mngr->self_del_all_event = delAllEvent;
	disp_mngr->self_schedule_timeout = scheduleTimeout;

	pthread_mutex_init(&(disp_mngr->state), NULL);
	DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id,
			"Dispatcher manager created");
	return disp_mngr;
			
del_disp_mngr:
	deleteDispManager(disp_mngr);
	return NULL;
}


static void endDispManager(dispatcher_mngr_t* disp_mngr) {

	int i = 0;
	for (; i < disp_mngr->num_disps; i++)
		disp_mngr->disps[i]->end_dispatcher(disp_mngr->disps[i]);
}


static void deleteDispManager(dispatcher_mngr_t* disp_mngr) {

	DBG_DISPLOG(DISP_MOD_NAME, DISP_MSG, disp_log_id,
			"Dispatcher manager deleted");
	if (disp_mngr->disps)
		free(disp_mngr->disps);
	// Note: disp_mngr->mngr_cnxt will be cleaned in the dispatcher
	if (disp_mngr->fd_flag)
		free(disp_mngr->fd_flag);
	if (disp_mngr->disp_thrds)
		free(disp_mngr->disp_thrds);
		
	disp_mngr->fd_lk->delete_fd_state(disp_mngr->fd_lk);
	pthread_mutex_destroy(&(disp_mngr->state));
	free(disp_mngr);
}


static void* startDispManager(void* args) {

        size_t stacksize;
	pthread_attr_t attr;

	if (disp_thread_conf_hdlr != NULL)
		disp_thread_conf_hdlr(disp_thr_count);
	disp_thr_count++;
	dispatcher_mngr_t* disp_mngr = (dispatcher_mngr_t*) args;
	int8_t i = 0, ret;
	struct timeval now;
	gettimeofday(&now, NULL);
	now.tv_sec += MAX_DISP_WAIT;

	struct sigaction signal_action;
	memset(&signal_action, 0, sizeof(struct sigaction));
	signal_action.sa_handler = SIG_IGN;
	signal_action.sa_flags = 0;
	if (sigaction(SIGPIPE, &signal_action, NULL) == -1) {
		perror("sigpipe action set failed: ");
		exit(-1);
	}	

	for (; i < disp_mngr->num_disps; i++) {
		// registering callback timer : to prevent indefinite poll wait
		disp_mngr->mngr_cnxt[i]->to_be_scheduled.tv_sec = now.tv_sec;
		disp_mngr->mngr_cnxt[i]->to_be_scheduled.tv_usec = now.tv_usec;
		ret = disp_mngr->register_entity(disp_mngr->mngr_cnxt[i]);

		if (ret > 0) {
		        pthread_attr_init(&attr);
			pthread_attr_getstacksize (&attr, &stacksize);
			stacksize = 512*1024;
			pthread_attr_setstacksize (&attr, stacksize);
			pthread_create(&(disp_mngr->disp_thrds[i]), NULL, 
				disp_mngr->disps[i]->start_dispatcher, 
				disp_mngr->disps[i]);
		} else
			break;
	}

	for (i = 0; i < disp_mngr->num_disps; i++)
		pthread_join(disp_mngr->disp_thrds[i], NULL);

	deleteDispManager(disp_mngr); 
	return NULL;
}


static int8_t dispManagerTimeout(entity_context_t* ent_context) {

	// receive event, perform no action, reschedule timeout	
	gettimeofday(&(ent_context->to_be_scheduled), NULL);
	ent_context->to_be_scheduled.tv_usec += (MAX_DISP_WAIT * 1000);
	//printf("Received timeout\n");
	return ent_context->disp_mngr->self_schedule_timeout(ent_context);
}


static int8_t registEntity(entity_context_t* ent_context) {

	int8_t ret = -1;
	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	if (disp_mngr->fd_lk->
			acquire_lock(disp_mngr->fd_lk, ent_context->fd) == 1) {

		pthread_mutex_lock(&(disp_mngr->state));
		if (disp_mngr->fd_flag[ent_context->fd] == -1) {//not registered
			dispatcher_t* disp = 
				disp_mngr->disps[disp_mngr->next_disp_to_use];
			ret = disp->register_entity(disp, ent_context);
			if (ret > 0) {
				disp_mngr->fd_flag[ent_context->fd] = 
					disp_mngr->next_disp_to_use;
				disp_mngr->next_disp_to_use = 
					(disp_mngr->next_disp_to_use + 1)
						%disp_mngr->num_disps;
			}
		}
		pthread_mutex_unlock(&(disp_mngr->state));
		disp_mngr->fd_lk->release_lock(disp_mngr->fd_lk,
				ent_context->fd);
	}
	return ret;
}


static int8_t selfUnregister(entity_context_t* ent_context) {

	int8_t ret = -1;
	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	if (disp_mngr->fd_flag[ent_context->fd] != -1) {
		int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
		int32_t fd = ent_context->fd;
		dispatcher_t* disp = disp_mngr->disps[disp_pos];
		ret = disp->unregister_entity(disp, ent_context);
		if (ret > 0)
			disp_mngr->fd_flag[fd] = -1;
	}
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t unregistEntity(entity_context_t* ent_context) {
	
	int8_t ret = -1;
	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	if (disp_mngr->fd_lk->
		acquire_lock(disp_mngr->fd_lk, ent_context->fd) == 1) {

		ret = disp_mngr->self_unregister(ent_context);
		disp_mngr->fd_lk->release_lock(disp_mngr->fd_lk, 
				ent_context->fd);
	}
	return ret;
}


static int8_t setRead(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->set_read_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t unsetRead(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->unset_read_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t setWrite(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->set_write_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t unsetWrite(entity_context_t*ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->unset_write_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t setError(entity_context_t*ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->set_error_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t unsetError(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->unset_error_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t setHup(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->set_hup_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t unsetHup(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->unset_hup_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t delAllEvent(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->del_all_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


static int8_t scheduleTimeout(entity_context_t* ent_context) {

	dispatcher_mngr_t* disp_mngr = ent_context->disp_mngr;
	pthread_mutex_lock(&(disp_mngr->state));
	int32_t disp_pos = disp_mngr->fd_flag[ent_context->fd];
	dispatcher_t* disp = disp_mngr->disps[disp_pos];
	int8_t ret = disp->sched_timeout_event(disp, ent_context);
	pthread_mutex_unlock(&(disp_mngr->state));
	return ret;
}


