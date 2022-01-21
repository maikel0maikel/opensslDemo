package com.zbq.myapplication

import android.content.Context

/**
 *@author:bq
 *@description:
 *@date:2022/1/21 11:23
 */
interface IMainContract {

    interface View{
        fun getContext():Context

        fun showIPInfo(viewId: Int,type:Int,ip:String,port:Int)

        fun showClientMessage(type: Int,message:String)
    }


    interface Presenter{
        fun initMqtt(deviceId:String)

        fun startUClient(viewId:Int)

        fun startStunClient(viewId: Int)

        fun startLocalUDP()

        fun startRelayUDP()

        fun startReflexiveUDP()

        fun destroy()
    }

}