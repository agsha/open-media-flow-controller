#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/epoll.h>
 

struct itimerval tick;
struct itimerval save;

extern void (* so_100ms_timer)(void * tv);

void sig_timer (int nn);
void sig_timer (int nn __attribute((unused)))
{
	static struct timeval tv1={0, 0};
	struct timeval tv2;

	gettimeofday(&tv2, NULL);
	if( abs(tv2.tv_usec - tv1.tv_usec) >= 10000 ) {
		// It is 10 ms.
		so_100ms_timer(&tv2);
	}
	tv1.tv_sec = tv2.tv_sec;
	tv1.tv_usec = tv2.tv_usec;
}
