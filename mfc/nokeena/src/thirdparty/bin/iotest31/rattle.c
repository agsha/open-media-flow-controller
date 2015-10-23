/*  rattle.c               */
/*  Rich Cooper   5/25/98  */
/*  Version 1.5    */

/* Added large device support  */
/* Changed to srand48 and drand48  */
/*  2/16/01  rbc   */

#include <stddef.h>
#include "rattle.h"

#define DO_READ 1
#define DO_WRITE 2
static FILE *in;
static int i;			/*  current position in devtable structure */
static int p;			/*  number of entrys in devtable structure */
static int n, fd, rwpercent, min_read_size, max_read_size;
static int rdwr_type, procs;
static long int charsread, charswrote;
static long max_xfer, min_xfer, size;
static long long block_count;
static long long random_block, nblocks;
static char wrbuffer[MAX_TRANSFER_SIZE];
static char rbuffer[MAX_TRANSFER_SIZE];
static char fname[25], ans[3];
static pid_t pid;
struct devs
{
  char device_name[25];
  int percent;
  int procs;
  long long blocks;
  int file_descriptor;
  pid_t pid;
};
static struct devs devtable[MAX_DEVICES];

static void timeofday (void);
static void io_error (char *);
static long long getsize (int fd);

int
main (int argc, char *argv[])
{
  if (!argv[1]) {
    printf ("Rattle version 1.05 starting at ");
    timeofday ();
  }
  if (argv[1]) {
    strcat (fname, argv[1]);
  } else {

    printf ("Enter data file name: ");
    (void) fgets (fname, (int) sizeof (fname), stdin);
    fname[strlen (fname) - 1] = '\0';	/* get rid of the null character */
  }

  if ((in = fopen (fname, "r")) == NULL) {
    printf ("failed to open %s\n", fname);
    exit (EXIT_FAILURE);
  }
/* Read the data file line by line, and put the device name, read/write */
/* percentage and # procs into the devtable structure.                  */
  p = 1;			/* first line */
  while (1) {
    if ((n =
	 fscanf (in, "%s%d%d", (char *) &devtable[p].device_name,
		 &devtable[p].percent, &devtable[p].procs)) == -1) {
      --p;			/* ran out of lines in the files  */
      break;
    }
    if (devtable[p].percent < 0 || devtable[p].percent > 100) {
      printf ("percent value out of range [0-100] for entry # %d\n", p);
      exit (EXIT_FAILURE);
    }
    if (devtable[p].procs < 0 || devtable[p].procs > MAX_PROC_COUNT) {
      printf ("Process count out of range [1-%d] for entry %d\n", MAX_PROC_COUNT, p);
      exit (EXIT_FAILURE);
    }
    if (!argv[1])
      printf
	(" %d  dev = %s  read/write percentage = %d   # processes = %d\n",
	 p, devtable[p].device_name, devtable[p].percent, devtable[p].procs);
    ++p;			/* next line ... */
  }				/* end while(1) */

/*  At this point, i = current line in devtable, p = # devtable entries */

/*  Cycle through devtable, looking for "write requests" */
/*  a "write request" is a read/write percentage < 100   */
/*  if datafile name was specifed on command line, don't check writes */

  if (!argv[1]) {
    for (i = 1; i <= p; ++i) {
      if (devtable[i].percent < 100) {
	printf ("You will write on at least one disk!\n");
	printf ("     Do you want to continue [yes,no]?  ");
	if (scanf ("%s", ans) == EOF) {
	  printf ("scanf failed\n");
	  exit (EXIT_FAILURE);
	}
	if ((strcmp (ans, "yes") != 0) && (strcmp (ans, "y") != 0))
	  exit (EXIT_SUCCESS);
	break;
      }
    }
  }
  /* end if (!arg....  */
  /*  Close the input file  */
  (void) fclose (in);

  if (!argv[1]) {
    printf ("\nStarting disk exercisers at ");
    timeofday ();
    printf ("%d disks will be tested\n", p);
    printf ("Transfers will vary between %d and %d bytes\n", MIN_TRANSFER_SIZE, MAX_TRANSFER_SIZE);

/* Open the devices, and put the file descriptors into the devtable structure */
    printf ("Opening and sizing devices...\n\n");
  }
  for (i = 1; i <= p; ++i) {

/*  Are we doing reads or read/writes??  */
/*  writing enabled if percent is < 100  */
    if (devtable[i].percent < 100) {
      if (SYNC_WRITES) {
	if ((devtable[i].file_descriptor =
	     open ((char *) devtable[i].device_name, O_RDWR | O_SYNC)) == -1) {
	  printf ("unable to open %s O_RDWR | O_SYNC\n", devtable[i].device_name);
	  exit (EXIT_FAILURE);
	}
      } else {
	if ((devtable[i].file_descriptor = open ((char *) devtable[i].device_name, O_RDWR)) == -1) {
	  printf ("unable to open %s O_RDWR\n", devtable[i].device_name);
	  exit (EXIT_FAILURE);
	}
      }

    } else {

/* reading (percent = 100)                        */
/* why open RDONLY? - Maybe you're testing a file */
/* on a read-only file system or cdrom!!                   */

      if ((devtable[i].file_descriptor = open ((char *) devtable[i].device_name, O_RDONLY)) == -1) {
	printf ("unable to open %s O_RDONLY\n", devtable[i].device_name);
	exit (EXIT_FAILURE);
      }
    }

    /*  Get the size of each device, and save it in devtable  */
    nblocks = 0;
    devtable[i].blocks = getsize (devtable[i].file_descriptor);
    if (!argv[1])
      printf ("%lld blocks will be tested on device %s\n",
	      devtable[i].blocks, devtable[i].device_name);
  }				/* end for loop */

  min_read_size = MIN_TRANSFER_SIZE;
  max_read_size = MAX_TRANSFER_SIZE;

  if (!argv[1])
    printf ("\nStarting read/write exercisers as forked processes..\n");
/*  loop through the devtable structure forking read/write exercisers */
  for (i = 1; i <= p; ++i) {
    fd = devtable[i].file_descriptor;
    block_count = devtable[i].blocks;
    procs = devtable[i].procs;
    rwpercent = devtable[i].percent;
    max_xfer = max_read_size / 512;
    min_xfer = min_read_size / 512;

    for (n = 1; n <= (int) max_read_size; ++n)	/* fill the write buffer */
      wrbuffer[n] = (char) n;

    block_count = block_count - (max_read_size / 512) - 1;	/* stay away from the end  */
    pid = 1;
    for (n = 1; n <= procs; ++n) {
      pid = fork ();

      if (pid > 0) {
	devtable[n].pid = pid;
	/*printf("child pid = %d\n",devtable[n].pid); */
      }
      if (pid == 0)
	break;
    }
    switch (pid) {
    case -1:
      exit (EXIT_FAILURE);
    case 0:
      /*printf("child pid = %d for device %s\n",getpid(),devtable[i].device_name); */

      srand48 ((unsigned int) time (NULL) + getpid ());	/* seed the random number generator */
      while (1) {

	/* random starting block */
	random_block = (drand48 () * (block_count - 1)) + 1;

	/* random size for transfer  */
	if (min_read_size != max_read_size) {
	  size = 512 * ((rand () % ((max_xfer + 1) - min_xfer)) + 1) + (min_read_size - 512);
	} else {
	  size = max_read_size;
	}

	if ((lseek64 ((int) fd, (off64_t) (random_block * 512), SEEK_SET)) < 0) {
	  printf ("error in lseek64\n");
	  exit (EXIT_FAILURE);
	}
	/*  Read or Write ?? */
	rdwr_type = ((drand48 () * 100) >= rwpercent) ? DO_WRITE : DO_READ;

	switch (rdwr_type) {
	case DO_READ:

	  if ((charsread = (long int) read (fd, rbuffer, (size_t) size)) < 0) {
	    printf
	      ("Error during transfer of %ld bytes starting at block %lld\n", size, random_block);
	    io_error ("write error");
	    exit (EXIT_FAILURE);
	  }
	  break;

	case DO_WRITE:
	  if ((charswrote = write (fd, wrbuffer, (size_t) size)) < 0) {
	    printf
	      ("Error during transfer of %ld bytes starting at block %lld\n", size, random_block);
	    io_error ("write error");
	    exit (EXIT_FAILURE);
	  }
	  break;

	default:
	  break;
	}
      }
    default:
      break;
    }
  }

/*for(i=1;i<=procs;++i) */
/*printf("%s being tested by pid  %d\n", devtable[i].device_name, devtable[i].pid); */
  (void) sleep (4);
  if (argv[1]) {
    for (i = 1; i <= p; ++i) {
      pid_t child_pid;
      child_pid = wait (NULL);
    }

  } else {
/*  Crude, but very effective... */
    printf ("\nDone???   Type ^C to kill all the exerciser jobs\n");
    if (scanf ("%s", ans) == EOF) {
      printf ("scanf failed\n");
      exit (EXIT_FAILURE);
    }
    if (strcmp (ans, "yes") == 0) {
      exit (EXIT_FAILURE);

      /*   we'll never get here...  */
      /*for (i = 1; i <= p; ++i) { */
      /*    if ((close(devtable[i].file_descriptor)) == -1) { */
      /*      printf("unable to close %s\n", devtable[i].device_name); */
      /*      exit(EXIT_FAILURE); */
      /*  } */
      /*  printf("closed %s  descriptor = %d\n", devtable[i].device_name, devtable[i].file_descriptor); */
      /*} */
      /*exit(EXIT_SUCCESS); */
    }
  }
  return 0;
}

/**************/
/*  FUNCTIONS */
/**************/
static void
io_error (char *message)
{
  char err_buf[100];
  sprintf (err_buf, "IOtest: I/O error (%s)", message);
  perror (err_buf);
  exit (EXIT_FAILURE);
}

/************************************************************************************/
static long long
getsize (int fd)
/* Read device pointed to by fd, and return the number of  */
/* blocks as a long long.                                  */
{
  static char rbuf[512];
  int chars = 512;
  while (chars == 512) {
    nblocks = nblocks + 100000;
    if ((lseek64 ((int) fd, (off64_t) (nblocks * 512), SEEK_SET) < 0)) {	/* Set Pointer */
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
    chars = (int) read (fd, rbuf, 512);	/* Attempt read */
  }

  nblocks = nblocks - 100000;
#if DEBUG
  printf ("first pass  %lld blocks ..  fd = %d\n", nblocks, fd);
#endif
  chars = 512;

  /* We are within 100000 blocks of the right size.  Change */
  /* increment to 100 blocks and keep reading.             */
  while (chars == 512) {
    nblocks = nblocks + 100;
    if ((lseek64 ((int) fd, (off64_t) (nblocks * 512), SEEK_SET) < 0)) {	/* Set Pointer */
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
    chars = (int) read (fd, rbuf, 512);	/* Attempt read */
  }
  nblocks = nblocks - 100;

#if DEBUG
  printf ("second pass  %lld blocks ..  \n", nblocks);
#endif
  chars = 512;

  /*  Now within 100 blocks.  Change increment to 1, and keep going */
  while (chars == 512) {
    nblocks = nblocks + 1;
    if ((lseek64 ((int) fd, (off64_t) (nblocks * 512), SEEK_SET) < 0)) {	/* Set Pointer */
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
    chars = (int) read (fd, rbuf, 512);	/* Attempt read */
  }
#if DEBUG
  printf ("last pass  %lld blocks ..  \n", nblocks);
#endif

  /* printf("Drive size = %lld blocks (%lld bytes)\n", nblocks, (nblocks * 512)); */
  return (nblocks);		/* return # of 512 byte blocks */
}

/***************************************************************************************/
/***************************************************************************************/
static void
timeofday (void)
{
  struct tm *tm_ptr;
  time_t the_time;
  (void) time (&the_time);
  tm_ptr = localtime (&the_time);
  printf ("%02d:%02d:%02d\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec);
}
