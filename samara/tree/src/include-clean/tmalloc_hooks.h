/*
 *
 * tmalloc_hooks.h
 *
 *
 *
 */

#ifndef __TMALLOC_HOOKS_H_
#define __TMALLOC_HOOKS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* NOTE: we are not including any of the Tall Maple headers on purpose here! */

/* Environment variable containing output filename */
#define TMALLOC_OUTPUT_ENV "TMALLOC_OUTPUT"
#define TMALLOC_DETAIL_ENABLE_ENV "TMALLOC_DETAIL_ENABLE"
#define TMALLOC_DETAIL_INCLUDE_TIME_ENV "TMALLOC_DETAIL_INCLUDE_TIME"

/**
 * Initialize our internal state.  This includes installing our hooks into
 * glibc malloc, so that we get called back for all malloc family functions.
 * If 'filename' is non-NULL , or the environment variable TMALLOC_OUTPUT is
 * non-NULL , we will log a line to an output file for each malloc family
 * function.  In any case, we will keep stats on memory allocation, and
 * allow 'triggers' which will call tmalloc_debug() when certain conditions
 * are met, like large memory allocations.  See tmalloc_hooks.c for more
 * information on these.
 *
 * See tmalloc_hooks.c for information on automatically initializing tmalloc
 * using the __malloc_initialize_hook technique, without having to change
 * any program code.  Otherwise, you will have to explicitly call
 * tmalloc_init() at the beginning of each program.
 */
int tmalloc_init(int enable, const char *filename);

void tmalloc_init_hook(void);

int tmalloc_deinit(void);

/** Enable stats gathering and output logging */
int tmalloc_enable(void);

/** Disable stats gathering and output logging */
int tmalloc_disable(void);

/**
 * If tmalloc is enabled overall, re-enable detailed logging
 * (which is enabled by default).
 */
int tmalloc_detail_enable(void);

/**
 * If tmalloc is enabled overall, disable detailed logging (which is
 * enabled by default).  If detailed logging is disabled, tmalloc will
 * still track the same information, but will only output a summary at
 * the end, rather than the details of every single allocation-related
 * API call.
 */
int tmalloc_detail_disable(void);


/**
 * If tmalloc detail is enabled, include a timestamp in each detail log
 * line.
 */
int tmalloc_detail_include_time_enable(void);

/**
 * If tmalloc detail is enabled, disable including a timestamp in each
 * detail log line.
 */
int tmalloc_detail_include_time_disable(void);

/**
 * Flush the output file.  Output is heavily buffered, so this may be useful.
 */
int tmalloc_flush(void);

/** Output the statistics to the output file, and flush the output file. */
int tmalloc_output_stats(void);

/** Clear all of our statistics counters */
int tmalloc_clear_stats(void);

/**
 * Function called by the 'triggers' which you can set a breakpoint on in
 * the debugger.
 */
void tmalloc_debug(void);


#ifdef __cplusplus
}
#endif

#endif /* __TMALLOC_HOOKS_H_ */
