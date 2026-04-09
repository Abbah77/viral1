/*
 * ViralEngine_iOS.h
 *
 * Objective-C bridge for iOS.
 * Swift talks to this, which calls the C ViralEngine.
 *
 * Swift usage:
 *   let engine = ViralEngineiOS()
 *   engine.setUrls(["https://cdn.example.com/video1.mp4"])
 *   engine.scrollTo(0)
 *   engine.setLayer(playerView.layer)
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ViralEventType) {
    ViralEventReady      = 1,
    ViralEventStarted    = 2,
    ViralEventPaused     = 3,
    ViralEventCompleted  = 4,
    ViralEventError      = 5,
    ViralEventBuffering  = 6,
};

@protocol ViralFeedDelegate <NSObject>
- (void)viralFeed:(NSInteger)feedIndex didReceiveEvent:(ViralEventType)event;
@end

@interface ViralEngineiOS : NSObject

@property (nonatomic, weak, nullable) id<ViralFeedDelegate> delegate;

// Lifecycle
+ (void)initEngine;
+ (void)uninitEngine;

- (instancetype)initWithDelegate:(id<ViralFeedDelegate>)delegate;
- (void)destroy;

// Feed
- (void)setUrls:(NSArray<NSString *> *)urls;
- (void)scrollTo:(NSInteger)index;

// Playback
- (void)play;
- (void)pause;
- (void)mute:(BOOL)muted;
- (void)setLoop:(BOOL)loop;

// Display — attach to your UIView's layer
- (void)setLayer:(CALayer *)layer;
- (void)clearLayer;

// Position
- (NSTimeInterval)position;
- (NSTimeInterval)duration;

@end

NS_ASSUME_NONNULL_END
