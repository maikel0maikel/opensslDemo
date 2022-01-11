package com.zbq.myapplication

import android.content.Context
import android.net.ConnectivityManager
import android.os.Build
import android.os.Handler
import android.util.Log
import org.bouncycastle.jce.provider.BouncyCastleProvider
import org.bouncycastle.openssl.PEMDecryptorProvider
import org.bouncycastle.openssl.PEMEncryptedKeyPair
import org.bouncycastle.openssl.PEMKeyPair
import org.bouncycastle.openssl.PEMParser
import org.bouncycastle.openssl.jcajce.JcaPEMKeyConverter
import org.bouncycastle.openssl.jcajce.JcePEMDecryptorProviderBuilder
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileReader
import java.lang.Exception
import java.nio.charset.StandardCharsets
import java.security.KeyPair
import java.security.KeyStore
import java.security.Security
import java.security.cert.Certificate
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate
import javax.net.ssl.KeyManagerFactory
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManagerFactory
import kotlin.Throws

/**
 *@author:zhengbq
 *@description:
 *@date:2022/1/11 13:55
 */
open class MqttWrapper(val context: Context) {


    interface MsgHandler{
        @Throws(Exception::class)
        fun handle(topic:String,msg:JSONObject)
    }

    /**
     * @Description ConnectCompleteHandler
     */
    interface ConnectCompleteHandler {
        @Throws(Exception::class)
        fun handle()
    }

    /**
     * @Description FetchWillingHandler
     */
    interface FetchWillingHandler {
        @Throws(Exception::class)
        fun handle(): Willing?
    }

    class Willing(var topic: String, var msg: String)


    /* connect config */
    var serverURI = "ssl://127.0.0.1:8883"
    var clientId = "clientId"
    var authUserName = "admin"
    var authPassword = "public"
    var certDir: String? = null
    var certCaCert = "cacert.pem"
    var certClientCert = "client-cert.pem"
    var certClientKey = "client-key.pem"
    var certClientKeyPwd = ""

    /* inner variable */
    private val handlerPeriod2Sec = Handler()
    private var connectivityManager: ConnectivityManager? = null
    internal var msgHandler: MsgHandler? = null
    internal var connectCompleteHandler: ConnectCompleteHandler? = null
    internal var fetchWillingHandler: FetchWillingHandler? = null

    @Throws(Exception::class)
    internal open fun init() {
        if (certDir == null) {
            certDir =  "${context.getExternalFilesDir(null).toString()}/cert/"
        }
    }

   open fun isConnected(): Boolean {
        return false
    }

    @Throws(Exception::class)
    protected open fun connect() {
    }

    @Throws(Exception::class)
    open fun subscribe(topicFilter: String?, qos: Int) {
    }

    @Throws(Exception::class)
    open fun publish(message: JSONObject?, topic: String?, qos: Int?) {
    }

    @Throws(Exception::class)
    open fun destroy() {
    }

    /**
     * @Description reconnect
     * @Param delayMillis
     * @Return
     */
    protected fun reconnect(delayMillis: Long) {
        handlerPeriod2Sec.postDelayed({ doClientConnection() }, delayMillis)
    }

    /**
     * @Description 连接mqtt服务器
     * @Param
     * @Return
     */
    protected fun doClientConnection() {
        if (isConnected()) {
//            log.i("error, try to reconnect mqtt, while it's connected.");
            return
        }
        if (!isConnectionNormal()) {
            Log.e(TAG,"doClientConnection: error, try to reconnect while network unready.")
            reconnect(4000)
            return
        }
        try {
            Log.e(TAG,"doClientConnection: start connection.")
            connect()
        } catch (e: Exception) {
            Log.e(TAG,"doClientConnection: connect error", e)
            reconnect(4000)
        }
    }

    /**
     * @Description 判断网络是否连接
     * @Param
     * @Return
     */
    private fun isConnectionNormal(): Boolean {
        if (connectivityManager == null) {
            connectivityManager =
                context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        }
        if (connectivityManager == null) {
            return false
        }
        val info = connectivityManager!!.activeNetworkInfo
        if (info != null && info.isAvailable) {
            val name = info.typeName
            Log.i(TAG,"isConnectionNormal: network Name=$name")
            return true
        }
        Log.i(TAG,"isConnectionNormal: no network available")
        return false
    }

    /**
     * @Description handleArrivedMessage
     * @Param topic , payload
     * @Return
     */
    protected fun handleArrivedMessage(topic: String, payload: ByteArray?) {
        val msg = String(payload!!)
        Log.e(TAG,"handleArrivedMessage: topic=$topic, msg=$msg")
        if (msgHandler != null) {
            try {
                val jsonMsg = JSONObject(msg)
                msgHandler!!.handle(topic, jsonMsg)
            } catch (e: Exception) {
                Log.e(TAG,"handleArrivedMessage: error", e)
            }
        }
    }

    /**
     * @Description handleConnectComplete
     * @Param
     * @Return
     */
    protected fun handleConnectComplete() {
        if (connectCompleteHandler != null) {
            try {
                connectCompleteHandler!!.handle()
            } catch (e: Exception) {
                Log.e(TAG,"handleConnectComplete: error", e)
            }
        }
    }

    /**
     * @Description 证书配置
     * @Param caCrtFile 根证书
     * @Param crtFile 客户端证书
     * @Param keyFile 客户端端密钥文件
     * @Param password 客户端端密钥密码
     * @Return SSLSocketFactory
     */
    @Throws(Exception::class)
    protected fun getSocketFactory(
        caCrtFile: String?,
        crtFile: String?,
        keyFile: String?,
        password: String
    ): SSLSocketFactory? {
        Security.addProvider(BouncyCastleProvider())

        // load CA certificate
        var caCert: X509Certificate? = null
        var bis = BufferedInputStream(FileInputStream(caCrtFile))
        val cf = CertificateFactory.getInstance("X.509")
        if (bis.available() > 0) {
            caCert = cf.generateCertificate(bis) as X509Certificate
        }
        assert(caCert != null)
        //        log.i("caCert=" + caCert);

        // load client certificate
        var cert: X509Certificate? = null
        bis = BufferedInputStream(FileInputStream(crtFile))
        if (bis.available() > 0) {
            cert = cf.generateCertificate(bis) as X509Certificate
        }
        assert(cert != null)
        //        log.i("cert=" + cert);

        // load client private key
        val key: KeyPair
        val pemParser = PEMParser(FileReader(keyFile))
        val `object`: Any = pemParser.readObject()
        val converter = JcaPEMKeyConverter()
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            converter.setProvider("BC")
        }
        key = if (`object` is PEMEncryptedKeyPair) {
            Log.e(TAG,"getSocketFactory: Encrypted key - we will use provided password")
            val decProv: PEMDecryptorProvider =
                JcePEMDecryptorProviderBuilder().build(password.toCharArray())
            converter.getKeyPair((`object` as PEMEncryptedKeyPair).decryptKeyPair(decProv))
        } else {
            Log.e(TAG,"getSocketFactory: Unencrypted key - no password needed")
            converter.getKeyPair(`object` as PEMKeyPair)
        }
        pemParser.close()

        // CA certificate is used to authenticate server
        val caKs = KeyStore.getInstance(KeyStore.getDefaultType())
        caKs.load(null, null)
        caKs.setCertificateEntry("ca-certificate", caCert)
        val tmf = TrustManagerFactory.getInstance("X509")
        tmf.init(caKs)

        // client key and certificates are sent to server so it can authenticate us
        val ks = KeyStore.getInstance(KeyStore.getDefaultType())
        ks.load(null, null)
        ks.setCertificateEntry("certificate", cert)
        ks.setKeyEntry(
            "private-key",
            key.private,
            password.toCharArray(),
            arrayOf<Certificate?>(cert)
        )
        val kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm())
        kmf.init(ks, password.toCharArray())

        // finally, create SSL socket factory
        val context = SSLContext.getInstance("TLSv1.2")
        //        SSLContext context = SSLContext.getInstance("SSL");
        context.init(kmf.keyManagers, tmf.trustManagers, null)
        return context.socketFactory
    }
    companion object{
        private const val TAG = "zbq"
        fun checkDir(dir: String?) {
            if (null == dir || "" == dir) {
                return
            }
            val d = File(dir)
            if (!d.exists()) {
                try {
                    val ret = d.mkdir()
                    Log.i(TAG,"checkDir: mkdir $dir, ret=$ret")
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }
    }
}