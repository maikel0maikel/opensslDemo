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

    fun startC2C(
        remoteAddress: String,
        port: Int,
        uName: String,
        uPwd: String,
        observer: OnIPAddressObserver
    ) {
        startUClient(nativePtr, remoteAddress, port, uName, uPwd, observer)
    }

    fun startStun(
        remoteAddress: String,
        port: Int,
        observer: OnIPAddressObserver
    ) {
        startStun(nativePtr, remoteAddress, port, observer)
    }
    fun startPeerClient(
        peerAddr: String,
        peerPort: Int,
        remoteAddress: String,
        port: Int,
        uName: String,
        uPwd: String,
        observer: OnIPAddressObserver
    ){
        startPeerClient(nativePtr,peerAddr,peerPort,remoteAddress,port,uName,uPwd,observer)
    }

    fun bindClient(type:Int,address:String,port: Int){

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

    private external fun startPeerClient(
        nativePtr: Long,
        peerAddr: String,
        peerPort: Int,
        remoteAddress: String,
        port: Int,
        uName: String,
        uPwd: String,
        observer: OnIPAddressObserver
    )

    private external fun bindClient(nativePtr: Long,type: Int,address: String,port: Int)
}