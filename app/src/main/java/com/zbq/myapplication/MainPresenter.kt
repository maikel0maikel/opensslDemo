package com.zbq.myapplication

import android.util.Log
import kotlinx.coroutines.*
import org.json.JSONObject
import java.lang.Exception
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress

/**
 *@author:bq
 *@description:
 *@date:2022/1/21 11:25
 */
class MainPresenter(private var mView:IMainContract.View):IMainContract.Presenter {

    private lateinit var mqttWrapper: CoturnMqttWrapper
    private var remoteLanAddr: String? = null
    private var remotePort: Int = 0
    private var localAddr: String? = null
    private var localPort: Int = 0
    private var sendLanJob: Job? = null
    private var receiveLanJob: Job? = null
    private var remoteRelayAddr: String? = null
    private var remoteRelayPort: Int = 0

    private var remoteReflexiveAddr: String? = null
    private var remoteReflexivePort: Int = 0

    private var sendRelayJob: Job? = null
    private var receiveRelayJob: Job? = null

    private var sendReflexiveJob: Job? = null
    private var receiveReflexiveJob: Job? = null

    private var localReflexiveAddr: String? = null
    private var localReflexivePort: Int = 0


    override fun initMqtt(deviceId:String) {
        mqttWrapper = CoturnMqttWrapper(mView.getContext())
        mqttWrapper.serverURI = "ssl://47.100.81.87:8883"
        mqttWrapper.clientId = deviceId
        mqttWrapper.init()
        mqttWrapper.connectCompleteHandler = object : MqttWrapper.ConnectCompleteHandler {
            override fun handle() {
                mqttWrapper.subscribe(BuildConfig.TOPIC, 2)
            }
        }
        mqttWrapper.fetchWillingHandler = object : MqttWrapper.FetchWillingHandler {
            override fun handle(): MqttWrapper.Willing? {
                return MqttWrapper.Willing(BuildConfig.TOPIC, "{}")
            }
        }

        mqttWrapper.msgHandler = object : MqttWrapper.MsgHandler {
            override fun handle(topic: String, msg: JSONObject) {
                GlobalScope.launch {
                    val type: Int = msg.getInt("type")
                    val address = msg.getString("address")
                    val port = msg.getInt("port")
                    when (type) {
                        1 -> {
                            remoteLanAddr = address
                        }
                        2 -> {
                            remoteRelayAddr = address
                            remoteRelayPort = port
                        }
                        3 -> {
                            remoteReflexiveAddr = address
                            remoteReflexivePort = port
                        }
                        4 -> {
                            remotePort = port
                        }
                    }
                }
            }
        }
    }

    override fun startUClient(viewId:Int) {
        GlobalScope.launch {
            CoturnManager.startC2C("47.100.81.87", 8100, "admin",
                "977a80adb587074183acb384f3ab4d56", object : OnIPAddressObserver {
                    override fun onIP(type: Int, ip: String, port: Int) {
                        Log.e("zbq", "receive native call $type,$ip,$port")
                        when (type) {
                            1 -> {
                                localAddr = ip
                            }
                        }
                        publishIp(ip, port, type, mqttWrapper)
                        mView.showIPInfo(viewId,type,ip,port)
                    }
                })
        }
    }

    override fun startStunClient(viewId: Int) {
        GlobalScope.launch {
            CoturnManager.startStun("47.100.81.87", 8100, object : OnIPAddressObserver {
                override fun onIP(type: Int, ip: String, port: Int) {
                    Log.e("zbq", "receive native call----》 $type,$ip,$port")
                    when (type) {
                        3 -> {
                            localReflexiveAddr = ip
                            localReflexivePort = port
                        }
                        4 -> {
                            localPort = port
                        }
                    }
                    publishIp(ip, port, type, mqttWrapper)
                    mView.showIPInfo(viewId,type,ip,port)
                }
            })
        }
    }

    private fun publishIp(
        ip: String,
        port: Int,
        type: Int,
        mqttWrapper: CoturnMqttWrapper
    ) {
        val jsonObject = JSONObject()
        jsonObject.put("address", ip)
        jsonObject.put("port", port)
        jsonObject.put("type", type)
        mqttWrapper.publish(jsonObject, BuildConfig.SEND_TOPIC, 2)
    }

    override fun startLocalUDP() {
        runBlocking {
            cancelAllJob()
        }
        sendLanJob = GlobalScope.launch {
            if (remoteLanAddr == null || localPort == 0) {
                Log.e("zbq", "has not get remote address or remote port yet!!!")
                return@launch
            }
            val sendSocket = DatagramSocket()
            val sendData = "hello--->${BuildConfig.TOPIC}  我订阅的主题是${BuildConfig.TOPIC}"
            val buffer = sendData.toByteArray(Charsets.UTF_8)

            while (isActive) {
                val datagramPacket = DatagramPacket(
                    buffer,
                    0,
                    buffer.size,
                    InetAddress.getByName(remoteLanAddr),
                    localPort
                )
                sendSocket.send(datagramPacket)
                delay(1000)
            }
        }
        receiveLanJob = GlobalScope.launch {
            if (remotePort == 0) {
                Log.e("zbq", "has not get port yet!!!")
                return@launch
            }
            try {
                val reveSocket = DatagramSocket(null)
                reveSocket.reuseAddress = true
                reveSocket.bind(InetSocketAddress(remotePort))
                val buffer = ByteArray(1024)
                val packet = DatagramPacket(buffer, 0, buffer.size)
                var packetCount = 0
                while (isActive) {
                    reveSocket.receive(packet)
                    packetCount++
                    val result = String(packet.data, 0, packet.length)
                    Log.e("zbq", "receive data is ---->$result")
                    mView.showClientMessage(1,"来自主题为${BuildConfig.SEND_TOPIC}的消息， 第$packetCount 包")
                }
            } catch (e: Exception) {
                Log.e("zbq", "e:${e.message}")
            }
        }
    }

    override fun startRelayUDP() {
        runBlocking {
            cancelAllJob()
        }
        sendRelayJob = GlobalScope.launch {
            Log.e(
                "zbq",
                "sendRelayJob ----> remoteRelayAddr = $remoteRelayAddr,remoteRelayPort=$remoteRelayPort"
            )
            if (remoteRelayAddr == null || remoteRelayPort == 0) {
                Log.e("zbq", "has not get remote relay address or remote port yet!!!")
                return@launch
            }
            val sendSocket = DatagramSocket()
            val sendData = "hello--->${BuildConfig.TOPIC}  我订阅的主题是${BuildConfig.TOPIC}"
            val buffer = sendData.toByteArray(Charsets.UTF_8)

            while (isActive) {
                val datagramPacket = DatagramPacket(
                    buffer,
                    0,
                    buffer.size,
                    InetAddress.getByName(remoteRelayAddr),
                    remotePort
                )
                sendSocket.send(datagramPacket)
                delay(1000)
            }
        }
        receiveRelayJob = GlobalScope.launch {
            Log.e("zbq", "receiveRelayJob ----> localPort = $localPort")
            if (localPort == 0) {
                Log.e("zbq", "has not get port yet!!!")
                return@launch
            }
            try {
                val reveSocket = DatagramSocket(null)
                reveSocket.reuseAddress = true
                reveSocket.bind(InetSocketAddress(localPort))
                val buffer = ByteArray(1024)
                val packet = DatagramPacket(buffer, 0, buffer.size)
                var packetCount = 0
                while (isActive) {
                    reveSocket.receive(packet)
                    val result = String(packet.data, 0, packet.length)
                    packetCount++
                    Log.e("zbq", "receive data is ---->$result  ${packet.address.hostAddress}")
                    mView.showClientMessage(2,"来自主题为${BuildConfig.SEND_TOPIC}的消息：$result， 第$packetCount 包")
                }
            } catch (e: Exception) {
                Log.e("zbq", "e:${e.message}")
            }
        }
    }

    override fun startReflexiveUDP() {
        runBlocking {
            cancelAllJob()
        }

        sendReflexiveJob = GlobalScope.launch {
            Log.e(
                "zbq",
                "sendReflexiveJob ----> remoteReflexiveAddr = $remoteReflexiveAddr,remoteReflexivePort=$remoteReflexivePort"
            )
            if (remoteReflexiveAddr == null || remoteReflexivePort == 0) {
                Log.e("zbq", "has not get remote relay address or remote port yet!!!")
                return@launch
            }

            val sendSocket = DatagramSocket()
            val sendData = "hello--->${BuildConfig.TOPIC}  我订阅的主题是${BuildConfig.TOPIC}"
            val buffer = sendData.toByteArray(Charsets.UTF_8)
            sendSocket.reuseAddress = true
            while (isActive) {
                Log.e("zbq","start to send")
                try {
                    val datagramPacket = DatagramPacket(
                        buffer,
                        0,
                        buffer.size,
                        InetAddress.getByName(remoteRelayAddr),
                        remoteReflexivePort
                    )
                    sendSocket.send(datagramPacket)
                }catch (e:Exception){
                    Log.e("zbq","send error${e.message}")
                }

                delay(1000)
            }
        }
        receiveReflexiveJob = GlobalScope.launch {
            Log.e("zbq", "receiveReflexiveJob ----> localPort = $localPort,localReflexivePort:$localReflexivePort")
            if (localPort == 0) {
                Log.e("zbq", "has not get port yet!!!")
                return@launch
            }
            try {
                val reveSocket = DatagramSocket()
                reveSocket.reuseAddress = true
                reveSocket.bind(InetSocketAddress(localPort))
                val buffer = ByteArray(1024)
                var packetCount = 0
                while (isActive) {
                    val packet = DatagramPacket(buffer, 0, buffer.size)
                    reveSocket.receive(packet)
                    val result = String(packet.data, 0, packet.length)
                    Log.e("zbq", "receive data is ---->$result")
                    packetCount++
                    mView.showClientMessage(3,"$result， 第$packetCount 包")
                }
            } catch (e: Exception) {
                Log.e("zbq", "e:${e.message}")
            }
        }
    }

    override fun bindClient(type:Int,address: String, port: Int) {
        CoturnManager.bindClient(type,address,port)
    }

    override fun destroy() {
        GlobalScope.launch {
            cancelAllJob()
        }
    }
    private fun cancelAllJob() {
        cancelLanJob()
        cancelRelayJob()
        cancelReflexiveJob()
    }

    private fun cancelLanJob() {
        sendLanJob?.cancel()
        receiveLanJob?.cancel()
    }

    private fun cancelRelayJob() {
        sendRelayJob?.cancel()
        receiveRelayJob?.cancel()
    }

    private fun cancelReflexiveJob() {
        sendReflexiveJob?.cancel()
        receiveReflexiveJob?.cancel()
    }


}