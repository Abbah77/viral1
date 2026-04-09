/*
 * ViralChatEngine.cpp
 *
 * Real-time chat implementation using IXWebSocket.
 * Handles connection, messaging, offline queue,
 * typing indicators, and presence.
 */

#include "ViralChatEngine.h"

// IXWebSocket
#include "ixwebsocket/IXWebSocket.h"
#include "ixwebsocket/IXWebSocketMessage.h"

#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>

namespace Viral {

/* ─────────────────────────────────────────
 * PIMPL — hides IXWebSocket from header
 * ───────────────────────────────────────── */

struct ViralChatEngine::Impl {
    ix::WebSocket ws;
};

/* ─────────────────────────────────────────
 * CONSTRUCTOR / DESTRUCTOR
 * ───────────────────────────────────────── */

ViralChatEngine::ViralChatEngine()
    : m_impl(std::make_unique<Impl>())
{
    // Auto-reconnect — essential for mobile (network switches)
    m_impl->ws.enableAutomaticReconnection();
    m_impl->ws.setMinWaitBetweenReconnectionRetries(1000);   // 1s min
    m_impl->ws.setMaxWaitBetweenReconnectionRetries(30000);  // 30s max
    m_impl->ws.setPingInterval(25);                           // keep-alive ping
}

ViralChatEngine::~ViralChatEngine() {
    disconnect();
}

/* ─────────────────────────────────────────
 * CONNECT
 * ───────────────────────────────────────── */

void ViralChatEngine::connect(const std::string& url,
                               const std::string& token,
                               const std::string& userId)
{
    m_token  = token;
    m_userId = userId;

    m_impl->ws.setUrl(url);

    // Auth header sent on handshake
    ix::WebSocketHttpHeaders headers;
    headers["Authorization"] = "Bearer " + token;
    headers["X-User-Id"]     = userId;
    m_impl->ws.setExtraHeaders(headers);

    // Message callback — fires on every WS event
    m_impl->ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                onConnected();
                break;

            case ix::WebSocketMessageType::Close:
            case ix::WebSocketMessageType::Error:
                onDisconnected();
                break;

            case ix::WebSocketMessageType::Message:
                onRawMessage(msg->str);
                break;

            default:
                break;
        }
    });

    m_impl->ws.start();
}

void ViralChatEngine::disconnect() {
    m_impl->ws.stop();
    m_connected = false;
}

bool ViralChatEngine::isConnected() const {
    return m_connected;
}

/* ─────────────────────────────────────────
 * CALLBACKS SETUP
 * ───────────────────────────────────────── */

void ViralChatEngine::setOnMessageReceived (OnMessageReceived  cb) { m_onMessage    = cb; }
void ViralChatEngine::setOnMessageStatus   (OnMessageStatus    cb) { m_onStatus     = cb; }
void ViralChatEngine::setOnTypingChanged   (OnTypingChanged    cb) { m_onTyping     = cb; }
void ViralChatEngine::setOnPresenceChanged (OnPresenceChanged  cb) { m_onPresence   = cb; }
void ViralChatEngine::setOnConnectionChange(OnConnectionChange cb) { m_onConnection = cb; }

/* ─────────────────────────────────────────
 * CONNECTION EVENTS
 * ───────────────────────────────────────── */

void ViralChatEngine::onConnected() {
    m_connected = true;
    if (m_onConnection) m_onConnection(true);

    // Announce presence
    setUserOnline(true);

    // Flush any messages queued while offline
    flushOfflineQueue();
}

void ViralChatEngine::onDisconnected() {
    m_connected = false;
    if (m_onConnection) m_onConnection(false);
}

/* ─────────────────────────────────────────
 * INCOMING MESSAGE PARSER
 * Simple JSON parsing without dependencies
 * Your team can swap in nlohmann/json later
 * ───────────────────────────────────────── */

// Minimal JSON field extractor
static std::string jsonGet(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) {
        // Try numeric
        search = "\"" + key + "\":";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        auto end = json.find_first_of(",}", pos);
        return json.substr(pos, end - pos);
    }
    pos += search.size();
    auto end = json.find("\"", pos);
    return json.substr(pos, end - pos);
}

void ViralChatEngine::onRawMessage(const std::string& json) {
    std::string type = jsonGet(json, "type");

    if (type == "message") {
        ChatMessage msg;
        msg.id             = jsonGet(json, "id");
        msg.conversationId = jsonGet(json, "conversationId");
        msg.senderId       = jsonGet(json, "senderId");
        msg.recipientId    = m_userId;
        msg.text           = jsonGet(json, "text");
        msg.mediaUrl       = jsonGet(json, "mediaUrl");
        msg.status         = MessageStatus::DELIVERED;

        std::string msgType = jsonGet(json, "msgType");
        if      (msgType == "image") msg.type = MessageType::IMAGE;
        else if (msgType == "video") msg.type = MessageType::VIDEO;
        else if (msgType == "like")  msg.type = MessageType::LIKE;
        else                         msg.type = MessageType::TEXT;

        storeMessage(msg);
        if (m_onMessage) m_onMessage(msg);

        // Auto-acknowledge receipt
        sendRaw("{\"type\":\"delivered\",\"msgId\":\"" + msg.id + "\"}");

    } else if (type == "status") {
        std::string msgId  = jsonGet(json, "msgId");
        std::string status = jsonGet(json, "status");
        MessageStatus ms = MessageStatus::SENT;
        if      (status == "delivered") ms = MessageStatus::DELIVERED;
        else if (status == "read")      ms = MessageStatus::READ;
        if (m_onStatus) m_onStatus(msgId, ms);

    } else if (type == "typing") {
        std::string convId    = jsonGet(json, "conversationId");
        std::string isTyping  = jsonGet(json, "isTyping");
        if (m_onTyping) m_onTyping(convId, isTyping == "true" || isTyping == "1");

    } else if (type == "presence") {
        std::string userId   = jsonGet(json, "userId");
        std::string isOnline = jsonGet(json, "isOnline");
        if (m_onPresence) m_onPresence(userId, isOnline == "true" || isOnline == "1");
    }
}

/* ─────────────────────────────────────────
 * SEND MESSAGES
 * ───────────────────────────────────────── */

std::string ViralChatEngine::generateMsgId() {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, 15);
    const char hex[] = "0123456789abcdef";
    std::string id = "msg-";
    for (int i = 0; i < 16; i++) id += hex[dist(rng)];
    return id;
}

std::string ViralChatEngine::buildMessageJson(const ChatMessage& msg) {
    std::string typeStr = "text";
    if      (msg.type == MessageType::IMAGE) typeStr = "image";
    else if (msg.type == MessageType::VIDEO) typeStr = "video";
    else if (msg.type == MessageType::LIKE)  typeStr = "like";

    return "{\"type\":\"message\","
           "\"id\":\""             + msg.id             + "\","
           "\"conversationId\":\"" + msg.conversationId + "\","
           "\"senderId\":\""       + msg.senderId       + "\","
           "\"recipientId\":\""    + msg.recipientId    + "\","
           "\"msgType\":\""        + typeStr            + "\","
           "\"text\":\""           + msg.text           + "\","
           "\"mediaUrl\":\""       + msg.mediaUrl       + "\"}";
}

std::string ViralChatEngine::sendText(const std::string& conversationId,
                                       const std::string& recipientId,
                                       const std::string& text)
{
    ChatMessage msg;
    msg.id             = generateMsgId();
    msg.conversationId = conversationId;
    msg.senderId       = m_userId;
    msg.recipientId    = recipientId;
    msg.type           = MessageType::TEXT;
    msg.text           = text;
    msg.status         = MessageStatus::PENDING;
    msg.timestamp      = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();

    storeMessage(msg);
    sendRaw(buildMessageJson(msg));
    return msg.id;
}

std::string ViralChatEngine::sendMedia(const std::string& conversationId,
                                        const std::string& recipientId,
                                        const std::string& mediaUrl,
                                        MessageType        type)
{
    ChatMessage msg;
    msg.id             = generateMsgId();
    msg.conversationId = conversationId;
    msg.senderId       = m_userId;
    msg.recipientId    = recipientId;
    msg.type           = type;
    msg.mediaUrl       = mediaUrl;
    msg.status         = MessageStatus::PENDING;
    msg.timestamp      = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();

    storeMessage(msg);
    sendRaw(buildMessageJson(msg));
    return msg.id;
}

std::string ViralChatEngine::sendLike(const std::string& conversationId,
                                       const std::string& recipientId)
{
    return sendMedia(conversationId, recipientId, "", MessageType::LIKE);
}

void ViralChatEngine::markRead(const std::string& conversationId) {
    sendRaw("{\"type\":\"read\",\"conversationId\":\"" + conversationId + "\"}");
}

/* ─────────────────────────────────────────
 * TYPING
 * ───────────────────────────────────────── */

void ViralChatEngine::sendTypingStart(const std::string& conversationId) {
    sendRaw("{\"type\":\"typing\",\"conversationId\":\"" + conversationId + "\",\"isTyping\":true}");
}

void ViralChatEngine::sendTypingStop(const std::string& conversationId) {
    sendRaw("{\"type\":\"typing\",\"conversationId\":\"" + conversationId + "\",\"isTyping\":false}");
}

/* ─────────────────────────────────────────
 * PRESENCE
 * ───────────────────────────────────────── */

void ViralChatEngine::setUserOnline(bool online) {
    sendRaw("{\"type\":\"presence\",\"isOnline\":" + std::string(online ? "true" : "false") + "}");
}

/* ─────────────────────────────────────────
 * SEND RAW — queues if offline
 * ───────────────────────────────────────── */

void ViralChatEngine::sendRaw(const std::string& json) {
    if (m_connected) {
        m_impl->ws.sendText(json);
    } else {
        // Queue for when we reconnect
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_offlineQueue.push(json);
    }
}

void ViralChatEngine::flushOfflineQueue() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_offlineQueue.empty()) {
        m_impl->ws.sendText(m_offlineQueue.front());
        m_offlineQueue.pop();
    }
}

/* ─────────────────────────────────────────
 * MESSAGE STORE
 * ───────────────────────────────────────── */

void ViralChatEngine::storeMessage(const ChatMessage& msg) {
    std::lock_guard<std::mutex> lock(m_storeMutex);
    m_messages[msg.conversationId].push_back(msg);
}

std::vector<ChatMessage> ViralChatEngine::getMessages(
    const std::string& conversationId, int limit)
{
    std::lock_guard<std::mutex> lock(m_storeMutex);
    auto& msgs = m_messages[conversationId];
    if ((int)msgs.size() <= limit) return msgs;
    return std::vector<ChatMessage>(msgs.end() - limit, msgs.end());
}

std::vector<Conversation> ViralChatEngine::getConversations() {
    std::lock_guard<std::mutex> lock(m_storeMutex);
    std::vector<Conversation> result;
    for (auto& kv : m_conversations)
        result.push_back(kv.second);
    return result;
}

} // namespace Viral
