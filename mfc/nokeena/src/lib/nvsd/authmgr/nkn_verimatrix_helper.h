#ifndef _NKN_VERMIMATRIX_HELPER_
#define _NKN_VERMIMATRIX_HELPER_

#define HTTP_VER_STR "HTTP/1.1 "
#define HTTP_VER_STR_LEN (8+1)

/* VERIMATRIX HELPER API */
int32_t verimatrix_helper_get_key(kms_info_t *ki, kms_out_t *ko);
int32_t verimatrix_helper(auth_msg_t *data);

#endif
