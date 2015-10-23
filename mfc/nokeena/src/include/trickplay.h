#ifndef NKN_TRICKPLAY
#define NKN_TRICKPLAY

#include "inttypes.h"
#include "nkn_vpe_metadata.h"

typedef struct pktlist{
	int32_t num_pkts;
	uint32_t* pkt_nums;
}pkt_list_t;

pkt_list_t* nkn_vpe_get_first_idr_pkts(uint8_t *index_file_pointer, uint64_t index_file_size, uint32_t pkt_num,int16_t speed,uint8_t mode);

#endif
