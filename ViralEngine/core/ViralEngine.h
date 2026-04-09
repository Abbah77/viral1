/*
 * ViralEngine.h
 * 
 * Master engine header for TikTok-style short video app.
 * Sits on top of ijkplayer and manages:
 *   - Feed preloading (N+2 ahead)
 *   - Player pool (reuse instances)
 *   - Scroll-sync (pause/play on swipe)
 *   - Memory management (release old players)
 *
 * Both Android (JNI) and iOS (ObjC) bridges call this API.
 */

#ifndef VIRAL_ENGINE_H
#define VIRAL_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────
 * CONFIG
 * ───────────────────────────────────────── */

#define VIRAL_PRELOAD_COUNT     2   // preload N videos ahead
#define VIRAL_POOL_SIZE         5   // max player instances alive
#define VIRAL_CACHE_SIZE_MB     200 // local video cache size

/* ─────────────────────────────────────────
 * TYPES
 * ───────────────────────────────────────── */

typedef struct ViralPlayer   ViralPlayer;
typedef struct ViralFeed     ViralFeed;

// Playback state (mirrors ijkplayer states cleanly)
typedef enum {
    VIRAL_STATE_IDLE       = 0,
    VIRAL_STATE_PRELOADING = 1,
    VIRAL_STATE_READY      = 2,
    VIRAL_STATE_PLAYING    = 3,
    VIRAL_STATE_PAUSED     = 4,
    VIRAL_STATE_ERROR      = 5,
} ViralState;

// Callback events fired up to Kotlin/Swift
typedef enum {
    VIRAL_EVENT_READY       = 1,  // video is buffered, ready to play
    VIRAL_EVENT_STARTED     = 2,  // playback started
    VIRAL_EVENT_PAUSED      = 3,  // playback paused
    VIRAL_EVENT_COMPLETED   = 4,  // video finished
    VIRAL_EVENT_ERROR       = 5,  // playback error
    VIRAL_EVENT_BUFFERING   = 6,  // buffering in progress
} ViralEvent;

// Event callback — fired to Kotlin/Swift layer
typedef void (*ViralEventCallback)(int feed_index, ViralEvent event, void *userdata);

/* ─────────────────────────────────────────
 * ENGINE LIFECYCLE
 * ───────────────────────────────────────── */

// Call once at app start
void viral_engine_init(void);

// Call once at app shutdown
void viral_engine_uninit(void);

/* ─────────────────────────────────────────
 * FEED MANAGEMENT
 * The feed is the vertical scroll list.
 * ───────────────────────────────────────── */

// Create a new feed (one per screen/tab)
ViralFeed* viral_feed_create(ViralEventCallback callback, void *userdata);

// Destroy a feed and release all players
void viral_feed_destroy(ViralFeed *feed);

// Tell the engine the full list of video URLs
void viral_feed_set_urls(ViralFeed *feed, const char **urls, int count);

// Called when user swipes to a new index
// Engine will: play current, preload next N, release old
void viral_feed_scroll_to(ViralFeed *feed, int index);

// Get current playing index
int viral_feed_current_index(ViralFeed *feed);

/* ─────────────────────────────────────────
 * PLAYBACK CONTROL
 * ───────────────────────────────────────── */

void viral_feed_play(ViralFeed *feed);
void viral_feed_pause(ViralFeed *feed);
void viral_feed_mute(ViralFeed *feed, bool muted);
void viral_feed_set_loop(ViralFeed *feed, bool loop);
void viral_feed_seek(ViralFeed *feed, long ms);

// Get playback position of current video
long viral_feed_get_position(ViralFeed *feed);
long viral_feed_get_duration(ViralFeed *feed);

/* ─────────────────────────────────────────
 * SURFACE BINDING
 * Android: pass ANativeWindow*
 * iOS:     pass CAEAGLLayer* or MTLLayer*
 * ───────────────────────────────────────── */
void viral_feed_set_surface(ViralFeed *feed, void *surface);
void viral_feed_clear_surface(ViralFeed *feed);

/* ─────────────────────────────────────────
 * PLAYER POOL (internal, exposed for bridge)
 * ───────────────────────────────────────── */

// Get the underlying player for a specific index
// Used by bridge layer to attach native surfaces
ViralPlayer* viral_feed_get_player(ViralFeed *feed, int index);

// Get state of a specific player slot
ViralState viral_player_get_state(ViralPlayer *player);

#ifdef __cplusplus
}
#endif

#endif // VIRAL_ENGINE_H
