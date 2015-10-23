/*
 * get_manager.h -- fqueue consumer which loads the given object into
 *      the cache via GET and TFM put tasks.
 *      Requests consist of an URL and mime_header_t.
 *      The get_manager is an in process version of the offline origin manager.
 */
#ifndef _GET_MANAGER_H_
#define _GET_MANAGER_H_

extern int get_manager_init(void);

#endif /* _GET_MANAGER_H_ */

/*
 * End of get_manager.h
 */
