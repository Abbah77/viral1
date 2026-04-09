package com.viral.engine

/**
 * ViralStorage.kt — SQLite local storage wrapper
 *
 * Usage:
 *   // Application.onCreate()
 *   ViralStorage.init(context.getDatabasePath("viral.db").absolutePath)
 *
 *   // Cache a downloaded video
 *   ViralStorage.registerCache(videoUrl, localFilePath, fileSizeBytes)
 *
 *   // Check if video is cached before downloading
 *   val localPath = ViralStorage.getCachedPath(videoUrl)
 *   if (localPath.isNotEmpty()) playLocal(localPath) else downloadAndPlay(videoUrl)
 *
 *   // Settings
 *   ViralStorage.setSetting("muted", "true")
 *   val muted = ViralStorage.getSetting("muted", "false").toBoolean()
 *
 *   // On logout
 *   ViralStorage.clearAll()
 */

object ViralStorage {

    init { System.loadLibrary("viralengine") }

    fun init(dbPath: String, maxCacheMB: Int = 500) =
        nativeInit(dbPath, maxCacheMB)

    fun destroy() = nativeDestroy()

    // Feed
    fun setVideoLiked(videoId: String, liked: Boolean) = nativeSetVideoLiked(videoId, liked)
    fun clearFeedCache() = nativeClearFeed()

    // Disk cache
    fun registerCache(key: String, localPath: String, sizeBytes: Long) =
        nativeRegisterCache(key, localPath, sizeBytes)
    fun getCachedPath(key: String): String = nativeGetCachedPath(key)
    fun getCacheSizeBytes(): Long = nativeGetCacheSize()
    fun getCacheSizeMB(): Float = getCacheSizeBytes() / (1024f * 1024f)

    // Settings
    fun setSetting(key: String, value: String) = nativeSetSetting(key, value)
    fun getSetting(key: String, default_: String = ""): String =
        nativeGetSetting(key, default_)

    // Logout
    fun clearAll() = nativeClearAll()

    @JvmStatic private external fun nativeInit(dbPath: String, maxCacheMB: Int)
    @JvmStatic private external fun nativeDestroy()
    @JvmStatic private external fun nativeSetVideoLiked(videoId: String, liked: Boolean)
    @JvmStatic private external fun nativeClearFeed()
    @JvmStatic private external fun nativeRegisterCache(key: String, localPath: String, size: Long)
    @JvmStatic private external fun nativeGetCachedPath(key: String): String
    @JvmStatic private external fun nativeGetCacheSize(): Long
    @JvmStatic private external fun nativeSetSetting(key: String, value: String)
    @JvmStatic private external fun nativeGetSetting(key: String, default_: String): String
    @JvmStatic private external fun nativeClearAll()
}
