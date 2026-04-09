/*
 * ViralStorageEngine.h
 *
 * Local storage engine using SQLite.
 * Handles:
 *   - Video metadata cache (feed items)
 *   - Chat message persistence
 *   - User profile cache
 *   - App settings/preferences
 *   - LRU disk cache tracking for videos
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Viral {

/* ─────────────────────────────────────────
 * DATA MODELS
 * ───────────────────────────────────────── */

struct VideoItem {
    std::string id;
    std::string url;
    std::string thumbnailUrl;
    std::string authorId;
    std::string authorName;
    std::string description;
    int64_t     likes;
    int64_t     comments;
    int64_t     shares;
    int64_t     timestamp;
    bool        isLiked;
    bool        isFollowing;
};

struct UserProfile {
    std::string id;
    std::string username;
    std::string displayName;
    std::string avatarUrl;
    std::string bio;
    int64_t     followers;
    int64_t     following;
    int64_t     videoCount;
    bool        isFollowing;
};

struct CacheEntry {
    std::string key;        // video URL or asset key
    std::string localPath;  // local file path
    int64_t     size;       // bytes
    int64_t     lastAccess; // unix ms — for LRU eviction
};

/* ─────────────────────────────────────────
 * VIRAL STORAGE ENGINE
 * ───────────────────────────────────────── */

class ViralStorageEngine {
public:
    /**
     * @param dbPath  Full path to SQLite database file
     *                Android: context.getDatabasePath("viral.db").absolutePath
     *                iOS:     NSDocumentDirectory + "/viral.db"
     * @param maxCacheMB  Max disk cache for videos (default 500MB)
     */
    explicit ViralStorageEngine(const std::string& dbPath, int maxCacheMB = 500);
    ~ViralStorageEngine();

    // ── Feed / Video Cache ─────────────────

    /** Save a batch of feed videos (from API response) */
    void saveFeedVideos(const std::vector<VideoItem>& videos);

    /** Get cached feed videos (shown while loading fresh data) */
    std::vector<VideoItem> getFeedVideos(int limit = 20, int offset = 0);

    /** Update like state locally (optimistic UI) */
    void setVideoLiked(const std::string& videoId, bool liked);

    /** Clear feed cache (on logout or refresh) */
    void clearFeedCache();

    // ── User Profiles ──────────────────────

    void      saveUserProfile(const UserProfile& profile);
    UserProfile getUserProfile(const std::string& userId);
    void      setFollowing(const std::string& userId, bool following);

    // ── Chat Messages ──────────────────────

    /** Persist a chat message (for offline history) */
    void saveMessage(const std::string& convId,
                     const std::string& msgId,
                     const std::string& senderId,
                     int                type,
                     const std::string& text,
                     const std::string& mediaUrl,
                     int64_t            timestamp,
                     int                status);

    /** Load message history for a conversation */
    struct StoredMessage {
        std::string id, convId, senderId, text, mediaUrl;
        int type, status;
        int64_t timestamp;
    };
    std::vector<StoredMessage> getMessages(const std::string& convId, int limit = 50);

    /** Update message delivery status */
    void updateMessageStatus(const std::string& msgId, int status);

    // ── Disk Cache (LRU) ───────────────────

    /** Register a downloaded video file in cache */
    void registerCacheEntry(const std::string& key,
                            const std::string& localPath,
                            int64_t            sizeBytes);

    /** Get local path for a cached video (empty if not cached) */
    std::string getCachedPath(const std::string& key);

    /** Evict oldest entries until under maxCacheMB */
    void evictIfNeeded();

    /** Total cache size in bytes */
    int64_t getCacheSizeBytes();

    // ── Settings ───────────────────────────
    void        setSetting(const std::string& key, const std::string& value);
    std::string getSetting(const std::string& key, const std::string& defaultValue = "");

    // ── Misc ───────────────────────────────
    void clearAll();  // on logout

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    int m_maxCacheBytes;

    void createTables();
    bool exec(const std::string& sql);
};

} // namespace Viral
