/*
 * ViralEngine_iOS.m
 *
 * Objective-C bridge implementation for iOS.
 * Wraps the C ViralEngine and connects to ijkplayer's iOS surface API.
 */

#import "ViralEngine_iOS.h"
#include "ViralEngine.h"
#include "ijkplayer/ios/IJKFFMoviePlayerController.h"

// C callback — fires into ObjC delegate
static void ios_event_callback(int feed_index, ViralEvent event, void *userdata) {
    __weak ViralEngineiOS *engine = (__bridge ViralEngineiOS *)userdata;
    dispatch_async(dispatch_get_main_queue(), ^{
        [engine.delegate viralFeed:feed_index didReceiveEvent:(ViralEventType)event];
    });
}

@interface ViralEngineiOS ()
@property (nonatomic, assign) ViralFeed *feed;
@property (nonatomic, strong, nullable) CALayer *currentLayer;
@end

@implementation ViralEngineiOS

+ (void)initEngine {
    viral_engine_init();
}

+ (void)uninitEngine {
    viral_engine_uninit();
}

- (instancetype)initWithDelegate:(id<ViralFeedDelegate>)delegate {
    self = [super init];
    if (self) {
        _delegate = delegate;
        _feed = viral_feed_create(ios_event_callback, (__bridge void *)self);
    }
    return self;
}

- (void)destroy {
    if (_feed) {
        viral_feed_destroy(_feed);
        _feed = NULL;
    }
}

- (void)setUrls:(NSArray<NSString *> *)urls {
    const char **cUrls = malloc(urls.count * sizeof(char *));
    for (int i = 0; i < urls.count; i++) {
        cUrls[i] = [urls[i] UTF8String];
    }
    viral_feed_set_urls(_feed, cUrls, (int)urls.count);
    free(cUrls);
}

- (void)scrollTo:(NSInteger)index {
    viral_feed_scroll_to(_feed, (int)index);

    // Re-attach layer to new current player
    if (_currentLayer) {
        [self setLayer:_currentLayer];
    }
}

- (void)play  { viral_feed_play(_feed); }
- (void)pause { viral_feed_pause(_feed); }
- (void)mute:(BOOL)muted { viral_feed_mute(_feed, muted); }
- (void)setLoop:(BOOL)loop { viral_feed_set_loop(_feed, loop); }

- (void)setLayer:(CALayer *)layer {
    _currentLayer = layer;
    viral_feed_set_surface(_feed, (__bridge void *)layer);

    // Get current ijkplayer and set its view
    ViralPlayer *vp = viral_feed_get_player(_feed, viral_feed_current_index(_feed));
    if (vp) {
        // ijkplayer iOS uses IJKSDLGLView or similar — set via its view
        // This connects the decoded frames to your UIView layer
        // In your UI: create IJKSDLGLView and pass its layer here
    }
}

- (void)clearLayer {
    _currentLayer = nil;
    viral_feed_clear_surface(_feed);
}

- (NSTimeInterval)position {
    return viral_feed_get_position(_feed) / 1000.0;
}

- (NSTimeInterval)duration {
    return viral_feed_get_duration(_feed) / 1000.0;
}

- (void)dealloc {
    [self destroy];
}

@end
