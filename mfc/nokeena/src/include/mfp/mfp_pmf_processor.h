#ifndef _MFP_PFM_PROCESSOR_
#define _MFP_PFM_PROCESSOR_

#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif
	struct tag_pmf_process_queue_item {
		char pmf_path[256];
		TAILQ_ENTRY(tag_pmf_process_queue_item) entries;
	};

	typedef struct tag_pmf_process_queue_item \
		pmf_process_queue_item_t;

	typedef int32_t (*fnp_start_session)(char*);
    
	typedef struct tag_pmf_proc_mgr {
		uint32_t id;
		int32_t (*start_session)(char *);

		// list that holds the list of pmfs that need to be processed
		TAILQ_HEAD(, tag_pmf_process_queue_item) pmf_process_queue_head;
		// lock for all queue operations
		pthread_mutex_t pmf_process_queue_lock;
	}pmf_proc_mgr_t;

        struct tag_dir_watch_thread_obj;
        typedef int32_t (*fnp_dir_watch_th_obj_cleanup)(struct tag_dir_watch_thread_obj *);
        typedef struct tag_dir_watch_thread_obj {
	    char *watch_dir_name;
	    fnp_dir_watch_th_obj_cleanup cleanup;
	    pmf_proc_mgr_t *pmgr;
	    int fd;
	    int wd;
	    uint8_t volatile stop;
	    LIST_ENTRY(tag_dir_watch_thread_obj) entries;
	} dir_watch_th_obj_t;
        LIST_HEAD(watch_dir_list, tag_dir_watch_thread_obj) g_watch_dir_list;
	
        /**
	 * initializes the resources for pmf processor 
	 */
        void init_mfp_pmf_processor(void);
	
	/**
	 * creates a instance of the PMF processor
	 * @param proc - the session start handler function
	 * @return returns a valid address pointing the instance of PMF
	 * processor, NULL on error
	 */
	pmf_proc_mgr_t* create_pmf_processor_mgr(fnp_start_session proc);

        /** 
	 * creates and initializes and object of the type
	 * 'dir_watch_th_obj_t' which can be used in calls to the
	 * other dir_watch API's
	 * @param dir_name - name of the directory that needs to be
	 * watched
	 * @param pmgr - the worker thread set for processing the
	 * events raised in the directory watch
	 * @param out - a fully initialized object
	 * @return returs 0 on sucess and a non - zero negative number
	 * on error
	 */
        int32_t create_dir_watch_obj(const char *dir_name, 
				     pmf_proc_mgr_t *pmgr, 
				     dir_watch_th_obj_t** out);

         /**
	  * creates a directory watch thread and starts the watcher
	  * @param dwto - a fully initialized directory watch object
	  * @return returns the thread id
	  */
         pthread_t start_dir_watch(dir_watch_th_obj_t *dwto);

         /**
	  * stops a directory watch
	  * @param dir_name - path to the directory for which the
	  * directory watch needs to be stopped 
	  * @return returns 0 on success and a non-zero negative
	  * integer on error
	  */
         int32_t stop_dir_watch(const char *dir_name);

	/********************************************************** 
	 *       HELPER - (STATIC) FUNCTION PROTOTYPES
	 **********************************************************/
	static int32_t mfp_add_pmf_to_queue(pmf_proc_mgr_t*, 
					    char *base_path,  
					    char  *relative_path);
	static void mfp_cleanup_pmf_process_queue_item(\
				pmf_process_queue_item_t *item);  
	/**
	 * event loop that processes the INOTIFY events for for a particular
	 * directory. Processes publishing requests in the form of PMF files
	 * @param fd - the fd created with the INOTIFY API
	 * @param path - path to the top of of the watch dir, required to
	 * construct the absoulte path of every event
	 * @return - runs in a thread does not return
	 */
	static int32_t start_dir_watch_event_loop(pmf_proc_mgr_t* pmgr, 
						  int fd, char *path); 

        static int32_t cleanup_dir_watch_obj(dir_watch_th_obj_t *dwto);
	
        /**
	 * the main PMF processor loop
	 */
	 void * mfp_process_pmf(void *private_data);

#ifdef __cplusplus
}
#endif

#endif
