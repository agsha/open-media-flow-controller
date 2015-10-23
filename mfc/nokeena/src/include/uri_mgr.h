#ifndef __URI_API__H
#define __URI_API__H

/*
 *
 * These functions are exported to all modules
 *
 * The central uri management is designed for the following purposes.
 *
 * 1. fast uri lookup and comparison.
 * 2. save memory, so nobody should make/keep a copy of uri any more.
 * 3. statistic counter report purpose. So we can follow to figure out
 *    hot Video contents.
 *
 * Special information in the design:
 *
 * 1. After urimgr_add_uri() is called, caller MUST call uri_del_uri()
 *    to release the reference counter. Currently there is not time out
 *    implementation in uri manager. If not called, a memory leak is
 *    the result.
 * 2. Uri is case sesensitive in this design.
 * 3. Uri should not include http://ip:port/ part although it does not hurt.
 * 4. Internal structure is not exposed to any module. So any module
 *    should not try to cast the returned uri_id into a structure.
 *
 */


/*
 * This function should be called only by server module once.
 */
void urimgr_init(void);

/*
 * Any module can call to add one uri into urimgr.
 * If the same uri is found, it will increase the counter by one.
 * The return id should NOT be casted into a pointer.
 *
 * urimgr_add_uri makes a copy of uri, so caller can release this buffer
 * after called.
 *
 * return 0: failed to add this uri into uri manager.
 */
uint64_t urimgr_add_uri(char * uri);

/*
 * When provided the uri_id, you can get the address of uri string.
 * After get the pointer, any module should not modify the 
 * content returned by this function.
 */
char * urimgr_get_uri_addr(uint64_t uri_id);

/*
 * For performance concern, any module does not need to calculate the
 * size of uri if needed. Instead of it, any module can apply this 
 * function to withdraw the size of uri.
 */
int32_t urimgr_get_uri_size(uint64_t uri_id);

/*
 * Lookup up the uri_id based on uri
 *
 * return 0: failed to lookup this uri.
 */
uint64_t urimgr_lookup_uri(char * uri);

/*
 * This function should be called to decrease the counter if 
 * urimgr_add_uri() is called.
 */
void urimgr_del_uri(uint64_t uri_id);

#endif //  __URI_API__H
