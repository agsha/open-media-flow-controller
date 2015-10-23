#ifndef DS_CONFIG_H
#define DS_CONFIG_H

#include <stdint.h>

typedef struct cfg_parms{
    char* domain_name;
    char* cg_name;
    uint32_t numCE;
    char** ce_names;
    uint32_t numPOP;
    char** pop_names;
    uint32_t num_ip;
    char** ip_address;
}config_parms_t;


int32_t parse_config_file(char* config_fname,config_parms_t*);
int32_t configure_all_entities(config_parms_t* cfg);

#endif
