/*
 *
 */

#ifndef __PERF_H_
#define __PERF_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file perf.h Performance measurement functions
 * \ingroup lc
 */

#include "common.h"

#define lc_perf_max_timers 1000

/**
 * Initialize the performance library.  Calling this is optional, as
 * the first call to lc_perf_timer_start() or lc_perf_timer_stop()
 * will automatically call it if it has not been already.
 */
int lc_perf_init(void);

/**
 * Start a timer, optionally resetting it first.
 *
 * \param timer_id ID of timer to start.  It may be between 0 and 
 * lc_perf_max_timers - 1.
 *
 * \param log Should we log something saying we're starting this timer?
 *
 * \param reset Should we reset the timer before starting it?
 * Generally will be \c true unless you want to start and stop it
 * multiple times to get a sum of nonconsecutive time spent.
 * We will only log starting of a timer if it is reset also.
 *
 * \param replace_name If this timer already has a name, should we
 * still take the one specified here?  This attempts to minimize
 * memory allocation when a given timer will always have the same
 * name.  We only do the vsmprintf() once at the beginning, but then
 * preserve that result subsequently.
 *
 * \param name_fmt Format string for name to give this timer, if any.
 * Pass NULL to leave the name the same as before.
 */
int lc_perf_timer_start(int timer_id, tbool log, tbool reset, 
                        tbool replace_name, const char *name_fmt, ...)
    __attribute__ ((format (printf, 5, 6)));

/**
 * Stop a timer, optionally logging its elapsed time.
 *
 * \param timer_id ID of timer to stop.  An error will be returned
 * if this timer was not running.
 * \param log Should we log this timer's elapsed time?  Generally will
 * be \c true unless you are planning on starting it later to measure
 * nonconsecutive time intervals.
 */
int lc_perf_timer_stop(int timer_id, tbool log);

/**
 * Print a report to stdout listing all of the timers, the elapsed
 * time on each, and the number of laps on each.
 */
int lc_perf_print_report(void);

/**
 * Tell if a specified timer_id is currently running.
 */
tbool lc_perf_timer_is_running(int timer_id);

/*
 * Everything below this point is behind the abstraction offered by
 * the above functions.  You can still use it if you want, but we
 * don't want to confuse people in the average case by presenting them
 * in Doxygen docs.
 */
/** \cond */

/* 
 * A fast cycle count.
 * This is about 15 times faster on our systems than gettimeofday().  
 * On a 2.6 GHz P4, it takes about 33ns, or does about 30 million / sec.
 */
#if defined(PROD_TARGET_ARCH_I386) || (defined(PROD_TARGET_ARCH_X86_64) && defined(PROD_TARGET_CPU_WORDSIZE_32))

/* i386 */

#define lc_perf_get_cycles() \
({       register unsigned long long ret; \
         __asm__ __volatile__ ("rdtsc" : "=A" (ret)); \
         ret;  \
})

#endif

#if defined(PROD_TARGET_ARCH_X86_64) && !defined(PROD_TARGET_CPU_WORDSIZE_32)

/* x86_64 */

#define lc_perf_get_cycles() \
    ({       register unsigned int __low, __high;                         \
        __asm__ __volatile__ ("rdtsc" : "=a" (__low), "=d" (__high));     \
        ( __low | (((unsigned long) __high) << 32) );                     \
    })

#endif

#if defined(PROD_TARGET_ARCH_SPARC) || defined(PROD_TARGET_ARCH_SPARC64)
/* XXX: this means our maximum time is only 4 seconds on a 1GHz sparc */
#define lc_perf_get_cycles() \
({      unsigned long ret; \
        __asm__ __volatile__("rd %%tick, %0" : "=r" (ret)); \
        ret; \
})

#endif

#if defined(PROD_TARGET_ARCH_FAMILY_PPC)
#define lc_perf_get_cycles()                            \
    ({                                                  \
    unsigned int tbl, tbu0, tbu1;                       \
    do {                                                \
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0)); \
        __asm__ __volatile__ ("mftb %0" : "=r"(tbl));   \
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1)); \
    } while (tbu0 != tbu1);                             \
    ((((unsigned long long)tbu0) << 32) | tbl);         \
 })

#endif

#ifndef lc_perf_get_cycles

/*
 * If we don't have a cycle counter for this target arch, just use
 * gettimeofday
 */
unsigned long long lc_perf_get_cycles(void);

#endif

/*
 * Number of cycles / per second, to make cycle counts displayable to user
 */
int lc_perf_get_cpu_speed(unsigned long long *ret_cycles_sec);

/** \endcond */

#ifdef __cplusplus
}
#endif

#endif /* __PERF_H_ */
