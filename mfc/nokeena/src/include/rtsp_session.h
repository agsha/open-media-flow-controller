#ifndef RTSP_SESSION__HPP
#define RTSP_SESSION__HPP

#include <sys/queue.h>
#include <sys/time.h>
#include <string.h>

typedef unsigned Boolean;
#define False 0
#define True 1

#define MILLION 1000000


#define MAX_TTL 255

#define MAKE_SOCKADDR_IN(var,adr,prt) /*adr,prt must be in network order*/\
    struct sockaddr_in var;\
    var.sin_family = AF_INET;\
    var.sin_addr.s_addr = (adr);\
    var.sin_port = (prt);
extern uint32_t eth0_ip_address; // network order


typedef void BackgroundHandlerProc(void* clientData, int mask);
typedef void TaskFunc(void* clientData);
typedef void* TaskToken;



/*
 * For handling socket reads in the background:
 * Possible bits to set in "mask".
 */
#define SOCKET_READABLE    (1<<1)
#define SOCKET_WRITABLE    (1<<2)
#define SOCKET_EXCEPTION   (1<<3)
void nkn_register_epoll_read(int socketNum,
                                BackgroundHandlerProc* handlerProc,
                                void* clientData);
void nkn_unregister_epoll_read(int socketNum);

void nkn_doEventLoop(char* watchVariable);

TaskToken nkn_scheduleDelayedTask(int64_t microseconds, TaskFunc* proc, void* clientData);
TaskToken nkn_rtscheduleDelayedTask(int64_t microseconds, TaskFunc* proc, void* clientData);

void nkn_unscheduleDelayedTask(TaskToken prevTask);
void nkn_rtunscheduleDelayedTask(TaskToken prevTask);


#define MAX_SESSION	10
struct rtsp_cb;

typedef struct rtsp_session_t {
        LIST_ENTRY(rtsp_session_t) entries;

	void * fSubsessionsHead[MAX_SESSION];
	unsigned TrackNumber[MAX_SESSION];
        unsigned fSubsessionCounter;

        Boolean fIsSSM;
        char* fStreamName;
        char* fInfoSDPString;
        char* fDescriptionSDPString;
        char* fMiscSDPLines;
        struct timeval fCreationTime;

	float minSubsessionDuration;
	float maxSubsessionDuration;

        unsigned fReferenceCount;
        Boolean fDeleteWhenUnreferenced;
} rtsp_session;

LIST_HEAD(SMS_queue, rtsp_session_t);


rtsp_session * new_rtsp_session( char * streamName,
                     char * info, char * description,
                     Boolean isSSM, char * miscSDPLines);
void free_rtsp_session(rtsp_session * psm);


Boolean sm_addSubsession(struct rtsp_cb * prtsp, rtsp_session * psm, void * subsession);
float sm_testScaleFactor(struct rtsp_cb * prtsp, rtsp_session * psm, float scale);

// a result == 0 means an unbounded session (the default)
// a result < 0 means: subsession durations differ; the result is -(the largest)
// a result > 0 means: this is the duration of a bounded session
//float nkn_sm_duration(struct rtsp_cb * prtsp, rtsp_session * psm);

extern void rtsp_streaminglog_write(struct rtsp_cb * prtsp);

void nkn_updateRTSPCounters(uint64_t bytesSent);

#endif // RTSP_SESSION__HPP
