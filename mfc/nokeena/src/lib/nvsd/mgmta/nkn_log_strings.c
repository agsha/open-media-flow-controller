#include "nkn_mgmt_smi.h"
#include "nkn_log_strings.h"
#include <stdlib.h>

OFF_GEN_MSG_t gGEN_MSG = {
  .MSG = 0 ,
};

OFF_NVSD_MAX_FD_SET_t gNVSD_MAX_FD_SET = {
  .MAX = 0 ,
};

OFF_NVSD_SETSOCKOPT_FAILED_t gNVSD_SETSOCKOPT_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_BIND_FAILED_t gNVSD_BIND_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_ALREADY_RUNNING_t gNVSD_ALREADY_RUNNING = {
  .PID = 0 ,
};

OFF_NKNVD_ALREADY_RUNNING_t gNKNVD_ALREADY_RUNNING = {
  .PID = 0 ,
};

OFF_NVSD_MAX_FD_UNKNOWN_t gNVSD_MAX_FD_UNKNOWN = {
  .REASON = 0 ,
};

OFF_NVSD_MAX_FD_NOT_SET_t gNVSD_MAX_FD_NOT_SET = {
  .REASON = 0 ,
};

OFF_NVSD_RLIMIT_UNKNOWN_t gNVSD_RLIMIT_UNKNOWN = {
  .REASON = 0 ,
};

OFF_NVSD_SCHEDULER_INIT_FAILED_t gNVSD_SCHEDULER_INIT_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_SHUTTING_DOWN_t gNVSD_SHUTTING_DOWN = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_RATE_LIMIT_REACHED_t gNVSD_RATE_LIMIT_REACHED = {
  .MAX_GET_RATE_LIMIT = 0 ,
  .CUR_DATESTR = 1 ,
};

OFF_NVSD_MAX_FD_REDUCED_t gNVSD_MAX_FD_REDUCED = {
  .MAX = 0 ,
};

OFF_NVSD_CONN_ARRAY_ALLOC_FAILED_t gNVSD_CONN_ARRAY_ALLOC_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_EVENT_STRUCT_ALLOC_FAILED_t gNVSD_EVENT_STRUCT_ALLOC_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_EPOLL_CREATE_FAILED_t gNVSD_EPOLL_CREATE_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_EPOLL_CTL_ADD_FAILED_t gNVSD_EPOLL_CTL_ADD_FAILED = {
  .CODE_LOCN = 0 ,
  .ERRNO = 1 ,
};

OFF_NVSD_EPOLL_WAIT_FAILED_t gNVSD_EPOLL_WAIT_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_TIMER_TASK_FAILED_t gNVSD_TIMER_TASK_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_EPOLL_INVAL_SOCK_t gNVSD_EPOLL_INVAL_SOCK = {
  .FD = 0 ,
};

OFF_CM_URI_CANNOT_STAT_t gCM_URI_CANNOT_STAT = {
  .URI_FILE_NAME = 0 ,
};

OFF_OM_ACCESS_URI_t gOM_ACCESS_URI = {
  .URI_NAME = 0 ,
  .CODE = 1 ,
};

OFF_NVSD_OOMGR_REQ_TASK_FAILED_t gNVSD_OOMGR_REQ_TASK_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_TASK_STILL_ACTIVE_t gNVSD_TASK_STILL_ACTIVE = {
  .TID = 0 ,
};

OFF_NVSD_FMGR_REQ_TASK_FAILED_t gNVSD_FMGR_REQ_TASK_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_RECV_OVERFLOW_t gNVSD_RECV_OVERFLOW = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_IDLE_TASK_CLOSED_t gNVSD_IDLE_TASK_CLOSED = {
  .TID = 0 ,
  .SID = 1 ,
  .TIMEOUT = 2 ,
};

OFF_NVSD_MAX_FD_EXCEEDED_t gNVSD_MAX_FD_EXCEEDED = {
  .MAX = 0 ,
};

OFF_NVSD_NON_BLOCK_FAILED_t gNVSD_NON_BLOCK_FAILED = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_NON_IMPL_t gNVSD_NON_IMPL = {
  .CODE_LOCN = 0 ,
};

OFF_NVSD_SOCKFD_FAILED_t gNVSD_SOCKFD_FAILED = {
  .CODE_LOCN = 0 ,
};


NknLogFragOffsets nknLogFragOffsets[] = {
  { 0, 2 }, 
  { 2, 2 }, 
  { 4, 2 }, 
  { 6, 2 }, 
  { 8, 2 }, 
  { 10, 2 }, 
  { 12, 2 }, 
  { 14, 2 }, 
  { 16, 2 }, 
  { 18, 2 }, 
  { 20, 2 }, 
  { 22, 4 }, 
  { 26, 2 }, 
  { 28, 2 }, 
  { 30, 2 }, 
  { 32, 2 }, 
  { 34, 4 }, 
  { 38, 2 }, 
  { 40, 2 }, 
  { 42, 2 }, 
  { 44, 2 }, 
  { 46, 4 }, 
  { 50, 2 }, 
  { 52, 2 }, 
  { 54, 2 }, 
  { 56, 2 }, 
  { 58, 6 }, 
  { 64, 2 }, 
  { 66, 2 }, 
  { 68, 2 }, 
  { 70, 2 }, 
  {-1, -1 }
};

NknLogFragValues nknLogFragValues[] = {
// GEN_MSG
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_MAX_FD_SET
  { NKN_STR_CONST_TYPE, { .str = "Set max file descriptors to : " } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NVSD_SETSOCKOPT_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_BIND_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_ALREADY_RUNNING
  { NKN_STR_CONST_TYPE, { .str = "There is another running nvsd process (pid=" } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NKNVD_ALREADY_RUNNING
  { NKN_STR_CONST_TYPE, { .str = "There is another running nknvd process (pid=" } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NVSD_MAX_FD_UNKNOWN
  { NKN_STR_CONST_TYPE, { .str = "Couldn't get max filedescriptors " } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_MAX_FD_NOT_SET
  { NKN_STR_CONST_TYPE, { .str = "Couldn't set max filedescriptors " } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_RLIMIT_UNKNOWN
  { NKN_STR_CONST_TYPE, { .str = "Failed to set rlimit " } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_SCHEDULER_INIT_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_SHUTTING_DOWN
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_RATE_LIMIT_REACHED
  { NKN_STR_CONST_TYPE, { .str = "Max rate control limit " } },
  { NKN_INT_TYPE, { .fldName = 0 } },
  { NKN_STR_CONST_TYPE, { .str = " reached at " } },
  { NKN_STR_PTR_TYPE, { .fldName = 1 } },

// NVSD_MAX_FD_REDUCED
  { NKN_STR_CONST_TYPE, { .str = "Because this is not root, cannot set rlimit, reduce max_client to " } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NVSD_CONN_ARRAY_ALLOC_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_EVENT_STRUCT_ALLOC_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_EPOLL_CREATE_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_EPOLL_CTL_ADD_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },
  { NKN_STR_CONST_TYPE, { .str = ": epoll ctl add failed, errno = " } },
  { NKN_INT_TYPE, { .fldName = 1 } },

// NVSD_EPOLL_WAIT_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_TIMER_TASK_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_EPOLL_INVAL_SOCK
  { NKN_STR_CONST_TYPE, { .str = "epoll returned an invalid socket : " } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// CM_URI_CANNOT_STAT
  { NKN_STR_CONST_TYPE, { .str = "Content Manager : Could not stat " } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// OM_ACCESS_URI
  { NKN_STR_CONST_TYPE, { .str = "Accessed : " } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },
  { NKN_STR_CONST_TYPE, { .str = " " } },
  { NKN_INT_TYPE, { .fldName = 1 } },

// NVSD_OOMGR_REQ_TASK_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_TASK_STILL_ACTIVE
  { NKN_STR_CONST_TYPE, { .str = "Task " } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NVSD_FMGR_REQ_TASK_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_RECV_OVERFLOW
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NVSD_IDLE_TASK_CLOSED
  { NKN_STR_CONST_TYPE, { .str = "task " } },
  { NKN_INT_TYPE, { .fldName = 0 } },
  { NKN_STR_CONST_TYPE, { .str = " for socket id " } },
  { NKN_INT_TYPE, { .fldName = 1 } },
  { NKN_STR_CONST_TYPE, { .str = " has been idled for " } },
  { NKN_FLOAT_TYPE, { .fldName = 2 } },

// NVSD_MAX_FD_EXCEEDED
  { NKN_STR_CONST_TYPE, { .str = "Max file descriptor limit exceeded " } },
  { NKN_INT_TYPE, { .fldName = 0 } },

// NVSD_NON_BLOCK_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_NON_IMPL
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

// NVSD_SOCKFD_FAILED
  { NKN_STR_CONST_TYPE, { .str = "" } },
  { NKN_STR_PTR_TYPE, { .fldName = 0 } },

  { NKN_UNKNOWN_TYPE, { .str = "" } },
  { NKN_UNKNOWN_TYPE, { .fldName = 0 } },
};

