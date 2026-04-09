/*
 * ViralUploadEngine.cpp
 *
 * Implementation of the upload engine.
 * Uses ctus TusClient for chunked resumable uploads.
 */

#include "ViralUploadEngine.h"

// ctus — TUS protocol C++ client
#include "ctus/TusClient.h"
#include "ctus/TusStatus.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <condition_variable>

namespace Viral {

/* ─────────────────────────────────────────
 * CONSTRUCTOR / DESTRUCTOR
 * ───────────────────────────────────────── */

ViralUploadEngine::ViralUploadEngine() {
    m_running = true;
    m_worker  = std::thread(&ViralUploadEngine::workerLoop, this);
}

ViralUploadEngine::~ViralUploadEngine() {
    m_running = false;
    if (m_worker.joinable())
        m_worker.join();
}

/* ─────────────────────────────────────────
 * AUTH
 * ───────────────────────────────────────── */

void ViralUploadEngine::setAuthToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authToken = token;
}

/* ─────────────────────────────────────────
 * CALLBACKS
 * ───────────────────────────────────────── */

void ViralUploadEngine::setProgressCallback(ProgressCallback cb) {
    m_progressCb = cb;
}

void ViralUploadEngine::setStatusCallback(StatusCallback cb) {
    m_statusCb = cb;
}

/* ─────────────────────────────────────────
 * JOB ID GENERATOR
 * ───────────────────────────────────────── */

std::string ViralUploadEngine::generateJobId() {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, 15);
    const char hex[] = "0123456789abcdef";
    std::string id = "job-";
    for (int i = 0; i < 12; i++)
        id += hex[dist(rng)];
    return id;
}

/* ─────────────────────────────────────────
 * ENQUEUE
 * ───────────────────────────────────────── */

std::string ViralUploadEngine::enqueue(
    const std::string& filePath,
    const std::string& serverUrl,
    const std::string& title,
    const std::string& description)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if ((int)m_queue.size() >= VIRAL_UPLOAD_QUEUE_MAX) {
        if (m_statusCb)
            m_statusCb("", UploadStatus::FAILED, "Upload queue is full");
        return "";
    }

    UploadJob job;
    job.id          = generateJobId();
    job.filePath    = filePath;
    job.serverUrl   = serverUrl;
    job.title       = title;
    job.description = description;
    job.status      = UploadStatus::QUEUED;
    job.progress    = 0.0f;
    job.retryCount  = 0;

    m_jobs.push_back(job);
    m_queue.push(job.id);

    if (m_statusCb)
        m_statusCb(job.id, UploadStatus::QUEUED, "");

    return job.id;
}

/* ─────────────────────────────────────────
 * CONTROL
 * ───────────────────────────────────────── */

void ViralUploadEngine::pause(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& job : m_jobs) {
        if (job.id == jobId && job.status == UploadStatus::UPLOADING) {
            job.status = UploadStatus::PAUSED;
            if (m_statusCb) m_statusCb(jobId, UploadStatus::PAUSED, "");
            break;
        }
    }
}

void ViralUploadEngine::resume(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& job : m_jobs) {
        if (job.id == jobId && job.status == UploadStatus::PAUSED) {
            job.status = UploadStatus::QUEUED;
            m_queue.push(jobId);
            if (m_statusCb) m_statusCb(jobId, UploadStatus::QUEUED, "Resuming...");
            break;
        }
    }
}

void ViralUploadEngine::cancel(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& job : m_jobs) {
        if (job.id == jobId) {
            job.status = UploadStatus::CANCELLED;
            if (m_statusCb) m_statusCb(jobId, UploadStatus::CANCELLED, "");
            break;
        }
    }
}

void ViralUploadEngine::retry(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& job : m_jobs) {
        if (job.id == jobId && job.status == UploadStatus::FAILED) {
            job.status     = UploadStatus::QUEUED;
            job.retryCount = 0;
            job.progress   = 0.0f;
            m_queue.push(jobId);
            if (m_statusCb) m_statusCb(jobId, UploadStatus::QUEUED, "Retrying...");
            break;
        }
    }
}

void ViralUploadEngine::pauseAll() {
    m_paused = true;
}

void ViralUploadEngine::resumeAll() {
    m_paused = false;
}

/* ─────────────────────────────────────────
 * STATUS / PROGRESS
 * ───────────────────────────────────────── */

UploadStatus ViralUploadEngine::getStatus(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& job : m_jobs)
        if (job.id == jobId) return job.status;
    return UploadStatus::CANCELLED;
}

float ViralUploadEngine::getProgress(const std::string& jobId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& job : m_jobs)
        if (job.id == jobId) return job.progress;
    return 0.0f;
}

std::vector<UploadJob> ViralUploadEngine::getAllJobs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobs;
}

/* ─────────────────────────────────────────
 * WORKER LOOP
 * Background thread — picks jobs from queue
 * ───────────────────────────────────────── */

void ViralUploadEngine::workerLoop() {
    while (m_running) {
        // If paused or queue empty, sleep briefly
        if (m_paused || m_queue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::string jobId;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty()) continue;
            jobId = m_queue.front();
            m_queue.pop();
        }

        // Find the job
        UploadJob* jobPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& j : m_jobs)
                if (j.id == jobId) { jobPtr = &j; break; }
        }

        if (!jobPtr || jobPtr->status == UploadStatus::CANCELLED) continue;

        processJob(*jobPtr);
    }
}

/* ─────────────────────────────────────────
 * PROCESS JOB
 * The actual upload using TusClient
 * ───────────────────────────────────────── */

void ViralUploadEngine::processJob(UploadJob& job) {
    // Update status → UPLOADING
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        job.status = UploadStatus::UPLOADING;
    }
    if (m_statusCb) m_statusCb(job.id, UploadStatus::UPLOADING, "");

    try {
        // Create TusClient
        // chunkSize = VIRAL_UPLOAD_CHUNK_MB * 1024 * 1024 bytes
        TUS::TusClient tusClient(
            "ViralApp",                              // app name (for cache key)
            job.serverUrl,                           // tus server URL
            std::filesystem::path(job.filePath),     // local video file
            VIRAL_UPLOAD_CHUNK_MB * 1024 * 1024      // chunk size in bytes
        );

        // Set auth token if available
        if (!m_authToken.empty()) {
            tusClient.setBearerToken(m_authToken);
        }

        // Poll progress on a side thread while uploading
        std::atomic<bool> uploadDone{false};
        std::string capturedJobId = job.id;

        std::thread progressPoller([&]() {
            while (!uploadDone) {
                float p = tusClient.progress();  // 0.0 - 1.0
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    job.progress = p;
                }
                if (m_progressCb) m_progressCb(capturedJobId, p);

                // Check if cancelled externally
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (job.status == UploadStatus::CANCELLED) {
                        tusClient.cancel();
                        uploadDone = true;
                        return;
                    }
                    if (job.status == UploadStatus::PAUSED) {
                        tusClient.pause();
                        uploadDone = true;
                        return;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });

        // Start upload — blocks until complete or error
        bool success = tusClient.upload();
        uploadDone = true;
        if (progressPoller.joinable()) progressPoller.join();

        // Handle result
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (job.status == UploadStatus::CANCELLED) return;
            if (job.status == UploadStatus::PAUSED)    return;

            if (success) {
                job.status   = UploadStatus::COMPLETED;
                job.progress = 1.0f;
                if (m_statusCb) m_statusCb(job.id, UploadStatus::COMPLETED, "");
            } else {
                job.retryCount++;
                if (job.retryCount < VIRAL_UPLOAD_MAX_RETRY) {
                    // Auto retry
                    job.status = UploadStatus::QUEUED;
                    m_queue.push(job.id);
                    if (m_statusCb) m_statusCb(job.id, UploadStatus::QUEUED,
                        "Retrying (" + std::to_string(job.retryCount) + "/" +
                        std::to_string(VIRAL_UPLOAD_MAX_RETRY) + ")");
                } else {
                    job.status = UploadStatus::FAILED;
                    if (m_statusCb) m_statusCb(job.id, UploadStatus::FAILED,
                        "Upload failed after " + std::to_string(VIRAL_UPLOAD_MAX_RETRY) + " retries");
                }
            }
        }

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(m_mutex);
        job.status = UploadStatus::FAILED;
        if (m_statusCb) m_statusCb(job.id, UploadStatus::FAILED, e.what());
    }
}

} // namespace Viral
