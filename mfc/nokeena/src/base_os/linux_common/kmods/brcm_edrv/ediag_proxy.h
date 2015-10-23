#ifndef __EDIAG_PROXY_H__
#define __EDIAG_PROXY_H__

#if (LINUX_VERSION_CODE >= 0x20614)
typedef struct delayed_work work_element_t;
#else
typedef struct work_struct  work_element_t;
#endif


struct proxy_t {
	void                   * host;         /* Pointer to the host struct.
	                                          This field MUST be the first!!!
						  Never move it!!! 
						*/
	work_element_t		 work;         /* work element */
	int sb_id;                             /* status block ID */
	char name[PROXY_NAME_LEN + 1];
};

#endif /* __EDIAG_PROXY_H__ */
