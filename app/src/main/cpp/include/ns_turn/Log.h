//
// Created by DELL on 2022/1/10.
//

#ifndef MY_APPLICATION_LOG_H
#define MY_APPLICATION_LOG_H
#include <android/log.h>
#define TAG "zbq"
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)
#define LOCAL_IP 1
#define RELAY_IP 2
#define REFLEXIVE_IP 3
#define REMOTE_IP 4
#define OTHER_IP 5

typedef void (*local_address_cb)(int type,const char *address,int port);
#endif //MY_APPLICATION_LOG_H
