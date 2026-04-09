package com.viral.engine

/**
 * ViralChat.kt
 *
 * Kotlin wrapper for the C++ ViralChatEngine.
 * Real-time chat backed by IXWebSocket.
 *
 * Features:
 *   - Auto-reconnect on network loss
 *   - Offline message queue
 *   - Typing indicators
 *   - Online presence
 *   - Text, image, video, like messages
 *
 * Usage:
 *   ViralChat.init(listener)
 *   ViralChat.connect("wss://api.yourapp.com/chat", jwtToken, currentUserId)
 *
 *   // Send a message
 *   val msgId = ViralChat.sendText(conversationId, recipientId, "Hey!")
 *
 *   // Typing
 *   ViralChat.typingStart(conversationId)
 *   ViralChat.typingStop(conversationId)
 *
 *   // App lifecycle
 *   ViralChat.setOnline(true)   // onResume
 *   ViralChat.setOnline(false)  // onPause
 */

import android.os.Handler
import android.os.Looper

object ViralChat {

    init {
        System.loadLibrary("viralengine")
    }

    // ─────────────────────────────────────────
    // MESSAGE TYPE
    // ─────────────────────────────────────────
    enum class MessageType(val id: Int) {
        TEXT(1), IMAGE(2), VIDEO(3), LIKE(4), SYSTEM(5)
    }

    enum class MessageStatus(val id: Int) {
        PENDING(0), SENT(1), DELIVERED(2), READ(3), FAILED(4)
    }

    // ─────────────────────────────────────────
    // DATA CLASSES
    // ─────────────────────────────────────────
    data class Message(
        val id:             String,
        val conversationId: String,
        val senderId:       String,
        val type:           MessageType,
        val text:           String,
        val mediaUrl:       String,
        val timestamp:      Long
    )

    // ─────────────────────────────────────────
    // LISTENER
    // ─────────────────────────────────────────
    interface ChatListener {
        fun onMessage(message: Message)
        fun onMessageStatus(msgId: String, status: MessageStatus)
        fun onTyping(conversationId: String, isTyping: Boolean)
        fun onPresence(userId: String, isOnline: Boolean)
        fun onConnection(connected: Boolean)
    }

    // ─────────────────────────────────────────
    // INTERNAL JNI LISTENER
    // Called from C++ thread → routed to main
    // ─────────────────────────────────────────
    private class JniListener(private val delegate: ChatListener) {
        private val main = Handler(Looper.getMainLooper())

        // Called from C++ (JNI method signature must match ViralChatEngine_jni.cpp)
        fun onMessage(id: String, convId: String, senderId: String,
                      type: Int, text: String, mediaUrl: String, timestamp: Long) {
            val msg = Message(id, convId, senderId,
                MessageType.values().firstOrNull { it.id == type } ?: MessageType.TEXT,
                text, mediaUrl, timestamp)
            main.post { delegate.onMessage(msg) }
        }

        fun onStatus(msgId: String, statusId: Int) {
            val status = MessageStatus.values().firstOrNull { it.id == statusId } ?: MessageStatus.FAILED
            main.post { delegate.onMessageStatus(msgId, status) }
        }

        fun onTyping(conversationId: String, isTyping: Boolean) {
            main.post { delegate.onTyping(conversationId, isTyping) }
        }

        fun onPresence(userId: String, isOnline: Boolean) {
            main.post { delegate.onPresence(userId, isOnline) }
        }

        fun onConnection(connected: Boolean) {
            main.post { delegate.onConnection(connected) }
        }
    }

    // ─────────────────────────────────────────
    // LIFECYCLE
    // ─────────────────────────────────────────

    fun init(listener: ChatListener) {
        nativeInit(JniListener(listener))
    }

    fun connect(url: String, token: String, userId: String) {
        nativeConnect(url, token, userId)
    }

    fun disconnect() = nativeDisconnect()

    val isConnected: Boolean get() = nativeIsConnected()

    // ─────────────────────────────────────────
    // MESSAGING
    // ─────────────────────────────────────────

    fun sendText(conversationId: String, recipientId: String, text: String): String =
        nativeSendText(conversationId, recipientId, text)

    fun sendImage(conversationId: String, recipientId: String, imageUrl: String): String =
        nativeSendMedia(conversationId, recipientId, imageUrl, MessageType.IMAGE.id)

    fun sendVideo(conversationId: String, recipientId: String, videoUrl: String): String =
        nativeSendMedia(conversationId, recipientId, videoUrl, MessageType.VIDEO.id)

    fun sendLike(conversationId: String, recipientId: String): String =
        nativeSendLike(conversationId, recipientId)

    fun markRead(conversationId: String) = nativeMarkRead(conversationId)

    // ─────────────────────────────────────────
    // TYPING
    // ─────────────────────────────────────────
    fun typingStart(conversationId: String) = nativeTypingStart(conversationId)
    fun typingStop(conversationId: String)  = nativeTypingStop(conversationId)

    // ─────────────────────────────────────────
    // PRESENCE
    // ─────────────────────────────────────────
    fun setOnline(online: Boolean) = nativeSetOnline(online)

    // ─────────────────────────────────────────
    // JNI
    // ─────────────────────────────────────────
    @JvmStatic private external fun nativeInit(listener: Any)
    @JvmStatic private external fun nativeConnect(url: String, token: String, userId: String)
    @JvmStatic private external fun nativeDisconnect()
    @JvmStatic private external fun nativeIsConnected(): Boolean
    @JvmStatic private external fun nativeSendText(convId: String, recipientId: String, text: String): String
    @JvmStatic private external fun nativeSendMedia(convId: String, recipientId: String, mediaUrl: String, type: Int): String
    @JvmStatic private external fun nativeSendLike(convId: String, recipientId: String): String
    @JvmStatic private external fun nativeMarkRead(convId: String)
    @JvmStatic private external fun nativeTypingStart(convId: String)
    @JvmStatic private external fun nativeTypingStop(convId: String)
    @JvmStatic private external fun nativeSetOnline(online: Boolean)
}
