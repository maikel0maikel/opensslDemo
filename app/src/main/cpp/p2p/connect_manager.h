//
// Created by DELL on 2022/1/21.
//

#ifndef OPENSSLDEMO_CONNECT_MANAGER_H
#define OPENSSLDEMO_CONNECT_MANAGER_H

#include "ns_turn_utils.h"
#include "session.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "p2p_client.h"

int rare_event(void);
int not_rare_event(void);
void add_origin(stun_buffer *message);

int start_c2c_connection(uint16_t clnet_remote_port,
                         const char *remote_address,
                         const unsigned char* ifname, const char *local_address,
                         int verbose,
                         app_ur_conn_info *clnet_info1,
                         uint16_t *chn1);

int start_connection(uint16_t clnet_remote_port,
                     const char *remote_address,
                     const unsigned char* ifname, const char *local_address,
                     int verbose,
                     app_ur_conn_info *clnet_info,
                     uint16_t *chn);

int read_mobility_ticket(app_ur_conn_info *clnet_info, stun_buffer *message);
int socket_connect(evutil_socket_t clnet_fd, ioa_addr *remote_addr, int *connect_err);
void set_local_addr_cb(local_address_cb cb);
void notify_ip(int type ,const ioa_addr *addr,local_address_cb cb);
void bind_real(int type,const char * address,int port,app_ur_conn_info *clnet_info);
#ifdef __cplusplus
}
#endif
#endif //OPENSSLDEMO_CONNECT_MANAGER_H
