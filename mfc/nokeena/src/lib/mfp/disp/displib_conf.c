#include "displib_conf.h"
#include <stdint.h>
#include <stdio.h>


uint32_t disp_thr_count = 0;

uint64_t disp_log_id = 0;

disp_thread_conf_fptr disp_thread_conf_hdlr = NULL;

disp_malloc_fptr disp_malloc_hdlr = disp_malloc;

disp_calloc_fptr disp_calloc_hdlr = disp_calloc;

disp_log_hdlr_fptr disp_log_hdlr = disp_log;

void* disp_malloc(uint32_t num) {

	return malloc(num);
}


void* disp_calloc(uint32_t num, uint32_t size) {

	return calloc(num, size);
}

void disp_log(int dlevel, uint64_t dmodule, const char * fmt, ...) {

	return;
}

