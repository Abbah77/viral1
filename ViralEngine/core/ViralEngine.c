/*
 * ViralEngine.c
 *
 * Core implementation of the ViralEngine feed manager.
 * Wraps ijkplayer instances in a pool and manages
 * TikTok-style preloading, scroll sync, and memory.
 */

#include "ViralEngine.h"

// ijkplayer public API
#include "ijkplayer/ijkplayer.h"
#include "ijksdl/ijksdl_log.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define TAG "ViralEngine"
#define LOGI(...) ALOGD(__VA_ARGS__)

/* ─────────────────────────────────────────
 * INTERNAL STRUCTS
 * ───────────────────────────────────────── */

struct ViralPlayer {
    IjkMediaPlayer  *mp;          // underlying ijkplayer instance
    ViralState       state;
    int              feed_index;  // which slot in the feed this covers
    char            *url;         // video URL
    bool             surface_set;
};

struct ViralFeed {
    // URL list
    char           **urls;
    int              url_count;

    // Player pool — fixed size ring
    ViralPlayer      pool[VIRAL_POOL_SIZE];

    // Current position
    int              current_index;

    // Surface (ANativeWindow* on Android, layer on iOS)
    void            *surface;

    // Callback to Kotlin/Swift
    ViralEventCallback callback;
    void              *userdata;

    // Thread safety
    pthread_mutex_t  lock;
};

/* ─────────────────────────────────────────
 * INTERNAL: IJK MSG LOOP
 * Runs on a background thread per player,
 * fires events up to ViralFeed callback
 * ───────────────────────────────────────── */

typedef struct {
    ViralFeed   *feed;
    ViralPlayer *player;
} MsgLoopArg;

static int viral_msg_loop(void *arg) {
    MsgLoopArg *marg   = (MsgLoopArg *)arg;
    ViralFeed  *feed   = marg->feed;
    ViralPlayer *vp    = marg->player;
    IjkMediaPlayer *mp = vp->mp;
    free(marg);

    AVMessage msg;
    while (1) {
        int ret = ijkmp_get_msg(mp, &msg, 1);
        if (ret < 0) break;

        switch (msg.what) {
            case FFP_MSG_PREPARED:
                vp->state = VIRAL_STATE_READY;
                if (feed->callback)
                    feed->callback(vp->feed_index, VIRAL_EVENT_READY, feed->userdata);
                // If this is the current index, auto-start
                if (vp->feed_index == feed->current_index) {
                    ijkmp_start(mp);
                }
                break;

            case FFP_MSG_PLAYBACK_STATE_CHANGED:
                if (ijkmp_is_playing(mp)) {
                    vp->state = VIRAL_STATE_PLAYING;
                    if (feed->callback)
                        feed->callback(vp->feed_index, VIRAL_EVENT_STARTED, feed->userdata);
                }
                break;

            case FFP_MSG_COMPLETED:
                vp->state = VIRAL_STATE_IDLE;
                if (feed->callback)
                    feed->callback(vp->feed_index, VIRAL_EVENT_COMPLETED, feed->userdata);
                break;

            case FFP_MSG_ERROR:
                vp->state = VIRAL_STATE_ERROR;
                if (feed->callback)
                    feed->callback(vp->feed_index, VIRAL_EVENT_ERROR, feed->userdata);
                break;

            case FFP_MSG_BUFFERING_START:
                if (feed->callback)
                    feed->callback(vp->feed_index, VIRAL_EVENT_BUFFERING, feed->userdata);
                break;
        }
        msg_free_res(&msg);
    }
    return 0;
}

/* ─────────────────────────────────────────
 * INTERNAL: CREATE A PLAYER SLOT
 * ───────────────────────────────────────── */

static void viral_player_init_slot(ViralFeed *feed, ViralPlayer *vp, int feed_index) {
    // Release existing player if reusing slot
    if (vp->mp) {
        ijkmp_stop(vp->mp);
        ijkmp_shutdown(vp->mp);
        ijkmp_dec_ref_p(&vp->mp);
        vp->mp = NULL;
    }
    if (vp->url) { free(vp->url); vp->url = NULL; }

    vp->feed_index  = feed_index;
    vp->state       = VIRAL_STATE_IDLE;
    vp->surface_set = false;

    if (feed_index < 0 || feed_index >= feed->url_count) return;

    vp->url = strdup(feed->urls[feed_index]);

    // Build msg loop arg
    MsgLoopArg *marg = malloc(sizeof(MsgLoopArg));
    marg->feed   = feed;
    marg->player = vp;

    // Create ijkplayer instance
    vp->mp = ijkmp_create(viral_msg_loop);
    // Pass msg loop arg via weak_thiz hack
    ijkmp_set_weak_thiz(vp->mp, marg);

    // Options — tune for short video / TikTok feel
    ijkmp_set_option(vp->mp,     IJKMP_OPT_CATEGORY_PLAYER, "mediacodec",          "1");
    ijkmp_set_option(vp->mp,     IJKMP_OPT_CATEGORY_PLAYER, "mediacodec-auto-rotate", "1");
    ijkmp_set_option(vp->mp,     IJKMP_OPT_CATEGORY_PLAYER, "videotoolbox",        "1");
    ijkmp_set_option_int(vp->mp, IJKMP_OPT_CATEGORY_PLAYER, "start-on-prepared",   0);  // we control start
    ijkmp_set_option_int(vp->mp, IJKMP_OPT_CATEGORY_FORMAT, "analyzeduration",     100000); // fast start
    ijkmp_set_option_int(vp->mp, IJKMP_OPT_CATEGORY_FORMAT, "probesize",           10000);  // fast start
    ijkmp_set_option_int(vp->mp, IJKMP_OPT_CATEGORY_PLAYER, "max-buffer-size",     VIRAL_CACHE_SIZE_MB * 1024 * 1024);

    // Attach surface if already available and this is current
    if (feed->surface && feed_index == feed->current_index) {
        // Platform bridge will handle surface attachment
        // via viral_feed_set_surface()
    }

    // Set URL and start async prepare (preloading)
    ijkmp_set_data_source(vp->mp, vp->url);
    vp->state = VIRAL_STATE_PRELOADING;
    ijkmp_prepare_async(vp->mp);  // ← THIS is the preload magic
}

/* ─────────────────────────────────────────
 * INTERNAL: GET POOL SLOT FOR INDEX
 * Uses modulo ring buffer so pool recycles
 * ───────────────────────────────────────── */

static ViralPlayer* viral_get_pool_slot(ViralFeed *feed, int index) {
    return &feed->pool[index % VIRAL_POOL_SIZE];
}

/* ─────────────────────────────────────────
 * ENGINE LIFECYCLE
 * ───────────────────────────────────────── */

void viral_engine_init(void) {
    ijkmp_global_init();
    ijkmp_global_set_log_level(AV_LOG_WARNING);
}

void viral_engine_uninit(void) {
    ijkmp_global_uninit();
}

/* ─────────────────────────────────────────
 * FEED MANAGEMENT
 * ───────────────────────────────────────── */

ViralFeed* viral_feed_create(ViralEventCallback callback, void *userdata) {
    ViralFeed *feed = calloc(1, sizeof(ViralFeed));
    feed->callback      = callback;
    feed->userdata      = userdata;
    feed->current_index = -1;
    pthread_mutex_init(&feed->lock, NULL);
    return feed;
}

void viral_feed_destroy(ViralFeed *feed) {
    if (!feed) return;
    pthread_mutex_lock(&feed->lock);

    for (int i = 0; i < VIRAL_POOL_SIZE; i++) {
        ViralPlayer *vp = &feed->pool[i];
        if (vp->mp) {
            ijkmp_stop(vp->mp);
            ijkmp_shutdown(vp->mp);
            ijkmp_dec_ref_p(&vp->mp);
        }
        if (vp->url) free(vp->url);
    }

    if (feed->urls) {
        for (int i = 0; i < feed->url_count; i++)
            if (feed->urls[i]) free(feed->urls[i]);
        free(feed->urls);
    }

    pthread_mutex_unlock(&feed->lock);
    pthread_mutex_destroy(&feed->lock);
    free(feed);
}

void viral_feed_set_urls(ViralFeed *feed, const char **urls, int count) {
    pthread_mutex_lock(&feed->lock);

    // Free old URLs
    if (feed->urls) {
        for (int i = 0; i < feed->url_count; i++)
            if (feed->urls[i]) free(feed->urls[i]);
        free(feed->urls);
    }

    feed->urls      = calloc(count, sizeof(char*));
    feed->url_count = count;
    for (int i = 0; i < count; i++)
        feed->urls[i] = strdup(urls[i]);

    pthread_mutex_unlock(&feed->lock);
}

void viral_feed_scroll_to(ViralFeed *feed, int index) {
    if (!feed || index < 0 || index >= feed->url_count) return;

    pthread_mutex_lock(&feed->lock);

    int prev = feed->current_index;
    feed->current_index = index;

    // 1. Pause the previous video
    if (prev >= 0 && prev != index) {
        ViralPlayer *old = viral_get_pool_slot(feed, prev);
        if (old->mp && old->feed_index == prev)
            ijkmp_pause(old->mp);
    }

    // 2. Play or prepare current
    ViralPlayer *cur = viral_get_pool_slot(feed, index);
    if (cur->feed_index == index && cur->mp) {
        // Already preloaded — just start
        if (cur->state == VIRAL_STATE_READY || cur->state == VIRAL_STATE_PAUSED)
            ijkmp_start(cur->mp);
    } else {
        // Not ready yet — init and prepare
        viral_player_init_slot(feed, cur, index);
    }

    // 3. Preload next VIRAL_PRELOAD_COUNT videos ahead
    for (int i = 1; i <= VIRAL_PRELOAD_COUNT; i++) {
        int next = index + i;
        if (next >= feed->url_count) break;
        ViralPlayer *slot = viral_get_pool_slot(feed, next);
        if (slot->feed_index != next) {
            viral_player_init_slot(feed, slot, next);
        }
    }

    // 4. Release players that are too far behind (save memory)
    int release_before = index - VIRAL_PRELOAD_COUNT - 1;
    if (release_before >= 0) {
        ViralPlayer *old = viral_get_pool_slot(feed, release_before);
        if (old->feed_index == release_before && old->mp) {
            ijkmp_stop(old->mp);
            ijkmp_shutdown(old->mp);
            ijkmp_dec_ref_p(&old->mp);
            old->mp = NULL;
            old->state = VIRAL_STATE_IDLE;
            if (old->url) { free(old->url); old->url = NULL; }
        }
    }

    pthread_mutex_unlock(&feed->lock);
}

/* ─────────────────────────────────────────
 * PLAYBACK CONTROL
 * ───────────────────────────────────────── */

void viral_feed_play(ViralFeed *feed) {
    if (!feed) return;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) ijkmp_start(vp->mp);
}

void viral_feed_pause(ViralFeed *feed) {
    if (!feed) return;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) ijkmp_pause(vp->mp);
}

void viral_feed_mute(ViralFeed *feed, bool muted) {
    if (!feed) return;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) ijkmp_set_playback_volume(vp->mp, muted ? 0.0f : 1.0f);
}

void viral_feed_set_loop(ViralFeed *feed, bool loop) {
    if (!feed) return;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) ijkmp_set_loop(vp->mp, loop ? -1 : 0);
}

void viral_feed_seek(ViralFeed *feed, long ms) {
    if (!feed) return;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) ijkmp_seek_to(vp->mp, ms);
}

long viral_feed_get_position(ViralFeed *feed) {
    if (!feed) return 0;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) return ijkmp_get_current_position(vp->mp);
    return 0;
}

long viral_feed_get_duration(ViralFeed *feed) {
    if (!feed) return 0;
    ViralPlayer *vp = viral_get_pool_slot(feed, feed->current_index);
    if (vp && vp->mp) return ijkmp_get_duration(vp->mp);
    return 0;
}

int viral_feed_current_index(ViralFeed *feed) {
    return feed ? feed->current_index : -1;
}

/* ─────────────────────────────────────────
 * SURFACE BINDING
 * ───────────────────────────────────────── */

void viral_feed_set_surface(ViralFeed *feed, void *surface) {
    if (!feed) return;
    feed->surface = surface;
    // Platform bridge handles the actual surface attachment
    // (ANativeWindow_setBuffersGeometry on Android, etc.)
}

void viral_feed_clear_surface(ViralFeed *feed) {
    if (!feed) return;
    feed->surface = NULL;
}

ViralPlayer* viral_feed_get_player(ViralFeed *feed, int index) {
    if (!feed) return NULL;
    return viral_get_pool_slot(feed, index);
}

ViralState viral_player_get_state(ViralPlayer *player) {
    return player ? player->state : VIRAL_STATE_IDLE;
}
