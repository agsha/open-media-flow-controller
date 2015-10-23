/* Copyright (c) 1996-2001 by Solid Data Systems.
   *
   * Permission to use, copy, modify, and distribute this software for any
   * purpose with or without fee is hereby granted, provided that the above
   * copyright notice and this permission notice appear in all copies.
   * THE SOFTWARE IS PROVIDED "AS IS" AND SOLID DATA SYSTEMS DISCLAIMS
   * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
   * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL SOLID DATA
   * SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
   * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
   * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
   * THIS SOFTWARE.
 */

/******************************************************/
/*   maint.c    various maintenance functions V2.31   */
/*   INPUT:                                           */
/*   int fd        file descriptor for device to test */
/*   long blocks    # of blocks on the device to test */
/*   Richard Cooper  11/19/97                         */
/******************************************************/

/*************************************************************/
/*  Fixed a problem in the reliability test.  Transfer sizes */
/*  were being incremented by 512 bytes after each pass when */
/*  a transfer size less then 65536 was requested            */
/*  rbc 10/11/99                                             */
/*************************************************************/

/******************************************************************/
/*  Fixed problem with continuous write/scan block under Solaris  */
/*  6/1/00  rbc                                                   */
/******************************************************************/

#include "IOtest.h"
#include <termios.h>		/* get structure for termios */


extern void timeofday (void);
extern void current_time (void);
extern void current_time_to_log (void);
extern void read_block (int fd, long long block_number);
extern void dump_buffer (int little_buffer[]);
unsigned long int swap (unsigned long int data_to_swap);
void dump (int fd, long long block_number_to_dump, int display_mode, int do_log);

static unsigned long wbuffer[MAX_TRANSFER_SIZE / 4];
static unsigned long rbuffer[MAX_TRANSFER_SIZE / 4];
static int big_endian;
static int y = 1;

/************************************************/
/* Reliability  (Test 1)                        */
/*                                              */
/* Write zeros, read back, and compare.  Repeat */
/* with one's, aaaa's, and 5555's               */
/* transfer size = max_xfer_size                */
/* Set _ALL_SIZES = 1 for all transfer sizes    */
/************************************************/

void
reliability (int fd, long long blocks, int do_log, int min_xfer_size,
	     int max_xfer_size, char *rawdevice, int skip_block_zero)
{
  int little_buffer[128];
  static long charswrote;
#ifdef _REL_READ
  static long charsread;
#endif
  static long transfer_size = 0;
  static int pass_count;
  int j;
  int i;
  int all_transfer_sizes = 0;
  int min_transfer = 0, max_transfer = 0;
  unsigned long data[] = {
    0x00000000,
    0xFFFFFFFF,
    0xaaaaaaaa,
    0x55555555,
    0x0F0F0F0F,
    0xF0F0F0f0,
    0xFF00FF00,
    0x00FF00FF
  };

  int skip_zero = skip_block_zero;
  int PATTERNS;
  long long start_block;	/* starting block # for transfer  */


#ifdef PERCENT
  float percent = 0;
  int percent_incr = 5;
#endif

#ifdef _ALL_SIZES
  all_transfer_sizes = 1;
#endif

  sprintf (buf, "Starting reliability test on device %s at ", rawdevice);
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
  PATTERNS = sizeof (data) / 4;
  if (skip_block_zero == 1) {
    sprintf (buf, "Block zero(0) will not be tested\n");
    printf ("%s", buf);
    if (do_log == 1)
      logit (buf);
  }
  for (pass_count = 1; pass_count <= REL_PASS_COUNT; ++pass_count) {

    transfer_size = max_xfer_size;
    min_transfer = min_xfer_size;
    max_transfer = max_xfer_size;

    if (all_transfer_sizes == 1) {
      sprintf (buf, "Transfer sizes from %d to %d bytes will be used\n", min_transfer,
	       max_transfer);
      printf ("%s", buf);
      if (do_log == 1)
	logit (buf);
    } else
      min_transfer = max_transfer;
    for (transfer_size = min_transfer; transfer_size <= max_xfer_size; transfer_size += 512) {
      sprintf (buf, "\nTransfer size = %ld bytes", transfer_size);
      printf ("%s", buf);
      if (do_log == 1)
	logit (buf);

      for (j = 0; j < PATTERNS; ++j) {	/* fill write buffer  */

#ifdef PERCENT
	percent = 0;
#endif

	for (i = 0; i < (int) (transfer_size / 4); ++i) {
	  wbuffer[i] = data[j];
	}
/****************/
/* Writing ...  */
/****************/

	sprintf (buf, "\nwriting with 0x%08lx's       ", data[j]);
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	start_block = (long long) skip_zero;

	while (start_block < blocks) {

#ifdef PERCENT
	  if ((float)
	      ((float) start_block / (float) blocks * 100) >= percent) {
	    printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
	    percent = percent + percent_incr;
	    if ((int) percent == 100)
	      printf ("\b\b\b\b100%%");
	    if (fflush (stdout) == EOF) {
	      printf ("Error flushing stdout\n");
	      exit (EXIT_FAILURE);
	    }
	  }
#endif
#ifdef _LARGEFILE64_SOURCE
	  if ((lseek64 (fd, (off64_t) (start_block * 512), SEEK_SET))
	      < 0) {
	    perror ("IOtest: lseek64");
	    exit (EXIT_FAILURE);
	  }
#else
	  if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
	    perror ("IOtest: lseek");
	    exit (EXIT_FAILURE);
	  }
#endif
	  if ((charswrote = (long int) write (fd, wbuffer, (size_t) transfer_size)) < 0) {
	    perror ("IOtest: write");
	    exit (EXIT_FAILURE);
	  }
	  start_block = start_block + (transfer_size / 512);
	}
/*************************************************************/
/*  Reading and Comparing                                    */
/*************************************************************/
#ifdef PERCENT
	percent = 0;
#endif
#ifdef _REL_READ
	start_block = (long long) skip_zero;
	sprintf (buf, "\nreading and comparing 0x%08lx's        ", data[j]);
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	while (start_block < blocks) {
#ifdef PERCENT
	  if ((float)
	      ((float) start_block / (float) blocks * 100) >= percent) {
	    printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
	    percent = percent + percent_incr;
	    if ((int) percent == 100)
	      printf ("\b\b\b\b100%%");
	    if (fflush (stdout) == EOF) {
	      printf ("Error flushing stdout\n");
	      exit (EXIT_FAILURE);
	    }
	  }
#endif
#ifdef _LARGEFILE64_SOURCE
	  if ((lseek64 (fd, (start_block * 512), SEEK_SET)) < 0) {
	    perror ("IOtest: lseek64");
	    exit (EXIT_FAILURE);
	  }
#else
	  if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
	    perror ("IOtest: lseek");
	    exit (EXIT_FAILURE);
	  }
#endif
	  if ((charsread = (long int) read (fd, rbuffer, (size_t) transfer_size)) < 0) {
	    perror ("IOtest: read");
	    exit (EXIT_FAILURE);
	  }
	  /*  data read back is compared to the write buffer using "!="         */
	  /*  This should be done differently, keeping track of bit positions.  */

	  for (i = 0; i < (int) (transfer_size / 4); ++i) {
	    if (wbuffer[i] != rbuffer[i]) {

	      sprintf (buf, "\nError comparing write and read buffers\n");
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);
	      sprintf (buf, "block = %lld\n", start_block + (i / 128));
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);
	      sprintf (buf, "Error is in 32 bits starting at byte %d\n", (i % 128) * 4);
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);
	      sprintf (buf,
		       "Expected data = 0x%08lX  Received data = 0x%08lX\n",
		       swap (wbuffer[i]), swap (rbuffer[i]));
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);

	      for (j = 0; j <= 127; ++j) {
		little_buffer[j] = (int) rbuffer[((i / 128) * 512 / 4) + j];
	      }

	      sprintf (buf, "\nHEX dump of block %lld --  read from read buffer\n",
		       start_block + (i / 128));
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);
	      dump_buffer (little_buffer);
	      sprintf (buf, "\n\nRe-reading block number %lld\n", start_block + (i / 128));
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);

	      sprintf (buf, "HEX dump of block # %lld -- read from %s\n", start_block + (i / 128),
		       rawdevice);
	      printf ("%s", buf);
	      if (do_log == 1)
		logit (buf);
	      dump (fd, start_block + (i / 128), 1, do_log);
	      exit (EXIT_FAILURE);
	    }
	  }

	  start_block = start_block + (transfer_size / 512);
	}
#endif
      }

    }
    sprintf (buf, "\nEnd of pass %d on %s at ", pass_count, rawdevice);
    printf ("%s", buf);
    if (do_log == 1)
      logit (buf);
    timeofday ();
    current_time_to_log ();

  }
}

/****************************************/
/* Quick read  (Test 2)                 */
/*                                      */
/* Read device using MAX_TRANSFER_SIZE  */
/* transfers                            */
/****************************************/
void
quick_read (int fd, long long blocks, int max_xfer_size, int do_log)
{
  long long start_block = 0;
  long charsread;
  char rbuff[MAX_TRANSFER_SIZE];


#ifdef PERCENT
  float percent = 0;
  int percent_incr = 5;
#endif
  sprintf (buf, "Starting quick read at ");
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();

  while (start_block < (blocks - 1)) {
#ifdef PERCENT
    if ((float) ((float) start_block / (float) blocks * 100) >= percent) {
      printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
      percent = percent + percent_incr;
      if ((int) percent == 100)
	printf ("\b\b\b\b100%%");
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
    }
#endif
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (start_block * 512), SEEK_SET)) < 0) {
      printf (buf, "lseek64 error\n");
      printf ("%s", buf);
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
      printf (buf, "lseek error\n");
      printf ("%s", buf);
      exit (EXIT_FAILURE);
    }
#endif
    if ((charsread = (long int) read (fd, rbuff, (size_t) max_xfer_size)) < 0) {
      io_error ("read error");
    }

    if (charsread < max_xfer_size) {
      sprintf (buf, "\nDone!  %lld blocks read at ", start_block + (charsread / 512));
      printf ("%s", buf);
      if (do_log == 1)
	logit (buf);
      timeofday ();
      current_time_to_log ();
      return;
    }
    start_block = start_block + (max_xfer_size / 512);
  }
  sprintf (buf, "\nDone ! %lld blocks read at ", start_block);
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
  if (do_log == 1) {
    sprintf (buf, "Done! %lld blocks read at ", start_block);
    logit (buf);
  }
}


#ifdef _OLD_WRITE_BLOCK

/********************************************/
/* Write Blocks with block numbers (test 3) */
/********************************************/
void
write_block (int fd, long long blocks, int max_xfer_size)
{
#ifdef PERCENT
  float percent = 0;
  int percent_incr = 5;
#endif
  extern int do_log;
  int charswrote;
  int i = 0, j = 0, k = 0;
  long long start_block;
  /* long long where_it_went = 0; */
  sprintf (buf, "\nStarting write block at ");
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
  start_block = 0;
  while (start_block < blocks) {

#ifdef PERCENT
    if ((float) ((float) start_block / (float) blocks * 100) >= percent) {
      printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
      percent = percent + percent_incr;
      if ((int) percent == 100)
	printf ("\b\b\b\b100%%");
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
    }
#endif

/****************************************************************/
/* I'll document the next few lines later.  The process of      */
/* filling the wbuffer with the block numbers before writing    */
/* it out to the disk is fairly involved; and somewhat cryptic. */
/****************************************************************/


/*  k is the relative block number withing wbuffer[] being filled    */
/*  j = the (long int) number within the block.  There are 128 long  */
/*  int "j's" in a 512 byte block                                    */
/*  i = the (long int) number within wbuffer                         */

    k = 0;
    for (i = 0; i < (MAX_TRANSFER_SIZE / 4); i = i + 128) {
      for (j = 0; j < 128; ++j) {
	wbuffer[i + j] = (unsigned long int) start_block + k;
      }
      k = k + 1;
    }
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      exit (EXIT_FAILURE);
    }
#endif
    if ((charswrote = (int) write (fd, wbuffer, (size_t) max_xfer_size)) < 0) {
      perror ("IOtest: write");
      exit (EXIT_FAILURE);
    }
    start_block = start_block + (max_xfer_size / 512);
  }
  sprintf (buf, "\nDone!  %lld blocks written at ", blocks);
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
}
#endif


/********************************************/
/* Write Blocks with block numbers (E5 pattern */
/********************************************/
void
write_block_new (int fd, long long blocks, int max_xfer_size)
{

  static unsigned int mask1 = 0x000000FFu;
  static unsigned int mask2 = 0x0000FF00u;
  static unsigned int mask3 = 0x00FF0000u;

#ifdef PERCENT
  float percent = 0;
  int percent_incr = 5;
#endif

  extern int do_log;
  char wbuff[BUFF_SIZE];
  int charswrote;
  int i = 0, j;
  static long long start_block = 0;
  static long block_number = 0, bn = 0;
  sprintf (buf, "\nStarting write block at ");
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
  start_block = 0;

/*printf("START=%d END=%d\n",START,END);*/
  if (START > 0)
    start_block = START;
  if (END > 0)
    blocks = END;

  while ((start_block) < blocks) {

#ifdef PERCENT
    if ((float) ((float) start_block / (float) blocks * 100) >= percent) {
      printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
      percent = percent + percent_incr;
      if ((int) percent == 100)
	printf ("\b\b\b\b100%%");
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
    }
#endif

    bn = start_block / (PATT_SIZE / BLOCKSIZE);
    for (j = 0; j < (BUFF_SIZE / PATT_SIZE); ++j) {
      for (i = 0; i < (PATT_SIZE); i = i + NBYTES) {
	wbuff[i + (PATT_SIZE * j)] = bn & mask1;

/* mask second byte of block number, shift right, and */
/* put it in the next buffer location */
	wbuff[i + (PATT_SIZE * j) + 1] = (bn & mask2) >> 8;

/* do the same with the third byt */
	wbuff[i + (PATT_SIZE * j) + 2] = (bn & mask3) >> 16;
      }
      ++bn;
    }

#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      exit (EXIT_FAILURE);
    }
#endif
    if ((charswrote = (int) write (fd, wbuff, BUFF_SIZE)) < 0) {
      perror ("IOtest: write");
      exit (EXIT_FAILURE);
    }
    start_block = start_block + BUFF_SIZE / BLOCKSIZE;
    ++block_number;

  }
  sprintf (buf, "\nDone!  %lld blocks written at ", blocks);
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
}


#ifdef _OLD_WRITE_BLOCK
/***********************************/
/* Scan block numbers (Test 4)     */
/***********************************/
void
scan_blocks (int fd, long long blocks, int max_xfer_size)
{
#ifdef PERCENT
  float percent = 0;
  int percent_incr = 5;
#endif
  extern int do_log;
  int little_buffer[128];
  int i = 0, j = 0, k = 0;
  int charsread;
  long long start_block;

  start_block = 0;
  sprintf (buf, "Starting block number scan at ");
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
  while (start_block < blocks) {

#ifdef PERCENT
    if ((float) ((float) start_block / (float) blocks * 100) >= percent) {
      printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
      percent = percent + percent_incr;
      if ((int) percent == 100)
	printf ("\b\b\b\b100%%");
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
    }
#endif

    k = 0;
    /* fill write buffer with expected data   */
    for (i = 0; i < (MAX_TRANSFER_SIZE / 4); i = i + 128) {
      for (j = 0; j < 128; ++j) {
	wbuffer[i + j] = (unsigned long int) start_block + k;
      }
      k = k + 1;
    }
    /* Go get data from device and put it in rbuffer  */
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      exit (EXIT_FAILURE);
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      exit (EXIT_FAILURE);
    }
#endif
    if ((charsread = (int) read (fd, rbuffer, (size_t) max_xfer_size)) < 0) {
      perror ("IOtest: read");
      exit (EXIT_FAILURE);
    }
    for (i = 0; i < (charsread / 4); ++i) {
      /* compare wbuffer and rbuffer                   */
      if (wbuffer[i] != rbuffer[i]) {
	sprintf (buf, "\nError comparing expected and received data at ");
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	sprintf (buf, "block %lld\n", start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	sprintf (buf, "Error is in 32 bits starting at byte %d\n", (i % 128) * 4);
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	sprintf (buf,
		 "Expected data = %08lX  Received data = %08lX\n",
		 swap (wbuffer[i]), swap (rbuffer[i]));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);

	for (j = 0; j <= 127; ++j) {
	  little_buffer[j] = rbuffer[((i / 128) * 512 / 4) + j];
	}

	sprintf (buf, "\nHEX dump of block %lld --  read from read buffer\n",
		 start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	dump_buffer (little_buffer);

	sprintf (buf, "\n\nRe-reading block number %lld\n", start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);

	sprintf (buf, "HEX block dump of block %lld\n", start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	dump (fd, start_block + (i / 128), 1, do_log);
	/*read_block (fd, start_block + (i / 128)); */

	exit (EXIT_FAILURE);
      }
    }

    if (charsread < max_xfer_size) {

      sprintf (buf, "\nDone!  %lld blocks read at ", start_block + (charsread / 512));
      timeofday ();
      printf ("%s", buf);
      if (do_log == 1)
	logit (buf);
      current_time_to_log ();
      return;
    }
/*  Increment starting block # and do it again   */
    start_block = start_block + (max_xfer_size / 512);
  }
  printf ("\nDone!  %lld blocks scanned at ", blocks);
  timeofday ();
  if (do_log == 1) {
    sprintf (buf, "Done! block number scan finished at ");
    logit (buf);
    current_time_to_log ();
  }
}
#endif

/****************************************/
/*  scan_blocks_new()                   */
/****************************************/
void
scan_blocks_new (int fd, long long blocks, int max_xfer_size)
{
#ifdef PERCENT
  float percent = 0;
  int percent_incr = 5;
#endif
  static unsigned int mask1 = 0x000000FFu;
  static unsigned int mask2 = 0x0000FF00u;
  static unsigned int mask3 = 0x00FF0000u;

  extern int do_log;
  union bu
  {
    char wbuff[BUFF_SIZE];
    unsigned long wbuff2[BUFF_SIZE / 4];
  };
  union bu buffer;

  static unsigned long rbuff[BUFF_SIZE / 4];
  int little_buffer[128];
  int i = 0, j = 0;
  int charsread;
  long long start_block = 0;	/* This is the starting block number of the read */
  static long bn;

/*printf("START = %d END = %d\n",START,END);*/
  if (START > 0)
    start_block = START;
  if (END > 0)
    blocks = END;


  sprintf (buf, "Starting block number scan at ");
  printf ("%s", buf);
  if (do_log == 1)
    logit (buf);
  timeofday ();
  current_time_to_log ();
  while (start_block < blocks) {

#ifdef PERCENT
    if ((float) ((float) start_block / (float) blocks * 100) >= percent) {
      printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) blocks * 100));
      percent = percent + percent_incr;
      if ((int) percent == 100)
	printf ("\b\b\b\b100%%");
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
    }
#endif

    bn = start_block / (PATT_SIZE / BLOCKSIZE);
    for (j = 0; j < (BUFF_SIZE / PATT_SIZE); ++j) {
      for (i = 0; i < (PATT_SIZE); i = i + NBYTES) {
	buffer.wbuff[i + (PATT_SIZE * j)] = bn & mask1;
	buffer.wbuff[i + (PATT_SIZE * j) + 1] = (bn & mask2) >> 8;
	buffer.wbuff[i + (PATT_SIZE * j) + 2] = (bn & mask3) >> 16;
      }
      ++bn;
    }

#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      exit (EXIT_FAILURE);
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (start_block * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      exit (EXIT_FAILURE);
    }
#endif
    if ((charsread = (int) read (fd, rbuff, (size_t) BUFF_SIZE)) < 0) {
      perror ("IOtest: read");
      exit (EXIT_FAILURE);
    }

    for (i = 0; i < (charsread / 4); ++i) {
      /* compare wbuffer and rbuffer                   */
      if (buffer.wbuff2[i] != rbuff[i]) {
	sprintf (buf, "\nError comparing expected and received data at ");
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	sprintf (buf, "block %lld\n", start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	sprintf (buf, "Error is in 32 bits starting at byte %d\n", (i % 128) * 4);
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	sprintf (buf,
		 "Expected data = %08lX  Received data = %08lX\n",
		 swap (buffer.wbuff2[i]), swap (rbuff[i]));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);


	for (j = 0; j <= 127; ++j) {
	  little_buffer[j] = buffer.wbuff2[((i / 128) * 512 / 4) + j];
	}
	sprintf (buf, "\nHEX dump of block %lld --  read from write buffer\n",
		 start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	dump_buffer (little_buffer);

	for (j = 0; j <= 127; ++j) {
	  little_buffer[j] = rbuff[((i / 128) * 512 / 4) + j];
	}
	sprintf (buf, "\nHEX dump of block %lld --  read from read buffer\n",
		 start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	dump_buffer (little_buffer);

	sprintf (buf, "\n\nRe-reading block number %lld\n", start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);

	sprintf (buf, "HEX block dump of block %lld\n", start_block + (i / 128));
	printf ("%s", buf);
	if (do_log == 1)
	  logit (buf);
	dump (fd, start_block + (i / 128), 1, do_log);

/*read_block (fd, start_block + (i / 128)); */

	exit (EXIT_FAILURE);
      }
    }

    if (charsread < BUFF_SIZE) {

      sprintf (buf, "\nDone!  %lld blocks read at ", start_block + (charsread / 512));
      printf ("%s", buf);
      if (do_log == 1)
	logit (buf);
      timeofday ();
      current_time_to_log ();
      return;
    }

/*  Increment starting block # and do it again   */

    start_block = start_block + BUFF_SIZE / BLOCKSIZE;

  }
  printf ("\nDone!  %lld blocks scanned at ", blocks);
  timeofday ();
  if (do_log == 1) {
    sprintf (buf, "Done! block number scan finished at ");
    logit (buf);
    current_time_to_log ();
  }
}

#ifdef _OLD_WRITE_BLOCK
/************************************************/
/*  Test 5  continuous write block / scan block */
/************************************************/
void
write_scan_blocks (int fd, long long blocks, int max_xfer_size)
{
  while (1) {
    write_block (fd, blocks, max_xfer_size);
    scan_blocks (fd, blocks, max_xfer_size);
  }
}
#endif

/************************************************/
/*  Test 11  continuous write block / scan block (new)  */
/************************************************/
void
write_scan_blocks_new (int fd, long long blocks, int max_xfer_size)
{
  while (1) {
    write_block_new (fd, blocks, max_xfer_size);
    scan_blocks_new (fd, blocks, max_xfer_size);
  }
}

/*******************************************************************/
/*  Dump block number (Test 6)                                     */
/*******************************************************************/
void
dump_block (int fd, long long blocks, int do_log)
{
  int i, j;
  long long block_number_to_dump = 0;
  char line[10];
  char character;
  static int display_mode = 1;

/*******************************************************************/
/* The following lines save the terminal settings and then put the */
/* terminal in "raw" mode.  Characters are passed to the program   */
/* immediately without waiting for a "\n" (unbuffered mode)        */
/* This allows you to press "n" to get next block without a return */
/*******************************************************************/
  static struct termios initial_settings;
  static struct termios new_settings;

  for (j = 0; j <= 10; ++j)	/* zero out line buffer  */
    line[j] = (char) 0x0;

  if ((tcgetattr (0, &initial_settings)) < 0) {
    printf ("tcgetattr error\n");
    exit (EXIT_FAILURE);
  }
  new_settings = initial_settings;
  new_settings.c_lflag &= ~ICANON;	/*  turn off canonical mode   */
  new_settings.c_cc[VMIN] = (cc_t) 1;
  new_settings.c_cc[VTIME] = (cc_t) 0;
  new_settings.c_lflag &= ~ISIG;

  if (tcsetattr (0, TCSANOW, &new_settings) != 0) {
    fprintf (stderr, "could not set attributes\n");
  }
/*  Print header and go get block number to test  */
/*  Do a couple of sanity checks                  */
/*****************************************************************/
/*  The next line gets one character from stdin; and ignores it  */
/*  For some reason stdin has a character "waiting".  If you     */
/*  don't do this, the main loop sees a request for block "0"    */
/*  the first time through.                                      */
/*****************************************************************/
  /* character = (char) getchar(); */

  printf ("\nHEX block dump  \"q\" to quit\n");
  printf ("next block = \"n\"  previous block = \"p\"\n");
  printf ("Display in HEX = \"h\", ASCII = \"a\"\n");

  while ((int) block_number_to_dump >= -1) {
    printf ("Enter block number (0 to %lld):  ", blocks - 1);

    i = 0;			/* reset character counter for next block # request */
    while (1) {
      character = (char) getchar ();	/* get character from stdin */
      if (character == 'n') {	/* next block?? */
	if (block_number_to_dump < (blocks - 1))
	  block_number_to_dump++;
	break;
      }
      if (character == 'p') {	/* previous block?? */
	if (block_number_to_dump >= 1)
	  block_number_to_dump--;
	break;
      }
      if (character == 'h') {	/* Display in HEX???? */
	display_mode = 1;
	break;
      }
      if (character == 'a') {	/* Display in ascii?? */
	display_mode = 0;
	break;
      }
      if (character == 'q') {	/* quit?? */
	if ((tcsetattr (0, TCSANOW, &initial_settings)) < 0) {
	  printf ("tssetattr error\n");
	  exit (EXIT_FAILURE);
	}
	return;
      }
      if (character == '-') {	/* quit?? */
	if ((tcsetattr (0, TCSANOW, &initial_settings)) < 0) {
	  printf ("tssetattr error\n");
	  exit (EXIT_FAILURE);
	}
	return;
      }
      if (character == '\n') {	/* some one hit return */
	block_number_to_dump = (unsigned int) atoi (line);
	for (j = 0; j <= 10; ++j)
	  line[j] = (char) 0x0;	/*zero buffer */
	break;
      }
      line[i] = character;	/* one character of a block # */
      ++i;			/* ++i, and go get next character */
    }

/*  We have a block #!! Check it against size of device  */

    if (block_number_to_dump < 0) {	/* negative block # - exit */
      if ((tcsetattr (0, TCSANOW, &initial_settings)) < 0) {	/* reset terminal */
	printf ("tcsetattr error\n");
	exit (EXIT_FAILURE);
      }
      break;
    }
    if (block_number_to_dump >= blocks) {
      printf ("Block number too large for this device. \n");
      if ((tcsetattr (0, TCSANOW, &initial_settings)) < 0) {
	printf ("tcsetattr error\n");
	exit (EXIT_FAILURE);
      }
      break;
    }
/*  Valid block #  dump it to screen  */

    sprintf (buf, "\nBlock number %lld\n", block_number_to_dump);
    printf ("%s", buf);
    if (do_log == 1)logit(buf);
    dump (fd, block_number_to_dump, display_mode, do_log);  /* dump the block to the screen in HEX */
  }
}

/********************************************************************/
/*  Test 7                                                          */
/* Write a byte. This function will prompt for a block #, byte #,   */
/* and one byte of data.  The block number is read, the data at the */
/* byte location is changed, and the block is written back to the   */
/* device.                                                          */
/* WARNING WARNING!! There are no limits to the amount of damage    */
/* you can cause with this test!!!                                  */
/********************************************************************/
void
write_a_byte (int fd, long long blocks, int do_log)
{
  static int byte = 1, i = 0, j = 0;
  static int data_byte;
  static int charsread, charswrote;
  static long long block_number_to_dump = 0;
  static char line[25];
  static char character;
  static char wbuff[512];
  static int display_mode;
  for (j = 0; j <= 25; ++j)	/*  fill buffer with 0's  */
    line[j] = (char) 0x0;

  sprintf (buf, "Write a byte // Enter `q` to quit.\n");
  printf ("%s", buf);
  if (do_log == 1)logit(buf);
  while (1) {
    i = 0;			/* make sure i=0 if going through loop second time */
    sprintf (buf, "\nEnter block number (0 to %lld): ", blocks - 1);
    printf ("%s", buf);
    if (do_log == 1) {
      logit (buf);
      sprintf (buf, "\n");
      logit (buf);
    }
/*****************************************************************/
/* use getchar to get block number so we can check for "\n" only */
/*****************************************************************/
    while (1) {
      character = (char) getchar ();
      if ((character == '\n') && (i == 0)) {	/* hit return only       */
	/* use last block number */
	sprintf (buf, "\nBlock number = %lld\n", block_number_to_dump);
	if (do_log == 1) {
	  printf ("%s", buf);
	  logit (buf);
	}
	for (j = 0; j <= 25; ++j)
	  line[j] = (char) 0x0;
	break;
      }

      if (character == 'q') {
	return;			/* quit?? */
}
      if (character == '\n') {
	block_number_to_dump = (unsigned int) atoi (line);
	for (j = 0; j <= 25; ++j)
	  line[j] = (char) 0x0;
	sprintf (buf, "\nBlock number = %lld\n", block_number_to_dump);
	printf ("%s", buf);
	if (do_log == 1)logit(buf);
	break;
      }
      line[i] = character;
      ++i;
    }
    if (block_number_to_dump < 0)
      break;
    if (block_number_to_dump >= blocks) {
      sprintf (buf, " Block number too large for this device.\n");
      printf ("%s", buf);
      if (do_log == 1)logit(buf);
      break;
    }
/****************************************************/
/* Dump the block requested so we can look at bytes */
/****************************************************/
display_mode = 1;  /* default to HEX */
dump (fd, block_number_to_dump, display_mode, do_log);
/*  Go get byte number within block to modify  */
    while (1) {
      sprintf (buf, "\nEnter byte number to change (0-511): ");
      printf ("%s", buf);
      if (do_log == 1)logit(buf);
      if (scanf ("%d", &byte) == EOF) {
	printf ("scanf error\n");
	exit (EXIT_FAILURE);
      }
      if ((0 <= byte) && (byte <= 511))
	break;
    }
    if (do_log == 1) {
      sprintf (buf, " %d\n", byte);
      logit (buf);
    }

/*  Get new data */
    while (1) {
      sprintf (buf, "\nEnter byte (HEX 00-FF):  ");
      printf ("%s", buf);
      if (do_log == 1)logit(buf);
      if (scanf ("%X", &data_byte) == EOF) {
	printf ("scanf error\n");
	exit (EXIT_FAILURE);
      }
      if ((0 <= data_byte) && (data_byte <= 255))
	break;
    }
    if (do_log == 1) {
      sprintf (buf, " %X\n", data_byte);
      logit (buf);
    }
    sprintf (buf, "\nBlock number = %lld\n", block_number_to_dump);
    printf ("%s", buf);
    if (do_log == 1)logit(buf);

/* Go read the requested block    */

#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (block_number_to_dump * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (block_number_to_dump * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      exit (EXIT_FAILURE);
    }
#endif
    if ((charsread = (int) read (fd, wbuff, 512)) < 0) {
      perror ("IOtest: read");
      exit (EXIT_FAILURE);
    }
/********************************************************/
/* See if big_endian flag is set and adjust accordingly */
/********************************************************/
    if (*(char *) &y == 1)
      big_endian = 0;		/* little endian,  Intel, VAX etc.  */
    else
      big_endian = 1;		/* big endian,  most UNIX systems   */
    if (big_endian == 0) {	/* little endian? if so, change the byte */
      wbuff[byte] = (char) data_byte;
    } else {			/* big endian, put the new byte in the right place */
      switch (byte % 4) {
      case 0:
	wbuff[byte + 3] = (char) data_byte;
	break;
      case 1:
	wbuff[byte + 1] = (char) data_byte;
	break;
      case 2:
	wbuff[byte - 1] = (char) data_byte;
	break;
      case 3:
	wbuff[byte - 3] = (char) data_byte;
	break;
      }
    }

/*******************************************/
/* write modified block back out to device */
/*******************************************/
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (block_number_to_dump * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (block_number_to_dump * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      exit (EXIT_FAILURE);
    }
#endif
    if ((charswrote = (int) write (fd, wbuff, 512)) < 0) {
      perror ("IOtest: write");
      exit (EXIT_FAILURE);
    }
    dump (fd, block_number_to_dump, display_mode, do_log);	/* dump out the changed block  */

    character = (char) getchar ();
  }				/* go back and get next block number */
}

/*************************************************************************/
void
read_block (int fd, long long block_number)
{
  static unsigned long int rbuf[512 / 4];
 /* extern long long block_number; */
  static int bytes_read;
  static int i;
  for (i = 1; i <= 500; ++i) {
/*while(1) { */
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (block_number * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek64");
      printf ("Seek didn't go to block %lld (on write)\n", (block_number));
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (block_number * 512), SEEK_SET)) < 0) {
      perror ("IOtest: lseek");
      printf ("Seek didn't go to block %lld (on write)\n", (block_number));
      exit (EXIT_FAILURE);
    }
#endif

    if ((bytes_read = (long int) read (fd, rbuf, 512)) < 0) {
      printf ("Error during read of 512 bytes starting at block %Lu\n", block_number);
      perror ("IOtest: read");

    }
  }
}


/**************************************************************************/
/*************************************************************************/
/*DUMP */
void dump (int fd, long long block_number_to_dump, int display_mode, int do_log)
/*  Dump the requested block on screen in HEX or ASCII */
/*  if display_mode = 1, dump in hex    */
/*  if display_mode = 0, dump in ascii  */
{
  static int ncols, nrows;
  static int i, j, jj, k = 0, charsread, x1;
  static unsigned long int rbuf[512 / 4];	/* place to put data */
  static char *x, buff[5];
  static unsigned long int mask = 0x000000FFu;	/* mask for first byte */

/*  Seek to block and read 512 bytes    */
#ifdef _LARGEFILE64_SOURCE
  if ((lseek64 (fd, (off64_t) (block_number_to_dump * 512), SEEK_SET)) < 0) {
    perror ("IOtest: lseek64");
    exit (EXIT_FAILURE);
  }
#else
  if ((lseek (fd, (long) (block_number_to_dump * 512), SEEK_SET)) < 0) {
    perror ("IOtest: lseek");
    exit (EXIT_FAILURE);
  }
#endif
  if ((charsread = (int) read (fd, rbuf, 512)) < 0) {
    perror ("IOtest: read");
    exit (EXIT_FAILURE);
  }
/*  Print Byte numbers for columns and rows on screen   */

  if (display_mode == 1) {

    sprintf (buf,
	     "Byte  0 1 2 3  4 5 6 7  8 91011 12131415 16171819 20212223 24252627 28293031 \n");
    ncols = 8;
    nrows = 16;
  } else {
    sprintf (buf, "Byte   0  1  2  3   4  5  6  7   8  9 10 11  12 13 14 15  16 17 18 19 \n");
    ncols = 5;
    nrows = 26;
  }
  printf ("%s", buf);
  if (do_log == 1)logit(buf);

  sprintf (buf,
	   "----------------------------------------------------------------------------\n  0| ");
  printf ("%s", buf);
  if (do_log == 1)logit(buf);
/*  i = row number, j = column number, k = 32 bit integer number in block  */

  k=0;
  for (i = 0; i < nrows; ++i) {
    for (j = 0; j < ncols; ++j) {
      if (i == 25 && j == 3) {
	printf (" <--- byte 512");
	break;			/*  don't finish last row in ascii mode  */
      }
      rbuf[k + j] = swap (rbuf[k + j]);
      if (display_mode == 1) {	/* HEX  */
	sprintf (buf, "%08lX ", (unsigned long int) rbuf[k + j]);
	printf ("%s", buf);
	if (do_log == 1)logit(buf);
      } else {			/*  ASCII  */
	rbuf[k + j] = swap ((unsigned long int) rbuf[k + j]);
	for (jj = 1; jj <= 4; ++jj) {
	  x1 = (short int) (rbuf[k + j] & mask);	/*first byte */

	  switch (x1) {
	  case '\0':
	    x = " \\0";
	    break;
	  case '\007':
	    x = " \\a";
	    break;
	  case '\b':
	    x = " \\b";
	    break;
	  case '\f':
	    x = " \\f";
	    break;
	  case '\n':
	    x = " \\n";
	    break;
	  case '\r':
	    x = " \\r";
	    break;
	  case '\t':
	    x = " \\t";
	    break;
	  case '\v':
	    x = " \\v";
	    break;
	  default:
	    sprintf (buff, "  %c", (char) x1);
	    x = (char *) buff;
	  }			/* end switch  */
	  printf ("%3s", x);	/* print it as ascii character */
	  rbuf[k + j] = (unsigned int) rbuf[k + j] >> 8;	/* shift the buffer right 8 bits and do it again */

	}			/* end for .. jj  */

	printf (" ");

      }				/* end else {  */

    }				/* end for ... ncols  */

    k = k + ncols;

    if ((k * 4) < 512) {
      sprintf (buf,"\n%3d| ", k * 4);
     printf("%s",buf);
    if(do_log == 1)logit(buf);
 } 
}				/* end for .. nrows  */

  sprintf (buf, "\n");
  printf ("%s", buf);
  if (do_log == 1)logit(buf);
}				/* end dump */


unsigned long int
swap (unsigned long int data_to_swap)
{

/***********************************************************/
/* In data_to_swap, bytes are arranged 04030201 This swaps */
/* bytes and returns them as 01020304                      */
/***********************************************************/

  unsigned int x;		/* x marks the spot, it's on the floor to left of your desk */
  unsigned int mask1 = 0x000000FFu;	/* mask for first byte    */
  unsigned int mask2 = 0x0000FF00u;	/* mask for second  byte  */
  x = (mask1 & data_to_swap);
  x = x << 16;
  x = x | (mask2 & data_to_swap);
  x = x << 8;
  data_to_swap = data_to_swap >> 8;
  x = x | (mask2 & data_to_swap);
  data_to_swap = data_to_swap >> 16;
  x = x | (mask1 & data_to_swap);
  return (unsigned long int) (x);
}

/**********************************************************************/
void
set_fl (int fd, int flags)
{
  int val;
  if ((val = fcntl (fd, F_GETFL, NULL)) < 0)
    printf ("fcntl F_GETFL error");
  val |= flags;
  if (fcntl (fd, F_SETFL, val) < 0)
    printf ("fcnt; F_SETFL error");
}

/***********************************************************************/
void
flags (int fd)
{
  int accmode, val;
  if ((val = fcntl (fd, F_GETFL, NULL)) < 0)
    printf (" error on fd %d\n", fd);
  accmode = val & O_ACCMODE;
  if (accmode == O_RDONLY)
    printf ("read only");
  else if (accmode == O_WRONLY)
    printf ("write only");
  else if (accmode == O_RDWR)
    printf ("read write");
  else
    printf ("unknown access mode");
  if ((val & O_APPEND) == 1)
    printf (", append");
  if ((val & O_NONBLOCK) == 1)
    printf (", nonblocking");
/*#if !defined(_POSIX_SOURCE) && defined(O_SYNC) */
  if ((val & O_SYNC) == 1)
    printf (", sync writes");
/*#endif */
  if (putchar ('\n') == EOF) {
    printf ("putchar error\n");
    exit (EXIT_FAILURE);
  }
}
