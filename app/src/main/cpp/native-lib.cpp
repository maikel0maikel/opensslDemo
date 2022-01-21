#include <jni.h>
#include <string>
#include "coturn_manager.h"

JavaVM *_vm;

jlong init_native(JNIEnv *env, jobject obj) {
    auto *coturnManager = new coturn_manager(_vm);
    coturnManager->init();
    return reinterpret_cast<jlong>(coturnManager);
}

void
start_stun(JNIEnv *env, jobject obj, jlong native_ptr, jstring addr, jint port, jobject observer) {

    coturn_manager *manager = reinterpret_cast<coturn_manager *>(native_ptr);
    const char *remote_address = env->GetStringUTFChars(addr, 0);

    manager->startStun(remote_address, port, observer);

    env->ReleaseStringUTFChars(addr, remote_address);
}

void startuclient(JNIEnv *env, jobject thiz, jlong native_ptr, jstring address, jint port,
                  jstring u_name, jstring u_pwd,
                  jobject observer) {
    coturn_manager *manager = reinterpret_cast<coturn_manager *>(native_ptr);
    const char *remote_addr = env->GetStringUTFChars(address, 0);
    const char *_u_name = env->GetStringUTFChars(u_name, 0);
    const char *_u_pwd = env->GetStringUTFChars(u_pwd, 0);
    manager->setc2c(1);//client to client
    manager->startUClient(remote_addr, port, _u_name, _u_pwd, observer);
    env->ReleaseStringUTFChars(address, remote_addr);
    env->ReleaseStringUTFChars(u_name, _u_name);
    env->ReleaseStringUTFChars(u_pwd, _u_pwd);
}

void startPeerClient(JNIEnv *env, jobject thiz, jlong native_ptr,
                     jstring peerAddress,
                     jint peerPort,jstring remoteAddr,jint remotePort,jstring uName,jstring uPwd,jobject observer) {

    const char *peer_addr = env->GetStringUTFChars(peerAddress, 0);
    const char *remote_addr = env->GetStringUTFChars(remoteAddr, 0);
    const char *_u_name = env->GetStringUTFChars(uName, 0);
    const char *_u_pwd = env->GetStringUTFChars(uPwd, 0);

    coturn_manager *manager = reinterpret_cast<coturn_manager *>(native_ptr);
    manager->set_peer(peer_addr, peerPort);
    manager->startUClient(remote_addr, remotePort, _u_name, _u_pwd, observer);
    env->ReleaseStringUTFChars(peerAddress, peer_addr);
    env->ReleaseStringUTFChars(remoteAddr, remote_addr);
    env->ReleaseStringUTFChars(uName, _u_name);
    env->ReleaseStringUTFChars(uPwd, _u_pwd);
}

void bind_client(JNIEnv* env,jobject thiz,jlong native_ptr,jint type,jstring address,jint port){
    const char * _address = env->GetStringUTFChars(address,0);
    coturn_manager *manager = reinterpret_cast<coturn_manager *>(native_ptr);

    env->ReleaseStringUTFChars(address,_address);

}

const char *coturnClzName = "com/zbq/myapplication/CoturnManager";
const JNINativeMethod methods[] = {
        {"initNative",      "()J",                                                                                                                       (void *) init_native},
        {"startStun",
                            "(JLjava/lang/String;ILcom/zbq/myapplication/OnIPAddressObserver;)V",
                                                                                                                                                         (void *) start_stun},
        {"startUClient",
                            "(JLjava/lang/String;ILjava/lang/String;Ljava/lang/String;Lcom/zbq/myapplication/OnIPAddressObserver;)V",
                                                                                                                                                         (void *) startuclient},
        {"startPeerClient", "(JLjava/lang/String;ILjava/lang/String;ILjava/lang/String;Ljava/lang/String;Lcom/zbq/myapplication/OnIPAddressObserver;)V", (void *) startPeerClient},
        {"bindClient","(JILjava/lang/String;I)V",(void *)bind_client}
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    _vm = vm;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    LOGE("zbq  JNI_OnLoad-------->");
    jclass coturn_class = env->FindClass(coturnClzName);
    env->RegisterNatives(coturn_class, methods, sizeof(methods) / sizeof(methods[0]));
    return JNI_VERSION_1_6;
}


JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return;
    }
    jclass coturn_class = env->FindClass(coturnClzName);
    env->UnregisterNatives(coturn_class);
}
