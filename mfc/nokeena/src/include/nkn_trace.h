#ifndef NKN_TRACE_H
#define NKN_TRACE_H
/*
 * Trace debug facility.
 *
 * A very low overhead implementation for a circular trace.  It allows various
 * NVS modules to add tracepoints.  Each tracepoint has a string name and some
 * number of parameters.  Each trace is timestamped.  The trace can be
 * printed in a debugger or printed out to a file for later use.  The trace
 * is useful for debugging problems as well looking at latency of operations.
 *
 * User guide:
 *
 * Use the macros below to add appropriate trace points.
 *
 * To enable the trace points compile your module/file with
 * "make NKN_TRACE=1" or by explictly adding -D_NKN_TRACE to CFLAGS.
 *
 * To display the trace, you have to use gdb.
 * call nkn_trace_printall() or nkn_trace_print(start, num) to display
 * call nkn_trace_write("filename") to create a file with the trace.
 */

void nkn_trace_init(void);
void nkn_trace_print(int start, int num);	/* print to stdout */
void nkn_trace_printall(void);	/* print to stdout */
void nkn_trace_save(void);		/* save trace to a file */

/* Add a trace record */
void nkn_trace_add(char *name, void *arg1, void *arg2, void *arg3);

#ifdef _NKN_TRACE
#define NKN_TRACE1(name, a1)		nkn_trace_add(name, a1, 0, 0);
#define NKN_TRACE2(name, a1, a2)   	nkn_trace_add(name, a1, a2, 0);
#define NKN_TRACE3(name, a1, a2, a3) 	nkn_trace_add(name, a1, a2, a3);
#else
#define NKN_TRACE1(name, a1)
#define NKN_TRACE2(name, a1, a2)
#define NKN_TRACE3(name, a1, a2, a3)
#endif
#endif
