#ifndef DISPLIB_CONF_HH
#define DISPLIB_CONF_HH

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
{
#endif

#define DISP_MOD_NAME "DISP"
#define DISP_USER_MOD 0x0000000100000000

enum disp_log_level {
	DISP_ALARM = 0,
	DISP_SEVERE = 1,
	DISP_ERROR = 2,
	DISP_WARNING = 3,
	DISP_MSG = 4,
};


void disp_log(int32_t dlevel, uint64_t dmodule, const char * fmt, ...);

void* disp_malloc(uint32_t num);

void* disp_calloc(uint32_t num, uint32_t size);


typedef void (*disp_log_hdlr_fptr)(int32_t delevel, uint64_t dmodule,
		const char* fmt, ...)
		__attribute__ ((format (printf, 3, 4)));

typedef void (*disp_thread_conf_fptr)(uint32_t thr_id);

typedef void* (*disp_malloc_fptr)(uint32_t num);

typedef void* (*disp_calloc_fptr)(uint32_t num, uint32_t size);


extern uint32_t disp_thr_count;

extern disp_thread_conf_fptr disp_thread_conf_hdlr;

extern disp_malloc_fptr disp_malloc_hdlr;

extern disp_calloc_fptr disp_calloc_hdlr;

extern disp_log_hdlr_fptr disp_log_hdlr;

extern uint64_t disp_log_id;

#define DBG_DISPLOG(dId, dlevel, dmodule, fmt, ...)                      \
{                                   \
	disp_log_hdlr(dlevel, dmodule, \
			"["#dmodule"."#dlevel"] %s:%d: ID[%s] "fmt"\n",   \
			__FUNCTION__,                \
			__LINE__, dId, ##__VA_ARGS__);                \
}


#ifdef __cplusplus
}
#endif

#endif

