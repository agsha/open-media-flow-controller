#ifndef NKN_MGMT_AGENTP_H
#define NKN_MGMT_AGENTP_H

#define SHM_BASE_KEY     25
#define SHM_SIZE         0x8000
#define SHM_BASE_ADDR    0xA0000000L
typedef struct {
  char  *qBegin;
  char  *qEnd;
  char  *segBegin;
  char  *segEnd;
  char  *segMax;
} NknLogQueue; 

#endif
