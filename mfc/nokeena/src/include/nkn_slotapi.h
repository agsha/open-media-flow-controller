#ifndef __NKN_SLOTAPI__
#define __NKN_SLOTAPI__
#define INIT_NUM_TASKS_PER_MS 1000
#define NKN_DEF_MAX_MSEC 1000
#define MIN_SYNC_OOT 120

TAILQ_HEAD(nkn_task_head, nkn_task);

typedef struct nkn_task_head nkn_task_head_t;
typedef struct nkn_slot_mgr_t {
	uint64_t magic;		// used for sanity check.
	struct nkn_task_head *head;
	int *state;		// Slot state
	int *availability;	// Availability check
	int cur_slot;		// current slot being read by sched
	int numof_slots;	// the slot array size
	int slot_interval;	// the time interval between two neighbor slots
	int real_time;
	int num_tasks_per_interval;
	int num_slots_per_ms;
	struct timespec next_ts;
	int task_run;
	struct timespec saved_ts;

	/* internal mutexes used for thread-safe design */
	pthread_cond_t cond;
	pthread_mutex_t *muts;
	pthread_mutex_t *mut_avail;
	pthread_mutex_t slotwait_lock;
	AO_t num_tasks;
	AO_t cur_wakeup;
} nkn_slot_mgr;

void * slot_init(int numof_slots, int slot_interval, int is_real_time);
int slot_add(void * p, int32_t msec, struct nkn_task *item);
struct nkn_task *slot_get_next_item(void * p);
int rtslot_add(void * p, int32_t min_msec, int32_t max_msec,
	struct nkn_task *item, uint32_t *next_avail);
int rtslot_remove(int tid);
struct nkn_task *rtslot_get_next_item(void * p);

void
add_slot_item(nkn_slot_mgr *restrict pslotmgr, int insert_slot,
	      struct nkn_task * restrict item);
 
struct nkn_task *
remove_slot_item(nkn_slot_mgr *restrict pslotmgr, int slot);
#endif
