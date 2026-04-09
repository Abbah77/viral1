/*
 * ViralStorageEngine.cpp
 *
 * SQLite-backed local storage.
 * SQLite is bundled with Android and iOS — no extra dependency needed.
 */

#include "ViralStorageEngine.h"
#include <sqlite3.h>
#include <chrono>
#include <algorithm>
#include <cstring>

namespace Viral {

/* ─────────────────────────────────────────
 * PIMPL
 * ───────────────────────────────────────── */

struct ViralStorageEngine::Impl {
    sqlite3* db = nullptr;
};

static int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/* ─────────────────────────────────────────
 * CONSTRUCTOR
 * ───────────────────────────────────────── */

ViralStorageEngine::ViralStorageEngine(const std::string& dbPath, int maxCacheMB)
    : m_impl(std::make_unique<Impl>())
    , m_maxCacheBytes(maxCacheMB * 1024 * 1024)
{
    sqlite3_open(dbPath.c_str(), &m_impl->db);
    // WAL mode — much faster for concurrent read/write
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    createTables();
}

ViralStorageEngine::~ViralStorageEngine() {
    if (m_impl->db) sqlite3_close(m_impl->db);
}

/* ─────────────────────────────────────────
 * HELPERS
 * ───────────────────────────────────────── */

bool ViralStorageEngine::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(m_impl->db, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

static std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

/* ─────────────────────────────────────────
 * CREATE TABLES
 * ───────────────────────────────────────── */

void ViralStorageEngine::createTables() {
    exec(R"(
        CREATE TABLE IF NOT EXISTS feed_videos (
            id TEXT PRIMARY KEY,
            url TEXT, thumbnail_url TEXT,
            author_id TEXT, author_name TEXT,
            description TEXT,
            likes INTEGER DEFAULT 0,
            comments INTEGER DEFAULT 0,
            shares INTEGER DEFAULT 0,
            timestamp INTEGER DEFAULT 0,
            is_liked INTEGER DEFAULT 0,
            is_following INTEGER DEFAULT 0
        );
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS user_profiles (
            id TEXT PRIMARY KEY,
            username TEXT, display_name TEXT,
            avatar_url TEXT, bio TEXT,
            followers INTEGER DEFAULT 0,
            following_count INTEGER DEFAULT 0,
            video_count INTEGER DEFAULT 0,
            is_following INTEGER DEFAULT 0
        );
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS chat_messages (
            id TEXT PRIMARY KEY,
            conv_id TEXT, sender_id TEXT,
            type INTEGER DEFAULT 1,
            text TEXT, media_url TEXT,
            timestamp INTEGER DEFAULT 0,
            status INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_chat_conv ON chat_messages(conv_id, timestamp);
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS disk_cache (
            key TEXT PRIMARY KEY,
            local_path TEXT,
            size INTEGER DEFAULT 0,
            last_access INTEGER DEFAULT 0
        );
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT
        );
    )");
}

/* ─────────────────────────────────────────
 * FEED VIDEOS
 * ───────────────────────────────────────── */

void ViralStorageEngine::saveFeedVideos(const std::vector<VideoItem>& videos) {
    exec("BEGIN TRANSACTION;");
    for (auto& v : videos) {
        std::string sql =
            "INSERT OR REPLACE INTO feed_videos VALUES ('"
            + esc(v.id)           + "','"
            + esc(v.url)          + "','"
            + esc(v.thumbnailUrl) + "','"
            + esc(v.authorId)     + "','"
            + esc(v.authorName)   + "','"
            + esc(v.description)  + "',"
            + std::to_string(v.likes)    + ","
            + std::to_string(v.comments) + ","
            + std::to_string(v.shares)   + ","
            + std::to_string(v.timestamp)+ ","
            + std::to_string(v.isLiked ? 1 : 0)     + ","
            + std::to_string(v.isFollowing ? 1 : 0)  + ");";
        exec(sql);
    }
    exec("COMMIT;");
}

std::vector<VideoItem> ViralStorageEngine::getFeedVideos(int limit, int offset) {
    std::vector<VideoItem> result;
    std::string sql = "SELECT * FROM feed_videos ORDER BY timestamp DESC LIMIT "
        + std::to_string(limit) + " OFFSET " + std::to_string(offset) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_impl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        VideoItem v;
        auto col = [&](int i) -> std::string {
            auto* t = (const char*)sqlite3_column_text(stmt, i);
            return t ? t : "";
        };
        v.id           = col(0);
        v.url          = col(1);
        v.thumbnailUrl = col(2);
        v.authorId     = col(3);
        v.authorName   = col(4);
        v.description  = col(5);
        v.likes        = sqlite3_column_int64(stmt, 6);
        v.comments     = sqlite3_column_int64(stmt, 7);
        v.shares       = sqlite3_column_int64(stmt, 8);
        v.timestamp    = sqlite3_column_int64(stmt, 9);
        v.isLiked      = sqlite3_column_int(stmt, 10) != 0;
        v.isFollowing  = sqlite3_column_int(stmt, 11) != 0;
        result.push_back(v);
    }
    sqlite3_finalize(stmt);
    return result;
}

void ViralStorageEngine::setVideoLiked(const std::string& videoId, bool liked) {
    exec("UPDATE feed_videos SET is_liked=" + std::string(liked?"1":"0")
        + " WHERE id='" + esc(videoId) + "';");
}

void ViralStorageEngine::clearFeedCache() {
    exec("DELETE FROM feed_videos;");
}

/* ─────────────────────────────────────────
 * USER PROFILES
 * ───────────────────────────────────────── */

void ViralStorageEngine::saveUserProfile(const UserProfile& p) {
    std::string sql =
        "INSERT OR REPLACE INTO user_profiles VALUES ('"
        + esc(p.id)          + "','"
        + esc(p.username)    + "','"
        + esc(p.displayName) + "','"
        + esc(p.avatarUrl)   + "','"
        + esc(p.bio)         + "',"
        + std::to_string(p.followers)  + ","
        + std::to_string(p.following)  + ","
        + std::to_string(p.videoCount) + ","
        + std::to_string(p.isFollowing ? 1 : 0) + ");";
    exec(sql);
}

UserProfile ViralStorageEngine::getUserProfile(const std::string& userId) {
    UserProfile p;
    std::string sql = "SELECT * FROM user_profiles WHERE id='" + esc(userId) + "';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_impl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return p;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto col = [&](int i) -> std::string {
            auto* t = (const char*)sqlite3_column_text(stmt, i);
            return t ? t : "";
        };
        p.id          = col(0);
        p.username    = col(1);
        p.displayName = col(2);
        p.avatarUrl   = col(3);
        p.bio         = col(4);
        p.followers   = sqlite3_column_int64(stmt, 5);
        p.following   = sqlite3_column_int64(stmt, 6);
        p.videoCount  = sqlite3_column_int64(stmt, 7);
        p.isFollowing = sqlite3_column_int(stmt, 8) != 0;
    }
    sqlite3_finalize(stmt);
    return p;
}

void ViralStorageEngine::setFollowing(const std::string& userId, bool following) {
    exec("UPDATE user_profiles SET is_following=" + std::string(following?"1":"0")
        + " WHERE id='" + esc(userId) + "';");
}

/* ─────────────────────────────────────────
 * CHAT MESSAGES
 * ───────────────────────────────────────── */

void ViralStorageEngine::saveMessage(const std::string& convId,
                                      const std::string& msgId,
                                      const std::string& senderId,
                                      int type,
                                      const std::string& text,
                                      const std::string& mediaUrl,
                                      int64_t timestamp,
                                      int status)
{
    std::string sql =
        "INSERT OR REPLACE INTO chat_messages VALUES ('"
        + esc(msgId)    + "','"
        + esc(convId)   + "','"
        + esc(senderId) + "',"
        + std::to_string(type)      + ",'"
        + esc(text)     + "','"
        + esc(mediaUrl) + "',"
        + std::to_string(timestamp) + ","
        + std::to_string(status)    + ");";
    exec(sql);
}

std::vector<ViralStorageEngine::StoredMessage>
ViralStorageEngine::getMessages(const std::string& convId, int limit) {
    std::vector<StoredMessage> result;
    std::string sql = "SELECT * FROM chat_messages WHERE conv_id='"
        + esc(convId) + "' ORDER BY timestamp DESC LIMIT " + std::to_string(limit) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_impl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StoredMessage m;
        auto col = [&](int i) -> std::string {
            auto* t = (const char*)sqlite3_column_text(stmt, i);
            return t ? t : "";
        };
        m.id        = col(0);
        m.convId    = col(1);
        m.senderId  = col(2);
        m.type      = sqlite3_column_int(stmt, 3);
        m.text      = col(4);
        m.mediaUrl  = col(5);
        m.timestamp = sqlite3_column_int64(stmt, 6);
        m.status    = sqlite3_column_int(stmt, 7);
        result.push_back(m);
    }
    sqlite3_finalize(stmt);
    return result;
}

void ViralStorageEngine::updateMessageStatus(const std::string& msgId, int status) {
    exec("UPDATE chat_messages SET status=" + std::to_string(status)
        + " WHERE id='" + esc(msgId) + "';");
}

/* ─────────────────────────────────────────
 * DISK CACHE (LRU)
 * ───────────────────────────────────────── */

void ViralStorageEngine::registerCacheEntry(const std::string& key,
                                             const std::string& localPath,
                                             int64_t sizeBytes)
{
    std::string sql =
        "INSERT OR REPLACE INTO disk_cache VALUES ('"
        + esc(key)       + "','"
        + esc(localPath) + "',"
        + std::to_string(sizeBytes) + ","
        + std::to_string(nowMs())   + ");";
    exec(sql);
    evictIfNeeded();
}

std::string ViralStorageEngine::getCachedPath(const std::string& key) {
    std::string sql = "SELECT local_path FROM disk_cache WHERE key='" + esc(key) + "';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_impl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return "";
    std::string path;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto* t = (const char*)sqlite3_column_text(stmt, 0);
        if (t) path = t;
        // Update LRU timestamp
        exec("UPDATE disk_cache SET last_access=" + std::to_string(nowMs())
            + " WHERE key='" + esc(key) + "';");
    }
    sqlite3_finalize(stmt);
    return path;
}

int64_t ViralStorageEngine::getCacheSizeBytes() {
    sqlite3_stmt* stmt;
    exec("SELECT SUM(size) FROM disk_cache;");
    if (sqlite3_prepare_v2(m_impl->db,
        "SELECT SUM(size) FROM disk_cache;", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    int64_t total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        total = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

void ViralStorageEngine::evictIfNeeded() {
    while (getCacheSizeBytes() > m_maxCacheBytes) {
        // Delete oldest accessed entry
        exec("DELETE FROM disk_cache WHERE key = "
             "(SELECT key FROM disk_cache ORDER BY last_access ASC LIMIT 1);");
    }
}

/* ─────────────────────────────────────────
 * SETTINGS
 * ───────────────────────────────────────── */

void ViralStorageEngine::setSetting(const std::string& key, const std::string& value) {
    exec("INSERT OR REPLACE INTO settings VALUES ('"
        + esc(key) + "','" + esc(value) + "');");
}

std::string ViralStorageEngine::getSetting(const std::string& key,
                                            const std::string& defaultValue)
{
    std::string sql = "SELECT value FROM settings WHERE key='" + esc(key) + "';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_impl->db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return defaultValue;
    std::string val = defaultValue;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto* t = (const char*)sqlite3_column_text(stmt, 0);
        if (t) val = t;
    }
    sqlite3_finalize(stmt);
    return val;
}

/* ─────────────────────────────────────────
 * CLEAR ALL (logout)
 * ───────────────────────────────────────── */

void ViralStorageEngine::clearAll() {
    exec("DELETE FROM feed_videos;");
    exec("DELETE FROM user_profiles;");
    exec("DELETE FROM chat_messages;");
    exec("DELETE FROM disk_cache;");
    exec("DELETE FROM settings;");
}

} // namespace Viral
