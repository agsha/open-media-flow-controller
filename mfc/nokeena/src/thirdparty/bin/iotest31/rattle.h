/*  rattle.h  V1.04        */
/*  Rich Cooper   5/25/98  */

#define _ANSI_SOURCE


#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#ifndef NULL
#define NULL 0
#endif

/*EXIT and RAND_MAX definitions needed for SunOS*/
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef RAND_MAX
#define RAND_MAX INT_MAX
#endif

#ifdef DEBUG
#define DEBUG   1
#endif
#ifdef NODEBUG
#define DEBUG   0
#endif

/**********************************************/
/* Sizes of read/write transfers in bytes     */
/* MIN should be less than MAX.....           */
/* set MIN = MAX to do transfer at one size   */
/**********************************************/
#define	MIN_TRANSFER_SIZE	(512*4)
#define MAX_TRANSFER_SIZE	(512*128)

/**********************************************/
/* limit for max # of processes to generate   */
/**********************************************/
#define MAX_PROC_COUNT 50

/*********************************/
/*  Maximum number of devices  */
/*********************************/
#define MAX_DEVICES 100

/******************************************/
/* Set to "1" to force synchronous writes */
/******************************************/
#define SYNC_WRITES 1
