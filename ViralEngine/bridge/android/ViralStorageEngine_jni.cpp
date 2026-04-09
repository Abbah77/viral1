/*
 * ViralStorageEngine_jni.cpp
 * JNI bridge for ViralStorageEngine
 */

#include <jni.h>
#include <string>
#include "ViralStorageEngine.h"

using namespace Viral;

static ViralStorageEngine* g_storage = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeInit(JNIEnv *env, jclass clz,
    jstring dbPath, jint maxCacheMB)
{
    const char* p = env->GetStringUTFChars(dbPath, nullptr);
    g_storage = new ViralStorageEngine(p, maxCacheMB);
    env->ReleaseStringUTFChars(dbPath, p);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeDestroy(JNIEnv *env, jclass clz) {
    delete g_storage; g_storage = nullptr;
}

// ── Feed ──────────────────────────────────

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeSetVideoLiked(JNIEnv *env, jclass clz,
    jstring videoId, jboolean liked)
{
    const char* id = env->GetStringUTFChars(videoId, nullptr);
    g_storage->setVideoLiked(id, liked);
    env->ReleaseStringUTFChars(videoId, id);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeClearFeed(JNIEnv *env, jclass clz) {
    g_storage->clearFeedCache();
}

// ── Cache ─────────────────────────────────

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeRegisterCache(JNIEnv *env, jclass clz,
    jstring key, jstring localPath, jlong size)
{
    const char* k = env->GetStringUTFChars(key,       nullptr);
    const char* p = env->GetStringUTFChars(localPath, nullptr);
    g_storage->registerCacheEntry(k, p, size);
    env->ReleaseStringUTFChars(key,       k);
    env->ReleaseStringUTFChars(localPath, p);
}

JNIEXPORT jstring JNICALL
Java_com_viral_engine_ViralStorage_nativeGetCachedPath(JNIEnv *env, jclass clz, jstring key) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    std::string path = g_storage->getCachedPath(k);
    env->ReleaseStringUTFChars(key, k);
    return env->NewStringUTF(path.c_str());
}

JNIEXPORT jlong JNICALL
Java_com_viral_engine_ViralStorage_nativeGetCacheSize(JNIEnv *env, jclass clz) {
    return (jlong)g_storage->getCacheSizeBytes();
}

// ── Settings ──────────────────────────────

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeSetSetting(JNIEnv *env, jclass clz,
    jstring key, jstring value)
{
    const char* k = env->GetStringUTFChars(key,   nullptr);
    const char* v = env->GetStringUTFChars(value, nullptr);
    g_storage->setSetting(k, v);
    env->ReleaseStringUTFChars(key,   k);
    env->ReleaseStringUTFChars(value, v);
}

JNIEXPORT jstring JNICALL
Java_com_viral_engine_ViralStorage_nativeGetSetting(JNIEnv *env, jclass clz,
    jstring key, jstring defaultValue)
{
    const char* k = env->GetStringUTFChars(key,          nullptr);
    const char* d = env->GetStringUTFChars(defaultValue, nullptr);
    std::string val = g_storage->getSetting(k, d);
    env->ReleaseStringUTFChars(key,          k);
    env->ReleaseStringUTFChars(defaultValue, d);
    return env->NewStringUTF(val.c_str());
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralStorage_nativeClearAll(JNIEnv *env, jclass clz) {
    g_storage->clearAll();
}

} // extern "C"
