#ifndef THREAD_H
#define THREAD_H

#include <dpi/libafc.h>
#include <dpi/libctl.h>
#include <libwrapper/uwrapper.h>
#include <pthread.h>
#include <pcap.h>


//Structure containing the per thread file pointer and  counters
typedef struct threadinfo
{
	int header_matched;
	FILE *fp;
	unsigned int bytes_written;
	unsigned long long counter;
	unsigned long long num_req_end;
	unsigned long long no_peer_counter;
	unsigned long long num_req;
	unsigned long long num_resp;
	unsigned long long nb_thread_pkts;
} thread_info;


/**
 * Double linked list structure
 */
struct list_entry {
        struct list_entry *next, *prev;
};

/**
 * Number of thread used by DPI engine
 */
#define NUM_THREADS 4 /* must be a power of 2 */

thread_info thread_details[NUM_THREADS];

/* Actual pcap packet information */
typedef struct pkt
{
	pcap_t *pcap;		  /* Pointer to the pcap pkt structure */
	struct dpi_engine_data  *deng_data;  /*DPI engine data pointer */
	//struct pcap_pkthdr *pkthdr; /* Pointer to the pcap header */
	struct timeval ts;
	unsigned int caplen;
	unsigned char *data; /*Actual data from the packet */
}realpkt;

/*
 * Packet with pointer on next packet
 * for list insertion
 */
struct smp_pkt {
        struct list_entry entry; /* list structure */
        realpkt pkt;      /* real packet information */
};

/*
 * List of packets for a thread
 */
struct smp_thread {
	int thread_number;
	pthread_t handle;         /* thread handle */
	pthread_spinlock_t lock;  /* lock for concurrent access */
	struct smp_pkt pkt_head;  /* pointer on first packet */
	unsigned int       core_id;        /* coreid dedicated for this thread */
	unsigned int       queue_id;       /* queue_id dedicated for this thread */
	struct rte_ring   *queue;
};

void init_list_head(struct list_entry *);
int list_empty(struct list_entry *);
void list_add_tail(struct list_entry *, struct list_entry *);
void list_del(struct list_entry *);
unsigned int get_hashkey(unsigned char *, unsigned int);
void *smp_thread_routine(void *);

#endif
