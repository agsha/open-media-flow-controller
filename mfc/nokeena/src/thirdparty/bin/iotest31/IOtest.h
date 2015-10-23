/* Copyright (c) 1996 - 2001 by Solid Data Systems.
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND DATABASE EXCELLERATION SYSTEMS DISCLAIMS
* ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL DATABASE EXCELLERATION
* SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

/*
 *  IOtest   random read benchmark
 *  General (global) definitions
 *  Richard Cooper  11/19/97
 */

/* Current major version number */
#define VERSION		"2.31"


#ifdef __linux__
#ifndef __STDC_EXT__
/*#define __STDC_EXT__ */
#endif
#ifndef _LARGEFILE64_SOURCE
/*#define _LARGEFILE64_SOURCE */
#endif
#ifndef __LP64__
/*#define __LP64__ */
#endif
#endif

#ifdef __sun__
#ifndef __STDC_EXT__
#define __STDC_EXT__
#endif
/*#define __LP64__*/
/*#define _LARGEFILE64_SOURCE*/
/*#define _FILE_OFFSET_BITS 64*/
#endif

#ifdef __hpux__
#ifndef __STDC_EXT__
#define __STDC_EXT__
#endif
#if _LFS_LARGEFILE == 1
#define _LARGEFILE64_SOURCE
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#ifndef NULL
#define NULL 0
#endif


#define ITER 1000

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef RAND_MAX
#define RAND_MAX INT_MAX
#endif

#ifndef O_SYNC
#define O_SYNC 010000
#endif

#ifdef DEBUG
#define DEBUG   1
#endif
#ifdef NODEBUG
#define DEBUG   0
#endif

#define INBUFLEN 50
#define LEN 1

/********************************************/
/* Iteration count for exerciser package    */
/********************************************/
#define EXER_ITERATIONS 25000
#define EXER_PASSCOUNT 1000000
/* Don't make EXER_PROCESSCOUNT > 1 Not yet...  */
#define EXER_PROCESSCOUNT 1

/*****************************************************/
/* Number of passes for maintenance mode reliability */
/* test                                              */
/*****************************************************/
#define REL_PASS_COUNT 50000
/* Normally, _REL_READ should be defined  */
#define _REL_READ 

/************************************************************/
/* default min, max, and default iterations for benchmarks  */
/************************************************************/
#define MIN_ITER_COUNT 100
#define DEF_ITER_COUNT 10000
#define MAX_ITER_COUNT 50000


/**********************************************/
/* Sizes of read/write transfers in bytes     */
/* MIN should be less than MAX.....           */
/* set MIN = MAX to do transfers at one size  */
/**********************************************/
#define	MIN_TRANSFER_SIZE	(512*1)
#define MAX_TRANSFER_SIZE	(2*1024*1024)

/***********************************************/
/* limits for max # of processes to generate   */
/* Do us all a favor, and make MIN < MAX       */
/***********************************************/
#define MIN_PROC_COUNT 1	/* nyi */
#define MAX_PROC_COUNT 40
#define DEF_PROC_COUNT 8



/****************************************************/
/* These are used by the write block test to define */
/* the transfer size and the byte patterns          */
/****************************************************/ 

#define NBYTES 3                       /* number of bytes in pattern   */
#define PATT_BLOCKS 3                  /* number of blocks for each block number */
#define BLOCKSIZE 512                  /* bytes per block      */

#define PATT_SIZE (PATT_BLOCKS*BLOCKSIZE)      /* number of blocks for each block number */
#define BUFF_SIZE (PATT_BLOCKS*BLOCKSIZE*50)   /* size of write buffer in bytes          */



/***************************************************/
/* START and END define starting and ending block  */
/* numbers for maintenance tests. They should both */
/* be set to "0" for normal operation              */    
/* Not fully implemented - minimal sanity checking */
/***************************************************/ 

#define START 0
#define END   0



/********************************************************/
/* Comment this next line out to disable %done output   */
/* in maintenance tests.  The %done is nice, but it     */
/* probably uses too much cpu time.  It would be better */
/* to check the %done once a second, not every time you */
/* go through the loops.                                */
/********************************************************/
#define PERCENT

void io_error (char *);
void logit (char *);
long int MSTime (void);

char buf[200];
char inbuf[INBUFLEN], ans[10];
