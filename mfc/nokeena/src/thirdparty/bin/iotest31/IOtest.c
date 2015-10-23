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

/* DES I/O TESTS  */
/* Version 2.0   Dim.c  George Tuma/Dave Wright  1994/5/6  */
/* corrected long device name problem */
/* corrected BofA math problem */
/* added millisecond resolution on clock */

/* Version 2.07 */
/* Added exerciser package, write benchmark, basic maintenance mode tests */
/* added logging, command line args, variable process counts              */
/* Added date/time and basic system information printout.  */
/* Iteration count now drops as read/write size increases */
/* Changed how the device is sized */
/* Added -s option to specify the device size */
/* Richard Cooper   11/19/97 */

/*  Version 2.08 */
/*  Most of the maintenance functions now work.   Need to be */
/*  be able to go to next block/previous block.              */
/*  rbc  12/23/97      */

/*  Version 2.09                                   */
/*  Fixed problem with reliability                 */
/*  re-named to IOtest                             */
/*  HEX dump now works !!                          */
/*  Added Byte editor under maintenance functions  */
/*  rbc  12/29/97                                  */

/*  Version 2.10                                   */
/*  Added check for big/little endian.             */
/*  Fixed an error reporting problem with the scan */
/*  maintenance function                           */
/*  Added SYNC_WRITES definition in IOtest.h to    */
/*  force synchronous IO on writes                 */
/*  rbc 1/6/98                                     */

/*  Version 2.11                                   */
/*  moved benchmarks to benchmks.c                 */
/*  fixed bug in getsize() that showed up when     */
/*  drivesize was close to (INT_MAX/512)           */
/*  dozens of small syntax changes, mostly to keep */
/*  compilers and LClint happy                     */
/*  Added "switch" to benchmks.c                   */
/*  Added "-i" to specify iteration count          */
/*  rbc  3/20/98                                   */

/*  Version 2.15                                   */
/*  Added "-a XX" option for random read/write     */
/*  benchmark.                                     */
/*  Changed data rate output from KB's to MB's     */
/*  Several minor changes to keep HPUX ANSI C      */
/*  compiler quiet.                                */
/*  Changed how the exercise function adjusts the  */
/*  iterations count.                              */
/*  Changed "usecs" to "secs" in benchmark test    */
/*  output.                                        */

/*  Version 2.16                                   */
/*  changed when random number generator is seeded. */
/*  Changed read/write BM to single process.       */
/*  Added  _CRYPTIC compile option                 */
/*  time options.                                  */

/*  Version 2.22                                   */
/*  added ascii mode to block dump                 */
/*  optional shared memory for rbuffer             */

/*  Version 2.23                                   */
/*  Got rid of "-g" in configure                   */
/*  changed function declaration for gettimeofday  */
/*  in IOtest.c at line 101                        */

/*  Version 2.26                                   */
/* changed rawdevice[20] to rawdevice[50]          */
/* Added fsync and sleep                           */
/* added additional logging                        */

/*  Version 2.30                                   */
/*  Added menu, complete re-org of IOtest.c        */
/*  Added support for > 2.1 GB devices             */


/*  Version 2.31                                       */
/*  fixed write/scan block problem under Solaris        */
/*  Added -D_LOOP_BM to loop benchmark tests            */
/*  Added -D_DO_QUICKIE for installation verification   */
/*  Added -D_DO_BDRS to send BDR under HPUX             */
/*  Fixed data rate bug in benchmark tests              */
/*  Fixed Soalris 2.5.1 compile                         */
/*  Changed exerciser to do a write-read-compare        */
/*  Fixed problem with incomplete benchmark output      */
/*  under HPUX                                          */
/*  rbc 1/3/01                                          */


#include "IOtest.h"
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>

#ifdef _DO_BDRS
#include <sys/diskio.h>
#include <sys/scsi.h>
#endif


/* FUNCTION DECLARATIONS */

#if !defined(_SGI) && !defined(_AIX)
int gettimeofday ();
#endif

/*#ifdef _SGI */
/*int gettimeofday __P((struct timeval * __tv, struct timezone * __tz)); */
/*#endif */

#ifdef __STDC__
static void current_time (void);
void current_time_to_log (void);
void timeofday (void);
static void startup (void);	/* most of these are only called once... */
static long long getsize (int fd);
void random_rdwr_mp (int fd, long long blocks, int procs, int min_xfer_size,
		     int max_xfer_size, int rdwr_iterations, int rwpercent,
		     int do_log, int skip_block_zero);
static void open_device (void);
static void close_device (int fd);
static void close_log (void);
void sig_handler (int);
#ifdef _DO_BDRS
static void do_bdr (int fd);
#endif
#ifdef _DO_QUICKIE
static void do_quickie (int fd);
#endif
int exercise (int fd, char *rawdevice, long long blocks, int min_xfer_size,
	      int max_xfer_size, int do_log);
void reliability (int fd, long long blocks, int do_log, int min_xfer_size,
		  int max_xfer_size, char *rawdevice, int skip_block_zero);
#ifdef _OLD_WRITE_BLOCK
void write_block (int fd, long long blocks, int max_xfer_size);
void scan_blocks (int fd, long long blocks, int max_xfer_size);
void write_scan_blocks (int fd, long long blocks, int max_xfer_size);
#endif

void quick_read (int fd, long long block, int max_xfer_size, int do_log);
void write_block_new (int fd, long long blocks, int max_xfer_size);
void scan_blocks_new (int fd, long long blocks, int max_xfer_size);
void write_scan_blocks_new (int fd, long long blocks, int max_xfer_size);

void dump_block (int fd, long long blocks,int do_log);
void read_block (int fd, long long block_number);
void write_a_byte (int fd, long long blocks, int do_log);
void set_fl (int fd, int flags);
void flags (int fd);
static void ask_questions (void);
static int ask_write (void);
static int ask_block0 (void);
static void quick_test (void);
static void run_exerciser (void);
static int maintenance_test (void);
static void benchmarks (void);
static void open_log (void);
#else
static void current_time ();
void current_time_to_log ();
void timeofday ();
static void startup ();		/* most of these are only called once... */
static long long getsize ();
random_rdwr_mp ();
static void open_device ();
static void close_device ();
static void close_log ();
void sig_handler ();
static void do_bdr ();
static void do_quickie ();
int exercise ();
void reliability ();
void scan_blocks ();
void write_scan_blocks ();
void write_scan_blocks_new ();
void quick_read ();
void write_block ();
void dump_block ();
void write_a_byte ();
void set_fl ();
void flags ();
void ask_questions ();
int ask_write ();
int ask_block0 ();
void quick_test ();
void run_exerciser ();
void maintenance_test ();
void benchmarks ();
void open_log ();
#endif

typedef int bool;

int min_xfer_size = MIN_TRANSFER_SIZE;
int max_xfer_size = MAX_TRANSFER_SIZE;
long min, max;
static int fd, ret;		/* file descriptor */
static int pass_count;
static bool do_write, do_read, do_readwrite, do_exercise, do_maint;
static bool do_menu, do_quick, sync_writes = 0;
static bool loop_benchmark = 0, do_install_test;
static int procs, minp = MIN_PROC_COUNT, maxp, defp = DEF_PROC_COUNT;
static int maxp2, test_number, rwpercent;
static int itercount, iterations, dpercent=100;
static int read_iterations, write_iterations;
static int rdwr_iterations;
int error_flag;
int fp_pos, do_log, skip_block_zero = 1;
static long long blocks = 0;
long long block_number;
char rawdevice[50];
static char character, log_file_name[50] = "";

/*static char usage[] = "Usage: IOtest -mrwx -a XX -d rawdevice [-f logfile] [-p process cnt] [-s # blocks] [-i iterations]\n"; */

#ifdef _NO_ASK_ABOUT_WRITE
static bool no_ask_about_write;
#endif

#ifdef _DO_BDRS
static int bdr_sleeptime;
#endif

static struct utsname uts;
static struct stat stbuf;
static struct timeval currentTime;

pid_t fork (void);

static FILE *fp_log;

void
usage (void)
{
  printf("Usage: IOtest [options]\n");
  printf("  -d [device name]\n");
  printf("  -f [log file name]\n");
  printf("  -h : help\n");
  printf("  -i [iterations]\n");
  printf("  -p [max processes]\n");
  printf("  -b [block size]\n");
  printf("  -q : don't ask question about writing\n");
  printf("  -r : do read only test\n");
  printf("  -a [read/write percentage]\n");
  printf("  -z [ZBR percentage]\n");
  printf("  -w : do write only test\n");
  printf("  -x : do exercise with sync writes\n");
  printf("  -m : do maintence with sync writes\n");
}

int
main (int argc, char *argv[])
{
  char **av = argv;
  if (argc > 1) {
    argc--, av++;		/*go to first argument */
    while (argc && *av[0] == '-') {
      while (*++av[0] > 0)
	switch (*av[0]) {
	case 'd':
	  argc--, av++;
	  strcat (rawdevice, av[0]);
	  /*printf("rawdevice= %s\n",rawdevice); */
	  goto next;
	case 'f':
	  argc--, av++;
	  strcat (log_file_name, av[0]);
	  goto next;
	case 'h':
	  usage();
	  exit (EXIT_SUCCESS);
	case 'i':
	  argc--, av++;
	  itercount = atoi (av[0]);
	  if (itercount < MIN_ITER_COUNT || itercount > MAX_ITER_COUNT) {
	    printf ("iteration count out of range (%d-%d)\n", MIN_ITER_COUNT, MAX_ITER_COUNT);
	    exit (EXIT_SUCCESS);
	  }
	  goto next;
	case 'p':
	  argc--, av++;
	  maxp = atoi (av[0]);
	  if (maxp < 1 || maxp > MAX_PROC_COUNT) {
	    printf ("max process count out of range (1-%d)\n", MAX_PROC_COUNT);
	    exit (EXIT_SUCCESS);
	  }
	  minp = maxp;
	  goto next;
	case 'b':
	  argc--, av++;
	  min_xfer_size = atoi (av[0]);
	  if (min_xfer_size < MIN_TRANSFER_SIZE || min_xfer_size > MAX_TRANSFER_SIZE) {
	    printf ("block size out of range (%d-%d)\n", MIN_TRANSFER_SIZE, MAX_TRANSFER_SIZE);
	    exit (EXIT_SUCCESS);
	  }
	  max_xfer_size = min_xfer_size;
	  goto next;

#ifdef _NO_ASK_ABOUT_WRITE
	case 'q':
	  no_ask_about_write = 1;
	  break;
#endif
	case 'r':
	  do_read = 1;
	  break;
	case 'a':
	  argc--, av++;
	  rwpercent = atoi (av[0]);
	  if (rwpercent < 5 || rwpercent > 95) {
	    printf ("read/write percent out of range (0-90)\n");
	    exit (EXIT_SUCCESS);
	  }
	  do_readwrite = 1;
	  break;
	case 'z':
	  argc--, av++;
	  dpercent = atoi (av[0]);
	  if (dpercent < 1 || dpercent > 95) {
	    printf ("zbr percent out of range (1-95)\n");
	    exit (EXIT_SUCCESS);
	  }
	  break;
	case 'w':
	  do_write = 1;
	  break;
	case 'x':
	  do_exercise = 1;
	  sync_writes = 1;
	  break;
	case 'm':
	  do_maint = 1;
	  sync_writes = 1;
	  break;

	default:
	  ;
	}
    next:
      argc--, av++;
    }
  } else {
    do_menu = 1;
  }

  startup ();

#ifdef _LOOP_BM
  loop_benchmark = 1;
#endif

#ifdef _DEFS
#ifdef _GNU_SOURCE
  printf ("GNU_SOURCE defined \n");
#endif
#ifdef __linux__
  printf ("__linux__ defined\n");
#endif
#ifdef __sun__
  printf ("__sun__ defined\n");
#endif
#ifdef __hpux__
  printf ("__hpux__ defined\n");
#endif
#if _LFS_LARGEFILE == 1
  printf ("_LARGEFILE64_SOURCE is available\n");
#endif
#ifdef _LARGEFILE64_SOURCE
  printf ("_LARGEFILE64_SOURCE defined\n");
#endif
#ifdef __LP64__
  printf ("__LP64__ defined\n");
#endif
#ifdef _FILE_OFFSET_BITS
  printf ("_FILE_OFFSET_BITS = %d\n", _FILE_OFFSET_BITS);
#endif
#ifdef __STDC_EXT__
  printf ("__STDC_EXT__ defined\n");
#endif
#endif /* DEFS */

#ifdef _DO_QUICKIE
  do_install_test = 1;
  sync_writes = 0;
  ask_questions ();
  open_device ();
  blocks = getsize (fd);
  do_quickie (fd);
  close_device (fd);
  printf ("end of installation quick test\n");
  exit (EXIT_SUCCESS);

#endif

  if (do_menu == 1 && !do_install_test) {
    while (1) {
      printf ("\n\n\t\tMain Menu \n\n\n");
      printf ("Quick Read Test                   1\n");
      printf ("Random Read Benchmark             2\n");
      printf ("Random Write Benchmark            3\n");
      printf ("Random Read/Write Benchmark       4\n");
      printf ("Exerciser                         5\n");
      printf ("Maintenance tests                 6\n");
      printf ("Exit                              7\n");
      printf ("\n\nEnter test number: ");
      if (scanf ("%s", ans) == EOF) {
	printf ("scanf failed");
	exit (EXIT_FAILURE);
      }
      test_number = atoi (ans);

      switch (test_number) {
      case 1:
	do_quick = 1;
	ask_questions ();
	open_device ();
	blocks = getsize (fd);
	quick_test ();
	do_quick = 0;
	do_read = 0;
	break;
      case 2:
	do_read = 1;
	ask_questions ();
	open_device ();
	blocks = getsize (fd);
	blocks = (blocks / 100) * dpercent;
	while (loop_benchmark) {
	  benchmarks ();
	}
	benchmarks ();
	do_read = 0;
	break;
      case 3:
	do_write = 1;
	ask_questions ();
	if (ask_write () == 0)
	  break;
	open_device ();
	blocks = getsize (fd);
	while (loop_benchmark) {
	  benchmarks ();
	}
	benchmarks ();
	do_write = 0;
	break;
      case 4:
	do_readwrite = 1;
	ask_questions ();
	if (ask_write () == 0)
	  break;
	open_device ();
	blocks = getsize (fd);
	while (loop_benchmark) {
	  benchmarks ();
	}
	benchmarks ();
	do_readwrite = 0;
	break;
      case 5:
	do_exercise = 1;
	error_flag = 0;
	ask_questions ();
	if (ask_write () == 0)
	  break;
	sync_writes = 1;
	open_device ();
	blocks = getsize (fd);
	run_exerciser ();
	do_exercise = 0;
	break;
      case 6:
	do_maint = 1;
	sync_writes = 1;
	ask_questions ();
	open_device ();
	blocks = getsize (fd);
	ret = maintenance_test ();
	do_maint = 0;
	break;
      case 7:
	close_device (fd);	/* Close Device */
	if (do_log == 1)
	  close_log ();
	exit (EXIT_SUCCESS);
      default:
	;
      }				/* end switch */
    }				/* end while  */
  }				/* end if   */
  do_menu = 0;
/*  if do_menu = 0, all args were on the command line  */

  itercount = (itercount > 0) ? itercount : DEF_ITER_COUNT;
  open_device ();
  open_log ();
  blocks = getsize (fd);
  if ((do_write) || (do_read) || (do_readwrite)) {
    if ((do_write) || (do_readwrite))
#ifdef _NO_ASK_ABOUT_WRITE
      if (!no_ask_about_write)
	if (!ask_write ())
	  exit (EXIT_SUCCESS);
#else
      if (ask_write () == 0)
	exit (EXIT_SUCCESS);
#endif
    blocks = (blocks / 100) * dpercent;
    benchmarks ();
    close_device (fd);
    exit (EXIT_SUCCESS);
  }


  if (do_exercise) {
#ifdef _NO_ASK_ABOUT_WRITE
    if (!no_ask_about_write)
      if (!ask_write ())
	exit (EXIT_SUCCESS);
#else
    if (ask_write () == 0)
      exit (EXIT_SUCCESS);
#endif
    run_exerciser ();
    exit (EXIT_SUCCESS);
  }


  if (do_maint) {
    if (ask_write () == 0)
      exit (EXIT_SUCCESS);
    maintenance_test ();
    exit (EXIT_SUCCESS);
  }


  close_device (fd);
  if (do_log == 1)
    close_log ();
  exit (EXIT_SUCCESS);
}

/********************************************/
/*   END OF MAIN()  FUNCTIONS FOLLOW ....   */
/********************************************/
static void
open_log (void)
{
  if (strcmp (log_file_name, "") != 0) {

#ifdef _APPEND_LOGFILE
    fp_log = fopen (log_file_name, "a+");
#else
    fp_log = fopen (log_file_name, "w");
#endif
    if (fp_log == NULL) {
      io_error ("Error opening log file");
      exit (EXIT_FAILURE);
    }
    do_log = 1;
    sprintf (buf, "\tSolid Data Systems IOtest V%s\n\n", VERSION);
    logit (buf);
    if (uname (&uts) < 0) {
      io_error ("Error getting uname information");
      exit (EXIT_FAILURE);
    }
    sprintf (buf, "Sysname: %s  Release: %s Machine: %s \n", uts.sysname, uts.release, uts.machine);
    logit (buf);
  }
}

/***************************************************************************/
static void
benchmarks (void)
{
  iterations = itercount;
  /*iterations = (itercount > 0) ? itercount : ITERATIONS; */
  sprintf (buf, "Starting iteration count = %d\n", iterations);
  printf ("%s", buf);
  if (do_log == 1) {
    logit (buf);
  }
  if (do_log == 1) {
    sprintf (buf, "Drive size = %lld blocks (%lld bytes)\n", blocks, (blocks * 512));
    logit (buf);
  }
  printf ("Transfers will vary between %d and %d bytes\n", min_xfer_size, max_xfer_size);

  if (do_readwrite == 1) {
    read_iterations = iterations * ((float) rwpercent / 100);
    write_iterations = iterations * ((100 - (float) rwpercent) / 100);
    sprintf (buf, "There will be ~%d reads and ~%d writes per test\n",
	     (int) read_iterations, (int) write_iterations);
    printf ("%s", buf);
    if (do_log == 1) {
      logit (buf);
    }
  }
  maxp2 = maxp + 2;
  procs = minp;			/* set procs to minp, and loop up to maxp */
  while (procs <= maxp) {
    iterations = itercount;
    /*iterations = (itercount > 0) ? itercount : ITERATIONS; */


    if ((sleep (2)) < 0) {
      printf ("sleep failed\n");
      exit (EXIT_FAILURE);
    }


    if (do_read == 1) {
      printf ("\nBeginning %d process random read test on %s\n", procs, rawdevice);
      printf ("\n");
      if (do_log == 1) {
	sprintf (buf,
		 "\nBeginning %d process random read test on %s [%d iterations]\n",
		 procs, rawdevice, iterations);
	logit (buf);
      }
      rwpercent = 100;

    } else if (do_write == 1) {
      printf ("\nBeginning %d process random write test on %s\n", procs, rawdevice);
      printf ("\n");
      if (do_log == 1) {
	sprintf (buf,
		 "\nBeginning %d process random write test on %s [%d iterations]\n",
		 procs, rawdevice, iterations);
	logit (buf);
      }
      rwpercent = 0;
    } else if (do_readwrite == 1) {
      printf ("\nBeginning %d process Read/Write test of %s\n", procs, rawdevice);
      printf ("\n");
      if (do_log == 1) {
	sprintf (buf,
		 "\nBeginning %d process Read/Write test on %s [%d iterations]\n",
		 procs, rawdevice, iterations);
	logit (buf);
      }
    }
#ifdef _NEW_OUTPUT_FORMAT

    printf (" Bytes\tET-Secs\t  IOPS\t  MB/Sec\n");
    if (do_log == 1) {
      sprintf (buf, " Bytes\tET-Secs\t  IOPS\t  Mb/Sec\n");
      logit (buf);
    }
#endif

    rdwr_iterations = iterations;

    (void) random_rdwr_mp (fd, blocks, procs, min_xfer_size,
			   max_xfer_size, rdwr_iterations, rwpercent, do_log, skip_block_zero);
    procs = ((procs % 2) == 0) ? (procs + 2) : (procs + 1);
  }
}

/****************************************************************************/
static void
quick_test (void)
{
  do_read = 1;
  procs = 1;
  min_xfer_size = 8192;
  max_xfer_size = 8192;
  rdwr_iterations = 1000;
  rwpercent = 100;
  random_rdwr_mp (fd, blocks, procs, min_xfer_size, max_xfer_size,
		  rdwr_iterations, rwpercent, do_log, skip_block_zero);
  do_read = 0;
/*exit(EXIT_SUCCESS); */
}

/*****************************************************************************/
static int
maintenance_test (void)
{
  while (1) {
    printf ("\n\tMaintenance mode tests\n");
    printf ("Reliability                    1\n");
    printf ("Quick read                     2\n");
    printf ("Write block numbers            3\n");
    printf ("Scan block numbers             4\n");
    printf ("Continuous write/scan block    5\n");
    printf ("HEX block dump                 6\n");
    printf ("Byte editor                    7\n");
#ifdef _DO_BDRS
    printf ("Send SCSI BDR                  8\n");
#endif
#ifdef _OLD_WRITE_BLOCK
    printf ("Write Block (old)              9\n");
    printf ("Scan block (old)              10\n");
    printf ("Write/scan block (old)        11\n");
#endif
    printf ("Exit                          12\n");
    printf ("\nEnter test number: ");
    if (scanf ("%s", ans) == EOF) {
      printf ("scanf failed");
      exit (EXIT_FAILURE);
    }
    test_number = atoi (ans);
    character = (char) getchar ();
    switch (test_number) {
    case 1:
      if (ask_write () == 0)
	break;
      reliability (fd, blocks, do_log, min_xfer_size, max_xfer_size, rawdevice, skip_block_zero);
      break;
    case 2:
      quick_read (fd, blocks, max_xfer_size, do_log);
      break;
    case 3:
      if (ask_write () == 0)
	break;
      if (ask_block0 () == 0)
	break;
      write_block_new (fd, blocks, max_xfer_size);
      break;
    case 4:
      scan_blocks_new (fd, blocks, max_xfer_size);
      break;
    case 5:
      if (ask_write () == 0)
	break;
      if (ask_block0 () == 0)
	break;
      write_scan_blocks_new (fd, blocks, max_xfer_size);
      break;
    case 6:
      dump_block (fd, blocks, do_log);
      break;
    case 7:
      if (ask_write () == 0)
	break;
      printf("IOt 1 do_log = %d\n",do_log);      
      write_a_byte (fd, blocks, do_log);
      printf("IOt 2 do_log = %d\n",do_log);      
      break;

#ifdef _DO_BDRS
    case 8:
      do_bdr (fd);
      break;
#endif
#ifdef _OLD_WRITE_BLOCK
    case 9:
      if (!ask_write ())
	break;
      if (!ask_block0 ())
	break;
      write_block (fd, blocks, max_xfer_size);
      break;
    case 10:
      scan_blocks (fd, blocks, max_xfer_size);
      break;
    case 11:
      if (ask_write () == 0)
	break;
      if (ask_block0 () == 0)
	break;
      write_scan_blocks (fd, blocks, max_xfer_size);
      break;
#endif

    case 12:
      return (EXIT_SUCCESS);
    default:
      ;
    }				/* end switch */
  }				/* end while  */
}

/****************************************************************************/
#ifdef _DO_BDRS
static void
do_bdr (int fd)
{
  int loop;
  while (1) {
    printf ("Enter number of seconds between BDR's (2-95 [10]): ");
    (void) fgets (inbuf, INBUFLEN, stdin);
    (void) sscanf (inbuf, "%d", &bdr_sleeptime);
    bdr_sleeptime = (bdr_sleeptime > 0) ? bdr_sleeptime : 10;

    if ((bdr_sleeptime >= 2) && (bdr_sleeptime <= 1000)) {
      printf ("sleep time between BDR's = %d\n", bdr_sleeptime);
      break;
    }
  }

  for (loop = 1; loop <= 50000; ++loop) {
    printf ("Sending BDR # %d to device %s\n", loop, rawdevice);
    if ((ioctl (fd, DIOC_RSTCLR, &flag)) < 0)
      exit (1);
    sleep (bdr_sleeptime);
  }
}
#endif
/*****************************************************************************/
#ifdef _DO_QUICKIE
static void
do_quickie (int fd)
{
  rdwr_iterations = 5000;
  procs = 2;
  rwpercent = 0;
  min_xfer_size = 512 * 16;
  max_xfer_size = 512 * 16;

  (void) random_rdwr_mp (fd, blocks, procs, min_xfer_size,
			 max_xfer_size, rdwr_iterations, rwpercent, do_log, skip_block_zero);
  sleep (2);
  rwpercent = 100;

  (void) random_rdwr_mp (fd, blocks, procs, min_xfer_size,
			 max_xfer_size, rdwr_iterations, rwpercent,
			 do_log, skip_block_zero);
  if ((ret = fsync (fd)) < 0) {
    perror ("test: fsync");
    printf ("Error during fsync of %s\n", rawdevice);
  }
}
#endif
/*****************************************************************************/
static void
run_exerciser (void)
{
  printf ("Transfers will vary between %d and %d bytes\n", min_xfer_size, max_xfer_size);
  printf ("Starting exerciser on device %s at ", rawdevice);
  timeofday ();
  for (pass_count = 1; pass_count <= EXER_PASSCOUNT; ++pass_count) {
    error_flag = exercise (fd, rawdevice, blocks, min_xfer_size, max_xfer_size, do_log);

    if (error_flag) {
      return;
    }

    printf ("End of pass %d on device %s at ", pass_count, rawdevice);
    timeofday ();
    if (do_log == 1) {
      sprintf (buf, "End of pass %d at ", pass_count);
      logit (buf);
      current_time_to_log ();
    }
  }
}

/***********************************************************************/
static void
ask_questions (void)
{
#ifndef _DO_QUICKIE
  character = (char) getchar ();
#endif

  printf ("Enter device name: ");
  (void) fgets (rawdevice, (int) sizeof (rawdevice), stdin);
  rawdevice[strlen (rawdevice) - 1] = '\0';
  printf ("device = %s\n", rawdevice);


  if (do_readwrite) {
    while (1) {
      printf ("Enter percentage of reads vs writes (5-95 [50]): ");
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &rwpercent);
      rwpercent = (rwpercent > 0) ? rwpercent : 50;

      if ((rwpercent >= 5) && (rwpercent <= 95)) {
	printf ("read percentage = %d\n", rwpercent);
	break;
      }
    }
  }
  if ((do_write) || (do_read) || (do_readwrite)) {
    while (1) {
      printf ("Enter iteration count (%d-%d [%d]: ",
	      MIN_ITER_COUNT, MAX_ITER_COUNT, DEF_ITER_COUNT);
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &itercount);
      itercount = (itercount > 0) ? itercount : DEF_ITER_COUNT;
      if ((itercount >= MIN_ITER_COUNT) && (itercount <= MAX_ITER_COUNT)) {
	printf ("itercount = %d\n", itercount);
	break;
      }
    }
  }

  if ((!do_quick) && (!do_install_test)) {
    printf ("Enter logfile name: ");
    (void) fgets (log_file_name, (int) sizeof (log_file_name), stdin);
    log_file_name[strlen (log_file_name) - 1] = '\0';
    printf ("logfile name = %s\n", log_file_name);
    open_log ();
  }
  if ((do_write) || (do_read) || (do_readwrite)) {
    while (1) {
      printf ("Enter minimum process count [1]: ");
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &minp);
      minp = (minp > 0) ? minp : 1;
      if ((minp >= 1) && (minp <= MAX_PROC_COUNT)) {
	printf ("minimum process count = %d\n", minp);
	break;
      }
    }
  }
  if ((do_write) || (do_read) || (do_readwrite)) {
    while (1) {
      printf ("Enter maximum process count (%d-%d [%d]): ", minp,
	      MAX_PROC_COUNT, (defp > minp ? defp : minp));
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &maxp);
      maxp = (maxp > 0) ? maxp : defp;
      if ((maxp >= minp) && (maxp <= MAX_PROC_COUNT)) {
	printf ("maximum process count = %d\n", maxp);
	break;
      }
    }
  }
  if ((do_write) || (do_read) || (do_readwrite) || (do_exercise)) {
    min_xfer_size = MIN_TRANSFER_SIZE;
    while (1) {
      printf ("Enter minimum transfer size (%d-%d [%d] bytes): ",
	      MIN_TRANSFER_SIZE, MAX_TRANSFER_SIZE, MIN_TRANSFER_SIZE);
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &min_xfer_size);
      min_xfer_size = (min_xfer_size > 0) ? min_xfer_size : MIN_TRANSFER_SIZE;
      if ((min_xfer_size % 512) == 0) {
	if ((min_xfer_size >= MIN_TRANSFER_SIZE)
	    && (min_xfer_size <= MAX_TRANSFER_SIZE)) {
	  printf ("minimum transfer size = %d\n", min_xfer_size);
	  break;
	}
      }
      printf ("transfer size must be a multiple of 512 bytes\n");
      printf ("512, 1024, 1536, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, etc\n");
    }
  }
  if ((do_write) || (do_read) || (do_readwrite) || (do_exercise)) {
    max_xfer_size = MAX_TRANSFER_SIZE;
    while (1) {
      printf ("Enter maximum transfer size (%d-%d [%d] bytes): ",
	      min_xfer_size, MAX_TRANSFER_SIZE, MAX_TRANSFER_SIZE);



      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &max_xfer_size);
      max_xfer_size = (max_xfer_size > 0) ? max_xfer_size : MAX_TRANSFER_SIZE;
      if ((max_xfer_size % 512) == 0) {
	if ((max_xfer_size >= min_xfer_size)
	    && (max_xfer_size <= MAX_TRANSFER_SIZE)) {
	  printf ("maximum transfer size = %d\n", max_xfer_size);
	  break;
	}
      }
      printf ("transfer size must be a multiple of 512 bytes\n");
      printf ("512, 1024, 1536, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, etc\n");
    }
  }
  if ((do_write) || (do_read) || (do_readwrite)) {
    while (1) {
      printf ("Enter drive percentage to use (%d-%d [%d]: ", 1, 100, 100);
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &dpercent);
      dpercent = (dpercent > 0) ? dpercent : 100;
      if ((dpercent >= 1) && (dpercent <= 100)) {
	printf ("drive used = %d percent\n", dpercent);
	break;
      }
    }
  }
  if (do_maint) {
    while (1) {
      printf ("Enter transfer size [%d bytes]: ", MAX_TRANSFER_SIZE);
      (void) fgets (inbuf, INBUFLEN, stdin);
      (void) sscanf (inbuf, "%d", &max_xfer_size);
      max_xfer_size = (max_xfer_size > 0) ? max_xfer_size : MAX_TRANSFER_SIZE;

      if ((max_xfer_size % 512) == 0) {
	if ((max_xfer_size >= MIN_TRANSFER_SIZE)
	    && (max_xfer_size <= MAX_TRANSFER_SIZE)) {
	  printf ("transfer size = %d\n", max_xfer_size);
	  break;
	}
      }
      printf ("transfer size must be a multiple of 512 bytes\n");
      printf ("512, 1024, 1536, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, etc\n");
    }
  }
  if (0) {
    printf ("Do you want to test block 0 (yes,no [no])?  ");
    (void) fgets (inbuf, INBUFLEN, stdin);
    if (sscanf (inbuf, "%s", ans) == EOF) {
      skip_block_zero = 1;
      printf ("Block 0 will not be tested\n");
    } else if ((strcmp (ans, "yes") == 0) || (strcmp (ans, "y") == 0)) {
      skip_block_zero = 0;
      printf ("Block 0 will be tested\n");
      printf ("\nIf you write block 0 under Solaris, you must re-label the\n");
      printf ("drive using format before re-starting IOtest!\n\n");
    } else if ((strcmp (ans, "no") == 0 || strcmp (ans, "n") == 0)) {
      skip_block_zero = 1;
      printf ("Block 0 will not be tested\n");
    }
  }
  if ((do_write) || (do_readwrite)) {
    printf ("Do you want writes to be synchronous (yes,no [no])?  ");
    (void) fgets (inbuf, INBUFLEN, stdin);
    if (sscanf (inbuf, "%s", ans) == EOF) {
      sync_writes = 0;
      printf ("Writes will be asynchronous\n");
    } else if ((strcmp (ans, "yes") == 0 || strcmp (ans, "y") == 0)) {
      sync_writes = 1;
      printf ("Writes will be synchronous\n");
    } else if ((strcmp (ans, "no") == 0 || strcmp (ans, "n") == 0)) {
      sync_writes = 0;
      printf ("Writes will be asynchronous\n");
    }
  }
#ifdef _LOOP_BM

  if ((do_write) || (do_read) || (do_readwrite)) {
    printf ("Do you want the benchmark to loop continuously (yes,no [no])?  ");
    (void) fgets (inbuf, INBUFLEN, stdin);
    if (sscanf (inbuf, "%s", ans) == EOF) {
      loop_benchmark = 0;
      printf ("benchmark will not loop\n");
    } else if ((strcmp (ans, "yes") == 0 || strcmp (ans, "y") == 0)) {
      loop_benchmark = 1;
      printf ("benchmark will loop continuously\n");
    } else if ((strcmp (ans, "no") == 0 || strcmp (ans, "n") == 0)) {
      loop_benchmark = 0;
      printf ("benchmark will not loop continuosly\n");
    }
  }
#endif

}

/*************************************************************/
static int
ask_block0 (void)
{
  printf ("This test will write on block 0\n");
  printf ("Do you want to continue (yes,no [no])?  ");
  (void) fgets (inbuf, INBUFLEN, stdin);
  if (sscanf (inbuf, "%s", ans) == EOF) {
    skip_block_zero = 1;
    return (0);
  } else if ((strcmp (ans, "yes") == 0) || (strcmp (ans, "y") == 0)) {
    skip_block_zero = 0;
    printf ("Block 0 will be tested\n");
    return (1);
    /*printf */
    /*("\nIf you write block 0 under Solaris, you must re-label the\n"); */
    /*printf ("drive using format before re-starting IOtest!\n\n"); */
  } else if ((strcmp (ans, "no") == 0 || strcmp (ans, "n") == 0)) {
    skip_block_zero = 1;
    return (0);
  }
  return (0);
}

/*************************************************************/
/*************************************************************/
static int
ask_write (void)
{
  if ((do_write) || (do_exercise) || (do_maint) || (do_readwrite)
      || (do_install_test)) {
    printf
      ("\nThis test will WRITE on device %s.\n Do you want to continue (yes,no [no])?  ",
       rawdevice);
    (void) fgets (inbuf, INBUFLEN, stdin);
    /*inbuf[strlen(inbuf) - 1] = '\0'; */
    if (sscanf (inbuf, "%s", ans) == EOF) {
      return (0);
    } else if ((strcmp (ans, "no") == 0 || strcmp (ans, "n") == 0)) {
      return (0);
    } else if ((strcmp (ans, "yes") == 0 || strcmp (ans, "y") == 0 || strcmp (ans, "hai") == 0))
      printf ("Writing on %s enabled.\n", rawdevice);
    return (1);
  }
  return (0);
}

/*************************************************************/
static void
open_device (void)
{
  if (do_write == 1 || do_exercise == 1 || do_maint == 1
      || do_readwrite == 1 || do_install_test == 1) {
    if (sync_writes) {
      if ((fd = open (rawdevice, O_RDWR | O_SYNC | O_DIRECT)) < 0) {
	io_error ("Error opening device RDWR, O_SYNC");
	exit (EXIT_FAILURE);
      }
      printf ("Synchronous writes enabled\n");
      if (do_log == 1) {
	sprintf (buf, "Synchronous writes enabled\n");
	logit (buf);
      }
    } else {

#ifdef _HAS_O_LARGEFILE
      if ((fd = open (rawdevice, O_RDWR | O_LARGEFILE | O_DIRECT)) < 0) {
#else
      if ((fd = open (rawdevice, O_RDWR | O_DIRECT)) < 0) {
#endif

	io_error ("Error opening device RDWR");
	exit (EXIT_FAILURE);
      }
      printf ("Asynchronous writes enabled\n");
      if (do_log == 1) {
	sprintf (buf, "Asynchronous writes enabled\n");
	logit (buf);
      }
    }
  }
  if ((do_read == 1) || (do_quick == 1)) {

#ifdef _HAS_O_LARGEFILE
    if ((fd = open (rawdevice, O_RDONLY | O_LARGEFILE | O_DIRECT)) < 0) {
#else
    if ((fd = open (rawdevice, O_RDONLY | O_DIRECT)) < 0) {
#endif

      io_error ("Error opening device RDONLY");
      exit (EXIT_FAILURE);
    }
  }
/*  (void) stat (name, &stbuf); */
/*  (void) fstat (fd, &stbuf); */
  if ((stbuf.st_mode & S_IFMT) == S_IFREG) {
    printf ("%s is a regular file \n", rawdevice);
  }
  if ((stbuf.st_mode & S_IFMT) == S_IFCHR) {
    printf ("%s is a character special device \n", rawdevice);
  }
  if ((stbuf.st_mode & S_IFMT) == S_IFBLK) {
    printf ("%s is a block special device \n", rawdevice);


  }
}

/*************************************************************/
static long long
getsize (int fd)
/* Read device pointed to by fd, and return the number of  */
/* blocks as a long long int.                                   */
{
  int ret, chars = 512;
  static char rbuf[512];
  long blks;

  ret = ioctl(fd, BLKGETSIZE, &blks);
  if (ret == 0) {
	printf("Device size is %d blocks\n", (int)blks);
	return (long long)blks;
  }
  printf ("Reading drive to determine size...\n");
  /* Do a read every 10000 blocks until we get an error */
  blocks = 0;
  while (chars == 512) {

    blocks = blocks + 10000;
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 ((int) fd, (off64_t) (blocks * 512), SEEK_SET) < 0)) {
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
#else
    if (blocks > (unsigned int) (INT_MAX / 512)) {
      printf ("size set to maximum (%d blocks)\n", INT_MAX / 512);
      blocks = (INT_MAX / 512) - (MAX_TRANSFER_SIZE / 512);
      return blocks;
    }
    if ((lseek (fd, (long) (blocks * 512), SEEK_SET) < 0)) {
      printf ("lseek failed\n");
      exit (EXIT_FAILURE);
    }
#endif
    chars = (int) read (fd, rbuf, 512);	/* Attempt read */
    /* if chars < 512, we hit the end  */
  }

  blocks = blocks - 10000;
#if DEBUG
  printf ("first pass  %lld blocks ..  \n", blocks);
#endif
  chars = 512;

  /* We are within 10000 blocks of the right size.  Change */
  /* increment to 100 blocks and keep reading.             */
  while (chars == 512) {
    blocks = blocks + 100;
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (blocks * 512), SEEK_SET) < 0)) {
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (blocks * 512), SEEK_SET) < 0)) {
      printf ("lseek failed\n");
      exit (EXIT_FAILURE);
    }
#endif
    chars = (int) read (fd, rbuf, 512);	/* Attempt read */
  }
  blocks = blocks - 100;

#if DEBUG
  printf ("second pass  %lld blocks ..  \n", blocks);
#endif
  chars = 512;

  /*  Now within 100 blocks.  Change increment to 1, and keep going */
  while (chars == 512) {
    blocks = blocks + 1;
#ifdef _LARGEFILE64_SOURCE
    if ((lseek64 (fd, (off64_t) (blocks * 512), SEEK_SET) < 0)) {	/* Set Pointer */
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
#else
    if ((lseek (fd, (long) (blocks * 512), SEEK_SET) < 0)) {	/* Set Pointer */
      printf ("lseek failed\n");
      exit (EXIT_FAILURE);
    }
#endif
    chars = (int) read (fd, rbuf, 512);	/* Attempt read */
  }

#if DEBUG
  printf ("final pass  %lld blocks ..  \n", blocks);
#endif
  printf ("Drive size = %lld blocks (%lld bytes)\n", blocks, (blocks * 512));

  return (blocks);		/* return # of 512 byte blocks */
}

/*************************************************************/
static void
startup (void)
{
  printf ("\n\t Solid Data Systems IOtest V%s\n\n", VERSION);
  current_time ();
  current_time_to_log ();
  if (uname (&uts) < 0) {
    io_error ("Error getting uname information");
    exit (EXIT_FAILURE);
  }
  printf ("Sysname: %s    Release: %s    Machine: %s \n", uts.sysname, uts.release, uts.machine);
}

/************************************************************/
static void
close_device (int fd)
{
  if (close (fd) == -1) {
    io_error ("Error closing device");
  }
}

/************************************************************/
void
io_error (char *message)
{
  char err_buf[100];
  sprintf (err_buf, "IOtest: I/O error (%s)", message);
  perror (err_buf);
  exit (EXIT_FAILURE);
}

/***********************************************************/
void
logit (char *buf)
{
  if ((fseek (fp_log, fp_pos, SEEK_SET)) < 0) {
    printf ("fseek failed\n");
    exit (EXIT_FAILURE);
  }
  fprintf (fp_log, "%s", buf);
  if ((fflush (fp_log) < 0)) {
    printf ("flush to logfile failed\n");
    exit (EXIT_FAILURE);
  }
  fp_pos = (int) ftell (fp_log);
}

/***********************************************************/
static void
close_log (void)
{
  if (fclose (fp_log) == EOF) {
    io_error ("Error closing log file");
  }
}

/******************************************************************/
static void
current_time (void)
{
  time_t timeval;
  (void) time (&timeval);
  printf ("Starting at: %s\n", ctime (&timeval));
}

/*****************************************************************/
void
current_time_to_log (void)
{
  time_t timeval;
  (void) time (&timeval);
  if (do_log == 1) {
    sprintf (buf, "%s", ctime (&timeval));
    logit (buf);
  }
}

/*****************************************************************/
void
timeofday (void)
{
  struct tm *tm_ptr;
  time_t the_time;
  (void) time (&the_time);
  tm_ptr = localtime (&the_time);
  printf ("%02d:%02d:%02d\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec);
}

/*****************************************************************/
long int
MSTime (void)
{
  (void) gettimeofday (&currentTime, NULL);
  return (long int) ((currentTime.tv_sec * 1000) + ((currentTime.tv_usec) / 1000));
}
