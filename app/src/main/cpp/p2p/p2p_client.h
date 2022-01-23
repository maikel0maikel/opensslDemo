//
// Created by DELL on 2022/1/21.
//

#ifndef OPENSSLDEMO_P2P_CLIENT_H
#define OPENSSLDEMO_P2P_CLIENT_H
#include "ns_turn_utils.h"
#include "stun_buffer.h"
#include "session.h"

#include "ns_turn_openssl.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "Log.h"
#include "connect_manager.h"
#define UCLIENT_SESSION_LIFETIME (777)
#define OAUTH_SESSION_LIFETIME (555)

extern int c2c;
extern struct event_base* client_event_base;
extern int default_address_family;
extern ioa_addr peer_addr;
extern int use_tcp;
extern int dual_allocation;
extern int no_rtcp;
extern int clmessage_length;
extern int dos;
extern int mobility;
extern char origin[STUN_MAX_ORIGIN_SIZE+1];
extern int use_fingerprints;
extern int no_permissions;
extern int oauth;
extern SHATYPE shatype;
extern oauth_key okey_array[3];
extern uint8_t g_uname[STUN_MAX_USERNAME_SIZE+1];
extern password_t g_upwd;
extern int negative_protocol_test;
extern uint8_t relay_transport;
extern int negative_test;
extern band_limit_t bps;
extern int dont_fragment;
extern int use_sctp;
extern int use_secure;
extern int root_tls_ctx_num;
extern SSL_CTX *root_tls_ctx[32];
extern int clnet_verbose;
extern int do_not_use_channel;
extern int extra_requests;

#define is_TCP_relay() (relay_transport == STUN_ATTRIBUTE_TRANSPORT_TCP_VALUE)

int send_buffer(app_ur_conn_info *clnet_info, stun_buffer* message, int data_connection, app_tcp_conn_info *atc);
int recv_buffer(app_ur_conn_info *clnet_info, stun_buffer* message, int sync, int data_connection, app_tcp_conn_info *atc, stun_buffer* request_message);

void start_p2p_client(const char *remote_address, int port,const unsigned char *ifname, const char *local_address);

void bind_client(int type,const char * address,int port);

void client_input_handler(evutil_socket_t fd, short what, void* arg);

turn_credential_type get_turn_credentials_type(void);

int add_integrity(app_ur_conn_info *clnet_info, stun_buffer *message);
int check_integrity(app_ur_conn_info *clnet_info, stun_buffer *message);
SOCKET_TYPE get_socket_type(void);
void set_notify_address_cb(local_address_cb cb);

#ifdef __cplusplus
}
#endif

#endif //OPENSSLDEMO_P2P_CLIENT_H
