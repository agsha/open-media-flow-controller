/*
 * (C) Copyright 2011 Juniper Networks
 */
#ifndef __TM_H__
#define __TM_H__

#include <glib.h>

#define MAX_LINE_LEN 1000

#define UPDATE_INTERVAL 10  // in seconds 

#define CONF_FILE "/config/nkn/tm.conf"

#define MAX_NAME_STR_LEN    100
#define MAX_IP_STR_LEN      50

#define WHITE_LIST_BUF_SIZE 100000 //100 K
#define URL_BUF_SIZE        100

#define MFC_KEYWORD  "MFC"
#define NS_KEYWORD  "NS"
#define IP_LIST     "ip-list"

typedef struct ip_network_t_ {
    char network_str[MAX_IP_STR_LEN + 1];  
    struct timespec timestamp;
} ip_network_t;

/* MFC data structure associated with a namespace */
typedef struct ns_mfc_ {
    char mfc_name[MAX_NAME_STR_LEN + 1];
    GList *white_list;        
} ns_mfc_t;

typedef struct ns_st_ {
    char name[MAX_NAME_STR_LEN + 1];     
    ns_mfc_t mfc_st;  // one namespace could be assigned to multiple mfcs. Use GList later.                                           
} ns_st_t;

/* traffic director main data structure */
typedef struct mfc_st_ {
    char name[MAX_NAME_STR_LEN + 1];                 
    char ip_str[MAX_IP_STR_LEN + 1];            
    GList *ns_list;     // list of namespaces associated with this mfc   
} mfc_st_t;

extern pthread_t tm_timer_tid;
extern pthread_mutex_t hash_table_mutex;
extern GHashTable *mfc_hash_table, *ns_hash_table;

void mfc_ns_list_data_free(gpointer data, gpointer user_data);
void ns_whitelist_data_free(gpointer data, gpointer user_data);
void mfc_hash_value_destroy(gpointer value);
void ns_hash_value_destroy(gpointer value);

int check_str_len(char *string_name, char *string_p, int maximum_len);
int tm_add_mfc_entry(char *mfc_name_str, char *mfc_ip_str);
int tm_add_network_entry(char *ns_name_str, char *mfc_name_str, char *network_str);
int tm_add_ns_entry(char *ns_name_str, char *mfc_name_str);

int tm_init(void);
void tm_cleanup(void);
int tm_read_config_mfc(char *line, int line_number);
int tm_read_config_ns(char *line, int line_number);
void tm_read_config(FILE *tm_cfg_file);
void tm_check_config(void);

void ns_white_list_visit_node(gpointer data, gpointer user_data);
void mfc_ns_list_visit_node(gpointer data, gpointer user_data);
void tm_send_one_white_list(gpointer key, gpointer value, gpointer user_data); 
void tm_send_white_list(void);
void *tm_timer_thread_func(void * arg);

#endif /* __TM_H__ */
