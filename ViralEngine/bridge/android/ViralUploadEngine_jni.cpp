/*
 * ViralUploadEngine_jni.cpp
 *
 * JNI bridge for ViralUploadEngine.
 * Kotlin calls these to upload videos.
 *
 * Kotlin usage:
 *   val jobId = ViralUploader.enqueue(filePath, serverUrl, title)
 *   ViralUploader.pause(jobId)
 *   ViralUploader.resume(jobId)
 *   ViralUploader.cancel(jobId)
 */

#include <jni.h>
#include <string>
#include "ViralUploadEngine.h"

using namespace Viral;

// Global engine instance (one per app)
static ViralUploadEngine* g_uploader = nullptr;

// JNI callback refs
static JavaVM*    g_jvm            = nullptr;
static jobject    g_progressListener = nullptr;
static jmethodID  g_onProgress     = nullptr;
static jmethodID  g_onStatus       = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeInit(JNIEnv *env, jclass clz, jobject listener) {
    env->GetJavaVM(&g_jvm);

    g_uploader = new ViralUploadEngine();
    g_progressListener = env->NewGlobalRef(listener);

    jclass lClass = env->GetObjectClass(listener);
    g_onProgress  = env->GetMethodID(lClass, "onProgress", "(Ljava/lang/String;F)V");
    g_onStatus    = env->GetMethodID(lClass, "onStatus",   "(Ljava/lang/String;ILjava/lang/String;)V");

    // Progress callback → Kotlin
    g_uploader->setProgressCallback([](const std::string& jobId, float progress) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        jstring jJobId = env->NewStringUTF(jobId.c_str());
        env->CallVoidMethod(g_progressListener, g_onProgress, jJobId, (jfloat)progress);
        env->DeleteLocalRef(jJobId);
        g_jvm->DetachCurrentThread();
    });

    // Status callback → Kotlin
    g_uploader->setStatusCallback([](const std::string& jobId, UploadStatus status, const std::string& msg) {
        JNIEnv* env = nullptr;
        g_jvm->AttachCurrentThread(&env, nullptr);
        jstring jJobId = env->NewStringUTF(jobId.c_str());
        jstring jMsg   = env->NewStringUTF(msg.c_str());
        env->CallVoidMethod(g_progressListener, g_onStatus, jJobId, (jint)status, jMsg);
        env->DeleteLocalRef(jJobId);
        env->DeleteLocalRef(jMsg);
        g_jvm->DetachCurrentThread();
    });
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeDestroy(JNIEnv *env, jclass clz) {
    delete g_uploader;
    g_uploader = nullptr;
    if (g_progressListener) {
        env->DeleteGlobalRef(g_progressListener);
        g_progressListener = nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_com_viral_engine_ViralUploader_nativeEnqueue(JNIEnv *env, jclass clz,
    jstring filePath, jstring serverUrl, jstring title, jstring description)
{
    const char* fp   = env->GetStringUTFChars(filePath,   nullptr);
    const char* url  = env->GetStringUTFChars(serverUrl,  nullptr);
    const char* t    = env->GetStringUTFChars(title,       nullptr);
    const char* desc = env->GetStringUTFChars(description, nullptr);

    std::string jobId = g_uploader->enqueue(fp, url, t, desc);

    env->ReleaseStringUTFChars(filePath,    fp);
    env->ReleaseStringUTFChars(serverUrl,   url);
    env->ReleaseStringUTFChars(title,        t);
    env->ReleaseStringUTFChars(description,  desc);

    return env->NewStringUTF(jobId.c_str());
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativePause(JNIEnv *env, jclass clz, jstring jobId) {
    const char* id = env->GetStringUTFChars(jobId, nullptr);
    g_uploader->pause(id);
    env->ReleaseStringUTFChars(jobId, id);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeResume(JNIEnv *env, jclass clz, jstring jobId) {
    const char* id = env->GetStringUTFChars(jobId, nullptr);
    g_uploader->resume(id);
    env->ReleaseStringUTFChars(jobId, id);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeCancel(JNIEnv *env, jclass clz, jstring jobId) {
    const char* id = env->GetStringUTFChars(jobId, nullptr);
    g_uploader->cancel(id);
    env->ReleaseStringUTFChars(jobId, id);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeRetry(JNIEnv *env, jclass clz, jstring jobId) {
    const char* id = env->GetStringUTFChars(jobId, nullptr);
    g_uploader->retry(id);
    env->ReleaseStringUTFChars(jobId, id);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativePauseAll(JNIEnv *env, jclass clz) {
    g_uploader->pauseAll();
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeResumeAll(JNIEnv *env, jclass clz) {
    g_uploader->resumeAll();
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralUploader_nativeSetToken(JNIEnv *env, jclass clz, jstring token) {
    const char* t = env->GetStringUTFChars(token, nullptr);
    g_uploader->setAuthToken(t);
    env->ReleaseStringUTFChars(token, t);
}

JNIEXPORT jfloat JNICALL
Java_com_viral_engine_ViralUploader_nativeGetProgress(JNIEnv *env, jclass clz, jstring jobId) {
    const char* id = env->GetStringUTFChars(jobId, nullptr);
    float p = g_uploader->getProgress(id);
    env->ReleaseStringUTFChars(jobId, id);
    return (jfloat)p;
}

} // extern "C"
