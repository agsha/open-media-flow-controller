/*
 * (C) Copyright 2011 Juniper Networks
 */ 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h> 
#include <string.h> 
#include <sys/stat.h>
#include <curl/curl.h>
#include "syslog.h"
#include "tm.h"

pthread_t tm_timer_tid;
static unsigned int timer_loop_counter = 0;  
static struct timespec current_time;
static int config_file_flag = FALSE, last_config_file_flag = FALSE;

static char white_list_buffer[WHITE_LIST_BUF_SIZE + 1];
static int white_list_counter = 0;

/* 
 * Read a mfc entry from config file
 *  
 * Arguments:
 *      line: the buffer for a line in config file
 *      line_number: for debug purpose
 *       
 * Return value: TRUE if successful, otherwise FALSE
 */
int
tm_read_config_mfc (char *line, int line_number)
{
    char mfc_name_str[MAX_LINE_LEN + 1], mfc_ip_str[MAX_LINE_LEN + 1];

    syslog(LOG_DEBUG, "add mfc: %s", line);

    mfc_name_str[MAX_LINE_LEN] = 0;
    mfc_ip_str[MAX_LINE_LEN] = 0;

    if (sscanf(line + strlen(MFC_KEYWORD) + 1, "%s %s", mfc_name_str, mfc_ip_str)
        != 2) {
        syslog(LOG_ERR, "wrong mfc entry %s at line line_number %d", line,
            line_number);
        return FALSE;
    }

    return (tm_add_mfc_entry(mfc_name_str, mfc_ip_str));
}

/* 
 * Read a namespace entry from config file
 *  
 * Arguments:
 *      line: the buffer for a line in config file
 *      line_number: for debug purpose
 *       
 * Return value: TRUE if successful, otherwise FALSE
 */
int
tm_read_config_ns (char *line, int line_number)
{
    char    mfc_name_str[MAX_LINE_LEN + 1], ns_name_str[MAX_LINE_LEN + 1];
    char    network_str[MAX_LINE_LEN + 1];
    char    *ip_list_scan;

    syslog(LOG_DEBUG, "add Namespace: %s", line);

    mfc_name_str[MAX_LINE_LEN] = 0;
    ns_name_str[MAX_LINE_LEN] = 0;

    if (sscanf(line + strlen(NS_KEYWORD) + 1, "%s mfc %s", ns_name_str,
        mfc_name_str) != 2) {
        syslog(LOG_ERR, "wrong NS entry %s at line line_number %d", line,
                line_number);
        return FALSE;
    }

    if(tm_add_ns_entry(ns_name_str, mfc_name_str) == FALSE) {
        return FALSE;
    }

    /* TODO: need to handle ip-list in mupltiple lines */
    if ((ip_list_scan = strstr(line + strlen(NS_KEYWORD), IP_LIST)) == NULL) {
        syslog(LOG_ERR, "wrong NS entry %s without IP_LIST at line_number %d",
               line, line_number);
        return FALSE;
    }

    ip_list_scan = ip_list_scan + strlen(IP_LIST) + 1;

    while (sscanf(ip_list_scan, "%s", network_str) == 1) {
        syslog(LOG_DEBUG, "get network %s", network_str);

        if (tm_add_network_entry(ns_name_str, mfc_name_str, network_str) ==
            FALSE) {
            return FALSE;
        }

        /* advance to next network string */
        ip_list_scan += strlen(network_str);        
        
        while(*ip_list_scan == ' ') {
            ip_list_scan++;
        }
    }

    return TRUE;
}

/* 
 * Read config file
 *
 * Arguments:
 *      tm_cfg_file: config file handler

Sample configuration file:

# Member MFC definition: MFC <MFC-name> <IP>
MFC MFC1 10.157.34.104
MFC MFC2 10.157.43.138

# Namespace definition: NS <namespace> mfc <mfc-name> ip-list <IP addresses/Masks>
# If ip-list too long, it can break into multiple lines, each begins with NS name
NS namespace1 mfc MFC1 ip-list 1.2.3.4/32 2.2.3.0/24
NS namespace2 mfc MFC2 ip-list 5.6.7.0/24 6.6.7.8/32
NS namespace3 mfc MFC1 ip-list 100.1.1.1/32 100.1.1.0/24
 
 */
void
tm_read_config (FILE *tm_cfg_file)
{
    char    line[MAX_LINE_LEN + 1];
    int     line_number = 0;

    line[MAX_LINE_LEN] = 0;
    while (fgets(line, MAX_LINE_LEN, tm_cfg_file)) {
        line_number++;                

        if(strncmp(MFC_KEYWORD, line, strlen(MFC_KEYWORD)) == 0) {
            if (tm_read_config_mfc(line, line_number) == FALSE) {
                return;
            }
        } else if (strncmp(NS_KEYWORD, line, strlen(NS_KEYWORD)) == 0) {
            if (tm_read_config_ns(line, line_number) == FALSE) {
                return;
            }
        }
    }
}

/* 
 * Check if need to read config file
 */
void 
tm_check_config (void)
{
    FILE    *tm_cfg_file; 
    struct  stat stat_st;

    /* check if config file exists */
    if (stat(CONF_FILE, &stat_st) == 0) {
        config_file_flag = TRUE;
    } else {
        config_file_flag = FALSE;
    }

    if (last_config_file_flag == FALSE && config_file_flag == FALSE) {
        /* config file not present */
        return;
    }

    if (last_config_file_flag != config_file_flag) {
        syslog(LOG_NOTICE, "log file changed from %d to %d",
               last_config_file_flag, config_file_flag);
    }

    last_config_file_flag = config_file_flag;

    /* If two locks are held simultaneously, always lock TD first to avoid
       deadlock! 
       This function should only return at the end to unlock hash tables */
    pthread_mutex_lock(&hash_table_mutex);

    /* clear all hash_tables from last iteration. Always free mfc hash table
       entries first */
    g_hash_table_remove_all(mfc_hash_table);
    g_hash_table_remove_all(ns_hash_table);

    pthread_mutex_unlock(&hash_table_mutex);

    if (config_file_flag) {

        if ( !(tm_cfg_file = (FILE *)fopen(CONF_FILE, "r")) )  {
            syslog(LOG_ERR, "Cannot open config file");
        } else {
            tm_read_config(tm_cfg_file);
            fclose(tm_cfg_file);
        }
    }

}

/* 
 * Visit one white-list node
 *
 * Arguments:
 *      data: data in a GList node
 *      user_data: user data supplied by the caller  
 */
void
ns_white_list_visit_node (gpointer data, gpointer user_data)
{
    ip_network_t *network_entry = (ip_network_t *)data;
    int network_str_len;

    (void)user_data;

    if (network_entry == NULL) {
        syslog(LOG_ERR, "network_entry corrupted!");
        return;
    }

    syslog(LOG_DEBUG, "find one network_entry %s", network_entry->network_str);

    /* + 1 to account for the '+' in the ip list to send to mfc */
    network_str_len = strlen(network_entry->network_str) + 1;
    if (white_list_counter + network_str_len >= WHITE_LIST_BUF_SIZE) {
        syslog(LOG_ERR, "white_list_counter reached maximum string length!");
        return;
    }  

    sprintf(white_list_buffer + white_list_counter, "+%s", 
            network_entry->network_str);
    white_list_counter += network_str_len;
}

/* 
 * Visit one namespace node associated with a mfc.
 * The hash table should be already locked when this function is called 
 *
 * Arguments:
 *      data: data in a GList node
 *      user_data: user data supplied by the caller  
 */
void
mfc_ns_list_visit_node (gpointer data, gpointer user_data)
{
    char *ns_name_buffer = (char *)data;
    ns_st_t *ns_entry;

    (void)user_data;
    syslog(LOG_DEBUG, "sending sub-list for namespace %s", ns_name_buffer);    

    /* hash table should be already locked when this function is called */
    if ((ns_entry = (ns_st_t *)g_hash_table_lookup(ns_hash_table, 
        (gpointer)ns_name_buffer)) == NULL) {
        /* The namespace doesn't exist. */
        syslog(LOG_ERR, "Namespace %s doesn't exist!", ns_name_buffer);

        return;    
    }    

    /* TODO: need to match namespace mfc string */
    g_list_foreach(ns_entry->mfc_st.white_list, ns_white_list_visit_node, NULL);   
}

/* 
 * Send one white-list to a mfc
 *
 * Arguments:
 *      key: hash key
 *      value: hash value
 *      user_data: user data supplied by the caller  
 */
void
tm_send_one_white_list (gpointer key, gpointer value, gpointer user_data)
{
    mfc_st_t *mfc_entry = (mfc_st_t *)value;
    CURL *curl;
    CURLcode res;
    char url_buffer[URL_BUF_SIZE + 1];

    (void)key;
    (void)user_data;

    if (mfc_entry == NULL) {
        syslog(LOG_ERR, "mfc_entry corrupted!");
        return;
    }         

    syslog(LOG_DEBUG, "sending white-list to mfc %s %s", mfc_entry->name,
           mfc_entry->ip_str);

    /* TODO: make connection persistent later */
    curl = curl_easy_init();
    if(curl == NULL) {
        syslog(LOG_ERR, "curl_easy_init error for mfc %s %s!", mfc_entry->name,
               mfc_entry->ip_str);
        return;
    }

    url_buffer[URL_BUF_SIZE] = 0;
    snprintf(url_buffer, URL_BUF_SIZE, "http://%s:8080/admin/td", 
             mfc_entry->ip_str);
    
    if ((res = curl_easy_setopt(curl, CURLOPT_URL, url_buffer)) != CURLE_OK) {
        syslog(LOG_ERR, 
               "curl_easy_setopt option CURLOPT_URL return error code %d", res);
    } else {
    
        white_list_buffer[WHITE_LIST_BUF_SIZE] = 0;
        sprintf(white_list_buffer, "data=");
        white_list_counter = strlen("data=");

        g_list_foreach(mfc_entry->ns_list, mfc_ns_list_visit_node, NULL);

        if ((res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
             white_list_buffer)) != CURLE_OK) {

            syslog(LOG_ERR, "curl_easy_setopt option CURLOPT_POSTFIELDS return error code %d", res);
        } else {

            if ((res = curl_easy_perform(curl)) != CURLE_OK) {
                syslog(LOG_ERR, "curl_easy_perform return error code %d", res);
            } else {
                syslog(LOG_DEBUG, "sent white list: %s", white_list_buffer);
            }
        }
    }

    curl_easy_cleanup(curl);
}

/* 
 * Send white-lists to all mfcs
 */
void
tm_send_white_list (void)
{
    pthread_mutex_lock(&hash_table_mutex);

    g_hash_table_foreach(mfc_hash_table, tm_send_one_white_list, NULL);

    pthread_mutex_unlock(&hash_table_mutex);
}

/*
 * Timer related functionalities
 */
void * 
tm_timer_thread_func (void * arg) 
{
    (void)arg;

    while(1) {
        timer_loop_counter++;

        if (clock_gettime(CLOCK_MONOTONIC, &current_time)) {
            /* reservered for future uses */
        }

        tm_check_config();

        tm_send_white_list();
         
        sleep(UPDATE_INTERVAL) ;
    }
}

