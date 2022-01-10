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
    set_reflexive_cb([](int type, const char *address, int port) -> void {
        LOGE("zbq  %d,%s,%d", type, address, port);
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
        std::map<int, jobject>::iterator it = objectsMap.find(REFLEXIVE_IP);
        if (it != objectsMap.end()) {
            jclass clz = env->GetObjectClass(it->second);
            if (!clz) {
                return;
            }
            jmethodID m = env->GetMethodID(clz, "onIP", "(ILjava/lang/String;I)V");
            if (!m) {
                return;
            }
            env->CallVoidMethod(it->second, m, type, env->NewStringUTF(address), port);
        } else {
            LOGE("not set call back");
        }
        if (isAttach) {
            mVm->DetachCurrentThread();
        }

    });
    run_stun(address, port);
}

void coturn_manager::startUClient(const char *remote_addr, int port, const char *u_name,
                                  const char *u_pwd, jobject callback) {
    objectsMap.erase(LOCAL_IP);
    objectsMap.insert(std::map<int, jobject>::value_type(LOCAL_IP, callback));
    start_uclient(remote_addr, port, u_name, u_pwd,
                  [](const char *remote, const char *local_ip, int port) -> void {
                      LOGE("on local_ip %s %s,%d,%d\n", remote, local_ip, port, objectsMap.size());
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
                      std::map<int, jobject>::iterator it = objectsMap.find(LOCAL_IP);
                      if (it != objectsMap.end()) {
                          jclass clz = env->GetObjectClass(it->second);
                          if (!clz) {
                              return;
                          }
                          jmethodID m = env->GetMethodID(clz, "onIP", "(ILjava/lang/String;I)V");
                          if (!m) {
                              return;
                          }
                          env->CallVoidMethod(it->second, m, LOCAL_IP, env->NewStringUTF(local_ip),
                                              port);
                      } else {
                          LOGE("not set call back");
                      }
                      if (isAttach) {
                          mVm->DetachCurrentThread();
                      }
                      objectsMap.erase(LOCAL_IP);
                  });

}

coturn_manager::~coturn_manager() {

}