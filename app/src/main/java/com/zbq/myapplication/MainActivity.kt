package com.zbq.myapplication

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import kotlinx.coroutines.*
import org.json.JSONObject
import java.lang.Exception
import java.net.*

class MainActivity : AppCompatActivity() {
    private var remoteLanAddr: String? = null
    private var remotePort: Int = 0
    private var localAddr: String? = null
    private var localPort: Int = 0
    private var sendLanJob: Job? = null
    private var receiveLanJob: Job? = null
    private lateinit var localIpEt: EditText
    private lateinit var reflexiveIpEt: EditText

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

    private lateinit var lanEt:EditText
    private lateinit var relayEt:EditText
    private lateinit var reflexiveEt:EditText

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        localIpEt = findViewById(R.id.local_ip_et)
        reflexiveIpEt = findViewById(R.id.reflexive_ip_et)
        lanEt = findViewById(R.id.lan_message)
        relayEt = findViewById(R.id.relay_message)
        reflexiveEt = findViewById(R.id.reflexive_message)
        val deviceTv = findViewById<TextView>(R.id.device_tv)
        val deviceId = Settings.System.getString(getContentResolver(), Settings.System.ANDROID_ID)
        deviceTv.text = "设备ID:${deviceId}"
        val mqttWrapper = CoturnMqttWrapper(this)
        mqttWrapper.serverURI = "ssl://47.100.81.87:8883"
        mqttWrapper.clientId = deviceId!!
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
                    Log.e("zbq", "MsgHandler---->type=${type},address=$address,port=$port")
                    // todo connect
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

        findViewById<Button>(R.id.start_uclient).setOnClickListener {
            localIpEt.text.clear()
            GlobalScope.launch {
                CoturnManager.startUClient("47.100.81.87", 8100, "admin",
                    "977a80adb587074183acb384f3ab4d56", object : OnIPAddressObserver {
                        override fun onIP(type: Int, ip: String, port: Int) {
                            Log.e("zbq", "receive native call $type,$ip,$port")
                            when (type) {
                                1 -> {
                                    localAddr = ip
                                }
                            }

                            publishIp(ip, port, type, mqttWrapper)
                            runOnUiThread {
                                localIpEt.setText("${localIpEt.text.toString()} ${if (type == 1) "本机IP:" else "中继IP:"}$ip :$port\n")
                            }
                        }
                    })
            }

        }

        findViewById<Button>(R.id.start_stunclient).setOnClickListener {
            reflexiveIpEt.text.clear()
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
                        runOnUiThread {
                            reflexiveIpEt.setText("${reflexiveIpEt.text.toString()} $ip :$port\n")
                        }
                    }
                })
            }
        }

        findViewById<Button>(R.id.start_lan_udp).setOnClickListener {
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
                        runOnUiThread {
                            lanEt.setText("来自主题为${BuildConfig.SEND_TOPIC}的消息， 第$packetCount 包")
                        }
                    }
                } catch (e: Exception) {
                    Log.e("zbq", "e:${e.message}")
                }
            }
        }
        findViewById<Button>(R.id.start_relay_udp).setOnClickListener {
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
                        runOnUiThread {
                            relayEt.setText("来自主题为${BuildConfig.SEND_TOPIC}的消息：$result， 第$packetCount 包")
                        }
                    }
                } catch (e: Exception) {
                    Log.e("zbq", "e:${e.message}")
                }
            }
        }
        findViewById<Button>(R.id.start_reflexive_udp).setOnClickListener {

            //startWanTcp()
            startReflexiveUdp()
        }
    }

    private fun startReflexiveUdp(){
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
        receiveReflexiveJob = GlobalScope.launch {
            Log.e("zbq", "receiveReflexiveJob ----> localPort = $localPort,localReflexivePort:$localReflexivePort")
            if (localPort == 0) {
                Log.e("zbq", "has not get port yet!!!")
                return@launch
            }
            try {
                val reveSocket = DatagramSocket(null)
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
                    runOnUiThread {
                        reflexiveEt.setText("来自主题为${BuildConfig.SEND_TOPIC}的消息， 第$packetCount 包")
                    }
                }
            } catch (e: Exception) {
                Log.e("zbq", "e:${e.message}")
            }
        }
    }

    private var tcpWanJob:Job? = null
    private var tcpWanServerJob:Job? = null
    private fun startWanTcp(){
        runBlocking {
            tcpWanJob?.cancel()
        }
        tcpWanJob = GlobalScope.launch {
            try {
                val tcpSocket = Socket(remoteReflexiveAddr,remoteReflexivePort)
//                val socketAddr = InetSocketAddress(remoteReflexiveAddr,remotePort)
//                tcpSocket.bind(socketAddr)
//                tcpSocket.connect(socketAddr,3000)
                val sendData = "hello--->${BuildConfig.TOPIC}  我订阅的主题是${BuildConfig.TOPIC}"
                val buffer = sendData.toByteArray(Charsets.UTF_8)
                while (isActive&&tcpSocket.isConnected){
                    tcpSocket.getOutputStream().write(buffer,0,buffer.size)
                    Log.e("zbq","send to ----->$remoteReflexiveAddr ,$remoteReflexivePort")
                    delay(1000)
                }
            }catch ( e:Exception){
                Log.e("zbq","tcp connect error:${e.message}")
            }

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


    override fun onDestroy() {
        super.onDestroy()
        GlobalScope.launch {
            cancelAllJob()
        }
    }

    private fun cancelAllJob() {
        cancelLanJob()
        cancelRelayJob()
        cancelReflexiveJob()
    }

}