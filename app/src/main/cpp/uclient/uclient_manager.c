//
// Created by maikel on 1/6/22.
//

#include "uclient_manager.h"
#include "apputils.h"
#include "uclient.h"
#include "ns_turn_utils.h"
#include "apputils.h"
#include "session.h"
#include "stun_buffer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ns_turn_openssl.h"

char rest_api_separator = ':';
char peer_address[129] = "\0";
int peer_port = PEER_DEFAULT_PORT;
int use_null_cipher=0;
static char cipher_suite[1025]="";
static char ca_cert_file[1025]="";

int messagenumber = 5;
int mclient = 1;
char cert_file[1025]="";
char pkey_file[1025]="";
SSL_CTX *root_tls_ctx[32];
int root_tls_ctx_num = 0;
int c2c=0;
int clmessage_length=100;
int do_not_use_channel=0;

int clnet_verbose=TURN_VERBOSE_NONE;
int use_tcp=0;
int use_sctp=0;
int use_secure=0;
int hang_on=0;
ioa_addr peer_addr;
int no_rtcp = 0;
int default_address_family = STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY_VALUE_DEFAULT;
int dont_fragment = 0;
uint8_t g_uname[STUN_MAX_USERNAME_SIZE+1];
password_t g_upwd;
char g_auth_secret[1025]="\0";
int g_use_auth_secret_with_timestamp = 0;
int use_fingerprints = 1;

uint8_t relay_transport = STUN_ATTRIBUTE_TRANSPORT_UDP_VALUE;
unsigned char client_ifname[1025] = "";
int passive_tcp = 0;
int mandatory_channel_padding = 0;
int negative_test = 0;
int negative_protocol_test = 0;
int dos = 0;
int random_disconnect = 0;
int no_permissions = 0;
int mobility = 0;
int oauth = 0;
SHATYPE shatype = SHATYPE_DEFAULT;
oauth_key okey_array[3];
int extra_requests = 0;
int dual_allocation = 0;
band_limit_t bps = 0;
char origin[STUN_MAX_ORIGIN_SIZE+1] = "\0";
char m_local_addr[256];

void set_peer_address(const char*peer_addr){
    strcpy(peer_address,peer_addr);
}
void set_c2c(int _c2c){
    c2c = _c2c;
}
void start_uclient_default(const char* remote_address,int remote_port){
    start_uclient(remote_address,remote_port,0,0);
}

void set_peer_port(int port){
    peer_port = port;
}

void set_use_null_cipher(int cipher){
    use_null_cipher = cipher;
}

void set_cipher_suite(const char *suite){
    memset(cipher_suite,0,sizeof(cipher_suite)/sizeof(cipher_suite[0]));
    strcpy(cipher_suite,suite);
}

void set_local_addr(const char*local_address){
    //strcpy(local_addr,local_address);
}
void set_addr_cb(local_address_cb cb){
    set_notify_address_cb(cb);
}
void start_uclient(const char* remote_address,int remote_port,const char* uname,const char * upwd){
    int  port = remote_port;
    strcpy(g_uname,uname);
    strcpy(g_upwd, upwd);
    bzero(m_local_addr, sizeof(m_local_addr));
    if(dual_allocation) {
        no_rtcp = 1;
    }

    if(g_use_auth_secret_with_timestamp) {

        {
            char new_uname[1025];
            const unsigned long exp_time = 3600 * 24; /* one day */
            if(g_uname[0]) {
                snprintf(new_uname,sizeof(new_uname),"%lu%c%s",(unsigned long)time(NULL) + exp_time,rest_api_separator, (char*)g_uname);
            } else {
                snprintf(new_uname,sizeof(new_uname),"%lu", (unsigned long)time(NULL) + exp_time);
            }
            STRCPY(g_uname,new_uname);
        }
        {
            uint8_t hmac[MAXSHASIZE];
            unsigned int hmac_len;

            switch(shatype) {
                case SHATYPE_SHA256:
                    hmac_len = SHA256SIZEBYTES;
                    break;
                case SHATYPE_SHA384:
                    hmac_len = SHA384SIZEBYTES;
                    break;
                case SHATYPE_SHA512:
                    hmac_len = SHA512SIZEBYTES;
                    break;
                default:
                    hmac_len = SHA1SIZEBYTES;
            };

            hmac[0]=0;

            if(stun_calculate_hmac(g_uname, strlen((char*)g_uname), (uint8_t*)g_auth_secret, strlen(g_auth_secret), hmac, &hmac_len, shatype)>=0) {
                size_t pwd_length = 0;
                char *pwd = base64_encode(hmac,hmac_len,&pwd_length);

                if(pwd) {
                    if(pwd_length>0) {
                        bcopy(pwd,g_upwd,pwd_length);
                        g_upwd[pwd_length]=0;
                    }
                }
                free(pwd);
            }
        }
    }

    if(is_TCP_relay()) {
        dont_fragment = 0;
        no_rtcp = 1;
        c2c = 1;
        use_tcp = 1;
        do_not_use_channel = 1;
    }

    if(port == 0) {
        if(use_secure)
            port = DEFAULT_STUN_TLS_PORT;
        else
            port = DEFAULT_STUN_PORT;
    }

    if (clmessage_length < (int) sizeof(message_info))
        clmessage_length = (int) sizeof(message_info);

    const int max_header = 100;
    if(clmessage_length > (int)(STUN_BUFFER_SIZE-max_header)) {
        fprintf(stderr,"Message length was corrected to %d\n",(STUN_BUFFER_SIZE-max_header));
        clmessage_length = (int)(STUN_BUFFER_SIZE-max_header);
    }



    if (!c2c) {
        LOGE("zbq !c2c  make_ioa_addr start --->");
        if (make_ioa_addr((const uint8_t*) peer_address, peer_port, &peer_addr) < 0) {
            LOGE("zbq !c2c  make_ioa_addr error --->");
            return ;
        }

        if(peer_addr.ss.sa_family == AF_INET6) {
            default_address_family = STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY_VALUE_IPV6;
        } else if(peer_addr.ss.sa_family == AF_INET) {
            default_address_family = STUN_ATTRIBUTE_REQUESTED_ADDRESS_FAMILY_VALUE_IPV4;
        }

    }

    /* SSL Init ==>> */

    if(use_secure) {

        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();

        const char *csuite = "ALL"; //"AES256-SHA" "DH"
        if(use_null_cipher)
            csuite = "eNULL";
        else if(cipher_suite[0])
            csuite=cipher_suite;

        if(use_tcp) {
            root_tls_ctx[root_tls_ctx_num] = SSL_CTX_new(SSLv23_client_method());
            SSL_CTX_set_cipher_list(root_tls_ctx[root_tls_ctx_num], csuite);
            root_tls_ctx_num++;

            root_tls_ctx[root_tls_ctx_num] = SSL_CTX_new(TLSv1_client_method());
            SSL_CTX_set_cipher_list(root_tls_ctx[root_tls_ctx_num], csuite);
            root_tls_ctx_num++;

#if TLSv1_1_SUPPORTED
            root_tls_ctx[root_tls_ctx_num] = SSL_CTX_new(TLSv1_1_client_method());
		  SSL_CTX_set_cipher_list(root_tls_ctx[root_tls_ctx_num], csuite);
		  root_tls_ctx_num++;
#if TLSv1_2_SUPPORTED
		  root_tls_ctx[root_tls_ctx_num] = SSL_CTX_new(TLSv1_2_client_method());
		  SSL_CTX_set_cipher_list(root_tls_ctx[root_tls_ctx_num], csuite);
		  root_tls_ctx_num++;
#endif
#endif
        } else {
#if !DTLS_SUPPORTED
            fprintf(stderr,"ERROR: DTLS is not supported.\n");
            exit(-1);
#else
            if(OPENSSL_VERSION_NUMBER < 0x10000000L) {
		  	TURN_LOG_FUNC(TURN_LOG_LEVEL_WARNING, "WARNING: OpenSSL version is rather old, DTLS may not be working correctly.\n");
		  }
		  root_tls_ctx[root_tls_ctx_num] = SSL_CTX_new(DTLSv1_client_method());
		  SSL_CTX_set_cipher_list(root_tls_ctx[root_tls_ctx_num], csuite);
		  root_tls_ctx_num++;
#if DTLSv1_2_SUPPORTED
		  root_tls_ctx[root_tls_ctx_num] = SSL_CTX_new(DTLSv1_2_client_method());
		  SSL_CTX_set_cipher_list(root_tls_ctx[root_tls_ctx_num], csuite);
		  root_tls_ctx_num++;
#endif
#endif
        }

        int sslind = 0;
        for(sslind = 0; sslind<root_tls_ctx_num; sslind++) {

            if(cert_file[0]) {
                if (!SSL_CTX_use_certificate_chain_file(root_tls_ctx[sslind], cert_file)) {
                    LOGE( "\nERROR: no certificate found!\n");
                    return;
                }
            }

            if (!SSL_CTX_use_PrivateKey_file(root_tls_ctx[sslind], pkey_file,
                                             SSL_FILETYPE_PEM)) {
                LOGE( "\nERROR: no private key found!\n");
                return;
            }

            if(cert_file[0]) {
                if (!SSL_CTX_check_private_key(root_tls_ctx[sslind])) {
                    LOGE( "\nERROR: invalid private key!\n");
                    return;
                }
            }

            if (ca_cert_file[0]) {
                if (!SSL_CTX_load_verify_locations(root_tls_ctx[sslind], ca_cert_file, NULL )) {
                    LOGE(
                                  "ERROR: cannot load CA from file: %s\n",
                                  ca_cert_file);
                }

                /* Set to require peer (client) certificate verification */
                SSL_CTX_set_verify(root_tls_ctx[sslind], SSL_VERIFY_PEER, NULL );

                /* Set the verification depth to 9 */
                SSL_CTX_set_verify_depth(root_tls_ctx[sslind], 9);
            } else {
                SSL_CTX_set_verify(root_tls_ctx[sslind], SSL_VERIFY_NONE, NULL );
            }

            if(!use_tcp)
                SSL_CTX_set_read_ahead(root_tls_ctx[sslind], 1);
        }
    }

    start_mclient(remote_address, port, client_ifname, m_local_addr, messagenumber, mclient);
}