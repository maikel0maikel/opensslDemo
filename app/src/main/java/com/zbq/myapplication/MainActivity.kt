package com.zbq.myapplication

import android.content.Context
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.provider.Settings
import android.widget.Button
import android.widget.EditText
import android.widget.TextView

class MainActivity : AppCompatActivity(),IMainContract.View {
    private lateinit var localIpEt: EditText
    private lateinit var reflexiveIpEt: EditText


    private lateinit var lanEt:EditText
    private lateinit var relayEt:EditText
    private lateinit var reflexiveEt:EditText

    private lateinit var mPresenter: IMainContract.Presenter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        localIpEt = findViewById(R.id.local_ip_et)
        reflexiveIpEt = findViewById(R.id.reflexive_ip_et)
        lanEt = findViewById(R.id.lan_message)
        relayEt = findViewById(R.id.relay_message)
        reflexiveEt = findViewById(R.id.reflexive_message)
        val deviceTv = findViewById<TextView>(R.id.device_tv)
        val deviceId = Settings.System.getString(contentResolver, Settings.System.ANDROID_ID)
        deviceTv.text = "设备ID:${deviceId}"
        mPresenter = MainPresenter(this)
        mPresenter.initMqtt(deviceId)

        findViewById<Button>(R.id.start_uclient).setOnClickListener {
            localIpEt.text.clear()
            mPresenter.startUClient(it.id)
        }

        findViewById<Button>(R.id.start_stunclient).setOnClickListener {
            reflexiveIpEt.text.clear()
            mPresenter.startStunClient(it.id)
        }

        findViewById<Button>(R.id.start_lan_udp).setOnClickListener {
            mPresenter.startLocalUDP()
        }


        findViewById<Button>(R.id.start_relay_udp).setOnClickListener {
           mPresenter.startRelayUDP()
        }
        findViewById<Button>(R.id.start_reflexive_udp).setOnClickListener {
            mPresenter.startReflexiveUDP()
        }

        findViewById<Button>(R.id.test_bind_bt).setOnClickListener{
            mPresenter.bindClient(1,"192.168.31.212",80)
        }
    }

//    private var showIp:((viewId:Int,type:Int,ip:String,port:Int)->Unit)? = { i: Int, i1: Int, s: String, i2: Int ->
//
//    }


    override fun onDestroy() {
        super.onDestroy()
        mPresenter.destroy()
    }


    override fun getContext(): Context  = this.applicationContext

    override fun showIPInfo(viewId:Int,type: Int, ip: String, port: Int) {
        when(viewId){
            R.id.start_uclient->{
                runOnUiThread {
                    fun  getMsg():String{
                        return when(type){
                            1->{
                                "本机IP："
                            }
                            2->{
                                "中继IP："
                            }
                            3->{
                                "外网IP："
                            }
                            4->{
                                "服务器IP："
                            }
                            else->{
                                ""
                            }
                        }
                    }
                    localIpEt.setText("${localIpEt.text.toString()} ${getMsg()}$ip :$port\n")
                }
            }

            R.id.start_stunclient->{
                runOnUiThread {
                    reflexiveIpEt.setText("${reflexiveIpEt.text.toString()} $ip :$port\n")
                }
            }

        }
    }

    override fun showClientMessage(type: Int, message: String) {
        when(type){
            1->{
                runOnUiThread {
                    lanEt.setText(message)
                }
            }
            2->{
                runOnUiThread {
                    relayEt.setText(message)
                }
            }
            3->{
                runOnUiThread {
                    reflexiveEt.setText(message)
                }
            }
        }
    }

}