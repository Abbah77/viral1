/*
 * ViralSDK_iOS.m
 *
 * Umbrella SDK implementation for iOS.
 * Ties together: ViralEngine + ViralUploadEngine
 *              + ViralChatEngine + ViralStorageEngine
 */

#import "ViralSDK_iOS.h"
#import "ViralEngine_iOS.h"

#include "ViralUploadEngine.h"
#include "ViralChatEngine.h"
#include "ViralStorageEngine.h"

using namespace Viral;

// ─────────────────────────────────────────
// ViralMessage impl
// ─────────────────────────────────────────

@implementation ViralMessage @end

// ─────────────────────────────────────────
// ViralSDK impl
// ─────────────────────────────────────────

@interface ViralSDK () <ViralFeedDelegate>
@property (nonatomic, strong) ViralEngineiOS     *feedEngine;
@property (nonatomic, assign) ViralUploadEngine  *uploadEngine;
@property (nonatomic, assign) ViralChatEngine    *chatEngine;
@property (nonatomic, assign) ViralStorageEngine *storageEngine;
@end

@implementation ViralSDK

+ (instancetype)shared {
    static ViralSDK *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{ instance = [[ViralSDK alloc] init]; });
    return instance;
}

- (void)startWithUserId:(NSString *)userId
                  token:(NSString *)token
                chatUrl:(NSString *)chatUrl
                 dbPath:(NSString *)dbPath
{
    // 1. Init engines
    [ViralEngineiOS initEngine];

    _feedEngine    = [[ViralEngineiOS alloc] initWithDelegate:self];
    _uploadEngine  = new ViralUploadEngine();
    _chatEngine    = new ViralChatEngine();
    _storageEngine = new ViralStorageEngine([dbPath UTF8String], 500);

    // 2. Auth token for uploads
    _uploadEngine->setAuthToken([token UTF8String]);

    // 3. Wire upload callbacks
    __weak ViralSDK *weakSelf = self;
    _uploadEngine->setProgressCallback([weakSelf](const std::string& jobId, float progress) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.uploadDelegate uploadJob:@(jobId.c_str()) didProgress:progress];
        });
    });
    _uploadEngine->setStatusCallback([weakSelf](const std::string& jobId,
                                                UploadStatus status,
                                                const std::string& msg) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.uploadDelegate uploadJob:@(jobId.c_str())
                              didChangeStatus:(ViralUploadStatus)status
                                      message:@(msg.c_str())];
        });
    });

    // 4. Wire chat callbacks
    _chatEngine->setOnMessageReceived([weakSelf](const ChatMessage& m) {
        ViralMessage *msg  = [[ViralMessage alloc] init];
        msg.messageId      = @(m.id.c_str());
        msg.conversationId = @(m.conversationId.c_str());
        msg.senderId       = @(m.senderId.c_str());
        msg.type           = (ViralMessageType)m.type;
        msg.text           = @(m.text.c_str());
        msg.mediaUrl       = @(m.mediaUrl.c_str());
        msg.timestamp      = m.timestamp;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.chatDelegate didReceiveMessage:msg];
        });
    });

    _chatEngine->setOnTypingChanged([weakSelf](const std::string& convId, bool isTyping) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.chatDelegate conversationId:@(convId.c_str()) typingChanged:isTyping];
        });
    });

    _chatEngine->setOnPresenceChanged([weakSelf](const std::string& uid, bool online) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.chatDelegate userId:@(uid.c_str()) onlineChanged:online];
        });
    });

    _chatEngine->setOnConnectionChange([weakSelf](bool connected) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.chatDelegate chatConnectionChanged:connected];
        });
    });

    // 5. Connect chat
    _chatEngine->connect([chatUrl UTF8String], [token UTF8String], [userId UTF8String]);
}

- (void)stop {
    if (_chatEngine)    { _chatEngine->disconnect(); delete _chatEngine;    _chatEngine    = nullptr; }
    if (_uploadEngine)  { delete _uploadEngine;  _uploadEngine  = nullptr; }
    if (_storageEngine) { delete _storageEngine; _storageEngine = nullptr; }
    [_feedEngine destroy];
    [ViralEngineiOS uninitEngine];
}

// ── Feed ──────────────────────────────────

- (void)setFeedUrls:(NSArray<NSString *> *)urls {
    [_feedEngine setUrls:urls];
}

- (void)scrollFeedTo:(NSInteger)index {
    [_feedEngine scrollTo:index];
}

- (void)setPlayerLayer:(CALayer *)layer {
    [_feedEngine setLayer:layer];
}

- (void)playFeed  { [_feedEngine play];  }
- (void)pauseFeed { [_feedEngine pause]; }
- (void)muteFeed:(BOOL)muted { [_feedEngine mute:muted]; }

// ViralFeedDelegate
- (void)feedIndex:(NSInteger)index didReceiveEvent:(ViralFeedEvent)event {
    [_feedDelegate feedIndex:index didReceiveEvent:event];
}

// ── Upload ────────────────────────────────

- (NSString *)enqueueUpload:(NSString *)filePath
                  serverUrl:(NSString *)serverUrl
                      title:(NSString *)title
                description:(NSString *)description
{
    std::string jobId = _uploadEngine->enqueue(
        [filePath UTF8String], [serverUrl UTF8String],
        [title UTF8String], [description UTF8String]);
    return @(jobId.c_str());
}

- (void)pauseUpload:(NSString *)jobId   { _uploadEngine->pause([jobId UTF8String]);  }
- (void)resumeUpload:(NSString *)jobId  { _uploadEngine->resume([jobId UTF8String]); }
- (void)cancelUpload:(NSString *)jobId  { _uploadEngine->cancel([jobId UTF8String]); }
- (void)pauseAllUploads                 { _uploadEngine->pauseAll();  }
- (void)resumeAllUploads                { _uploadEngine->resumeAll(); }

// ── Chat ──────────────────────────────────

- (NSString *)sendText:(NSString *)text conversationId:(NSString *)convId recipientId:(NSString *)recipientId {
    return @(_chatEngine->sendText([convId UTF8String], [recipientId UTF8String], [text UTF8String]).c_str());
}

- (NSString *)sendImage:(NSString *)imageUrl conversationId:(NSString *)convId recipientId:(NSString *)recipientId {
    return @(_chatEngine->sendMedia([convId UTF8String], [recipientId UTF8String], [imageUrl UTF8String], MessageType::IMAGE).c_str());
}

- (NSString *)sendLike:(NSString *)convId recipientId:(NSString *)recipientId {
    return @(_chatEngine->sendLike([convId UTF8String], [recipientId UTF8String]).c_str());
}

- (void)markRead:(NSString *)convId    { _chatEngine->markRead([convId UTF8String]);        }
- (void)typingStart:(NSString *)convId { _chatEngine->sendTypingStart([convId UTF8String]); }
- (void)typingStop:(NSString *)convId  { _chatEngine->sendTypingStop([convId UTF8String]);  }

// ── Storage ───────────────────────────────

- (void)setSetting:(NSString *)value forKey:(NSString *)key {
    _storageEngine->setSetting([key UTF8String], [value UTF8String]);
}

- (NSString *)getSetting:(NSString *)key defaultValue:(NSString *)def {
    return @(_storageEngine->getSetting([key UTF8String], [def UTF8String]).c_str());
}

- (NSString *)getCachedVideoPath:(NSString *)url {
    return @(_storageEngine->getCachedPath([url UTF8String]).c_str());
}

- (void)registerCachedVideo:(NSString *)url localPath:(NSString *)path sizeBytes:(int64_t)size {
    _storageEngine->registerCacheEntry([url UTF8String], [path UTF8String], size);
}

- (void)logout {
    _storageEngine->clearAll();
    _chatEngine->disconnect();
}

@end
