/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Origin manager header file
 *
 * Author: Hrushikesh Paralikar
 *
*/

#include <stdio.h>
#include <sys/time.h>
#include "nkn_om_api.h"
#include "nkn_global_defs.h"


#define SLEEP_DURATION 0
int OM_get(uri_entry_t requested_object)
{

	


	return 0;
}

void OM_main(void *arg)
{

	struct timespec timetosleep;
	struct timeval temp;
	struct timeval cur_time;
	//time(&timetosleep.tv_sec);
	gettimeofday(&cur_time,NULL);
	int rc;

	while(1)
	{





	//	printf(" THread Created \n" );
		
		

		timetosleep.tv_sec = cur_time.tv_sec + SLEEP_DURATION;
		timetosleep.tv_nsec = (cur_time.tv_usec/1000+ (1000*1000));
		
		/*
		if(element in queue and delay for origin fetch is over)
		{
			BM_put()

		}
		*/

		/*
		if(loop exit condition ( execution finishes, no more logs to process)
		{

			pthread_exit(0);

		}
		*/


		//gettimeofday(&temp,NULL);
		//printf(" time before sleep seconds %ld, micro %ld \n", temp.tv_sec,temp.tv_usec);
	
		pthread_mutex_lock(&OM_queue_mutex);
	
		rc = pthread_cond_timedwait(&OM_queue_cond,&OM_queue_mutex,&timetosleep);
		if (rc == 0) {
			pthread_mutex_unlock(&OM_queue_mutex);
	
			//gettimeofday(&temp,NULL);
        		//printf(" time after sleep seconds %d \n", temp.tv_sec);
		}
		else {
			pthread_mutex_unlock(&OM_queue_mutex);
			 //gettimeofday(&temp,NULL);
                        //printf(" time after sleep seconds %d, micro %ld\n", temp.tv_sec,temp.tv_usec);

			//printf(" Error in wait %d \n",rc);
		}
		pthread_exit(0);


	}

}


int OM_init(OM_data* register_module)
{
	

	TAILQ_INIT(&om_get_queue);

	register_module->OM_get = OM_get;
	register_module->OM_main = OM_main;
	
	pthread_mutex_init(&OM_queue_mutex,0);
	pthread_cond_init(&OM_queue_cond,0);

	return 0;

}



