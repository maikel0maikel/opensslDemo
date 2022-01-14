//
// Created by DELL on 2022/1/10.
//

#include "coturn_manager.h"

extern "C" {
#include "uclient_manager.h"
#include "stun_manager.h"
}

std::map<int, jobject> coturn_manager::objectsMap;
JavaVM *coturn_manager::mVm = nullptr;

coturn_manager::coturn_manager(JavaVM *vm) {
    mVm = vm;
}


void coturn_manager::startStun(const char *address, int port, jobject callback) {
    objectsMap.erase(REFLEXIVE_IP);
    objectsMap.insert(std::map<int, jobject>::value_type(REFLEXIVE_IP, callback));
    run_stun(address, port);
}

void coturn_manager::init() {
    set_addr_cb([](int type, const char *local_ip, int port) -> void {
        LOGE("on local_ip %d %s,%d,%d\n", type, local_ip, port, objectsMap.size());

        std::map<int, jobject>::iterator it = objectsMap.find(LOCAL_IP);
        if (it != objectsMap.end()) {
            JNIEnv *env;
            jint ret = mVm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
            bool isAttach = false;
            if (ret == JNI_EDETACHED) {
                ret = mVm->AttachCurrentThread(&env, nullptr);
                isAttach = true;
            }
            if (ret != JNI_OK) {
                LOGE("zbq jni error startUClient \n");
                return;
            }
            jclass clz = env->GetObjectClass(it->second);
            if (!clz) {
                return;
            }
            jmethodID m = env->GetMethodID(clz, "onIP", "(ILjava/lang/String;I)V");
            if (!m) {
                return;
            }
            env->CallVoidMethod(it->second, m, type, env->NewStringUTF(local_ip),
                                port);
            if (isAttach) {
                mVm->DetachCurrentThread();
            }
        } else {
            LOGE("not set call back");
        }

    });

    set_reflexive_cb([](int type, const char *address, int port) -> void {
        LOGE("zbq  %d,%s,%d", type, address, port);

        std::map<int, jobject>::iterator it = objectsMap.find(REFLEXIVE_IP);
        if (it != objectsMap.end()) {
            JNIEnv *env;
            jint ret = mVm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
            bool isAttach = false;
            if (ret == JNI_EDETACHED) {
                ret = mVm->AttachCurrentThread(&env, nullptr);
                isAttach = true;
            }
            if (ret != JNI_OK) {
                LOGE("zbq jni error startUClient \n");
                return;
            }
            jclass clz = env->GetObjectClass(it->second);
            if (!clz) {
                return;
            }
            jmethodID m = env->GetMethodID(clz, "onIP", "(ILjava/lang/String;I)V");
            if (!m) {
                return;
            }
            env->CallVoidMethod(it->second, m, type, env->NewStringUTF(address), port);
            if (isAttach) {
                mVm->DetachCurrentThread();
            }
        } else {
            LOGE("not set call back");
        }
    });
}

void coturn_manager::startUClient(const char *remote_addr, int port, const char *u_name,
                                  const char *u_pwd, jobject callback) {
    objectsMap.erase(LOCAL_IP);
    objectsMap.insert(std::map<int, jobject>::value_type(LOCAL_IP, callback));
    start_uclient(remote_addr, port, u_name, u_pwd);

}

void coturn_manager::set_peer(const char *peer_addr, int peer_port) {
    LOGE("coturn_manager::set_peer %s,%d\n",peer_addr,peer_port);
    set_peer_address(peer_addr);
    set_peer_port(peer_port);
}

coturn_manager::~coturn_manager() {

}