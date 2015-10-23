/*
 * dump_ptrie_ckpt.c -- Dump persistent trie checkpoint file
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <fcntl.h>

#include "ptrie/persistent_trie.h"

#include <getopt.h>


char *option_str = "f:hv";

static struct option long_options[] = {
    {"file", 1, 0, 'f'},
    {"help", 0, 0, 'h'},
    {"verbose", 0, 0, 'v'},
    {0, 0, 0, 0}
};

const char *helpstr = (
"Dump Ptrie checkpoint file.\n"
"Options:\n"
"\t-f --file\n"
"\t-h --help\n"
"\t-v --verbose\n"
);

char *opt_filename;
int opt_verbose;

static void
print_file_header(const file_header_t *fh, off_t foff)
{
    int n;

    printf("FileHeader(off=0x%lx):\n \tvers=%d pad1=%d magic=0x%lx seqno=%ld"
    	   "\n\tts.tv_sec=%ld ts.tv_nsec=%ld filesize=%ld\n\t",
	   foff, fh->u.hdr.version, fh->u.hdr.pad1, fh->u.hdr.magicno,
	   fh->u.hdr.seqno, fh->u.hdr.ts.tv_sec, fh->u.hdr.ts.tv_nsec,
	   fh->u.hdr.filesize);
    for (n = 0; n < sizeof(fh_user_data_t)/sizeof(long); n++) {
    	if (!n || (n % 3)) {
	    printf("ud.data[%d]=0x%lx ", n, fh->u.hdr.ud.data[n]);
	} else {
	    printf("\n\tud.data[%d]=0x%lx ", n, fh->u.hdr.ud.data[n]);
	}
    }
    printf("\n");
}

static void
print_file_node(const file_node_data_t *fn, off_t foff)
{
    int n;

    printf("%s off=0x%lx\n", fn->key, foff);
    printf("\tdh.magicno=0x%lx dh.loff=0x%lx dh.key_strlen=%d\n\t"
    	   "dh.pad=%d dh.incarnation=%ld dh.flags=0x%lx\n\t",
	   fn->nd.dh.magicno, fn->nd.dh.loff, fn->nd.dh.key_strlen,
	   fn->nd.dh.pad, fn->nd.dh.incarnation, fn->nd.dh.flags);
    for (n = 0; n < sizeof(app_data_t)/sizeof(long); n++) {
    	if (!n || (n % 4)) {
	    printf("ad.d[%d]=0x%lx ", n, fn->nd.ad.d[n]);
	} else {
	    printf("\n\tad.d[%d]=0x%lx ", n, fn->nd.ad.d[n]);
	}
    }
    printf("\n\n");
}

int dump_ckpt_file(const char *fname)
{
    int fd = -1;
    struct stat sbuf;
    char *fmap = 0;
    file_node_data_t *fn;
    file_header_t *filehdr_1;
    file_header_t *filehdr_2;
    int ret;
    int rv = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////
	
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
	perror("open()");
	rv = 1;
	break;
    }
    ret = fstat(fd, &sbuf);
    if (ret) {
    	perror("fstat()");
    	rv = 2;
	break;
    }

    fmap = mmap((void *) 0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmap == MAP_FAILED) {
    	perror("mmap()");
	rv = 3;
	break;
    }

    filehdr_1 = (file_header_t *)fmap;
    filehdr_2 = (file_header_t *)(fmap + FHDR_2_FOFF);
    print_file_header(filehdr_1, 0);
    printf("\n");
    print_file_header(filehdr_2, FHDR_2_FOFF);

    if (bcmp(filehdr_1, filehdr_2, sizeof(file_header_t)) == 0) {
    	printf("File headers Equal\n\n");
    } else {
    	printf("File headers NOT Equal\n\n");
    }

    for (fn = (file_node_data_t *)(fmap + LOFF2FOFF(0)); 
    	 (char *)fn < &fmap[sbuf.st_size]; fn++) {
    	if (fn->nd.dh.magicno == DH_MAGICNO) {
	    print_file_node(fn, (char *)fn - fmap);
	} else if (fn->nd.dh.magicno == DH_MAGICNO_FREE) { // Deleted entry
	    ;
	} else {
	    printf("Bad magicno=0x%lx fmap=%p fn=%p", 
		   fn->nd.dh.magicno, fmap, fn);
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (fmap && (fmap != MAP_FAILED)) {
    	munmap(fmap, sbuf.st_size);
    }
    if (fd >= 0) {
    	close(fd);
    }
    return rv;
}

int main(int argc, char **argv)
{
    int c;
    int option_index = 0;

    while (1) {
	c = getopt_long(argc, argv, option_str, long_options, &option_index);
	if (c == -1) {
	    break;
	}
	switch (c) {
	case 'f':
	    opt_filename = optarg;
	    break;
	case 'h':
	    printf("[%s]: \n%s\n", argv[0], helpstr);
	    exit(100);
	case 'v':
	    opt_verbose = 1;
    	break;
	case '?':
	    printf("[%s]: \n%s\n", argv[0], helpstr);
	    exit(200);
	}
    }

    if (opt_filename) {
	if (dump_ckpt_file(opt_filename)) {
	    exit(1);
	}
    } else {
	printf("[%s]: \n%s\n", argv[0], helpstr);
	exit(201);
    }
}

/*
 * End of dump_ptrie_ckpt.c
 */
