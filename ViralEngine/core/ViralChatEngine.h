/*
 * ViralChatEngine.h
 *
 * Real-time chat engine for TikTok-style app.
 * Built on top of IXWebSocket (C++11, iOS + Android tested).
 *
 * Features:
 *   - Real-time messaging via WebSocket
 *   - Auto-reconnect on network drop
 *   - Offline message queue (send when back online)
 *   - Typing indicators
 *   - Online presence (who is online)
 *   - Media message support (image/video URLs)
 *   - Message delivery status (sent, delivered, read)
 */

#pragma once

#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>

namespace Viral {

/* ─────────────────────────────────────────
 * MESSAGE TYPES
 * ───────────────────────────────────────── */

enum class MessageType {
    TEXT    = 1,
    IMAGE   = 2,
    VIDEO   = 3,
    LIKE    = 4,   // heart reaction
    SYSTEM  = 5,   // "User started following you" etc
};

enum class MessageStatus {
    PENDING   = 0,  // in offline queue
    SENT      = 1,  // delivered to server
    DELIVERED = 2,  // delivered to recipient device
    READ      = 3,  // recipient opened it
    FAILED    = 4,
};

/* ─────────────────────────────────────────
 * MESSAGE
 * ───────────────────────────────────────── */

struct ChatMessage {
    std::string   id;           // unique message ID
    std::string   conversationId;
    std::string   senderId;
    std::string   recipientId;
    MessageType   type;
    std::string   text;         // for TEXT messages
    std::string   mediaUrl;     // for IMAGE/VIDEO messages
    MessageStatus status;
    int64_t       timestamp;    // unix ms
};

/* ─────────────────────────────────────────
 * CONVERSATION
 * ───────────────────────────────────────── */

struct Conversation {
    std::string            id;
    std::string            participantId;   // other user
    std::string            participantName;
    std::string            participantAvatar;
    ChatMessage            lastMessage;
    int                    unreadCount;
    bool                   isOnline;
    bool                   isTyping;
};

/* ─────────────────────────────────────────
 * CALLBACKS → fired to Kotlin/Swift
 * ───────────────────────────────────────── */

using OnMessageReceived  = std::function<void(const ChatMessage&)>;
using OnMessageStatus    = std::function<void(const std::string& msgId, MessageStatus)>;
using OnTypingChanged    = std::function<void(const std::string& conversationId, bool isTyping)>;
using OnPresenceChanged  = std::function<void(const std::string& userId, bool isOnline)>;
using OnConnectionChange = std::function<void(bool connected)>;

/* ─────────────────────────────────────────
 * VIRAL CHAT ENGINE
 * ───────────────────────────────────────── */

class ViralChatEngine {
public:
    ViralChatEngine();
    ~ViralChatEngine();

    // ── Setup ─────────────────────────────

    /**
     * Connect to your chat WebSocket server.
     * url example: "wss://api.yourapp.com/chat"
     * token: JWT bearer token for auth
     * userId: current logged-in user's ID
     */
    void connect(const std::string& url,
                 const std::string& token,
                 const std::string& userId);

    void disconnect();

    bool isConnected() const;

    // ── Callbacks ─────────────────────────
    void setOnMessageReceived (OnMessageReceived  cb);
    void setOnMessageStatus   (OnMessageStatus    cb);
    void setOnTypingChanged   (OnTypingChanged    cb);
    void setOnPresenceChanged (OnPresenceChanged  cb);
    void setOnConnectionChange(OnConnectionChange cb);

    // ── Messaging ─────────────────────────

    /**
     * Send a text message.
     * Returns message ID. If offline, queues it automatically.
     */
    std::string sendText(const std::string& conversationId,
                         const std::string& recipientId,
                         const std::string& text);

    /**
     * Send a media message (image/video URL after upload).
     */
    std::string sendMedia(const std::string& conversationId,
                          const std::string& recipientId,
                          const std::string& mediaUrl,
                          MessageType        type);

    /**
     * Send a like/heart reaction to a video.
     */
    std::string sendLike(const std::string& conversationId,
                         const std::string& recipientId);

    /** Mark messages as read */
    void markRead(const std::string& conversationId);

    // ── Typing ────────────────────────────
    void sendTypingStart(const std::string& conversationId);
    void sendTypingStop (const std::string& conversationId);

    // ── Presence ──────────────────────────
    void setUserOnline(bool online);

    // ── Conversations ─────────────────────
    std::vector<Conversation> getConversations();
    std::vector<ChatMessage>  getMessages(const std::string& conversationId,
                                          int limit = 50);

private:
    void onRawMessage(const std::string& json);
    void onConnected();
    void onDisconnected();
    void flushOfflineQueue();
    void sendRaw(const std::string& json);
    std::string generateMsgId();
    std::string buildMessageJson(const ChatMessage& msg);
    void storeMessage(const ChatMessage& msg);

    // IXWebSocket instance (forward declared to avoid header pollution)
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string m_userId;
    std::string m_token;

    // Offline queue — messages sent while disconnected
    std::queue<std::string>     m_offlineQueue;
    std::mutex                  m_queueMutex;

    // In-memory message store (keyed by conversationId)
    std::map<std::string, std::vector<ChatMessage>> m_messages;
    std::map<std::string, Conversation>             m_conversations;
    std::mutex                                      m_storeMutex;

    std::atomic<bool> m_connected{false};

    // Callbacks
    OnMessageReceived  m_onMessage;
    OnMessageStatus    m_onStatus;
    OnTypingChanged    m_onTyping;
    OnPresenceChanged  m_onPresence;
    OnConnectionChange m_onConnection;
};

} // namespace Viral
