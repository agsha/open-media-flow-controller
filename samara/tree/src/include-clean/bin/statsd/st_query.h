/*
 *
 * src/bin/statsd/st_query.h
 *
 *
 *
 */

#ifndef __ST_QUERY_H_
#define __ST_QUERY_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

int st_query_action(gcl_session *sess, const bn_binding_array *bindings,
                    uint32 msg_id, tbool *ret_sent_response);

int st_calc_action(gcl_session *sess, const bn_binding_array *bindings,
                   uint32 msg_id, tbool *ret_sent_response);


#ifdef __cplusplus
}
#endif

#endif /* __ST_QUERY_H_ */
