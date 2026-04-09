/*
 * ViralEngine_jni.c
 *
 * Android JNI bridge.
 * Kotlin calls these functions via System.loadLibrary("viralengine")
 *
 * Kotlin usage:
 *   ViralEngine.init()
 *   val feed = ViralEngine.createFeed()
 *   ViralEngine.setUrls(feed, arrayOf("https://..."))
 *   ViralEngine.scrollTo(feed, 0)
 *   ViralEngine.setSurface(feed, surfaceHolder.surface)
 */

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "ViralEngine.h"
#include "ijkplayer/android/ijkplayer_android.h"

#define JNI_CLASS "com/viral/engine/ViralEngine"

/* ─────────────────────────────────────────
 * CALLBACK BRIDGE
 * Fires Java/Kotlin listener from C
 * ───────────────────────────────────────── */

typedef struct {
    JavaVM  *jvm;
    jobject  listener; // WeakReference to Kotlin ViralFeedListener
    jmethodID onEvent;
} JniUserdata;

static void jni_event_callback(int feed_index, ViralEvent event, void *userdata) {
    JniUserdata *jud = (JniUserdata *)userdata;
    if (!jud || !jud->jvm) return;

    JNIEnv *env = NULL;
    jint res = (*jud->jvm)->AttachCurrentThread(jud->jvm, &env, NULL);
    if (res != JNI_OK || !env) return;

    (*env)->CallVoidMethod(env, jud->listener, jud->onEvent, (jint)feed_index, (jint)event);

    (*jud->jvm)->DetachCurrentThread(jud->jvm);
}

/* ─────────────────────────────────────────
 * JNI FUNCTIONS
 * ───────────────────────────────────────── */

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeInit(JNIEnv *env, jclass clz) {
    viral_engine_init();
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeUninit(JNIEnv *env, jclass clz) {
    viral_engine_uninit();
}

JNIEXPORT jlong JNICALL
Java_com_viral_engine_ViralEngine_nativeCreateFeed(JNIEnv *env, jclass clz, jobject listener) {
    JavaVM *jvm = NULL;
    (*env)->GetJavaVM(env, &jvm);

    JniUserdata *jud = calloc(1, sizeof(JniUserdata));
    jud->jvm      = jvm;
    jud->listener = (*env)->NewGlobalRef(env, listener);

    jclass listenerClass = (*env)->GetObjectClass(env, listener);
    jud->onEvent = (*env)->GetMethodID(env, listenerClass, "onEvent", "(II)V");

    ViralFeed *feed = viral_feed_create(jni_event_callback, jud);
    return (jlong)(intptr_t)feed;
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeDestroyFeed(JNIEnv *env, jclass clz, jlong feedPtr) {
    ViralFeed *feed = (ViralFeed *)(intptr_t)feedPtr;
    viral_feed_destroy(feed);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeSetUrls(JNIEnv *env, jclass clz, jlong feedPtr, jobjectArray urlArray) {
    ViralFeed *feed = (ViralFeed *)(intptr_t)feedPtr;
    int count = (*env)->GetArrayLength(env, urlArray);

    const char **urls = malloc(count * sizeof(char*));
    for (int i = 0; i < count; i++) {
        jstring jstr = (*env)->GetObjectArrayElement(env, urlArray, i);
        urls[i] = (*env)->GetStringUTFChars(env, jstr, NULL);
    }

    viral_feed_set_urls(feed, urls, count);

    // Release JNI strings
    for (int i = 0; i < count; i++) {
        jstring jstr = (*env)->GetObjectArrayElement(env, urlArray, i);
        (*env)->ReleaseStringUTFChars(env, jstr, urls[i]);
    }
    free(urls);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeScrollTo(JNIEnv *env, jclass clz, jlong feedPtr, jint index) {
    ViralFeed *feed = (ViralFeed *)(intptr_t)feedPtr;
    viral_feed_scroll_to(feed, index);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeSetSurface(JNIEnv *env, jclass clz, jlong feedPtr, jobject surface) {
    ViralFeed *feed = (ViralFeed *)(intptr_t)feedPtr;

    if (surface) {
        ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
        viral_feed_set_surface(feed, window);

        // Also set on the current player's ijkplayer instance
        ViralPlayer *vp = viral_feed_get_player(feed, viral_feed_current_index(feed));
        if (vp && vp->mp) {
            ijkmp_android_set_surface(env, vp->mp, surface);
        }
    } else {
        viral_feed_clear_surface(feed);
        ViralPlayer *vp = viral_feed_get_player(feed, viral_feed_current_index(feed));
        if (vp && vp->mp) {
            ijkmp_android_set_surface(env, vp->mp, NULL);
        }
    }
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativePlay(JNIEnv *env, jclass clz, jlong feedPtr) {
    viral_feed_play((ViralFeed *)(intptr_t)feedPtr);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativePause(JNIEnv *env, jclass clz, jlong feedPtr) {
    viral_feed_pause((ViralFeed *)(intptr_t)feedPtr);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeMute(JNIEnv *env, jclass clz, jlong feedPtr, jboolean muted) {
    viral_feed_mute((ViralFeed *)(intptr_t)feedPtr, muted);
}

JNIEXPORT void JNICALL
Java_com_viral_engine_ViralEngine_nativeSetLoop(JNIEnv *env, jclass clz, jlong feedPtr, jboolean loop) {
    viral_feed_set_loop((ViralFeed *)(intptr_t)feedPtr, loop);
}

JNIEXPORT jlong JNICALL
Java_com_viral_engine_ViralEngine_nativeGetPosition(JNIEnv *env, jclass clz, jlong feedPtr) {
    return (jlong)viral_feed_get_position((ViralFeed *)(intptr_t)feedPtr);
}

JNIEXPORT jlong JNICALL
Java_com_viral_engine_ViralEngine_nativeGetDuration(JNIEnv *env, jclass clz, jlong feedPtr) {
    return (jlong)viral_feed_get_duration((ViralFeed *)(intptr_t)feedPtr);
}
