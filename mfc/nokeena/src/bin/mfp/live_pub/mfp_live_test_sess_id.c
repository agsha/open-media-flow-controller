/* To compile standalone: gcc -std=c99 -g mfp_live_test_sess_id.c mfp_live_sess_id.c mfp_live_conf.c -o sess_id_test -lpthread -lm */
#include <stdio.h>
#include <math.h>

#include "mfp_live_sess_id.h"

#define MAX_SESSIONS 100
#ifdef LIVE_SESSID_TEST
/*******************************************************
 *                GLOBAL VARIABLES
 ******************************************************/
/* live session id global variables */
int32_t rc = -1;
live_id_t* live_state_cont;


/* live session id test suite prototypes */
static int32_t sessionCounter(live_id_t* live_state_cont,
			      int32_t max_session);
static int32_t test01_liveSessionID(void);
static int32_t test02_liveSessionID(void);
static int32_t test03_liveSessionID(void);
static int32_t test04_liveSessionID(void);
static int32_t testSuite_liveSessionID(void);
static void initTestVars_liveSessionID(void);

static void initTestVars_liveSessionID(void)
{
	live_state_cont = newLiveIdState(MAX_SESSIONS);
}

static int32_t test01_liveSessionID(void)
{
	int i = 0;
	printf("---------------------------------------------------\n");
	printf("TEST 1: Normal 'x' session insertion test\n");
	// Check the return values of the ids : Incr Seq
	for (; i < MAX_SESSIONS; i++) {
		rc = live_state_cont->insert(live_state_cont, NULL, NULL);
		printf("%d ", rc);
		if (((i+1) % 20) == 0)
			printf("\n");
		live_state_cont->acquire_ctx(live_state_cont, i);
		live_state_cont->release_ctx(live_state_cont, i);
	}
	printf("Number of active sessions: %d\n",
			sessionCounter(live_state_cont, MAX_SESSIONS));
	printf("---------------------------------------------------\n\n");
	return 0;
}

static int32_t test02_liveSessionID(void)
{
	printf("---------------------------------------------------\n");
	printf("TEST 2: Boundary check: Inserting more than max session\n");
	// Check the return values of the ids : Incr Seq
	rc = live_state_cont->insert(live_state_cont, NULL, NULL);
	printf("Insert Status: %d ", rc);
	printf("Number of active sessions: %d\n",
			sessionCounter(live_state_cont, MAX_SESSIONS));
	printf("---------------------------------------------------\n\n");

	printf("---------------------------------------------------\n");
	return 0;
}

static int32_t test03_liveSessionID(void)
{
	int32_t i;
	
	printf("TEST 3: 'x' session removal test\n");
	// Randomly remove 10 random sessions
	int32_t sess_id; 
	srand(1000);
	for (i = 0; i < 10; i++) {
		sess_id = (int32_t)(rand() * (((double)(MAX_SESSIONS))/RAND_MAX));
		printf("Removing Session with id: %d\n", sess_id);
		live_state_cont->remove(live_state_cont, sess_id);

	}
	printf("Number of active sessions: %d\n",
			sessionCounter(live_state_cont, MAX_SESSIONS));
	printf("---------------------------------------------------\n\n");

	printf("---------------------------------------------------\n");

	return 0;

}

static int32_t test04_liveSessionID(void)
{
	int32_t i;
	printf("TEST 4: Insert 'x' sessions after removal test\n");

	for (i = 0; i < 20; i++) {
		rc = live_state_cont->insert(live_state_cont, NULL, NULL);
		printf("Insert Status: %d\n", rc);
	}
	printf("Number of active sessions: %d\n",
			sessionCounter(live_state_cont, MAX_SESSIONS));
	printf("---------------------------------------------------\n\n");
	live_state_cont->delete_live_id(live_state_cont);
	return 0;


}

static int32_t testSuite_liveSessionID(void) 
{
	test01_liveSessionID();
	test02_liveSessionID();
	test03_liveSessionID();
	test04_liveSessionID();

	return 0;
}

int main(void) 
{
	initTestVars_liveSessionID();
	testSuite_liveSessionID();
}


static int32_t sessionCounter(live_id_t* live_state_cont1,
			      int32_t max_sess) 
{
	int32_t i, ctr = 0;
	for (i = 0; i < max_sess; i++)
		if (live_state_cont1->flag[i] != -1)
			ctr++;
	return ctr;
}

#endif //LIVE_SESSID_TEST

