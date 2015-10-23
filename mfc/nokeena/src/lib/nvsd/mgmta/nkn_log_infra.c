#include <stdio.h>
#include <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "nkn_mgmt_agent.h"
#include "nkn_mgmt_agentP.h"
#include "nkn_defs.h"

NknLogQueue *logQueue;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void NknLogInitQueue(char *k);
int NknIsLogEmpty( const NknLogQueue *lq );

void NknLogInitQueue(char *k)
{
  logQueue = (NknLogQueue *)k;
  logQueue->qBegin = logQueue->qEnd = (char *)(logQueue) + sizeof(NknLogQueue); 
  logQueue->segBegin = (char *)(logQueue) + sizeof(NknLogQueue);
  logQueue->segEnd = logQueue->segMax = (char *)logQueue + SHM_SIZE;
}

void NknLogInit()
{
  int zindex = 0;
  int  shmid = shmget( SHM_BASE_KEY + zindex, SHM_SIZE, IPC_CREAT );
  long startAddr = SHM_BASE_ADDR + zindex * SHM_SIZE;
  char *k = shmat( shmid, (void *)(startAddr), 0 );
  if ( (long)k != startAddr ) {
    perror( "Shared memory attachment failed\n" );
  }
  NknLogInitQueue(k);
}

#define BSZ          4
#define PAD_TO_BLOCK(len) ((len+BSZ-1)/BSZ)*BSZ

static int NknLogGetBlockLen(const NknLogEntry *l, const NknParamValue *p, char nParams)
{
  short k;
  int total_len = sizeof(NknLogEntry) + nParams * sizeof(NknParamShortValue);

  UNUSED_ARGUMENT(l);
  for( k = 0; k < nParams; k++ )
  {
    switch( p[k].name.paramType )
    {
    case NKN_INT_TYPE:
    case NKN_FLOAT_TYPE:
      break;
    case NKN_DOUBLE_TYPE:
      total_len += sizeof(double);
      break;
    case NKN_LONG_TYPE:
      total_len += sizeof(long);
      break;
    case NKN_STR_PTR_TYPE:
      total_len += PAD_TO_BLOCK(strlen(p[k].value.v.strPtr)+1);
      break;
    case NKN_STR_VAL_TYPE:
      // This should never happen
      break;
    
    }
  }
  return total_len;
}

static char *NknLogMalloc(NknLogQueue *lq, int total_len)
{
  char *start = lq->qEnd;
  if( start + total_len > lq->segMax )
  {
    // Wrap-around required
    if ( lq->segBegin + total_len > lq->qBegin )
    {
      // Log Queue allocation failed
      return NULL;
    } else {
      // Wrap around and allocate
      lq->segEnd = lq->qEnd;
      lq->qEnd   = lq->segBegin + total_len;
      ((NknLogEntry *)(lq->segBegin))->msgLen = total_len;
      return lq->segBegin;
    }
  } else {
    // No Wrap-around required
    if ( ( lq->qEnd < lq->qBegin ) && ( lq->qEnd + total_len > lq->qBegin ) )
    {
      // Log Queue allocation failed
      return NULL;
    } else {
      // Allocate
      char *ptr = lq->qEnd;
      lq->qEnd = lq->qEnd + total_len;
      ((NknLogEntry *)ptr)->msgLen = total_len;     
      return ptr;
    }
  }
}

static NknErr NknLogValidateHead( const NknLogQueue *lq )
{
  NknLogEntry *qle = (NknLogEntry *)(lq->qBegin);
  if ( (char *)qle == lq->segEnd ) qle = (NknLogEntry *)lq->segBegin;
  int req_len = qle->msgLen;
  if ( (char *)qle + req_len > lq->segEnd )
    // Error - the log should be contigous
    return NKN_ERR_LOG_INTERNAL_ERROR;
  int queue_len = ( lq->qBegin < lq->qEnd ) ? ( lq->qEnd - lq->qBegin ) 
    : ( lq->qEnd - lq->segBegin + lq->segEnd - lq->segBegin );
  if ( req_len > queue_len )
    // Error - request length is larger than the length in queue
    return NKN_ERR_LOG_INTERNAL_ERROR;
  return NKN_SUCCESS;
}

static char *NknLogFree(NknLogQueue *lq)
{
  char *qle = lq->qBegin;

  int status = NknLogValidateHead( logQueue );
  if ( status != NKN_SUCCESS ) return NULL;

  if ( qle == lq->qEnd ) return NULL;
  if ( qle == lq->segEnd ) {
    qle = lq->segBegin;
    lq->segEnd = lq->segMax;
  }

  int msgLen = ((NknLogEntry *)qle)->msgLen;
  lq->qBegin = qle + msgLen;
  return qle;
}
  
static void NknFillLog(char *dst, int msgLen, 
		       const NknLogEntry *srcLE, const NknParamValue *srcParams, char nParams)
{
  NknLogEntry *dstLE = (NknLogEntry *)dst;
  *dstLE = *srcLE;
  dstLE->msgLen = msgLen;

  NknParamShortValue *dstParams = (NknParamShortValue *)(dst + sizeof(NknLogEntry));
  char *dstStrs = (char *)(dstParams + nParams);

  int i;
  // Copy the parameter names and thier states
  for( i = 0; i < nParams; i++ )
  {
    dstParams[i].name = srcParams[i].name;
    dstParams[i].name.numParams = nParams;
    switch(dstParams[i].name.paramType)
    {
    case NKN_INT_TYPE:
      dstParams[i].value.intVal = srcParams[i].value.v.intVal;
      break;    
    case NKN_FLOAT_TYPE:
      dstParams[i].value.floatVal = srcParams[i].value.v.floatVal;
      break;    
    case NKN_LONG_TYPE:
      dstParams[i].value.offsetVal = dstStrs - dst;
      *((long *)dstStrs) = srcParams[i].value.v.longVal;
      dstStrs += sizeof(long);
      break;
    case NKN_DOUBLE_TYPE:
      dstParams[i].value.offsetVal = dstStrs - dst;
      *((double *)dstStrs) = srcParams[i].value.v.doubleVal;
      dstStrs += sizeof(double);
      break;
    case NKN_STR_PTR_TYPE:
      dstParams[i].value.offsetVal = dstStrs - dst;
      strcpy( dstStrs, srcParams[i].value.v.strPtr );
      dstStrs += PAD_TO_BLOCK(strlen(srcParams[i].value.v.strPtr)+1);
      dstParams[i].name.paramType = NKN_STR_VAL_TYPE;
      break;
    case NKN_STR_VAL_TYPE:
      // This should not happen
      break;
    }
  }
}

int NknIsLogEmpty( const NknLogQueue *lq ) 
{
  return ( lq->qBegin == lq->qEnd );
}
  
void NknAppendLog( const NknLogEntry *l, const NknParamValue *p , char np )
{
  // Get the length of data required to store this block of data
  int req_len = NknLogGetBlockLen( l, p, np );
  
  // Lock here
  pthread_mutex_lock( &log_mutex );

  // Allocate space for the log
  char *dst = NknLogMalloc( logQueue, req_len );

  // Fill out the log entry
  if ( dst != NULL )
    NknFillLog( dst, req_len, l, p, np );

  // Unlock here
  pthread_mutex_unlock( &log_mutex );
}

NknErr NknLogReadEntry(char **msg, int *len)
{
  // Lock here
  pthread_mutex_lock( &log_mutex );
  NknLogQueue *lq = logQueue;

  NknErr status = NknLogValidateHead(logQueue);
  if ( status != NKN_SUCCESS ) 
  {
    // Unlock here
    pthread_mutex_unlock( &log_mutex );
    return status;
  }

  if ( !NknIsLogEmpty(lq) )
  {
    NknLogEntry *qle = (NknLogEntry *)(lq->qBegin);
    *len = (qle->msgLen);
    *msg = (char *)nkn_malloc_type(*len, mod_mgmta_charbuf);    
    memcpy( *msg, lq->qBegin, *len );
    NknLogFree(lq);
    // Unlock here 
    pthread_mutex_unlock( &log_mutex );
    return NKN_SUCCESS;
  } else {
    // Unlock here
    pthread_mutex_unlock( &log_mutex );
    return NKN_ERR_LOG_QUEUE_EMPTY;
  }
}

