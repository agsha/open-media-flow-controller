/*
 * Implementation of trace facility.
 * Each trace is a record stored in a circular buffer.
 * TBD - locking may be required to avoid collisions between threads.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include "nkn_trace.h"
#include "nkn_util.h"
#include "nkn_memalloc.h"

#define MAXTRACENAME 8
/* Trace record.  TBD - add timestamp */
typedef struct {
	char name[MAXTRACENAME];
	u_int64_t tick;
	void *arg1, *arg2, *arg3;
} trace_rec_t;

trace_rec_t *tracebuf;
static int nkn_trace_size = 100000;	/* TBD need to make configurable */
static int numtrace=0;

void nkn_trace_write(char *file);

void
nkn_trace_init()
{
	if (!nkn_trace_size)
		return;

	tracebuf = (trace_rec_t *)nkn_calloc_type(nkn_trace_size,
						sizeof(trace_rec_t), mod_trace_rect_t);
	if (tracebuf == NULL)
		err(1, "Failed to allocate trace buffer\n");
}

void
nkn_trace_add(char *name, void *arg1, void *arg2, void *arg3)
{
	trace_rec_t *tr = &tracebuf[numtrace++ % nkn_trace_size];

	tr->tick = nkn_get_tick();
	strncpy(tr->name, name, MAXTRACENAME);
	tr->arg1 = arg1;
	tr->arg2 = arg2;
	tr->arg3 = arg3;
}

static void
print_trace(FILE *str, int num)
{
	trace_rec_t *tr = &tracebuf[num];

	/* Divide tick by 2**20 to approx get usecs */
	fprintf(str, "%ld: %s %p, %p, %p\n", tr->tick >> 20, tr->name, tr->arg1, tr->arg2, tr->arg3);
}

void
nkn_trace_print(int start, int num)
{
	int i,j;

	for (i=start, j=0; i<nkn_trace_size && j<num; i++, j++)
		print_trace(stdout, i);
}

void
nkn_trace_printall()
{
	int i;

	if (numtrace > nkn_trace_size) {
		for (i=numtrace%nkn_trace_size; i<nkn_trace_size; i++)
			print_trace(stdout, i);
	}
	for (i=0; i < numtrace; i++)
		print_trace(stdout, i);
}

void
nkn_trace_write(char *file)
{
	FILE *tf = fopen(file, "w");
	int i;

        if (numtrace > nkn_trace_size) {
                for (i=numtrace%nkn_trace_size; i<nkn_trace_size; i++)
                        print_trace(tf, i);
        }
        for (i=0; i < numtrace; i++)
                print_trace(tf, i);
	fclose(tf);
}
