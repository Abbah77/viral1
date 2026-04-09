package com.viral.engine

/**
 * ViralEngine.kt
 *
 * Kotlin wrapper around the C++ ViralEngine.
 * This is what your UI (RecyclerView, ViewPager2) talks to.
 *
 * Usage:
 *   val engine = ViralEngine()
 *   engine.setUrls(listOf("https://cdn.example.com/video1.mp4", ...))
 *   engine.scrollTo(0)                          // user lands on first video
 *   engine.bindSurface(surfaceView.holder)      // attach display
 *
 *   // When user swipes:
 *   engine.scrollTo(newIndex)                   // engine handles everything
 */

import android.view.Surface
import android.view.SurfaceHolder

class ViralEngine {

    companion object {
        init {
            System.loadLibrary("viralengine")  // loads libviralengine.so
        }

        // Called once at Application.onCreate()
        fun init() = nativeInit()

        // Called once at Application.onTerminate()
        fun uninit() = nativeUninit()

        @JvmStatic private external fun nativeInit()
        @JvmStatic private external fun nativeUninit()
        @JvmStatic private external fun nativeCreateFeed(listener: FeedListener): Long
        @JvmStatic private external fun nativeDestroyFeed(feedPtr: Long)
        @JvmStatic private external fun nativeSetUrls(feedPtr: Long, urls: Array<String>)
        @JvmStatic private external fun nativeScrollTo(feedPtr: Long, index: Int)
        @JvmStatic private external fun nativeSetSurface(feedPtr: Long, surface: Surface?)
        @JvmStatic private external fun nativePlay(feedPtr: Long)
        @JvmStatic private external fun nativePause(feedPtr: Long)
        @JvmStatic private external fun nativeMute(feedPtr: Long, muted: Boolean)
        @JvmStatic private external fun nativeSetLoop(feedPtr: Long, loop: Boolean)
        @JvmStatic private external fun nativeGetPosition(feedPtr: Long): Long
        @JvmStatic private external fun nativeGetDuration(feedPtr: Long): Long
    }

    // ─────────────────────────────────────────
    // EVENT TYPES (mirrors C ViralEvent enum)
    // ─────────────────────────────────────────
    enum class Event(val id: Int) {
        READY(1),
        STARTED(2),
        PAUSED(3),
        COMPLETED(4),
        ERROR(5),
        BUFFERING(6);

        companion object {
            fun from(id: Int) = values().firstOrNull { it.id == id } ?: ERROR
        }
    }

    // ─────────────────────────────────────────
    // LISTENER — implement in your Fragment/Activity
    // ─────────────────────────────────────────
    interface FeedListener {
        // Called from C background thread — post to main thread if needed
        fun onEvent(feedIndex: Int, event: Int)
    }

    // ─────────────────────────────────────────
    // STATE
    // ─────────────────────────────────────────
    private var feedPtr: Long = 0L
    private var listener: FeedListener? = null

    // ─────────────────────────────────────────
    // LIFECYCLE
    // ─────────────────────────────────────────
    fun create(listener: FeedListener) {
        this.listener = listener
        feedPtr = nativeCreateFeed(listener)
    }

    fun destroy() {
        if (feedPtr != 0L) {
            nativeDestroyFeed(feedPtr)
            feedPtr = 0L
        }
    }

    // ─────────────────────────────────────────
    // FEED CONTROL
    // ─────────────────────────────────────────

    fun setUrls(urls: List<String>) {
        nativeSetUrls(feedPtr, urls.toTypedArray())
    }

    /**
     * Call this when user swipes to a new video.
     * Engine will automatically:
     *  - Play the video at [index]
     *  - Preload next 2 videos
     *  - Pause/release old videos
     */
    fun scrollTo(index: Int) {
        nativeScrollTo(feedPtr, index)
    }

    fun bindSurface(holder: SurfaceHolder) {
        nativeSetSurface(feedPtr, holder.surface)
    }

    fun unbindSurface() {
        nativeSetSurface(feedPtr, null)
    }

    fun play()  = nativePlay(feedPtr)
    fun pause() = nativePause(feedPtr)
    fun mute(muted: Boolean) = nativeMute(feedPtr, muted)
    fun setLoop(loop: Boolean) = nativeSetLoop(feedPtr, loop)

    val position: Long get() = nativeGetPosition(feedPtr)
    val duration: Long get() = nativeGetDuration(feedPtr)
}
