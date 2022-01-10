package com.zbq.myapplication

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import java.util.*

class MainActivity : AppCompatActivity() {
    private lateinit var localIpEt:EditText
    private lateinit var reflexiveIpEt:EditText

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        localIpEt = findViewById(R.id.local_ip_et)
        reflexiveIpEt = findViewById(R.id.reflexive_ip_et)
        // Example of a call to a native method
//        findViewById<TextView>(R.id.sample_text).text = stringFromJNI()
//
//        val test = "zbq"
//        val result = sha256(test.toByteArray())
//        Log.e("zbq","result=${Arrays.toString(result)},${String(result)}")
        findViewById<Button>(R.id.start_uclient).setOnClickListener{
            localIpEt.text.clear()
            GlobalScope.launch {
                CoturnManager.startUClient("47.100.81.87",8100,"admin",
                    "977a80adb587074183acb384f3ab4d56",object :OnIPAddressObserver{
                        override fun onIP(type: Int, ip: String, port: Int) {
                            Log.e("zbq","receive native call $type,$ip,$port")
                            runOnUiThread{
                                localIpEt.setText("${localIpEt.text.toString()} $ip :$port\n")
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
                        runOnUiThread{
                            reflexiveIpEt.setText("${reflexiveIpEt.text.toString()} $ip :$port\n")
                        }
                    }
                })
            }
        }
    }

//    /**
//     * A native method that is implemented by the 'native-lib' native library,
//     * which is packaged with this application.
//     */
//    external fun stringFromJNI(): String
//
//    external fun sha256(content:ByteArray):ByteArray
//
//    external fun startUClient(remoteAddress:String,port:Int,uName:String,uPwd:String)
//    external fun startUClient2(nativePtr: Long,remoteAddress:String,port:Int,uName:String,uPwd:String,observer:OnIPAddressObserver)
//    companion object {
//        // Used to load the 'native-lib' library on application startup.
//        init {
//            System.loadLibrary("native-lib")
//        }
//    }
}