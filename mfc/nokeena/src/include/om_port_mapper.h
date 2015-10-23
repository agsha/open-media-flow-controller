#ifndef __OM_PORT_MAPPER_H__
#define __OM_PORT_MAPPER_H__


#include <errno.h>

typedef void*	pmapper_t;


extern int om_pmapper_delete(pmapper_t context);

extern pmapper_t om_pmapper_new(void);

extern int om_pmapper_init(pmapper_t context);

extern int om_pmapper_deinit(pmapper_t context);

extern int om_pmapper_get_port(pmapper_t context, uint32_t client_ip,
			uint32_t dest_ip, uint16_t dport, uint16_t *port);

extern int om_pmapper_put_port(pmapper_t context, uint32_t client_ip, uint16_t port);

extern int om_pmapper_set_port_base(pmapper_t context, unsigned short port);

extern int om_pmapper_set_port_count(pmapper_t context, unsigned int count);


enum {
    NOERROR = 0,


    ENOPORT = 512,
    EBADPORT,
    ENOCTXT,
    EBADCONTEXT,

};

struct om_portmap_conf {
    uint8_t	    pmapper_disable;
};

extern struct om_portmap_conf	om_pmap_config;

#endif
