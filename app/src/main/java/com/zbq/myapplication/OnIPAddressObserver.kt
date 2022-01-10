package com.zbq.myapplication

/**
 *@author:zhengbq
 *@description:
 *@date:2022/1/10 14:35
 */
interface OnIPAddressObserver {
    fun onIP(type:Int,ip:String,port:Int)
}