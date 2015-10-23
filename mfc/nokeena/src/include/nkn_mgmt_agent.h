#ifndef NKN_MGMT_AGENT_H
#define NKN_MGMT_AGENT_H

// Error codes used by the module
typedef int NknErr;

#define NKN_SUCCESS                 0
#define NKN_ERR_LOG_QUEUE_FULL     -2
#define NKN_ERR_LOG_QUEUE_EMPTY    -3
#define NKN_ERR_LOG_INTERNAL_ERROR -4
#define NKN_ERR_CFG_INVAL_TYPE     -5
#define NKN_ERR_CFG_NAME_ABSENT    -6
#define NKN_ERR_CFG_PARSE_ERROR    -7

/* Logging and event related macros */

/* Name value creation */
#define NVS( fldName, strVal ) \
  { { g->fldName, NKN_STR_PTR_TYPE }, { .v.strPtr = (char *)strVal } }
#define NVI( fldName, iVal ) \
  { { g->fldName, NKN_INT_TYPE }, { .v.intVal = iVal } }
#define NVF( fldName, fVal ) \
  { { g->fldName, NKN_INT_TYPE }, { .v.floatVal = fVal } }

#define NKN_LOGS( string, moduleId, logLevel )		\
  NKN_LOG( GEN_MSG, moduleId, logLevel, NVS( MSG, string ) )

#define NKN_LOG( stringId, moduleId, logLevel, ... ) \
  { \
    OFF_##stringId##_t *g = & g##stringId ; \
    NknParamValue p[] = { \
       __VA_ARGS__	\
    }; \
    NknLogEntry l = { \
      stringId, \
      moduleId, \
      logLevel	\
     }; \
    NknAppendLog(&l,p,sizeof(p)/sizeof(NknParamValue));	\
  }

typedef char NknLogLevel;
#define NKN_LOG_EMERG    0
#define NKN_LOG_ALERT    1
#define NKN_LOG_CRIT     2
#define NKN_LOG_ERR      3
#define NKN_LOG_WARNING  4
#define NKN_LOG_NOTICE   5
#define NKN_LOG_INFO     6
#define NKN_LOG_DEBUG    7

typedef char NknModuleId;
#define  NKN_MOD_OM      1
#define  NKN_MOD_CM      2
#define  NKN_MOD_DM      3
#define  NKN_MOD_SCHED   4
#define  NKN_MOD_HTTPD   5
#define  NKN_MOD_NVSD    6
#define  NKN_MOD_DBG     7

typedef char NknParamType;
#define NKN_UNKNOWN_TYPE    0
#define NKN_INT_TYPE        1
#define NKN_FLOAT_TYPE      2
#define NKN_STR_VAL_TYPE    3
#define NKN_STR_PTR_TYPE    4
#define NKN_STR_CONST_TYPE  5
#define NKN_DOUBLE_TYPE     6
#define NKN_LONG_TYPE       7
#define NKN_STR_LIST_TYPE   8
#define NKN_FUNC_TYPE       9
#define NKN_CB_FUNC_TYPE    10

// Internal Data Structures

typedef struct {
  short        fieldName;
  NknParamType paramType;
  char         numParams;
} NknParam;

typedef struct {
  union {
    int    intVal;
    float  floatVal;
    long   longVal;
    double doubleVal;
    char   *strPtr;
  } v;
} NknValue;

typedef struct {
  NknParam   name;
  NknValue   value;
} NknParamValue;

typedef union {
    int    intVal;
    float  floatVal;
    int    offsetVal;
} NknValue32;

typedef struct {
  NknParam   name;
  NknValue32 value;
} NknParamShortValue;

typedef struct {
  int            stringId;
  NknModuleId    moduleId;
  NknLogLevel    logLevel;
  short          msgLen;
} NknLogEntry;

extern void NknLogInit(void);
extern void NknAppendLog(const NknLogEntry *l, const NknParamValue *p, char numParams);
extern NknErr NknLogReadEntry(char **msg, int *len);

extern int read_nkn_cfg(char *configfile);
#endif

