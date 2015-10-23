#ifndef NKN_SHM_H
#define NKN_SHM_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY	    0x5467
#define SHM_SIZE    256
#define SHM_MAGIC   0xdeaddead

struct nkn_hugepage {
	unsigned int magic;
	int locked;
	int total_huge_pages;
	int nvsd_huge_pages;
	int http_analayer_huge_pages;
};

static int set_huge_pages(char *name, int pages)
{
	int shmid;
	struct nkn_hugepage *p;
	char cmd[256];
	int ret;

	shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
	if (shmid == -1) {
		return -1;
	}

	p = (struct nkn_hugepage *)shmat(shmid, 0 ,0);
	if (p == (void *)-1) {
		return -1;
	}
	if (p->magic != 0xdeaddead) {
		memset(p, 0, sizeof(struct nkn_hugepage));
		p->magic = 0xdeaddead;
		p->locked = 1;
	} else {
		while (p->locked) {
		    sleep(1);
		}
	}
	p->locked = 1;
	if (strcmp(name ,"nvsd") == 0) {
		p->nvsd_huge_pages = pages;
	} else if (strcmp(name, "http_analyzer") == 0) {
		p->http_analayer_huge_pages = pages;
	}
	p->total_huge_pages = p->nvsd_huge_pages + p->http_analayer_huge_pages;
	snprintf(cmd, 256,  "echo %d > /proc/sys/vm/nr_hugepages", (int)p->total_huge_pages);
	ret = system(cmd);
	p->locked = 0;

	return ret;
}

#endif
