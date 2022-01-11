package com.zbq.myapplication

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.util.*

class MainActivity : AppCompatActivity() {
    private lateinit var localIpEt:EditText
    private lateinit var reflexiveIpEt:EditText
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        localIpEt = findViewById(R.id.local_ip_et)
        reflexiveIpEt = findViewById(R.id.reflexive_ip_et)
        val deviceTv = findViewById<TextView>(R.id.device_tv)
        val deviceId = Settings.System.getString(getContentResolver(), Settings.System.ANDROID_ID)
        deviceTv.text = "设备ID:${deviceId}"
        val mqttWrapper = CoturnMqttWrapper(this)
        mqttWrapper.serverURI = "ssl://47.100.81.87:8883"
        mqttWrapper.clientId = deviceId!!
        mqttWrapper.init()
        mqttWrapper.connectCompleteHandler = object : MqttWrapper.ConnectCompleteHandler{
            override fun handle() {
                mqttWrapper.subscribe(BuildConfig.TOPIC,2)
            }
        }
        mqttWrapper.fetchWillingHandler = object : MqttWrapper.FetchWillingHandler{
            override fun handle(): MqttWrapper.Willing? {
                return MqttWrapper.Willing(BuildConfig.TOPIC,"{}")
            }
        }

        mqttWrapper.msgHandler = object :MqttWrapper.MsgHandler{
            override fun handle(topic: String, msg: JSONObject) {
                GlobalScope.launch {
                    val type:Int = msg.getInt("type")
                    val address = msg.getString("address")
                    val port = msg.getInt("port")
                    Log.e("zbq","MsgHandler---->type=${type},address=$address,port=$port")
                    // todo connect
                }
            }
        }

        findViewById<Button>(R.id.start_uclient).setOnClickListener{
            localIpEt.text.clear()
            GlobalScope.launch {
                CoturnManager.startUClient("47.100.81.87",8100,"admin",
                    "977a80adb587074183acb384f3ab4d56",object :OnIPAddressObserver{
                        override fun onIP(type: Int, ip: String, port: Int) {
                            Log.e("zbq","receive native call $type,$ip,$port")
                            publishIp(ip, port, type, mqttWrapper)
                            runOnUiThread{
                                localIpEt.setText("${localIpEt.text.toString()} ${if(type==1) "本机IP:" else "中继IP:"}$ip :$port\n")
                            }
                        }
                    })
            }

        }

        findViewById<Button>(R.id.start_stunclient).setOnClickListener{
            reflexiveIpEt.text.clear()
            GlobalScope.launch {
                CoturnManager.startStun("47.100.81.87",8100,object :OnIPAddressObserver{
                    override fun onIP(type: Int, ip: String, port: Int) {
                        Log.e("zbq","receive native call----》 $type,$ip,$port")
                        publishIp(ip, port, type, mqttWrapper)
                        runOnUiThread{
                            reflexiveIpEt.setText("${reflexiveIpEt.text.toString()} $ip :$port\n")
                        }
                    }
                })
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

}