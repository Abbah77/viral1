/*
 * ViralSDK_iOS.h
 *
 * Single umbrella header for all iOS bridges.
 * Import this one file in your Swift project.
 *
 * Swift usage:
 *   import ViralEngine  (after adding framework to Xcode)
 *
 *   // App startup
 *   ViralSDK.shared.start(
 *       userId: currentUser.id,
 *       token:  currentUser.jwtToken,
 *       chatUrl: "wss://api.yourapp.com/chat",
 *       dbPath:  documentsDirectory + "/viral.db"
 *   )
 */

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

NS_ASSUME_NONNULL_BEGIN

// ─────────────────────────────────────────
// FEED EVENTS
// ─────────────────────────────────────────

typedef NS_ENUM(NSInteger, ViralFeedEvent) {
    ViralFeedEventReady     = 1,
    ViralFeedEventStarted   = 2,
    ViralFeedEventPaused    = 3,
    ViralFeedEventCompleted = 4,
    ViralFeedEventError     = 5,
    ViralFeedEventBuffering = 6,
};

@protocol ViralFeedDelegate <NSObject>
- (void)feedIndex:(NSInteger)index didReceiveEvent:(ViralFeedEvent)event;
@end

// ─────────────────────────────────────────
// UPLOAD STATUS
// ─────────────────────────────────────────

typedef NS_ENUM(NSInteger, ViralUploadStatus) {
    ViralUploadQueued     = 0,
    ViralUploadUploading  = 2,
    ViralUploadPaused     = 3,
    ViralUploadCompleted  = 4,
    ViralUploadFailed     = 5,
    ViralUploadCancelled  = 6,
};

@protocol ViralUploadDelegate <NSObject>
- (void)uploadJob:(NSString *)jobId didProgress:(float)progress;
- (void)uploadJob:(NSString *)jobId didChangeStatus:(ViralUploadStatus)status message:(NSString *)message;
@end

// ─────────────────────────────────────────
// CHAT
// ─────────────────────────────────────────

typedef NS_ENUM(NSInteger, ViralMessageType) {
    ViralMessageText  = 1,
    ViralMessageImage = 2,
    ViralMessageVideo = 3,
    ViralMessageLike  = 4,
};

typedef NS_ENUM(NSInteger, ViralMessageStatus) {
    ViralMessagePending   = 0,
    ViralMessageSent      = 1,
    ViralMessageDelivered = 2,
    ViralMessageRead      = 3,
    ViralMessageFailed    = 4,
};

@interface ViralMessage : NSObject
@property (nonatomic, strong) NSString *messageId;
@property (nonatomic, strong) NSString *conversationId;
@property (nonatomic, strong) NSString *senderId;
@property (nonatomic, assign) ViralMessageType type;
@property (nonatomic, strong) NSString *text;
@property (nonatomic, strong) NSString *mediaUrl;
@property (nonatomic, assign) int64_t   timestamp;
@end

@protocol ViralChatDelegate <NSObject>
- (void)didReceiveMessage:(ViralMessage *)message;
- (void)messageId:(NSString *)msgId didChangeStatus:(ViralMessageStatus)status;
- (void)conversationId:(NSString *)convId typingChanged:(BOOL)isTyping;
- (void)userId:(NSString *)userId onlineChanged:(BOOL)isOnline;
- (void)chatConnectionChanged:(BOOL)connected;
@end

// ─────────────────────────────────────────
// VIRAL SDK — main entry point for iOS/Swift
// ─────────────────────────────────────────

@interface ViralSDK : NSObject

+ (instancetype)shared;

/**
 * Call in AppDelegate.application:didFinishLaunchingWithOptions:
 */
- (void)startWithUserId:(NSString *)userId
                  token:(NSString *)token
                chatUrl:(NSString *)chatUrl
                 dbPath:(NSString *)dbPath;

/**
 * Call in AppDelegate.applicationWillTerminate:
 */
- (void)stop;

// ── Video Feed ─────────────────────────────

/**
 * Set the list of video URLs for the feed.
 * Call after fetching feed from your API.
 */
- (void)setFeedUrls:(NSArray<NSString *> *)urls;

/**
 * Call when user swipes to a new index.
 * Engine handles preload, pause old, play new.
 */
- (void)scrollFeedTo:(NSInteger)index;

/** Attach your UIView's layer to receive video frames */
- (void)setPlayerLayer:(CALayer *)layer;

- (void)playFeed;
- (void)pauseFeed;
- (void)muteFeed:(BOOL)muted;

@property (nonatomic, weak, nullable) id<ViralFeedDelegate> feedDelegate;

// ── Upload ─────────────────────────────────

/**
 * Enqueue a local video file for upload.
 * Returns jobId for tracking.
 */
- (NSString *)enqueueUpload:(NSString *)filePath
                  serverUrl:(NSString *)serverUrl
                      title:(NSString *)title
                description:(NSString *)description;

- (void)pauseUpload:(NSString *)jobId;
- (void)resumeUpload:(NSString *)jobId;
- (void)cancelUpload:(NSString *)jobId;
- (void)pauseAllUploads;
- (void)resumeAllUploads;

@property (nonatomic, weak, nullable) id<ViralUploadDelegate> uploadDelegate;

// ── Chat ───────────────────────────────────

- (NSString *)sendText:(NSString *)text
        conversationId:(NSString *)convId
           recipientId:(NSString *)recipientId;

- (NSString *)sendImage:(NSString *)imageUrl
         conversationId:(NSString *)convId
            recipientId:(NSString *)recipientId;

- (NSString *)sendLike:(NSString *)convId
           recipientId:(NSString *)recipientId;

- (void)markRead:(NSString *)conversationId;
- (void)typingStart:(NSString *)conversationId;
- (void)typingStop:(NSString *)conversationId;

@property (nonatomic, weak, nullable) id<ViralChatDelegate> chatDelegate;

// ── Storage ────────────────────────────────

- (void)setSetting:(NSString *)value forKey:(NSString *)key;
- (NSString *)getSetting:(NSString *)key defaultValue:(NSString *)defaultValue;
- (NSString *)getCachedVideoPath:(NSString *)url;
- (void)registerCachedVideo:(NSString *)url localPath:(NSString *)path sizeBytes:(int64_t)size;

// ── Session ────────────────────────────────

/** Call on logout — clears all local data */
- (void)logout;

@end

NS_ASSUME_NONNULL_END
