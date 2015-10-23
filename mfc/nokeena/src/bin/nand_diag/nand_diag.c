/*
 * (C) Copyright 2011-2012,2014 Juniper Networks, Inc
 *
 * This file contains code which implements the diag for MSC NAND DIMMs
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/uio.h>
#include <string.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sys/types.h>
#include <fcntl.h>

#define roundup(x, y) ((((x)+((y)-1))/(y))*(y))

#define PATTERN_FIXED 1
#define PATTERN_ADDRESS 2

#define SEQ_FIXED  0
#define SEQ_RANDOM 1
#define RAN_RANDOM 2
#define RAN_FIXED  3
#define SEQ_RANDOM_1PASS 4

typedef enum
{
    false = 0,
    true =! false
} bool;

typedef struct
{
    /* Start delimiter (e.g. [ )*/
    char startdelimiter;
    /* End Delimiter (e.g. ] )*/
    char enddelimiter;
    /* Central block (e.g. = )*/
    char block;
    /* Last block (e.g. > ) */
    char curblock;
    /* Width of the progress bar (in characters) */
    unsigned int width;
    /* Maximum value of the progress bar */
    double max;
    /* True if we have to print also the percentage of the operation */
    bool printpercentage;
    /* True if the bar must be redrawn;
       note that this must be just set to false before the first call,
       the function then will change it by itself.  */
    bool update;
} prg_bar_t;

prg_bar_t pbs;
struct iovec *wiov, *riov;
char *wbuf = NULL, *rbuf = NULL, *wr_tbuf, *rd_tbuf;
char dev_name[64], log_file_name[1024];
FILE *log_fp = NULL;

int32_t disk_pagesize = 0, disk_blocksize = 0;
uint32_t nblocks = 0;
uint32_t st_block;
int diag_fd;
int iov_count;
int sleep_time = 0;

/* Inits the settings of the progress bar to the default values */
static void
defaultprogressbar(prg_bar_t *settings)
{
    settings->startdelimiter='[';
    settings->enddelimiter=']';
    settings->block='=';
    settings->curblock='>';
    settings->printpercentage=true;
    settings->update=false;
    settings->max=100;
    settings->width=40;
}   /* defaultprogressbar */

/* Prints/updates the progress bar */
static void
printprogressbar(double pos, prg_bar_t *settings)
{
    /* Blocks to print */
    unsigned int printblocks=(unsigned int)(settings->width*pos/settings->max);
    /* Counter */
    unsigned int counter;

    /* If we are updating an existing bar...*/
    if (settings->update) {
        /* ... we get back to its first character to rewrite it... */
        for (counter=settings->width+2+(settings->printpercentage?5:0);
						counter;counter--)
            putchar('\b');
    } else
        settings->update=true; /* next time we'll be updating it */
    /* Print the first delimiter */
    putchar(settings->startdelimiter);
    /* Reset the counter */
    counter=settings->width;
    /* Print all the blocks except the last; in the meantime, we decrement
       the counter, so in the end we'll have the number of spaces to
       fill the bar */
    for (;printblocks>1;printblocks--,counter--)
        putchar(settings->block);
    /* Print the last block; if the operation ended, use the normal block,
       otherwise the one for the last block */
    putchar((settings->max==pos)?settings->block:settings->curblock);
    /* Another block was printed, decrement the counter */
    counter--;
    /* Fill the rest of the bar with spaces */
    for (;counter;counter--)
        putchar(' ');
    /* Print the end delimiter */
    putchar(settings->enddelimiter);
    /* If asked, print also the percentage */
    if (settings->printpercentage)
        printf(" %3d%%",(int)(100*pos/settings->max));
    /* Flush the output buffer */
    fflush(stdout);
}   /* printprogressbar */

static void
allocate_memory(void)
{
    /* Do the required memory allocation */
    wr_tbuf = malloc(disk_blocksize + 512);
    if (wr_tbuf == NULL) {
	fprintf(stderr, "Memory allocation failed \n");
	exit(1);
    }
    wbuf = (char *)roundup((off_t)wr_tbuf, 512);

    rd_tbuf = malloc(disk_blocksize + 512);
    if (rd_tbuf == NULL) {
	fprintf(stderr, "Memory allocation failed \n");
	exit(1);
    }
    rbuf = (char *)roundup((off_t)rd_tbuf, 512);

    iov_count = disk_blocksize/disk_pagesize;

    wiov = malloc(sizeof(struct iovec) * iov_count);
    riov = malloc(sizeof(struct iovec) * iov_count);
    if (!wiov || !riov) {
	fprintf(stderr, "Memory allocation failed \n");
	exit(1);
    }

    return;
}   /* allocate_memory */

static int
verify_block(char *wr_buf, char *rd_buf, int len, int block)
{
    uint32_t *wr_ptr, *rd_ptr;

    wr_ptr = (uint32_t *)wr_buf;
    rd_ptr = (uint32_t *)rd_buf;

    while (len > 0) {
	if (*rd_ptr != *wr_ptr) {
	   fprintf(log_fp, "ERROR: Data Mismatch on block: %d "
			   "Expected 0x%08x, Got 0x%08x\n",
			    block, *wr_ptr, *rd_ptr);
	   fflush(log_fp);
	   return -1;
	}
	rd_ptr++; wr_ptr++;
	len -= sizeof(uint32_t);
    }

    return 0;
}   /* verify_block */

static int
read_block(int		block,
	   struct iovec *iov,
	   int		iov_cnt)
{
    int ret = 0;
    off_t nbytes, seek_off;
    off_t tot_bytes = disk_blocksize;

    seek_off = ((off_t)block) * disk_blocksize;
    nbytes = lseek(diag_fd, seek_off, SEEK_SET);
    if (nbytes == seek_off) {
	nbytes = readv(diag_fd, iov, iov_cnt);
	if (nbytes == tot_bytes) {
	    return 0;
	} else if (nbytes <  0) {
	    ret = -errno;
	    fprintf(log_fp, "RAW READ at %ld (expected bytes=%ld) ERROR: %d\n",
		    seek_off, tot_bytes, ret);
	    goto error;
	} else {
	    fprintf(log_fp, "RAW SHORT READ at %ld: expected=%ld got=%ld\n",
		    seek_off, tot_bytes, nbytes);
	    goto error;
	}
    } else if (nbytes == -1) {
	ret = -errno;
	fprintf(log_fp, "RAW SEEK (expected bytes=%ld) ERROR: %d\n",
		seek_off, ret);
	goto error;
    } else {
	fprintf(log_fp, "RAW SHORT SEEK expected=%ld got=%ld\n",
		seek_off, nbytes);
	goto error;
    }
    return 0;

error:
    fflush(log_fp);
    return ret;
}   /* read_block */

static int
write_block(int		  block,
	    struct iovec  *iov,
	    int		  iov_cnt)
{
    int ret = 0;
    off_t nbytes, seek_off;
    off_t tot_bytes = disk_blocksize;

    seek_off = ((off_t)block)*disk_blocksize;

    nbytes = lseek(diag_fd, seek_off, SEEK_SET);
    if (nbytes == seek_off) {
	nbytes = writev(diag_fd, iov, iov_cnt);
	if (nbytes == -1) {
	    ret = -errno;
	    fprintf(log_fp, "RAW WRITE failed at %ld: %d\n", seek_off, ret);
	    goto error;
	} else if (nbytes != tot_bytes) {
	    fprintf(log_fp, "RAW SHORT WRITE at %ld: expected=%ld wrote=%ld\n",
		    seek_off, tot_bytes, nbytes);
	    goto error;
	} else {
	    return 0;
	}
    } else if (nbytes == -1) {
	ret = -errno;
	fprintf(log_fp, "RAW SEEK (expected bytes=%ld) ERROR: %d\n",
			seek_off, ret);
	goto error;
    } else {
	fprintf(log_fp, "RAW SHORT SEEK expected=%ld got=%ld\n",
			seek_off, nbytes);
	goto error;
    }
error:
    fflush(log_fp);
    return ret;
}   /* write_block */

static void
gen_write_iov(char   *wr_buf,
	      struct iovec *iov,
	      int    mode,
	      uint32_t off,
	      int    inverse)
{
    char* tbuf;
    uint32_t *tbuf1;
    int32_t pattern;
    int i, j;

    for (i = 0; i < (disk_blocksize/disk_pagesize); i++) {
	if (mode == PATTERN_FIXED) {
	    if (i%2)
		pattern = 0xaa55aa55;
	    else
		pattern = 0x55aa55aa;
	} else if (mode == PATTERN_ADDRESS) {
	    pattern = off+i;
	    if (inverse)
		pattern = ~pattern;
	}
	tbuf = wr_buf + (i * disk_pagesize);
	tbuf1 = (uint32_t *)tbuf;
	for (j = 0; j < (disk_pagesize/(int)(sizeof(int32_t))); j++) {
	   *tbuf1++ = pattern;
	}
    }

    for (i = 0; i < (disk_blocksize/disk_pagesize); i++) {
	iov[i].iov_base = wr_buf + (i * disk_pagesize);
	iov[i].iov_len  = disk_pagesize;
    }

    return;
}   /* gen_write_iov */

static void
gen_read_iov(char	  *rd_buf,
	     struct iovec *iov)
{
    int i;

    memset(rd_buf, 0, disk_blocksize);
    for (i = 0; i < (disk_blocksize/disk_pagesize); i++) {
	iov[i].iov_base = rd_buf + (i * disk_pagesize);
	iov[i].iov_len  = disk_pagesize;
    }
    return;
}   /* gen_read_iov */

static void
init_progressbar(prg_bar_t *pbs_in, const char *str)
{
    /* Init the bar settings */
    defaultprogressbar(pbs_in);
    pbs_in->max = nblocks;
    pbs_in->width = 55;
    printf("%s", str);
    /* Show the empty bar */
    printprogressbar(0, pbs_in);
}   /* init_progressbar */

static void
init_display(int test_mode)
{
    FILE *fp = NULL;
    int i;

    for (i=0; i<2; i++) {

	/* Log to file if a log file is available */
	if (i == 0)
	    fp = stderr;
	else if (log_fp != stderr)
	    fp = log_fp;
	else
	    return;

	fprintf(fp, "NAND Diagnostic Utility\n");
	fprintf(fp, "-----------------------\n");

	fprintf(fp, "Device: %s\t\t", dev_name);
	fprintf(fp, "Capacity :%ld bytes\n",
			    ((off_t)disk_blocksize)*nblocks);
	fprintf(fp, "Blocksize :%d bytes\t\t", disk_blocksize);
	fprintf(fp, "Pagesize :%d bytes\n", disk_pagesize);
	fprintf(fp, "Total Blocks : %d\n", nblocks);
	switch(test_mode) {
	    case SEQ_FIXED:
		fprintf(fp, "Test: Write/Read a fixed pattern sequentially\n");
		break;
	    case SEQ_RANDOM:
		fprintf(fp, "Test: Write address as data sequentially, "
				"Verify all blocks\n");
		fprintf(fp, "      Write the inverse of address as data "
				"sequentially, Verify all blocks\n");
		break;
	    case SEQ_RANDOM_1PASS:
		fprintf(fp, "Test: Write address as data sequentially, "
				"Verify all blocks\n");
		break;
	    case RAN_FIXED:
		fprintf(fp, "Test: Write/Read address as data sequentially\n");
		break;
	    case RAN_RANDOM:
		fprintf(fp, "Test: Write address as data on random blocks "
				"and then verify all the blocks\n");
		break;
	}
	fprintf(fp, "\n");
    }
}   /* init_display */

static void
print_usage(char* progname)
{
    fprintf(stderr,
	    "Usage: %s [options]\n"
	    "  -b <disk block size in bytes>\n"
	    "  -d <device name>\n"
	    "  -f <log file>\n"
	    "  -m <test mode>\n"
	    "  -n <number of blocks>\n"
	    "  -p <disk page size in bytes>\n"
	    "  -s <start block>\n"
	    "  -t <millisecond sleep between I/Os>\n"
	    , progname);
    exit(1);
}   /* print_usage */

static int
do_seq_fixed_test(char *wr_buf,
		  struct iovec *wr_iov,
		  char *rd_buf,
		  struct iovec *rd_iov,
		  int iov_cnt,
		  int start_block,
		  int num_blocks)
{
    int i, ret = 0;
    int bar_pos = num_blocks / 100;

    if (!bar_pos)
	bar_pos = 1;

    init_display(SEQ_FIXED);

    init_progressbar(&pbs, "Write & Verify : ");
    for (i=start_block; i < (start_block + num_blocks); i++) {
        gen_write_iov(wr_buf, wr_iov, PATTERN_FIXED, 0, 0);
        if ((ret = write_block(i, wr_iov, iov_cnt)) < 0)
            break;
        gen_read_iov(rd_buf, rd_iov);
        if ((ret = read_block(i, rd_iov, iov_cnt)) < 0)
            break;
        if ((ret = verify_block(wr_buf, rd_buf, disk_blocksize, i)) < 0)
            break;
	if (((i-start_block) % bar_pos) == 0)
	    printprogressbar(i-start_block, &pbs);
	if (sleep_time)
	    usleep(sleep_time);
    }
    printprogressbar(i-start_block, &pbs);
    if ((i-start_block) == num_blocks)
        puts(" Pass");
    else
        puts(" Failed");
    return ret;
}   /* do_seq_fixed_test */

static int
do_seq_random_test(char *wr_buf,
                  struct iovec *wr_iov,
                  char *rd_buf,
                  struct iovec *rd_iov,
                  int iov_cnt,
                  int start_block,
                  int num_blocks,
		  int inverse)
{
    int i, ret = 0;
    int bar_pos = num_blocks / 100;

    if (!bar_pos)
	bar_pos = 1;

    if (inverse)
	init_display(SEQ_RANDOM);
    else
	init_display(SEQ_RANDOM_1PASS);

    init_progressbar(&pbs, "Write  : ");
    /* Write the address as data */
    for (i=start_block; i < (start_block + num_blocks); i++) {
        gen_write_iov(wr_buf, wr_iov, PATTERN_ADDRESS, i, 0);
        if ((ret = write_block(i, wr_iov, iov_cnt)) < 0)
            break;
	if (((i-start_block) % bar_pos) == 0)
	    printprogressbar(i-start_block, &pbs);
	if (sleep_time)
	    usleep(sleep_time);
    }
    printprogressbar(i-start_block, &pbs);
    if ((i-start_block) == num_blocks)
        puts(" Pass");
    else
        puts(" Failed");

    if (ret)
	goto error;

    init_progressbar(&pbs, "Verify : ");
    for (i=start_block; i < (start_block + num_blocks); i++) {
        gen_write_iov(wr_buf, wr_iov, PATTERN_ADDRESS, i, 0);
        gen_read_iov(rd_buf, rd_iov);
        if ((ret = read_block(i, rd_iov, iov_cnt)) < 0)
            break;
        if ((ret = verify_block(wr_buf, rd_buf, disk_blocksize, i)) < 0)
            break;
	if (((i-start_block) % bar_pos) == 0)
	    printprogressbar(i-start_block, &pbs);
	if (sleep_time)
	    usleep(sleep_time);
    }
    printprogressbar(i-start_block, &pbs);
    if ((i-start_block) == num_blocks)
        puts(" Pass");
    else
        puts(" Failed");

    if (ret)
	goto error;

    if (inverse) {
	init_progressbar(&pbs, "Write  : ");
	/* Write the inverse of address as data */
	for (i=start_block; i < (start_block + num_blocks); i++) {
	    gen_write_iov(wr_buf, wr_iov, PATTERN_ADDRESS, i, 1);
	    if ((ret = write_block(i, wr_iov, iov_cnt)) < 0)
		break;
	    if (((i-start_block) % bar_pos) == 0)
		printprogressbar(i-start_block, &pbs);
	    if (sleep_time)
		usleep(sleep_time);
	}
	printprogressbar(i-start_block, &pbs);
	if ((i-start_block) == num_blocks)
	    puts(" Pass");
	else
	    puts(" Failed");

	if (ret)
	    goto error;

	init_progressbar(&pbs, "Verify : ");
	for (i=start_block; i < (start_block + num_blocks); i++) {
	    gen_write_iov(wr_buf, wr_iov, PATTERN_ADDRESS, i, 1);
	    gen_read_iov(rd_buf, rd_iov);
	    if ((ret = read_block(i, rd_iov, iov_cnt)) < 0)
		break;
	    if ((ret = verify_block(wr_buf, rd_buf, disk_blocksize, i)) < 0)
		break;
	    if (((i-start_block) % bar_pos) == 0)
		printprogressbar(i-start_block, &pbs);
	    if (sleep_time)
		usleep(sleep_time);
	}
	printprogressbar(i-start_block, &pbs);
	if ((i-start_block) == num_blocks)
	    puts(" Pass");
	else
	    puts(" Failed");
    }
 error:
    return ret;
}   /* do_seq_random_test */

#if 0
void
do_ran_fixed_test(char *wr_buf,
                  struct iovec *wr_iov,
                  char *rd_buf,
                  struct iovec *rd_iov,
                  int iov_cnt,
                  int start_block,
                  int num_blocks)
{
    int i;
    init_display(&pbs);

        gen_write_iov(wr_buf, wr_iov, PATTERN_FIXED);
        if (write_block(i*disk_blocksize, wr_iov, iov_cnt) < 0)
            break;
        gen_read_iov(rd_buf, rd_iov);
        if (read_block(i*disk_blocksize, rd_iov, iov_cnt) < 0)
            break;
        if (verify_block(wr_buf, rd_buf, disk_blocksize) < 0)
            break;
        printprogressbar(i, &pbs);
	if (sleep_time)
	    usleep(sleep_time);
    }
    printprogressbar(i, &pbs);
    if (i == num_blocks)
        puts(" Pass");
    else
        puts(" Failed");
    return;
}
#endif
static int
get_random_block(int ceiling)
{
    return  (1 + (int) ((ceiling * 1.0) * (rand() / (RAND_MAX + 1.0))));
}   /* get_random_block */

static void
reset_random_seed(void)
{
    srand(1);
}   /* reset_random_seed */

static int
do_ran_random_test(char *wr_buf,
                  struct iovec *wr_iov,
                  char *rd_buf,
                  struct iovec *rd_iov,
                  int iov_cnt,
                  int end_block,
                  int num_blocks)
{
    int i, block, ret = 0;
    int bar_pos = num_blocks / 100;

    if (!bar_pos)
	bar_pos = 1;

    init_display(RAN_RANDOM);

    init_progressbar(&pbs, "Write  : ");

    for (i=0; i < num_blocks; i++) {
	block = get_random_block(end_block);
        gen_write_iov(wr_buf, wr_iov, PATTERN_ADDRESS, block, 0);
        if ((ret = write_block(i, wr_iov, iov_cnt)) < 0)
            break;
	if (((i-st_block) % bar_pos) == 0)
	    printprogressbar(i, &pbs);
	if (sleep_time)
	    usleep(sleep_time);
    }
    printprogressbar(i, &pbs);
    if (i == num_blocks)
        puts(" Pass");
    else
        puts(" Failed");

    if (ret)
	goto error;

    init_progressbar(&pbs, "Verify : ");

    reset_random_seed();

    for (i=0;i < num_blocks; i++) {
	block = get_random_block(end_block);
        gen_write_iov(wr_buf, wr_iov, PATTERN_ADDRESS, block, 0);
        gen_read_iov(rd_buf, rd_iov);
        if ((ret = read_block(i, rd_iov, iov_cnt)) < 0)
            break;
        if ((ret = verify_block(wr_buf, rd_buf, disk_blocksize, i)) < 0)
            break;
	if (((i-st_block) % bar_pos) == 0)
	    printprogressbar(i, &pbs);
	if (sleep_time)
	    usleep(sleep_time);
    }
    printprogressbar(i, &pbs);
    if (i == num_blocks)
        puts(" Pass");
    else
        puts(" Failed");
 error:
    return ret;
}   /* do_ran_random_test */

/* Main Function */
int
main(int argc,
     char *argv[])
{
    char *progname;
    uint32_t tdisk_pagesize, tdisk_blocksize;
    int i, cnt, ch, start_block_flag = 0, test_mode = -1;
    int log_file = 0, ret = 0;

    progname = argv[0];
    if (argc < 6)
        print_usage(progname);

    while ((ch = getopt(argc, argv, "b:d:f:m:n:p:s:t:")) != -1) {
        switch (ch) {
	    case 'b':
		disk_blocksize = atol(optarg);
		break;
            case 'd':
		strcpy(dev_name, optarg);
                break;
	    case 'f':
		strcpy(log_file_name, optarg);
		log_file++;
		break;
	    case 'n':
		nblocks = atol(optarg);
		break;
	    case 'm':
		test_mode = atol(optarg);
		break;
            case 'p':
		disk_pagesize = atol(optarg);
                break;
            case 's':
		st_block =  atol(optarg);
		start_block_flag++;
                break;
	    case 't':
		/* Will be calling usleep, so make this millisecond count */
		sleep_time = 1000*atoi(optarg);
		break;
            default:
                print_usage(progname);
                break;
        }
    }

    if (!disk_pagesize || !disk_blocksize) {
	/* Use 256K/4K layout as default */
	disk_blocksize = 256*1024;
	disk_pagesize = 4*1024;
    }

    if (!nblocks) {
	fprintf(stderr, "Number of blocks should be specified\n");
	print_usage(progname);
	exit (1);
    }

    if (!start_block_flag) {
	fprintf(stderr, "Start block should be set\n");
	print_usage(progname);
	exit (1);
    }

    if (test_mode == -1) {
	fprintf(stderr, "Test Mode should be specified\n");
	print_usage(progname);
	exit (1);
    }

    /* Validate the Page Size */
    cnt = 0;
    tdisk_pagesize = disk_pagesize;
    for (i = 0; i < 32; i++) {
        if ((tdisk_pagesize & 0x1) == 0x1)
            cnt++;
        tdisk_pagesize >>= 1;
    }
    if (cnt != 1) {
        fprintf(stderr, "Disk pagesize should be power of 2\n");
        exit(1);
    }
    if (disk_pagesize < 4*1024 || disk_pagesize > 64*1024) {
        fprintf(stderr, "Disk pagesize should be 4KiB <= X <= 64KiB\n");
        exit(1);
    }

    /* Validate the Block size */
    cnt = 0;
    tdisk_blocksize = disk_blocksize;
    for (i = 0; i < 32; i++) {
        if ((tdisk_blocksize & 0x1) == 0x1)
            cnt++;
        tdisk_blocksize >>= 1;
    }
    if (cnt != 1) {
        fprintf(stderr, "Disk blocksize should be power of 2\n");
        exit(1);
    }
    if (disk_blocksize < 256*1024 || disk_blocksize > 2*1024*1024) {
        fprintf(stderr, "Disk blocksize should be 256KiB or 2MiB\n");
        exit(1);
    }

    if (log_file) {
	log_fp = fopen(log_file_name, "w+");
	if (log_fp == NULL) {
	    fprintf(stderr, "Failed to open log file %s : %d",
			    log_file_name, errno);
	    exit(1);
	}
    } else {
	log_fp = stderr;
    }

    /* Attempt to open the device */
    diag_fd = open(dev_name, O_RDWR | O_DIRECT);
    if (diag_fd < 0) {
	fprintf(stderr, "Failed to open device %s : %d", dev_name, errno);
	exit(1);
    }

    allocate_memory();

    switch(test_mode) {
	case SEQ_FIXED:
	    ret = do_seq_fixed_test(wbuf, wiov, rbuf, riov,
				    iov_count, st_block, nblocks);
	    break;
	case SEQ_RANDOM:
	    ret = do_seq_random_test(wbuf, wiov, rbuf, riov,
			      iov_count, st_block, nblocks, 1);
	    break;
#if 0
	case RAN_FIXED:
	    do_ran_fixed_test(wr_buf, wr_iov, rd_buf, rd_iov,
			      iov_cnt, start_block, num_blocks);
	    break;
#endif
	case RAN_RANDOM:
	    ret = do_ran_random_test(wbuf, wiov, rbuf, riov,
			      iov_count, st_block, nblocks);
	    break;
	case SEQ_RANDOM_1PASS:
	    ret = do_seq_random_test(wbuf, wiov, rbuf, riov,
			      iov_count, st_block, nblocks, 0);
	default:
	    break;
    }

    close(diag_fd);
    fprintf(log_fp, "STATUS: %s %s\n", dev_name,
	    ret == 0 ? "success" : "failed");
    return ret;
}   /* main */
