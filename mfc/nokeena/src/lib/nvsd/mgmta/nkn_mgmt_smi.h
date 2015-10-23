#ifndef NKN_MGMT_AGENT_SMI_H
#define NKN_MGMT_AGENT_SMI_H

#include "nkn_mgmt_agent.h"

typedef struct {
  short fragBegin;
  short nFrags;   
} NknLogFragOffsets;

typedef struct {
  NknParamType type; 
  union {
    const char *str;
    short fldName;
  }u;
} NknLogFragValues;

typedef struct {
  const char *name;
  NknParamType type;
} NknCfgParam;

typedef struct {
  NknCfgParam  defn;
  void        *pAddr;
} NknCfgParamValue;

extern NknCfgParamValue  nknCfgParams[];
extern NknLogFragOffsets nknLogFragOffsets[];
extern NknLogFragValues  nknLogFragValues[];

#endif
