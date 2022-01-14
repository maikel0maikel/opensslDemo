import android.util.Log
import com.zbq.myapplication.BuildConfig
import kotlinx.coroutines.*
import java.lang.Exception
import java.net.*


fun main(){
    val port = 8100
    println("---------1111111----------------")

//
//     GlobalScope.launch {
//         println("---------33333----------------")
//        try {
//
//            val reveSocket = DatagramSocket(InetSocketAddress(port))
//            reveSocket.reuseAddress = true
//            //reveSocket.bind(InetSocketAddress(port))
//            val buffer = ByteArray(1024)
//            val packet = DatagramPacket(buffer, 0, buffer.size)
//            var packetCount = 0
//            while (isActive) {
//                reveSocket.receive(packet)
//                val result = String(packet.data, 0, packet.length)
//                packetCount++
//                println("receive  $packetCount")
//            }
//        } catch (e: Exception) {
//            print( "e:${e.message}")
//        }
//
//
//    }
//
//    GlobalScope.launch {
//        println("---------222222----------------")
//        val sendSocket = DatagramSocket()
//        val sendData = "hello--->${BuildConfig.TOPIC}  我订阅的主题是${BuildConfig.TOPIC}"
//        val buffer = sendData.toByteArray(Charsets.UTF_8)
//
//        while (isActive) {
//            val datagramPacket = DatagramPacket(
//                buffer,
//                0,
//                buffer.size,
//                InetAddress.getByName("47.100.81.87"),
//                port
//            )
//            println("send ----")
//            sendSocket.send(datagramPacket)
//            delay(1000)
//        }
//    }
    GlobalScope.launch {
        println("---------4444444----------------")
        try {

            val reveSocket = ServerSocket(20177)
            reveSocket.reuseAddress = true
            //reveSocket.bind(InetSocketAddress(port))
            var packetCount = 0
            while (isActive) {
                reveSocket.accept()
                println("receive  $packetCount")
            }
        } catch (e: Exception) {
            print( "e:${e.message}")
        }
    }
    runBlocking {
        delay(5000)
    }

    GlobalScope.launch {
        println("---------33333----------------")
        try {
            val tcpSocket = Socket("47.100.81.87",20177)
            val sendData = "hello--->${BuildConfig.TOPIC}  我订阅的主题是${BuildConfig.TOPIC}"
            val buffer = sendData.toByteArray(Charsets.UTF_8)
            while (isActive&&tcpSocket.isConnected){
                tcpSocket.getOutputStream().write(buffer,0,buffer.size)
                delay(1000)
            }
        }catch ( e:Exception){
           println("tcp connect error:${e.message}")
        }
    }


    runBlocking {
        delay(10000000)
    }

}