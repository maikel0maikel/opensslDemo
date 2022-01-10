package com.zbq.myapplication

/**
 *@author:zhengbq
 *@description:
 *@date:2022/1/10 14:17
 */
object CoturnManager {

    private var nativePtr: Long = 0

    init {
        System.loadLibrary("native-lib")
        nativePtr = initNative()
    }

    fun startUClient(
        remoteAddress: String,
        port: Int,
        uName: String,
        uPwd: String,
        observer: OnIPAddressObserver
    ) {
        startUClient(nativePtr, remoteAddress, port, uName, uPwd, observer)
    }

    fun startStun(remoteAddress: String,
                  port: Int,
                  observer: OnIPAddressObserver){
        startStun(nativePtr,remoteAddress,port,observer)
    }

    private external fun initNative(): Long

    private external fun startStun(
        nativePtr: Long,
        remoteAddress: String,
        port: Int,
        observer: OnIPAddressObserver
    )

    private external fun startUClient(
        nativePtr: Long,
        remoteAddress: String,
        port: Int,
        uName: String,
        uPwd: String,
        observer: OnIPAddressObserver
    )
}