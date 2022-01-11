package com.zbq.myapplication

import java.lang.StringBuilder
import java.util.*
import android.os.Build
import java.lang.Exception


/**
 *@author:zhengbq
 *@description:
 *@date:2022/1/11 15:05
 */
object Utils {
    /**
     * @Description 拼接字符数组为一个字符串
     * @Param
     * @Return
     */
    fun joinStringArray(delimiter: CharSequence, elements: List<String>): String? {
        Objects.requireNonNull(delimiter)
        Objects.requireNonNull(elements)
        val stringBuilder = StringBuilder()
        if (elements.isNotEmpty()) {
            stringBuilder.append(elements[0])
        }
        for (i in 1 until elements.size) {
            stringBuilder.append(delimiter)
            stringBuilder.append(elements[i])
        }
        return stringBuilder.toString()
    }

    /**
     * @Description 生成随机字符串
     * @Param length
     * @Return randomString
     */
    fun generateRandomId(length: Int): String {
        val str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        val random = Random()
        val sb = StringBuilder()
        for (i in 0 until length) {
            val number = random.nextInt(str.length)
            sb.append(str[number])
        }
        return sb.toString()
    }

    fun getDeviceId(): String? {
        val m_szDevIDShort =
            "35" + Build.BOARD.length % 10 + Build.BRAND.length % 10 + Build.CPU_ABI.length % 10 + Build.DEVICE.length % 10 + Build.MANUFACTURER.length % 10 + Build.MODEL.length % 10 + Build.PRODUCT.length % 10
        var serial: String? = null
        try {
            serial = Build::class.java.getField("SERIAL")[null].toString()
            return UUID(m_szDevIDShort.hashCode().toLong(), serial.hashCode().toLong()).toString()
        } catch (exception: Exception) {
            serial = "serial"
        }
        return UUID(m_szDevIDShort.hashCode().toLong(), serial.hashCode().toLong()).toString()
    }

}