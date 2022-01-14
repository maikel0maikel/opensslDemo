//
// Created by DELL on 2022/1/10.
//

#ifndef MY_APPLICATION_COTURN_MANAGER_H
#define MY_APPLICATION_COTURN_MANAGER_H

#include <jni.h>
#include <map>
#include "Log.h"
class coturn_manager {

public:
    coturn_manager(JavaVM *vm);
    void startStun(const char *address,int port,jobject callback);
    void startUClient(const char *remote_addr,int port,const char *u_name,const char *u_pwd,jobject callback);
    void init();
    void set_peer(const char *peer_addr,int peer_port);
    ~coturn_manager();
private:
    static JavaVM * mVm ;
    static std::map<int,jobject> objectsMap;
};


#endif //MY_APPLICATION_COTURN_MANAGER_H
