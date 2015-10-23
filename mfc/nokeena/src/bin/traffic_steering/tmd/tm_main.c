/*
 * (C) Copyright 2011 Juniper Networks
 */ 
#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>
#include "syslog.h"
#include "tm.h"

/* TODO: add stats */

pthread_mutex_t hash_table_mutex;
GHashTable *mfc_hash_table = NULL, *ns_hash_table = NULL;

/*
 * Free the data in a mfc Namespace GList node.
 * 
 * Arguments:
 *      data: data in a GList node
 *      user_data: caller supplied data
 */
void
mfc_ns_list_data_free (gpointer data, gpointer user_data)
{
    (void)user_data;

    syslog(LOG_DEBUG, "free mfc namespace string %s\n", (char *)data);
    free(data); 
}

/*
 * Free the data in a Namespace white-list node.
 * 
 * Arguments:
 *      data: data in a GList node
 *      user_data: caller supplied data
 */
void
ns_whitelist_data_free (gpointer data, gpointer user_data)
{
    ip_network_t *network_entry = (ip_network_t *)data;

    (void)user_data;

    syslog(LOG_DEBUG, "free network_entry %s\n", network_entry->network_str);
    free(network_entry); 
}

/*
 * Free the value in a mfc hash table entry
 * 
 * Arguments:
 *  value: the values associated with a mfc hash table entry
 */
void
mfc_hash_value_destroy (gpointer value)
{
    mfc_st_t *mfc_entry = (mfc_st_t *)value;

    syslog(LOG_DEBUG, "free mfc entry %s, ip %s ns_list len %d\n", 
            mfc_entry->name, mfc_entry->ip_str, g_list_length(mfc_entry->ns_list));

    g_list_foreach(mfc_entry->ns_list, mfc_ns_list_data_free, NULL);
    g_list_free(mfc_entry->ns_list);

    free(mfc_entry);    
}

/*
 * Free the value in a Namespace hash table entry
 * 
 * Arguments:
 *  value: the values associated with a Namespace hash table entry
 */
void
ns_hash_value_destroy (gpointer value)
{
    ns_st_t *ns_entry = (ns_st_t *)value;

    syslog(LOG_DEBUG, "free namespace entry %s, mfc %s, white_list len %d\n", 
           ns_entry->name, ns_entry->mfc_st.mfc_name, 
           g_list_length(ns_entry->mfc_st.white_list));

    g_list_foreach(ns_entry->mfc_st.white_list, ns_whitelist_data_free, NULL);
    g_list_free(ns_entry->mfc_st.white_list);

    free(ns_entry);  
}

/*
 * Check if the length of a string exceeds maxmimum
 * 
 * Arguments:
 *  string_name: the string name used in debug log
 *  string_p: the string to be checked
 *  maximum_len: maximum allowed string length
 *
 *  Return value: TRUE if the string length is within maximum.
 */
int
check_str_len (char *string_name, char *string_p, int maximum_len)
{
    int str_len;

    if ((str_len = strlen(string_p)) > maximum_len) {
        syslog(LOG_ERR, "%s %s length %d exceeds maximum %d",
            string_name, string_p, str_len, maximum_len);
        return FALSE;
    }

    return TRUE;
}

/*
 * Add an entry to mfc hash table
 * 
 * Arguments:
 *  mfc_name_str: the name of the mfc
 *  mfc_ip_str: IP address string of the mfc
 *
 *  Return value: TRUE if successful.
 */
int
tm_add_mfc_entry (char *mfc_name_str, char *mfc_ip_str)
{
    mfc_st_t *mfc_entry;

    if (check_str_len((char *)"mfc_name_str", mfc_name_str, MAX_NAME_STR_LEN) 
        == FALSE ||
        check_str_len((char *)"mfc_ip_str", mfc_ip_str, MAX_IP_STR_LEN) 
        == FALSE) {

        return FALSE;
    }

    if ((mfc_entry = malloc(sizeof(mfc_st_t))) == NULL) {
        syslog(LOG_ERR, "malloc error for mfc_entry");
        return FALSE;
    }
    memset(mfc_entry, 0, sizeof(mfc_st_t));

    strncpy(mfc_entry->name, mfc_name_str, MAX_NAME_STR_LEN);
    strncpy(mfc_entry->ip_str, mfc_ip_str, MAX_IP_STR_LEN);

    pthread_mutex_lock(&hash_table_mutex);

    g_hash_table_insert(mfc_hash_table, (gpointer)mfc_entry->name,
                        (gpointer)mfc_entry);

    pthread_mutex_unlock(&hash_table_mutex);

    return TRUE;
}

/*
 * Add a network entry to TM white-list. When this 
 * function is called, the namespace entry with correct mfc 
 * assignment should be already created.
 *
 * Arguments:
 *      ns_name_str: the name of the namespace
 *      mfc_name_str: the name of the mfc associated with the namespace
 *      network_str: The string of the network entry
 *
 *  Return value: TRUE if successful.
 */
int
tm_add_network_entry (char *ns_name_str, char *mfc_name_str, char *network_str)
{
    ip_network_t *network_buffer;
    ns_st_t *ns_entry;

    if (check_str_len((char *)"network_str", network_str, MAX_IP_STR_LEN) 
        == FALSE) {
        return FALSE;
    }

    pthread_mutex_lock(&hash_table_mutex);

    if ((ns_entry = (ns_st_t *)g_hash_table_lookup(ns_hash_table,
        (gpointer)ns_name_str)) == NULL ||
        strncmp(ns_entry->mfc_st.mfc_name, mfc_name_str, MAX_NAME_STR_LEN + 1) 
        != 0) {
        /* The namespace doesn't exist. */
        syslog(LOG_ERR, "Namespace %s assigned to mfc %s doesn't exist!",
               ns_name_str, mfc_name_str);

        pthread_mutex_unlock(&hash_table_mutex);
        return FALSE;
    }

    /* add network to white-list */
    if ((network_buffer = (ip_network_t *)malloc(sizeof(ip_network_t))) 
        == NULL) {
            syslog(LOG_ERR, "malloc error for network_buffer");
        pthread_mutex_unlock(&hash_table_mutex);
        return FALSE;
    }
    memset(network_buffer, 0, sizeof(ip_network_t));

    strncpy(network_buffer->network_str, network_str, MAX_IP_STR_LEN);
    ns_entry->mfc_st.white_list = g_list_append(ns_entry->mfc_st.white_list,
                                 (gpointer)network_buffer);

    pthread_mutex_unlock(&hash_table_mutex);

    return TRUE;
}

/*
 * Add an entry to namespace hash table
 * 
 * Arguments:
 *      ns_name_str: the name of the namespace
 *      mfc_name_str: the name of the mfc associated with the namespace
 *
 *      Return value: TRUE if successful.
 */
int
tm_add_ns_entry (char *ns_name_str, char *mfc_name_str)
{
    ns_st_t *ns_entry;
    mfc_st_t *mfc_entry;
    int ns_name_str_len;
    char *ns_name_buffer;

    if (check_str_len((char *)"ns_name_str", ns_name_str, MAX_NAME_STR_LEN) 
        == FALSE ||
        check_str_len((char *)"mfc_name_str", mfc_name_str, MAX_NAME_STR_LEN) 
        == FALSE) {

        return FALSE;
    }

    /* Create a namespace entry in the hash table */

    if ((ns_entry = malloc(sizeof(ns_st_t))) == NULL) {
        syslog(LOG_ERR, "malloc error for ns_entry");
        return FALSE;
    }
    memset(ns_entry, 0, sizeof(ns_st_t));

    strncpy(ns_entry->name, ns_name_str, MAX_NAME_STR_LEN);
    strncpy(ns_entry->mfc_st.mfc_name, mfc_name_str, MAX_NAME_STR_LEN);

    pthread_mutex_lock(&hash_table_mutex);

    g_hash_table_insert(ns_hash_table, (gpointer)ns_entry->name,
                        (gpointer)ns_entry);

    /* Now try to find the associated mfc entry */
    if ((mfc_entry = (mfc_st_t *)g_hash_table_lookup(mfc_hash_table,
                    (gpointer)mfc_name_str)) == NULL) {
        /* The mfc specified in namespace config doesn't exist. */
        syslog(LOG_ERR, "mfc %s doesn't exist!", mfc_name_str);

        pthread_mutex_unlock(&hash_table_mutex);
        return FALSE;
    }

    ns_name_str_len = strlen(ns_name_str);
    if ((ns_name_buffer = malloc(ns_name_str_len + 1)) == NULL) {
        syslog(LOG_ERR, "malloc error for ns_name_buffer");

        pthread_mutex_unlock(&hash_table_mutex);
        return FALSE;
    }
  
    memset(ns_name_buffer, 0, ns_name_str_len + 1);

    strncpy(ns_name_buffer, ns_name_str, ns_name_str_len);
    mfc_entry->ns_list = g_list_append(mfc_entry->ns_list,
                        (gpointer)ns_name_buffer);

    pthread_mutex_unlock(&hash_table_mutex);

    return TRUE;
}

/*
 * Initialize TM
 * 
 *
 *  Return value: TRUE if successful.
 */
int
tm_init (void)
{
    int rc;

    g_thread_init(NULL); 

    if (curl_global_init(CURL_GLOBAL_ALL)) {
        syslog(LOG_ERR, "Error in curl_global_init");
        return FALSE;
    }

    if ((rc = pthread_mutex_init(&hash_table_mutex, NULL)) != 0) {
        syslog(LOG_ERR, "pthread_mutex_init error, return code %d", rc);
        return FALSE;
    }

    if ((mfc_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, 
                         NULL, mfc_hash_value_destroy)) == NULL ||
        (ns_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, 
                         NULL, ns_hash_value_destroy)) == NULL) {
        syslog(LOG_ERR, "Problems with g_hash_table_new_full");
        return FALSE;
    }

    if (pthread_create(&tm_timer_tid, NULL, tm_timer_thread_func, NULL) != 0) {
         syslog(LOG_ERR, "timer thread creation failed.\n");
         return FALSE;
    }

    return TRUE;
}

/*
 * Clean up TM data structures
 */
void 
tm_cleanup (void)
{
    pthread_mutex_destroy(&hash_table_mutex);
    g_hash_table_destroy(mfc_hash_table);
    g_hash_table_destroy(ns_hash_table);
}

int 
main (void)
{
    syslog(LOG_DEBUG, "Starting Traffic-Monitor\n");
        
    if (tm_init() == FALSE) {
        return 1;
    }

    while(TRUE) {
        sleep(10);
    }

    tm_cleanup();

    syslog(LOG_DEBUG, "Exit Traffic Monitor\n");
    return 0;
}
