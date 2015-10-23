/*
 * content_broker.h -- Content Broker parameters
 */
#ifndef _CONTENT_BROKER_H
#define _CONTENT_BROKER_H

#include <sys/types.h>
#include "ptrie/persistent_trie.h"
#include "cb/xml/OCRP_XMLops.h"

#define CB_TRIE_CKP_1 "/nkn/cb/CB_TRIE_CKP-1.data"
#define CB_TRIE_CKP_2 "/nkn/cb/CB_TRIE_CKP-2.data"

typedef struct OCRP_fh_user_data { // fh_user_data_t overlay
    union {
	struct d_OCRP_fh_user_data_struct {
	    uint64_t OCRP_seqno;
	    uint64_t OCRP_version;
	} d;
	fh_user_data_t fhud;
    } u;
} OCRP_fh_user_data_t;

typedef struct location {
    uint32_t fqdn_ix;
    uint16_t port;
    uint16_t weight;
} location_t;

#define OCRP_AD_TYPE_RECORD 1
#define OCRP_AD_TYPE_NAME   2
#define OCRP_AD_TYPE_NUM    3

#define OCRP_REC_FLAGS_PIN (1 << 0)

typedef struct OCRP_app_data { // app_data_t overlay
    union {
        struct d_OCRP_app_data_struct {
	    uint64_t type;
	    union {
	      	struct record {
		    location_t loc[L_ELEMENT_MAX_TUPLES];
		    uint64_t flags;
		    uint64_t seqno;
		    uint64_t vers;
		    int8_t num_loc;
		    char pad[31]; // pad to 128 bytes
		} rec;
		struct namestr {
		    int8_t nm_strlen;
		    char nm[119]; // null terminated
		} name;
		struct number {
		    int64_t d;
		    char pad[112];
		} num;
	    } u;
	} d;
	app_data_t apd;
    } u;
} OCRP_app_data_t;

/*
 * Content Broker (CB) system init.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
extern int 
cb_init(void);

/*
 * cb_route -- Given URL_abs_path return the associated route data.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */

#define CB_MAX_LOCATIONS L_ELEMENT_MAX_TUPLES
#define CB_FQDN_MAXLEN L_ELEMENT_FQDN_MAXLEN

typedef struct cb_loc_data {
    char fqdn[CB_FQDN_MAXLEN]; // Null terminated
    uint16_t port; // Host byte order
    uint16_t weight;
} cb_loc_data_t;

typedef struct cb_route_data {
    uint64_t options;
    uint64_t seqno;
    uint64_t vers;
    int num_locs;
    cb_loc_data_t loc[CB_MAX_LOCATIONS];
} cb_route_data_t;

#define CB_RT_OPT_PIN (1 << 0) // Pin object

extern int
cb_route(const char *URL_abs_path, int all_locations, cb_route_data_t *cb_rd);

#endif /* _CONTENT_BROKER_H */
/*
 * End of content_broker.h
 */
