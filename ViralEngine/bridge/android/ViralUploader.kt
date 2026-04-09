package com.viral.engine

/**
 * ViralUploader.kt
 *
 * Kotlin wrapper for the C++ ViralUploadEngine.
 * Handles TikTok-style video uploads with:
 *   - Chunked resumable upload (TUS protocol)
 *   - Auto-retry on network failure
 *   - Upload queue
 *   - Progress + status callbacks
 *
 * Usage:
 *   // In Application.onCreate()
 *   ViralUploader.init(object : ViralUploader.UploadListener { ... })
 *
 *   // When user posts a video
 *   val jobId = ViralUploader.enqueue(
 *       filePath    = "/storage/emulated/0/DCIM/video.mp4",
 *       serverUrl   = "https://api.yourapp.com/upload/tus",
 *       title       = "My video",
 *       description = "Check this out!"
 *   )
 *
 *   // Control
 *   ViralUploader.pause(jobId)
 *   ViralUploader.resume(jobId)
 *   ViralUploader.cancel(jobId)
 */

import android.os.Handler
import android.os.Looper

object ViralUploader {

    init {
        System.loadLibrary("viralengine")
    }

    // ─────────────────────────────────────────
    // UPLOAD STATUS (mirrors C++ enum)
    // ─────────────────────────────────────────
    enum class Status(val id: Int) {
        QUEUED(0),
        COMPRESSING(1),
        UPLOADING(2),
        PAUSED(3),
        COMPLETED(4),
        FAILED(5),
        CANCELLED(6);

        companion object {
            fun from(id: Int) = values().firstOrNull { it.id == id } ?: FAILED
        }
    }

    // ─────────────────────────────────────────
    // LISTENER
    // ─────────────────────────────────────────
    interface UploadListener {
        fun onProgress(jobId: String, progress: Float)  // 0.0 to 1.0
        fun onStatus(jobId: String, status: Status, message: String)
    }

    // ─────────────────────────────────────────
    // INTERNAL LISTENER (called from C++ thread)
    // Routes back to main thread for you
    // ─────────────────────────────────────────
    private class InternalListener(
        private val delegate: UploadListener
    ) {
        private val mainHandler = Handler(Looper.getMainLooper())

        // Called from C++ background thread
        fun onProgress(jobId: String, progress: Float) {
            mainHandler.post { delegate.onProgress(jobId, progress) }
        }

        fun onStatus(jobId: String, statusId: Int, message: String) {
            val status = Status.from(statusId)
            mainHandler.post { delegate.onStatus(jobId, status, message) }
        }
    }

    // ─────────────────────────────────────────
    // LIFECYCLE
    // ─────────────────────────────────────────
    fun init(listener: UploadListener) {
        nativeInit(InternalListener(listener))
    }

    fun destroy() = nativeDestroy()

    // ─────────────────────────────────────────
    // UPLOAD CONTROL
    // ─────────────────────────────────────────

    /**
     * Queue a video for upload. Returns a jobId.
     */
    fun enqueue(
        filePath:    String,
        serverUrl:   String,
        title:       String = "",
        description: String = ""
    ): String = nativeEnqueue(filePath, serverUrl, title, description)

    fun pause(jobId: String)  = nativePause(jobId)
    fun resume(jobId: String) = nativeResume(jobId)
    fun cancel(jobId: String) = nativeCancel(jobId)
    fun retry(jobId: String)  = nativeRetry(jobId)

    /** Call when app goes to background */
    fun pauseAll()  = nativePauseAll()

    /** Call when app comes to foreground */
    fun resumeAll() = nativeResumeAll()

    /** Set JWT/Bearer token for authenticated uploads */
    fun setAuthToken(token: String) = nativeSetToken(token)

    fun getProgress(jobId: String): Float = nativeGetProgress(jobId)

    // ─────────────────────────────────────────
    // JNI
    // ─────────────────────────────────────────
    @JvmStatic private external fun nativeInit(listener: Any)
    @JvmStatic private external fun nativeDestroy()
    @JvmStatic private external fun nativeEnqueue(filePath: String, serverUrl: String, title: String, description: String): String
    @JvmStatic private external fun nativePause(jobId: String)
    @JvmStatic private external fun nativeResume(jobId: String)
    @JvmStatic private external fun nativeCancel(jobId: String)
    @JvmStatic private external fun nativeRetry(jobId: String)
    @JvmStatic private external fun nativePauseAll()
    @JvmStatic private external fun nativeResumeAll()
    @JvmStatic private external fun nativeSetToken(token: String)
    @JvmStatic private external fun nativeGetProgress(jobId: String): Float
}
