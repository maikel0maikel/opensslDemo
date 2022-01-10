//
// Created by maikel on 1/6/22.
//

#ifndef UCLIENT_UCLIENT_MANAGER_H
#define UCLIENT_UCLIENT_MANAGER_H
#include "Log.h"

void start_uclient_default(const char* remote_address,int remote_port);

void start_uclient(const char* remote_address,int remote_port,const char* uname,const char * upwd,local_address_cb cb);

void set_peer_address(const char*peer_addr);

void set_peer_port(int port);

void set_use_null_cipher(int cipher);

void set_cipher_suite(const char *suite);

void set_local_addr(const char*local_address);
#endif //UCLIENT_UCLIENT_MANAGER_H
