/*
 *
 * src/bin/mgmtd/md_crypt.h
 *
 *
 *
 */

#ifndef __MD_CRYPT_H_
#define __MD_CRYPT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode.h"

typedef struct md_crypt_context md_crypt_context;

#ifdef PROD_FEATURE_SECURE_NODES

int md_crypt_init(md_crypt_context **ret_crypt_context);

int md_crypt_do_crypt(void *data, bn_binding *binding, tbool do_encrypt);

int md_crypt_free(md_crypt_context **inout_crypt_context);

#endif /* PROD_FEATURE_SECURE_NODES */

#ifdef __cplusplus
}
#endif

#endif /* __MD_CRYPT_H_ */
