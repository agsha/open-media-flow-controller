

#ifndef __LOG_ACCESSLOG_H__
#define __LOG_ACCESSLOG_H__


struct socku;
struct channel;

int al_handshake_epollin(struct socku *sku);
int al_handshake_epollout(struct socku *sku);
int al_handshake_epollerr(struct socku *sku);
int al_handshake_epollhup(struct socku *sku);


int uds_epollin_svr(struct socku *sku);
int uds_epollout_svr(struct socku *sku);
int uds_epollerr_svr(struct socku *sku);
int uds_epollhup_svr(struct socku *sku);

extern int acclog_uds_epollin(struct socku *sku);
extern int acclog_uds_epollout(struct socku *sku);
extern int acclog_uds_epollerr(struct socku *sku);
extern int acclog_uds_epollhup(struct socku *sku);

extern int al_handle_new_send_socket(struct channel* pnm);

#endif /* __LOG_ACCESSLOG_H__ */
