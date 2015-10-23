/*  create.c    */
/*  Rich Cooper   5/5/98  */
/*  Version 1.02          */

/* changed lseek() to  lseek64()  */
/* 2/17/01  rbc  */



#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define  XFER_SIZE 256		/*512 byte blocks */

static float percent = 0;
static int fd, percent_incr = 5;
static long int charswrote;
static long long start_block, filesize;
static long write_size;
static char wbuffer[512 * XFER_SIZE], fname[25];

int
main ()
{
  printf ("Create  version 1.02\n");
  printf ("Enter file name: ");
  (void) fgets (fname, (int) sizeof (fname), stdin);
  fname[strlen (fname) - 1] = '\0';	/* get rid of the null character */

  printf ("Enter number of 512 byte blocks: ");
  (void) scanf ("%lld", &filesize);

  printf ("Opening %s...\n", fname);

  printf ("filesize = %lld\n", filesize);

  if ((fd = open ((char *) fname, O_CREAT | O_RDWR)) == -1) {
    printf ("unable to open %s\n", fname);
    exit (EXIT_FAILURE);
  }
  printf ("Writing file %s with zero's\n", fname);
  start_block = 0;




  while (start_block < filesize) {

    if ((float) ((float) start_block / (float) filesize * 100) >= percent) {
      printf ("\b\b\b\b%3.0f%%", (float) ((float) start_block / (float) filesize * 100));
      percent = percent + percent_incr;
      if ((int) percent == 100)
	printf ("\b\b\b\b100%%");
      if (fflush (stdout) == EOF) {
	printf ("Error flushing stdout\n");
	exit (EXIT_FAILURE);
      }
    }
    if ((lseek64 ((int) fd, (off64_t) (start_block * 512), SEEK_SET) < 0)) {
      printf ("lseek64 failed\n");
      exit (EXIT_FAILURE);
    }
    if ((filesize - start_block) > XFER_SIZE) {
      write_size = XFER_SIZE * 512;
    } else {
      write_size = (filesize - start_block) * 512;
    }

/*    printf ("start = %lld  size = %ld\n", start_block, write_size); */

    if ((charswrote = write (fd, wbuffer, (size_t) write_size)) < 0) {
      printf
	("Error during transfer of %ld bytes starting at block %lld\n", write_size, start_block);
      perror ("create: write");
      exit (EXIT_FAILURE);
    }
    start_block = (long) (long) start_block + XFER_SIZE;
  }

  if (fchmod ((int) fd, (mode_t) S_IRWXU | S_IRWXG | S_IRWXO) < 0)
    printf ("Error changing ownership\n");

  printf ("\nClosing file\n");
  (void) close (fd);
  return 0;
}
