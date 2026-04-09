/*
 * ViralChatEngine_jni.cpp
 *
 * JNI bridge for ViralChatEngine.
 * Kotlin calls these to send/receive messages.
 *
 * Kotlin usage:
 *   ViralChat.connect("wss://api.yourapp.com/chat", token, userId)
 *   ViralChat.sendText(conversationId, recipientId, "Hello!")
 *   ViralChat.sendTypingStart(conversationId)
 */

#include <jni.h>
#include <string>
#include "ViralChatEngine.h"

using namespace Viral;

static ViralChatEngine* g_chat = nullptr;
static JavaVM*    g_jvm        = nullptr;
static jobject    g_listener   = nullptr;
static jmethodID  g_onMessage  = nullptr;
static jmethodID  g_onStatus   = nullptr;
static jmethodID  g_onTyping   = nullptr;
static jmethodID  g_onPresence = nullptr;
static jmethodID  g_onConnect  = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeInit(JNIEnv *env, jclass clz, jobject listener) {
    env->GetJavaVM(&g_jvm);
    g_listener = env->NewGlobalRef(listener);
    g_chat     = new ViralChatEngine();

    jclass lc = env->GetObjectClass(listener);
    g_onMessage  = env->GetMethodID(lc, "onMessage",  "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;J)V");
    g_onStatus   = env->GetMethodID(lc, "onStatus",   "(Ljava/lang/String;I)V");
    g_onTyping   = env->GetMethodID(lc, "onTyping",   "(Ljava/lang/String;Z)V");
    g_onPresence = env->GetMethodID(lc, "onPresence", "(Ljava/lang/String;Z)V");
    g_onConnect  = env->GetMethodID(lc, "onConnection","(Z)V");

    // Wire callbacks
    g_chat->setOnMessageReceived([](const ChatMessage& msg) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(g_listener, g_onMessage,
            env->NewStringUTF(msg.id.c_str()),
            env->NewStringUTF(msg.conversationId.c_str()),
            env->NewStringUTF(msg.senderId.c_str()),
            (jint)msg.type,
            env->NewStringUTF(msg.text.c_str()),
            env->NewStringUTF(msg.mediaUrl.c_str()),
            (jlong)msg.timestamp
        );
        g_jvm->DetachCurrentThread();
    });

    g_chat->setOnMessageStatus([](const std::string& msgId, MessageStatus status) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(g_listener, g_onStatus,
            env->NewStringUTF(msgId.c_str()), (jint)status);
        g_jvm->DetachCurrentThread();
    });

    g_chat->setOnTypingChanged([](const std::string& convId, bool isTyping) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(g_listener, g_onTyping,
            env->NewStringUTF(convId.c_str()), (jboolean)isTyping);
        g_jvm->DetachCurrentThread();
    });

    g_chat->setOnPresenceChanged([](const std::string& userId, bool isOnline) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(g_listener, g_onPresence,
            env->NewStringUTF(userId.c_str()), (jboolean)isOnline);
        g_jvm->DetachCurrentThread();
    });

    g_chat->setOnConnectionChange([](bool connected) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(g_listener, g_onConnect, (jboolean)connected);
        g_jvm->DetachCurrentThread();
    });
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeConnect(JNIEnv *env, jclass clz,
    jstring url, jstring token, jstring userId)
{
    const char* u  = env->GetStringUTFChars(url,    nullptr);
    const char* t  = env->GetStringUTFChars(token,  nullptr);
    const char* id = env->GetStringUTFChars(userId, nullptr);
    g_chat->connect(u, t, id);
    env->ReleaseStringUTFChars(url,    u);
    env->ReleaseStringUTFChars(token,  t);
    env->ReleaseStringUTFChars(userId, id);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeDisconnect(JNIEnv *env, jclass clz) {
    if (g_chat) g_chat->disconnect();
}

JNIEXPORT jstring JNICALL
Java_com_viral_engine_ViralChat_nativeSendText(JNIEnv *env, jclass clz,
    jstring convId, jstring recipientId, jstring text)
{
    const char* c = env->GetStringUTFChars(convId,      nullptr);
    const char* r = env->GetStringUTFChars(recipientId, nullptr);
    const char* t = env->GetStringUTFChars(text,        nullptr);
    std::string msgId = g_chat->sendText(c, r, t);
    env->ReleaseStringUTFChars(convId,      c);
    env->ReleaseStringUTFChars(recipientId, r);
    env->ReleaseStringUTFChars(text,        t);
    return env->NewStringUTF(msgId.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_viral_engine_ViralChat_nativeSendMedia(JNIEnv *env, jclass clz,
    jstring convId, jstring recipientId, jstring mediaUrl, jint type)
{
    const char* c = env->GetStringUTFChars(convId,      nullptr);
    const char* r = env->GetStringUTFChars(recipientId, nullptr);
    const char* m = env->GetStringUTFChars(mediaUrl,    nullptr);
    std::string msgId = g_chat->sendMedia(c, r, m, (MessageType)type);
    env->ReleaseStringUTFChars(convId,      c);
    env->ReleaseStringUTFChars(recipientId, r);
    env->ReleaseStringUTFChars(mediaUrl,    m);
    return env->NewStringUTF(msgId.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_viral_engine_ViralChat_nativeSendLike(JNIEnv *env, jclass clz,
    jstring convId, jstring recipientId)
{
    const char* c = env->GetStringUTFChars(convId,      nullptr);
    const char* r = env->GetStringUTFChars(recipientId, nullptr);
    std::string msgId = g_chat->sendLike(c, r);
    env->ReleaseStringUTFChars(convId,      c);
    env->ReleaseStringUTFChars(recipientId, r);
    return env->NewStringUTF(msgId.c_str());
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeMarkRead(JNIEnv *env, jclass clz, jstring convId) {
    const char* c = env->GetStringUTFChars(convId, nullptr);
    g_chat->markRead(c);
    env->ReleaseStringUTFChars(convId, c);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeTypingStart(JNIEnv *env, jclass clz, jstring convId) {
    const char* c = env->GetStringUTFChars(convId, nullptr);
    g_chat->sendTypingStart(c);
    env->ReleaseStringUTFChars(convId, c);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeTypingStop(JNIEnv *env, jclass clz, jstring convId) {
    const char* c = env->GetStringUTFChars(convId, nullptr);
    g_chat->sendTypingStop(c);
    env->ReleaseStringUTFChars(convId, c);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralChat_nativeSetOnline(JNIEnv *env, jclass clz, jboolean online) {
    if (g_chat) g_chat->setUserOnline(online);
}

JNIEXPORT jboolean JNICALL
Java_com_viral_engine_ViralChat_nativeIsConnected(JNIEnv *env, jclass clz) {
    return g_chat ? (jboolean)g_chat->isConnected() : JNI_FALSE;
}

} // extern "C"
