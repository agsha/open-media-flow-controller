/* Copyright (c) 1996 - 2000 by Solid Data Systems.
   *
   * Permission to use, copy, modify, and distribute this software for any
   * purpose with or without fee is hereby granted, provided that the above
   * copyright notice and this permission notice appear in all copies.
   *
   * THE SOFTWARE IS PROVIDED "AS IS" AND SOLID DATA SYSTEMS DISCLAIMS
   * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
   * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL SOLID DATA
   * SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
   * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
   * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
   * SOFTWARE. */

/**************************************************/
/* benchmks.c  version 2.30                       */
/* INPUT:                                         */
/*   fd        file descriptor for device to test */
/*   blocks    # of blocks on the device to test  */
/* Richard Cooper 11/19/97                        */
/**************************************************/

/* 2/12/98  Added  read_write_mp function for "-a XX" */
/* option.                                            */
/* Made most of the variables static to prevent       */
/* conflicts.                                         */
/* 4/28/98                                            */
/* Changed read_write_mp for "-a XX" to random_rdwr_mp */

/*  Added adjustable millisecond delay before read() */
/*  5/20/00   rbc                                    */

/*  fixed data rate overflow bug   1/3/01 rbc        */

#include <sys/mman.h>
#include <err.h>
#include "IOtest.h"

#define DO_READ 1
#define DO_WRITE 2

void srand48 ();
double drand48 ();

void
random_rdwr_mp (int fd, long long blocks, int procs, int min_xfer_size,
		int max_xfer_size, int rdwr_iterations, int rwpercent,
		int do_log, int skip_block_zero)
{
  static long long random_block;
  static long int charsread, charswrote;
  static long int rdwr_size;
  static long int elapsed_time;
  static long int start_time;
  static long int stop_time;
  static int p, j, count;
  static pid_t pid;
  static int i;
  static long int IOrate;
  static long long data_rate;

#if DEBUG
  long min, max;
#endif

  static char real_wrbuffer[MAX_TRANSFER_SIZE+512];
  char *wrbuffer;
  static char *rbuffer = NULL;
  static int rdwr_rand;
  static int rdwr_type;

  /* Align the buffer */
  wrbuffer = (char *)(((unsigned long long)real_wrbuffer+512) & ~0x1ff);
  if (rbuffer == NULL) {
	rbuffer = mmap(NULL, MAX_TRANSFER_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (rbuffer == NULL)
		err(1, "failed to get read buffer\n");
  }
  for (i = 1; i <= (int) max_xfer_size; ++i)
    wrbuffer[i] = (char) i;
  rdwr_size = min_xfer_size;
  blocks = blocks - 1;		/*Turn # of blocks into highest block # */
  blocks = blocks - (MAX_TRANSFER_SIZE / 512);	/*stay away from the end */

#ifndef NO_BUFFER
  blocks = blocks - 3000;
#endif

#if DEBUG
  min = 10000;
  max = blocks - 10000;
#endif
#ifdef DEBUG
  printf ("blocks=%lld procs=%d min=%d max=%d iter=%d per=%d\n", blocks,
	  procs, min_xfer_size, max_xfer_size, rdwr_iterations, rwpercent);
#endif

  while (rdwr_size <= max_xfer_size) {
    start_time = MSTime ();
    pid = 1;
    /*i = 0; */
    j = 0;
    for (p = 1; p <= procs; ++p) {
/*  Seed the random number generator.  Adding "p" insures that   */
/*  each seed is different, and that each child gets a different */
/*  sequence of random numbers.  That way, two consecutive IO's  */
/*  can't go to the same random block.                           */
      srand48 ((unsigned int) time (NULL) + p);

/*  fork 'procs' number of child processes, each executing the benchmark code */
/*  if pid = 0, you're in a forked process, if pid > 0, you're in the parent   */
      pid = fork ();
      if (pid == 0)
	break;
    }

    switch (pid) {
    case -1:
      exit (EXIT_FAILURE);
    case 0:
      count = 0;
      while (count < rdwr_iterations) {
	random_block = (drand48 () * (blocks - 1) + skip_block_zero);

#if DEBUG
	if (min > random_block)
	  min = random_block;
	if (max < random_block)
	  max = random_block;
#endif
#ifdef _LARGEFILE64_SOURCE
	if ((lseek64 (fd, (off64_t) (random_block * 512), SEEK_SET)) < 0) {
	  perror ("IOtest: lseek64");
	  exit (EXIT_FAILURE);
	}
#else
	if ((lseek (fd, (long) (random_block * 512), SEEK_SET)) < 0) {
	  perror ("IOtest: lseek");
	  exit (EXIT_FAILURE);
	}
#endif
	/*  Read or Write ????????? */
	/* if rwpercent = 0, this is a write only test  */
	/* if rwpercent = 100, this is a read only test */
	/* else its read/write                          */
	switch (rwpercent) {
	case 0:
	  rdwr_type = DO_WRITE;
	  break;
	case 100:
	  rdwr_type = DO_READ;
	  break;

	  /* If we get here, it's a read/write test.    */
	  /* generate a random number between 0 and 100 */
	  /* and compare it to the requested read/write */
	  /* percentage                                 */

	default:
	  rdwr_rand = ((float) rand () / RAND_MAX) * 100;
	  rdwr_type = (rdwr_rand >= rwpercent) ? DO_WRITE : DO_READ;
	  break;
	}

	switch (rdwr_type) {
	case DO_READ:

	  if ((charsread = (long int) read (fd, rbuffer, (size_t) rdwr_size)) < 0) {
	    perror ("IOtest: read");
	    printf
	      ("Error during transfer of %ld bytes starting at block %lld\n",
	       rdwr_size, random_block);
	    exit (EXIT_FAILURE);
	  }
	  if (charsread != rdwr_size) {
	    printf
	      ("%ld of %ld bytes read starting at block %lld\n",
	       charsread, rdwr_size, random_block);
	    exit (EXIT_FAILURE);
	  }
	  break;

	case DO_WRITE:
	  if ((charswrote = (long int) write (fd, wrbuffer, (size_t) rdwr_size)) < 0) {
	    perror ("IOtest: write");
	    printf
	      ("Error during transfer of %ld bytes starting at block %lld\n",
	       rdwr_size, random_block);
	    exit (EXIT_FAILURE);
	  }
	  if (charswrote != rdwr_size) {
	    printf
	      ("%ld of %ld bytes written starting at block %lld\n",
	       charswrote, rdwr_size, random_block);
	    exit (EXIT_FAILURE);
	  }
	  break;

	default:
	  break;
	}

#if _Shared_mem_for_rbuffer
/* touch each block in rbuffer to make sure data is really there   */
	if (DO_READ) {
	  for (j = 0; j < (int) rdwr_size; j += 512)
	    /*printf(" %d",j); */
	    rbuffer[j] &= 1;
	}
	/*printf("\n"); */
#endif

	count++;
      }

#if DEBUG
      printf ("blocks = %lld min block # = %ld  max block # = %ld \n", blocks, min, max);
#endif
      exit (EXIT_SUCCESS);
    default:
#if DEBUG
      printf ("This is the parent.. pid = %d\n", (int) getpid ());
#endif
      break;
    }
    /* Wait until all the children have finished   */
    if (pid > 0) {
      /*int stat_val; */
#if DEBUG
      printf ("Waiting for child..\n");
#endif
      for (p = 1; p <= procs; ++p) {
	static pid_t child_exit_status;
	child_exit_status = wait (NULL);
#if DEBUG
	printf ("PID %d done..\n", (int) child_exit_status);
#endif
      }

      stop_time = MSTime ();
      elapsed_time = stop_time - start_time;

      data_rate =
	(((long
	   long) rdwr_size * (long long) rdwr_iterations *
	  (long long) procs) / (long long) (elapsed_time));
      IOrate = ((long long)rdwr_iterations * (long long)procs * 1000) / (int) elapsed_time;

#ifndef _NEW_OUTPUT_FORMAT

      if (rwpercent == 100) {
	sprintf (buf, "%6ld byte read   ET=%9.3f secs  \
IOs/second = %5ld  Data Rate = %6.3f MBs\n", rdwr_size, (float) elapsed_time / 1000, IOrate, (double) data_rate / 1024);
      } else if (rwpercent == 0) {
	sprintf (buf, "%6ld byte write  ET=%7.3f secs  \
IOs/second = %5ld  Data Rate = %6.3f MBs\n", rdwr_size, (float) elapsed_time / 1000, IOrate, (double) data_rate / 1024);
      } else {
	sprintf (buf, "%6ld byte rdwr   ET=%7.3f secs  \
IOs/second = %5ld  Data Rate = %6.3f MBs\n", rdwr_size, (float) elapsed_time / 1000, IOrate, (double) data_rate / 1024);
      }

#else
      sprintf (buf, "%6ld\t%7.3f\t%6ld\t%7.3f\n", rdwr_size,
	       (float) elapsed_time / 1000, IOrate, (double) data_rate / 1024);
#endif

      printf ("%s", buf);
      if (do_log == 1) {
	logit (buf);
      }
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
/* drop the iteration count by 20% when the transfer size is doubled */
      rdwr_iterations = rdwr_iterations * 0.8;
      rdwr_size = rdwr_size * 2;	/* double the read size */
    }
  }
}

