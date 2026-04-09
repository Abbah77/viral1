/*
 * ViralUploadEngine.h
 *
 * Upload engine for TikTok-style video app.
 * Wraps ctus (TUS protocol C++ client) to provide:
 *   - Chunked resumable video upload
 *   - Background upload with progress callbacks
 *   - Auto-retry on network failure
 *   - Upload queue (multiple videos)
 *   - Bearer token auth support
 *   - Both Android (JNI) and iOS (ObjC) friendly
 */

#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace Viral {

/* ─────────────────────────────────────────
 * CONFIG
 * ───────────────────────────────────────── */

constexpr int   VIRAL_UPLOAD_CHUNK_MB   = 5;     // 5MB chunks (TUS minimum)
constexpr int   VIRAL_UPLOAD_MAX_RETRY  = 3;     // retry 3x on failure
constexpr int   VIRAL_UPLOAD_QUEUE_MAX  = 10;    // max queued uploads

/* ─────────────────────────────────────────
 * UPLOAD STATUS
 * ───────────────────────────────────────── */

enum class UploadStatus {
    QUEUED,
    COMPRESSING,   // optional pre-compression step
    UPLOADING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

/* ─────────────────────────────────────────
 * UPLOAD JOB
 * Represents one video upload task
 * ───────────────────────────────────────── */

struct UploadJob {
    std::string  id;           // unique job ID
    std::string  filePath;     // local file path
    std::string  serverUrl;    // tus server endpoint
    std::string  title;        // video title (metadata)
    std::string  description;  // video description (metadata)
    UploadStatus status;
    float        progress;     // 0.0 - 1.0
    int          retryCount;
};

/* ─────────────────────────────────────────
 * CALLBACKS
 * ───────────────────────────────────────── */

using ProgressCallback = std::function<void(
    const std::string& jobId,
    float progress           // 0.0 to 1.0
)>;

using StatusCallback = std::function<void(
    const std::string& jobId,
    UploadStatus status,
    const std::string& message  // error message if FAILED
)>;

/* ─────────────────────────────────────────
 * VIRAL UPLOAD ENGINE
 * ───────────────────────────────────────── */

class ViralUploadEngine {
public:
    ViralUploadEngine();
    ~ViralUploadEngine();

    // ── Auth ──────────────────────────────
    void setAuthToken(const std::string& token);

    // ── Callbacks ─────────────────────────
    void setProgressCallback(ProgressCallback cb);
    void setStatusCallback(StatusCallback cb);

    // ── Upload Control ────────────────────

    /**
     * Add a video to the upload queue.
     * Returns a jobId you can use to track/cancel it.
     */
    std::string enqueue(
        const std::string& filePath,
        const std::string& serverUrl,
        const std::string& title       = "",
        const std::string& description = ""
    );

    /** Pause a specific upload */
    void pause(const std::string& jobId);

    /** Resume a paused upload */
    void resume(const std::string& jobId);

    /** Cancel and remove from queue */
    void cancel(const std::string& jobId);

    /** Retry a failed upload */
    void retry(const std::string& jobId);

    /** Pause ALL uploads (e.g. app goes to background) */
    void pauseAll();

    /** Resume ALL uploads */
    void resumeAll();

    // ── Status ────────────────────────────
    UploadStatus getStatus(const std::string& jobId);
    float        getProgress(const std::string& jobId);

    /** Get all jobs (for upload history UI) */
    std::vector<UploadJob> getAllJobs();

private:
    // Worker thread that processes the queue
    void workerLoop();
    void processJob(UploadJob& job);
    std::string generateJobId();

    std::vector<UploadJob>      m_jobs;
    std::queue<std::string>     m_queue;    // jobIds waiting to upload
    std::mutex                  m_mutex;
    std::thread                 m_worker;
    std::atomic<bool>           m_running{false};
    std::atomic<bool>           m_paused{false};

    std::string                 m_authToken;
    ProgressCallback            m_progressCb;
    StatusCallback              m_statusCb;
};

} // namespace Viral
