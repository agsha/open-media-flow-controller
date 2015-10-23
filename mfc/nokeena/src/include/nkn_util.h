#ifndef NKN_UTIL_H
#define NKN_UTIL_H

#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>

/* Fast Timestamp Counters */

/*
 * monotonically increasing counter tied to the CPU clock (tdtsc).
 * Faster than using gettimeofday since it avoid the system call overhead.
 *
 * From intel.com support:
 *
 * As for the RDTSC synchronization; Linux will automatically synchronize this
 * between processors in a multiprocessor system as part of the normal boot
 * process. When Linux decides to use RDTSC internally to implement
 * gettimeofday(), the effect of Intel(R) SpeedStep Technology as described
 * in the article is automatically compensated for by the kernel; the
 * programmer does not need to do anything extra or otherwise worry about this.
 *
 * So in conclusion: Linux already is using the best of the timers that our
 * platforms offer today, and exposes this via the standard gettimeofday() API
 * to applications without any need for the application to be made aware of
 * new hardware capabilities. This also means that when we make an even better
 * timer available (not sure that is needed; the HPET timer we provide today
 * is very good), only the kernel needs to learn about it but applications do
 * not need to be changed.
 */

__inline static
u_int64_t nkn_get_tick(void)
{
	u_int32_t lo, hi;
       /*
        * cpuid will serialize the following rdtsc with respect to all other
        * instructions the processor may be handling.
        */
       __asm__ __volatile__ (
         "xorl %%eax, %%eax\n"
         "rdtsc\n"
         : "=a" (lo), "=d" (hi)
         :
         : "%ebx", "%ecx");
     return (u_int64_t)hi << 32 | lo;
}


/*
 * Structures and routines used to track latency with CPU ticks.
 *
 * nkn_get_tick() depends on the clock for each CPU be synchronized and
 * running at the same frequency.  To find the second latency, a conversion
 * must be made.
 *
 * total_ticks is the total number of CPU ticks
 * init_ticks is the CPU tick start value
 * count is the count just for the purpose of tracking latency.
 */
typedef struct nkn_latency {
    uint64_t total_ticks;
    uint64_t count;
} nkn_latency_t;

#define LATENCY_INIT(lat, name)						\
    {									\
	(lat).total_ticks = 0;						\
	(lat).count = 0;						\
	nkn_mon_add("latency_cnt", name, (void *)&(lat.count),		\
		    sizeof(lat.count));					\
	nkn_mon_add("latency_ticks", name, (void *)&(lat.total_ticks),	\
		    sizeof(lat.total_ticks));				\
    }

#define LATENCY_START()		nkn_get_tick()
#define LATENCY_END(lat, init_tick)				\
    {								\
	(lat)->total_ticks += (nkn_get_tick() - init_tick);	\
	(lat)->count++;						\
    }

/*
 * Very fast case sensitive comparison for two string.
 * return:
 *    0: p1 and p2 are equal for n bytes.
 *    1: p1 and p2 are not equal.
 */
int nkn_strcmp_case(const char * p1, const char * p2, int n);


/*
 * Very fast case insensitive comparison for two string.
 * return:
 *    0: p1 and p2 are equal for n bytes.
 *    1: p1 and p2 are not equal.
 */
int nkn_strcmp_incase(const char * p1, const char * p2, int n);


/*
 * Convert Hex format IPv4 address into unsigned int IPv4 address
 * the return ipv4 address is in network order.
 */
uint32_t convert_ip_hex_2_network(uint8_t * addr);


/*
 * convert MAC address format
 * from 00:15:17:57:C1:CC to 0x00, 0x15, 0x17, 0x57, 0xC1, 0xCC
 */
int32_t convert_mac_str_to_bin(char * mac_str, uint8_t * mac_bin);

int32_t convert_hexstr_2_int32(char * s);

int64_t convert_hexstr_2_int64(char * s);

/*
 * Iteratively create the full directory using a pessimistic algorithm
 */
int nkn_create_directory(char *dir_path, int  isdir);

/*
 * Do a 64-bit checksum on an arbitrary memory area..
 *
 * This isn't a great routine, but it's not _horrible_ either. The
 * inner loop could be unrolled a bit further, and there are better
 * ways to do the carry, but this is reasonable.
 */
unsigned long do_csum64_aligned(const unsigned char *buff, int len);
unsigned long do_csum64_iterate_aligned(const unsigned char *buff, int len,
					unsigned long old_checksum);
unsigned int do_csum32_iterate_aligned(const unsigned char *buff, int len,
					unsigned int old_checksum);
/*
 * New efficient checksum algorithms used for version 3 disk extents.
 */
unsigned int do_csum32_iterate_aligned_v3(const unsigned char *buff, int len,
					  unsigned int old_checksum);

/*
 * Use to get transaction id which is not the same as pid or pthread_id
 */
#ifndef __USE_MISC
int syscall(int number, ...); /* Prototype as in the man page */
#endif
#define gettid() syscall(__NR_gettid)

#endif	/* NKN_UTIL_H */
