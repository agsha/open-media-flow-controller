/* Copyright (c) 1996, 1997, 1998, 1999, 2000, 2001 by Solid Data Systems.
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
/* exercise.c  version 2.31                       */
/* INPUT:                                         */
/*   fd        file descriptor for device to test */
/*   blocks    # of blocks on the device to test  */
/* Richard Cooper 3/02/01                         */
/*                                                */
/*  This version does a write/read/compare sequence using one process.    */
/*  The locking code is experimental and is not used.                     */
/*  Errors are detected by comparing the data read back from the drive    */
/*  to the original data in the write buffer.  The bad block in the read  */
/*  buffer is dumped in HEX. The bad block is then re-read from the drive */
/*  and dumped in HEX a second time.                                      */
/************************************************************************/

#include "IOtest.h"

extern int error_flag;
extern void current_time_to_log (void);
extern unsigned long int swap (unsigned long int data_to_swap);
extern void dump (int fd, long long block_number_to_dump, int display_mode, int do_log);
void dump_buffer (int little_buffer[]);
void srand48 ();
double drand48 ();

int
exercise (int fd, char *rawdevice, long long blocks, int min_xfer_size,
	  int max_xfer_size, int do_log)
{
  static pid_t pid;
  struct range
  {
    int range_number;
    int start;
    int length;
  };
  static struct range ranges[EXER_PROCESSCOUNT];
  struct buffer
  {
    int rbuffer[MAX_TRANSFER_SIZE / 4];
    int wbuffer[MAX_TRANSFER_SIZE / 4];
  }
  buff[EXER_PROCESSCOUNT];

  int little_buffer[128];
  static int iterations;
  static long size;
  static int i, j, p = 1, iw = 0;
  static long write_block_count = 0;
  static long read_block_count = 0;
  static long int charsread, charswrote;

#ifdef DEBUG
  long write_min_start, write_max_start;
  long min_size, max_size;
#endif

  static long long random_block;
  static long max_xfer, min_xfer;	/* transfer sizes in blocks */

  min_xfer_size = (min_xfer_size > 0) ? min_xfer_size : MIN_TRANSFER_SIZE;
  max_xfer_size = (max_xfer_size > 0) ? max_xfer_size : MAX_TRANSFER_SIZE;

  if (do_log == 1) {
    sprintf (buf, "Starting exerciser on device %s at ", rawdevice);
    logit (buf);
    current_time_to_log ();
  }

  iterations = EXER_ITERATIONS;
  max_xfer = max_xfer_size / 512;
  min_xfer = min_xfer_size / 512;

/* Make sure we don't try to write off end... */
  blocks = blocks - (MAX_TRANSFER_SIZE / 512);

/* Don't exercise block "0". There might be a label there. Besides, there   */
/* are plenty of other blocks to test.                                      */
  ranges[0].start = 1;

  for (i = 0; i < EXER_PROCESSCOUNT; ++i) {
    ranges[i].range_number = i + 1;
    ranges[i].length = (int) blocks / EXER_PROCESSCOUNT;
    ranges[i + 1].start = (blocks / EXER_PROCESSCOUNT) * (i + 1) + 1;
  }

  for (i = 0; i < EXER_PROCESSCOUNT; ++i) {
    ranges[i].length = ranges[i].length - 150;
  }

/* Fill the write buffer with random data */
  srand48 ((unsigned int) time (NULL));
  for (i = 0; i < (int) (MAX_TRANSFER_SIZE / 4); ++i) {
    buff[0].wbuffer[i] = drand48 () * INT_MAX;
  }

  srand ((unsigned int) time (NULL) + getpid ());

#if DEBUG
  min_size = min_xfer_size + 1;
  max_size = max_xfer_size - 1;
#endif

  for (p = 0; p < EXER_PROCESSCOUNT; ++p) {
    pid = fork ();
    if (pid == 0)
      break;
  }

  switch (pid) {
  case -1:
    exit (EXIT_FAILURE);
  case 0:

#ifdef DEBUG
    write_min_start = ranges[p].start + 10000;
    write_max_start = ranges[p].start + (ranges[p].length - 10000);
#endif

    srand48 ((unsigned int) time (NULL));
    while (iw < iterations) {
      random_block = (drand48 () * ranges[p].length) + ranges[p].start;

#ifdef DEBUG
      if (write_min_start > random_block)
	write_min_start = random_block;
      if (write_max_start < random_block)
	write_max_start = random_block;
#endif

/*  generate random size for the transfer  */
      if (min_xfer_size != max_xfer_size) {
	size = 512 * ((rand () % ((max_xfer + 1) - min_xfer)) + 1) + (min_xfer_size - 512);
      } else {
	size = max_xfer_size;
      }

#if DEBUG
      if (min_size > size)
	min_size = size;
      if (max_size < size)
	max_size = size;
#endif

#ifdef _LARGEFILE64_SOURCE
      if ((lseek64 (fd, (off64_t) (random_block * 512), SEEK_SET)) < 0) {
	perror ("IOtest: lseek64");
	printf ("Seek didn't go to block %lld (on write)\n", (random_block));
	exit (EXIT_FAILURE);
      }
#else
      if ((lseek (fd, (long) (random_block * 512), SEEK_SET)) < 0) {
	perror ("IOtest: lseek");
	printf ("Seek didn't go to block %lld (on write)\n", (random_block * 512));
	exit (EXIT_FAILURE);
      }
#endif

      if ((charswrote = (long int) write (fd, buff[p].wbuffer, (size_t) size)) < 0) {
	printf ("Error during transfer of %ld bytes starting at block %Lu\n", size, random_block);
	perror ("IOtest:  write");
      }
      write_block_count = write_block_count + (size / 512);

#ifdef _LARGEFILE64_SOURCE
      if ((lseek64 (fd, (off64_t) (random_block * 512), SEEK_SET)) < 0) {
	perror ("IOtest: lseek64");
	printf ("Seek didn't go to block %lld (on read)\n", (random_block));
	exit (EXIT_FAILURE);
      }
#else
      if ((lseek (fd, (long) (random_block * 512), SEEK_SET)) < 0) {
	perror ("IOtest: lseek");
	printf ("Seek didn't go to block %lld (on read)\n", (random_block * 512));
	exit (EXIT_FAILURE);
      }
#endif

      if ((charsread = (long int) read (fd, buff[p].rbuffer, (size_t) size)) < 0) {
	printf ("Error during transfer of %ld bytes starting at block %Lu\n", size, random_block);
	perror ("IOtest: read");

      }
      read_block_count = read_block_count + (size / 512);



/*********************/
/*if ((memcmp(buff[p].wbuffer, buff[p].rbuffer, size)) != 0) { */
/*printf("memcmp error found\n");*/
/*}*/
/*********************/

/* compare the write and read buffers  */

      for (i = 0; i < (int) (size / 4); ++i) {
	if (buff[p].wbuffer[i] != buff[p].rbuffer[i]) {

	  while (p < 1) {	/*  sanity check. */
	    /*  only one process supported for the time being */

	    sprintf (buf, "\nError comparing write and read buffers\n");
	    printf ("%s", buf);
	    printf ("%ld block transfer starting at block number %lld\n", size / 512, random_block);
	    sprintf (buf,
		     "Error is in 32 bits starting at byte %d in block number %lld\n",
		     (i % 128) * 4, random_block + (i / 128));
	    printf ("%s", buf);
	    sprintf (buf,
		     "Expected data = %08lX  Received data = %08lX\n",
		     swap ((unsigned long int) buff[p].wbuffer[i]),
		     swap ((unsigned long int) buff[p].rbuffer[i]));
	    printf ("%s", buf);

	    for (j = 0; j <= 127; ++j) {
	      little_buffer[j] = buff[p].rbuffer[((i / 128) * 512 / 4) + j];
	    }

	    sprintf (buf, "\nHEX dump of block %lld --  read from read buffer\n",
		     random_block + (i / 128));
	    printf ("%s", buf);
	    dump_buffer (little_buffer);

	    printf ("\n\nRe-reading block number %lld\n", random_block + (1 / 128));
	    sprintf (buf, "HEX dump of block %lld -- read from %s\n", random_block + (i / 128),
		     rawdevice);
	    printf ("%s", buf);
	    dump (fd, random_block + (i / 128), 1, do_log);

	    printf ("\n\nDo you want to continue (yes,no [no])?  ");
	    (void) fgets (inbuf, INBUFLEN, stdin);
	    if (sscanf (inbuf, "%s", ans) == EOF) {
	      error_flag = 1;
	      iw = iterations;
	      return (error_flag);
	    } else if ((strcmp (ans, "yes") == 0)
		       || (strcmp (ans, "y") == 0)) {
	      error_flag = 0;
	      printf ("continuing...\n");
	      break;
	    } else if ((strcmp (ans, "no") == 0 || strcmp (ans, "n") == 0)) {
	      error_flag = 1;
	      iw = iterations;
	      /*printf ("error_flag = 1, exiting...\n"); */
	      exit (error_flag);
	    }
	  }
	}
      }
      iw++;			/* increment and do it again  */
    }

    if (write_block_count != read_block_count) {
      printf (" Big problem!!  The number of blocks read doesn't\n");
      printf (" match the number of blocks written!\n");
      exit (EXIT_FAILURE);
    }

    sprintf (buf, "%ld blocks written, read and compared\n", write_block_count);
    printf ("%s", buf);

#ifdef DEBUG
    printf ("min starting block = %ld  max starting block = %ld\n",
	    write_min_start, write_max_start);
    printf
      ("min xfer size = %ld blocks    max xfer size = %ld blocks\n",
       min_size / 512, max_size / 512);
    printf ("child pid = %d\n", (int) getpid ());
#endif

    exit (EXIT_SUCCESS);
  default:

#ifdef DEBUG
    printf ("parent process pid = %d\n", (int) getpid ());
#endif

    break;
  }

/*********************************************************************/
/* Wait for EXER_PROCESSCOUNT child processes to finish, then return */
/* EXER_PROCESSCOUNT should be 1.  Don't try to increase it yet!!    */
/*********************************************************************/

  for (p = 1; p <= EXER_PROCESSCOUNT; ++p) {
    static pid_t child_pid;
    child_pid = wait (&error_flag);
  }
  return (error_flag);
}


void
dump_buffer (int little_buffer[])
{
  int ncols, nrows, i, j, k = 0;
  extern int do_log;
/*  Print Byte numbers for columns and rows on screen   */
  sprintf (buf, "Byte  0 1 2 3  4 5 6 7  8 91011 12131415 16171819 20212223 24252627 28293031 \n");
  ncols = 8;
  nrows = 16;
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);

  sprintf (buf,
	   "----------------------------------------------------------------------------\n  0| ");
  printf ("%s", buf);
/*if (do_log == 1) logit(buf); */

/*  i = row number, j = column number, k = 32 bit integer number in block  */
  for (i = 0; i < nrows; ++i) {
    for (j = 0; j < ncols; ++j) {
      if (i == 25 && j == 3) {
	printf (" <--- byte 512");
	break;			/*  don't finish last row in ascii mode  */
      }
      little_buffer[(k + j)] = swap (little_buffer[(k + j)]);
      sprintf (buf, "%08lX ", (unsigned long int) little_buffer[(k + j)]);
      printf ("%s", buf);
      if (do_log == 1)
	logit (buf);
    }				/* end for ... ncols  */

    k = k + ncols;
    if ((k * 4) < 512)
      printf ("\n%3d| ", k * 4);
    sprintf (buf, "\n%3d| ", k * 4);
    if (do_log == 1)
      logit (buf);
  }				/* end for .. nrows  */

  printf ("\n");
}				/* end dump */
