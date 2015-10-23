#ifndef _MFP_SAFE_WRITE_
#define _MFP_SAFE_WRITE_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include "atomic_ops.h"

#define PTHREAD_MUTEX_INIT(mutex_ptr) { \
	macro_ret = pthread_mutex_init(mutex_ptr, NULL); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_MUTEX_LOCK(mutex_ptr) { \
	macro_ret = pthread_mutex_lock(mutex_ptr); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_MUTEX_UNLOCK(mutex_ptr) { \
	macro_ret = pthread_mutex_unlock(mutex_ptr); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_MUTEX_DESTROY(mutex_ptr) { \
	macro_ret = pthread_mutex_destroy(mutex_ptr); \
	assert(macro_ret == 0); \
}

    typedef struct tag_mfp_safe_io_ctx {
	AO_t size_reserved;
	AO_t size_written;
	char path[256];
	pthread_mutex_t mutex;
    } mfp_safe_io_ctx_t;

    typedef struct tag_mfp_safe_io_desc {
	mfp_safe_io_ctx_t *sioc;
	void *iod;
	AO_t size_reserved;
	AO_t size_written;
    } mfp_safe_io_desc_t;

    int32_t mfp_safe_io_glob_init(void);

    /**
     * computes stats for a mount point and adds to the global mount
     * list. if this mount point is already in the list we will do
     * nothing and return saying that this mount point is already in
     * the reservation list
     * @param mount_path - the storage path which from which the space
     * needs to be reserved
     * @param use - the percentage of space from the total available
     * space in mount path to be used for this context. The is
     * argument is used only when the 'size_reserved' parameter is zero
     * @param size_reserved - this is the absolute size in bytes that
     * you want to reserve; this paramter overrides the 'use'
     * parameter
     * @return - returns 0 on success and a non-zero negative number
     * on error
     */
    int32_t mfp_safe_io_add_reservn_list(const char *mount_path, 
					 float use, 
					  size_t size_reserved);
    
    /**
     * initializes the the safe io context 
     * @param mount_path - the storage path which from which the space
     * needs to be reserved
     * @param use - the percentage of space from the total available
     * space in mount path to be used for this context. The is
     * argument is used only when the 'size_reserved' parameter is zero
     * @param size_reserved - this is the absolute size in bytes that
     * you want to reserve; this paramter overrides the 'use' parameter
     * @param ret - the fully instantiated context 
     * @return - returns '0' on success and a non-zero negative number
     * if error occurs
     */
    int32_t mfp_safe_io_init(const char *mount_path, float use,
			     size_t size_reserved,
			     mfp_safe_io_ctx_t **ret);

    /**
     * returns the instantaneous free space available (not thread
     * safe)
     * @param sioc - a valid safe io context
     * @return returns the size available
     */
    size_t mfp_safe_io_get_free_space(mfp_safe_io_ctx_t *sioc);


    /**
     * creates a safe io descriptor via the 'open' semantic. this
     * function is libc fopen compliant with same error codes,
     * argument definitions etc
     * @param fname - name of the file to be opened
     * @param mode - file open mode
     * @return - returns a fully instantiated descriptor on success
     * and NULL on error
     */
    void *mfp_safe_io_open(const char *fname, const char *mode);

    /**
     * attach a descriptor to a safe io context. this is to let the
     * descriptor know 'context' from which it needs to borrow space
     * and to leverage on the the 'context' level API's
     * @param desc - a fully instantiated safe io descriptor
     * @param ctx - a fully instantiated safe io context
     * @return - returns 0 on success and a non-zero negative number
     * on error
     */
    int32_t mfp_safe_io_attach(mfp_safe_io_desc_t *desc, 
			       mfp_safe_io_ctx_t *ctx);

    
    /**
     * reserves space at a context level for a descriptor
     * @param siod - a valid safe io descriptor
     * @param size - the size to be reserved for this write session
     * @return returns 0 on success and non-zero negative number on
     * error 
     */
    int32_t mfp_safe_io_reserve_size(mfp_safe_io_desc_t *siod, 
				     size_t size);

    /** 
     * returns space at a context level back to the contexts space
     * pool
     * @param siod - a valid safe io descriptor
     * @param size - size to return to the context attached to the
     * descriptor 
     */
     int32_t mfp_safe_io_return_size(mfp_safe_io_desc_t *siod, 
				     size_t size);

    /**
     * performs a safe write; checks for reservation at a descriptor
     * level and at a context level before performing a write. this
     * API is libc fwrite compliant in terms of input arguments and
     * return codes
     */
    size_t mfp_safe_io_write(void *buf, size_t size, 
			     size_t nelem, void *desc);

    /**
     * cleanup the descriptor
     * @param siod - a valid safe io descriptor
     */
    void mfp_safe_io_clean_desc(mfp_safe_io_desc_t *siod);

    /**
     * cleanup the context
     * @param sioc - a valid safe io context
     */
    void mfp_safe_io_clean_ctx(mfp_safe_io_ctx_t *sioc);

    /** FILE IO CALLBACK */
    size_t sio_fread(void *buf, size_t n, size_t size,
			 void *desc);
    void * sio_fopen(char *p_data, const char *mode,
			 size_t size);
    size_t sio_ftell(void *desc);
    size_t sio_fseek(void *desc, size_t seekto,
			 size_t whence);
    void sio_fclose(void *desc);
    size_t sio_fwrite(void *buf, size_t n, size_t size,
		      void *desc);
    
#ifdef __cplusplus
}
#endif

#endif
