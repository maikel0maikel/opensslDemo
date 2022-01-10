//
// Created by maikel on 1/6/22.
//

#ifndef STUNCLIENT_STUN_MANAGER_H
#define STUNCLIENT_STUN_MANAGER_H
#include "Log.h"
typedef void (*on_address_cb)(int type,const char * address, int port);

int run_stun(const char* rip, int rport);

void set_forceRfc5780(int forceRfc5780);

void set_local_address(const char * local_address);

void init_stun(const char* remote_address,int port);

void set_reflexive_cb(on_address_cb cb);

void set_other_cb(on_address_cb cb);

void set_response_origin_cb(on_address_cb cb);

#endif //STUNCLIENT_STUN_MANAGER_H
