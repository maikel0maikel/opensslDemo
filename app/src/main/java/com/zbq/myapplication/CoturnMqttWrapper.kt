package com.zbq.myapplication

import android.content.Context
import android.util.Log
import org.eclipse.paho.mqttv5.client.*
import org.eclipse.paho.mqttv5.client.persist.MemoryPersistence
import org.eclipse.paho.mqttv5.common.MqttException
import org.eclipse.paho.mqttv5.common.MqttMessage
import org.eclipse.paho.mqttv5.common.packet.MqttProperties
import org.json.JSONObject
import java.lang.Exception
import java.nio.charset.StandardCharsets

/**
 *@author:zhengbq
 *@description:
 *@date:2022/1/11 13:53
 */
class CoturnMqttWrapper(context: Context) : MqttWrapper(context) {


    private lateinit var mqttAsyncClient: MqttAsyncClient
    private lateinit var mqttConnectionOptions: MqttConnectionOptions

    /**
     * @Description init
     * @Param
     * @Return
     */
    @Throws(Exception::class)
    override fun init() {
        super.init()
        Log.e(TAG, "init: clientId=$clientId, serverURI=$serverURI, certDir=$certDir")
        /* create */
        val persistence = MemoryPersistence()
        mqttAsyncClient = MqttAsyncClient(serverURI, clientId, persistence)
        mqttAsyncClient.setCallback(mqttCallback) // 设置监听订阅消息的回调

        /* config */mqttConnectionOptions = MqttConnectionOptions()
        mqttConnectionOptions.isCleanStart = true // 设置是否清除缓存
        mqttConnectionOptions.connectionTimeout = 10 // 设置超时时间，单位：秒
        mqttConnectionOptions.keepAliveInterval = 20 // 设置心跳包发送间隔，单位：秒
        mqttConnectionOptions.userName = authUserName // 设置用户名
        mqttConnectionOptions.password = authPassword.toByteArray(StandardCharsets.UTF_8) // 设置密码
        mqttConnectionOptions.isHttpsHostnameVerificationEnabled = false // 不校验服务器的域名地址
        mqttConnectionOptions.isAutomaticReconnect = true
        mqttConnectionOptions.maxReconnectDelay = 1000
        checkDir(certDir)
        val socketFactory = getSocketFactory(
            certDir + certCaCert,
            certDir + certClientCert,
            certDir + certClientKey,
            certClientKeyPwd
        )
        mqttConnectionOptions.socketFactory = socketFactory

        /* last will message */if (fetchWillingHandler != null) {
            val will = fetchWillingHandler!!.handle()
            if (will != null) {
                Log.e(
                    TAG,
                    "init: deviceId=" + clientId + ", set will: topic=" + will.topic + ", msg=" + will.msg
                )
                mqttConnectionOptions.setWill(
                    will.topic,
                    MqttMessage(will.msg.toByteArray(), 2, false, null)
                )
            }
        }

        /* connect */doClientConnection()
    }

    /**
     * @Description isConnected
     * @Param
     * @Return boolean
     */
    override fun isConnected(): Boolean {
        return if (mqttAsyncClient == null) {
            false
        } else mqttAsyncClient.isConnected()
    }

    /**
     * @Description connect
     * @Param
     * @Return
     */
    @Throws(Exception::class)
    override fun connect() {
        Log.e(TAG, "connect")
        mqttAsyncClient.connect(mqttConnectionOptions, null, object : MqttActionListener {
            /**
             * @Description 连接成功
             * @Param
             * @Return
             */
            override fun onSuccess(asyncActionToken: IMqttToken?) {
                Log.e(TAG, "connect: onSuccess")
            }

            /**
             * @Description 连接失败
             * @Param
             * @Return
             */
            override fun onFailure(asyncActionToken: IMqttToken?, exception: Throwable) {
                Log.e(TAG, "connect: onFailure, exception=$exception")
                reconnect(2000)
            }
        })
    }

    @Throws(Exception::class)
    override fun subscribe(topicFilter: String?, qos: Int) {
        Log.e(TAG, "subscribe: topic=$topicFilter")
        mqttAsyncClient.subscribe(topicFilter, qos)
    }

    @Throws(Exception::class)
    override fun publish(message: JSONObject?, topic: String?, qos: Int?) {
        var qos = qos
        if (qos == null) {
            qos = 2
        }
        val data = message.toString()
        Log.e(TAG, String.format("publish: topic=%s, data=%s", topic, data))
        val mqttMessage: org.eclipse.paho.mqttv5.common.MqttMessage =
            MqttMessage(data.toByteArray(), qos, false, null)
        mqttAsyncClient.publish(topic, mqttMessage)
    }

    /**
     * @Description destroy
     * @Param
     * @Return
     */
    @Throws(Exception::class)
    override fun destroy() {
        Log.e(TAG, "destroy")
        mqttAsyncClient.disconnect()
        mqttAsyncClient.close()
    }

    /**
     * @Description 订阅主题的回调
     */
    private val mqttCallback: MqttCallback = object : MqttCallback {
        /**
         * @Description 断开
         * @Param
         * @Return
         */
        override fun disconnected(disconnectResponse: MqttDisconnectResponse) {
            Log.e(TAG, "disconnected: " + disconnectResponse.toString())
            reconnect(5000)
        }

        /**
         * @Description 错误
         * @Param
         * @Return
         */
        override fun mqttErrorOccurred(exception: MqttException) {
            Log.e(TAG, "mqttErrorOccurred: " + exception.message)
        }

        /**
         * @Description 收到消息
         * @Param
         * @Return
         */
        override fun messageArrived(
            topic: String?,
            mqttMessage: org.eclipse.paho.mqttv5.common.MqttMessage
        ) {
            handleArrivedMessage(topic!!, mqttMessage.getPayload())
        }

        /**
         * @Description 发送完成
         * @Param
         * @Return
         */
        override fun deliveryComplete(token: IMqttToken?) {
            Log.e(TAG, "deliveryComplete")
        }

        /**
         * @Description 连接完成
         * @Param
         * @Return
         */
        override fun connectComplete(reconnect: Boolean, serverURI: String?) {
            Log.e(TAG, "connectComplete")
            handleConnectComplete()
        }

        /**
         * @Description 收到授权包
         * @Param
         * @Return
         */
        override fun authPacketArrived(reasonCode: Int, properties: MqttProperties?) {
            Log.e(TAG, "authPacketArrived")
        }
    }

    companion object {
        private const val TAG = "zbq"
    }
}