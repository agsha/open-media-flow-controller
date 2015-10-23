#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//#include "nkn_mem_counter_def.h"
#include "ds_glob_ctx.h"
#include "ds_api.h"
#include "ds_config.h"

/*Should be defined with all other global variables*/
ds_glob_ctx_t * ds_glob_ctx;

/*
typedef struct cfg_parms{
    char* domain_name;
    char* cg_name;
    uint32_t numCE;
    char** ce_names;
    uint32_t numPOP;
    char** pop_names;
}config_parms_t;
*/

static uint64_t get_file_size(FILE* fid);
static char* get_entity_name(uint32_t start_pos, uint32_t end_pos, char* buff);

#if 0 

int32_t main(int32_t argc, char *argv[]) {

    init_dstore();
    char* config_fname = argv[1];
    void* cg =NULL;
    config_parms_t* cfg = (config_parms_t*)nkn_calloc_type(1,sizeof(config_parms_t),  mod_cr_ds);
    parse_config_file(config_fname,cfg);
    configure_all_entities(cfg);
    //    lookup_dns_cg(cfg->domain_name,&cg);
    //    lookup_dns(char* dns, char* ip, void** domain_ds);
    return 0;
}

#endif


int32_t configure_all_entities(config_parms_t* cfg){
    uint32_t i;
    char lon[] = "100";
    char lat[] = "90";
    /*Create Domain*/
    update_attr_domain(cfg->domain_name, NULL, 0, 0, 
		       NULL, NULL, 0.0, 0.0, 0);
    /*Configure CG*/
    create_new_CG(cfg->cg_name);

    /*Add CG to domain*/
    add_cg_to_domain(cfg->domain_name, cfg->cg_name);
    /*Create CE , add to CG*/
    for(i=0; i<cfg->numCE;i++){
	create_new_CE(cfg->ce_names[i], cfg->ip_address[i], 
		      ce_addr_type_ipv4, 1, 2000, 2012, 90);
	add_CE_to_CG(cfg->cg_name,cfg->ce_names[i]);
    }
    /*Create POP and add CE to it*/
    for(i=0; i<cfg->numPOP;i++){
	create_new_POP(cfg->pop_names[i], lon, lat);
	map_CE_to_POP(cfg->pop_names[i],cfg->ce_names[i]);
    }
    //    add_domain_to_CG(cfg->cg_name, cfg->domain_name);
    return 0;
}

int32_t parse_config_file(char* config_fname,
			  config_parms_t *parms){
    FILE* fid;
    uint64_t fsize;
    char* buff, *domain_name, *cg_name;
    char **ce_name, **pop_name,**ip_address;
    uint32_t i,j,pos, num_CEs =0, num_POP=0,num_ip=0, pos1;
    size_t rv = 0;

    ce_name = (char**)calloc(1,100*sizeof(char*));
    pop_name=(char**)calloc(1,100*sizeof(char*));
    ip_address=(char**)calloc(1,100*sizeof(char*));
    fid = fopen(config_fname, "r");
    if(fid ==NULL){
	printf("Unable to open file %s\n", config_fname);
	return -1;
    }
    fsize = get_file_size(fid) + 1;
    buff = calloc(1, fsize);
    rv = fscanf(fid, "%s", buff);
    fclose(fid);
    /*Get domain name*/
    i = strcspn(buff,",");
    domain_name = get_entity_name(0, i-1, buff);
    /*Get CG name*/
    j = strcspn(&buff[i+1],",");
    cg_name = get_entity_name(0,j-1, &buff[i+1]);
    /*Get List of CEs*/
    pos = i+j;
    i = strcspn(&buff[pos+2],"}");
    pos+=3;
    pos1 = pos+i;
    for(;;){
	j = strcspn(&buff[pos],",");
	if(pos+j>=pos1)
	    j-=1;
	ce_name[num_CEs++] = get_entity_name(0, j-1,&buff[pos]);
	pos+= j+1;
	if(pos>=pos1)
	    break;

    }
    /*Get List of POPs*/
    j=pos1+2;//account for ",{"
    i = strcspn(&buff[j],"}");
    pos = j;
    pos1 = pos+i;
    for(;;){
	j = strcspn(&buff[pos],",");
        if(pos+j>=pos1)
	    j-=1;
	pop_name[num_POP++] = get_entity_name(0, j-1,&buff[pos]);
        pos+= j+1;
        if(pos>=pos1)
            break;
    }
    /*Get List ip address*/
    j=pos+2;//account for ",{"
    i = strcspn(&buff[j],"}");
    pos = j;
    pos1 = pos+i;
    for(;;){
        j = strcspn(&buff[pos],",");
        if(pos+j>=pos1)
            j-=1;
        ip_address[num_ip++] = get_entity_name(0, j-1,&buff[pos]);
        pos+= j+1;
        if(pos>=pos1)
            break;
    }
    parms->domain_name = domain_name;
    parms->cg_name = cg_name;
    parms->numCE = num_CEs;
    parms->numPOP = num_POP;
    parms->num_ip = num_ip;
    parms->ce_names = ce_name;
    parms->pop_names = pop_name; 
    parms->ip_address = ip_address;
    return 0;
}


static char* get_entity_name(uint32_t start_pos, uint32_t end_pos, char* buff){
    char* name =  calloc(1,end_pos-start_pos+2);
    strncpy(name,&buff[start_pos],end_pos-start_pos+1);
    return name;

}




static uint64_t get_file_size(FILE* fid){
    uint64_t lSize;
    fseek (fid , 0 , SEEK_END);
    lSize = ftell(fid);
    rewind(fid);
    return lSize;
}
