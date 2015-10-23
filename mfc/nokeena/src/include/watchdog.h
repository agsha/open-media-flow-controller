/* To hold the check function and the corresponding return status */


#define	    WD_RET_MSG_MAX_CHAR	    1024
#define	    SUCCESS		    "Success\n"
#define	    PYTHON_PATH		    "/usr/bin/python"
#define	    PROBE_SCRIPT	    "/opt/nkn/bin/pyAnkeenaProbe.py"

#define     STR_TMP_MAX		    256
#define     PROBE_URI		    "http://127.0.0.1/mfc_probe/mfc_probe_object.dat"
#define     PROBE_OBJ		    "/mfc_probe/mfc_probe_object.dat"

#include "watchdog_defs.h"

typedef struct watchdog_cb_arg_st {
    int alarm_idx;
    char ret_msg[WD_RET_MSG_MAX_CHAR];
    void *cb_arg;
} watchdog_cb_arg_t;

typedef tbool (*live_check) (watchdog_cb_arg_t *arg);

#define THRD_BT  "thread apply all backtrace full\n"
#define THRD_INF_AND_BT "info thread\nthread apply all backtrace full\n"

typedef struct nvsd_live_check_st {
    live_check check_function;     // Check function
    const char *name;           //Name of the check function,will be used to send out events
    const char *gdb_cmd;
    tbool ret_status;              //ret status
    wd_action_en_t  wd_action;
    tbool enabled;
    int threshold_counter;        //Counter for check failure
    int threshold;         // Threshold value for check
    int poll_time;               //sampling time for poll-func
    watchdog_cb_arg_t cb_arg;
    lew_event *watchdog_timer ;
}nvsd_live_check_t ;


