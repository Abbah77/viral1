# ViralEngine

TikTok-style short video engine built on top of ijkplayer (Bilibili).
Shared C core — one engine for both Android (Kotlin) and iOS (Swift).

## Architecture

```
Your Kotlin UI  ──→  ViralEngine.kt  ──→  ViralEngine_jni.c  ─┐
                                                                 ├──→  ViralEngine.c (C Core)
Your Swift UI   ──→  ViralEngineiOS  ──→  ViralEngine_iOS.m  ─┘         │
                                                                    ijkplayer API
                                                                          │
                                                                    ff_ffplay.c
                                                                    (FFmpeg engine)
```

## What The Engine Does Automatically

When you call `scrollTo(index)`:
1. **Plays** video at `index`
2. **Preloads** videos at `index+1` and `index+2` (async, background)
3. **Pauses** the previous video
4. **Releases** videos more than 2 positions behind (saves memory)

## Files

```
ViralEngine/
├── core/
│   ├── ViralEngine.h      ← Public API (C)
│   └── ViralEngine.c      ← Core implementation (C)
├── bridge/
│   ├── android/
│   │   ├── ViralEngine_jni.c   ← JNI bridge (C→Kotlin)
│   │   └── ViralEngine.kt      ← Kotlin wrapper
│   └── ios/
│       ├── ViralEngine_iOS.h   ← ObjC header
│       └── ViralEngine_iOS.m   ← ObjC bridge (C→Swift)
└── CMakeLists.txt         ← Build system (Android NDK + iOS Xcode)
```

## Android Integration

### 1. Build ijkplayer first
```bash
cd ~/viral1/ijk-player
./init-android.sh
cd android/contrib
./compile-ffmpeg.sh all
./compile-ijk.sh all
```

### 2. Add ViralEngine to your Android project
Copy the `ViralEngine/` folder into your Android project under `app/src/main/cpp/`

### 3. Use in Kotlin
```kotlin
// Application.onCreate()
ViralEngine.init()

// In your Fragment/Activity
val engine = ViralEngine()
engine.create(object : ViralEngine.FeedListener {
    override fun onEvent(feedIndex: Int, event: Int) {
        val e = ViralEngine.Event.from(event)
        // handle READY, STARTED, COMPLETED, ERROR etc.
    }
})

engine.setUrls(listOf(
    "https://cdn.example.com/video1.mp4",
    "https://cdn.example.com/video2.mp4",
    "https://cdn.example.com/video3.mp4",
))

// When user swipes — this is ALL you need to call
engine.scrollTo(newIndex)

// Attach your SurfaceView
engine.bindSurface(surfaceView.holder)
```

### 4. ViewPager2 integration
```kotlin
viewPager.registerOnPageChangeCallback(object : ViewPager2.OnPageChangeCallback() {
    override fun onPageSelected(position: Int) {
        engine.scrollTo(position)   // ← one line, engine handles everything
    }
})
```

## iOS Integration

### 1. Build ijkplayer for iOS
```bash
cd ~/viral1/ijk-player
./init-ios.sh
cd ios
./compile-ffmpeg.sh all
./compile-ijk.sh all
```

### 2. Use in Swift
```swift
// AppDelegate
ViralEngineiOS.initEngine()

// In your ViewController
let engine = ViralEngineiOS(delegate: self)
engine.setUrls(["https://cdn.example.com/video1.mp4", ...])
engine.setLayer(playerView.layer)
engine.scrollTo(0)

// UIScrollViewDelegate / UIPageViewController
func pageChanged(to index: Int) {
    engine.scrollTo(index)  // ← one line
}

// Implement delegate
extension ViewController: ViralFeedDelegate {
    func viralFeed(_ feedIndex: Int, didReceiveEvent event: ViralEventType) {
        switch event {
        case .ready:     print("Video \(feedIndex) ready")
        case .completed: engine.scrollTo(feedIndex + 1)  // auto-advance
        default: break
        }
    }
}
```

## Configuration (ViralEngine.h)

```c
#define VIRAL_PRELOAD_COUNT  2    // how many videos to preload ahead
#define VIRAL_POOL_SIZE      5    // max player instances in memory
#define VIRAL_CACHE_SIZE_MB  200  // local disk cache for videos
```

## Next Modules To Integrate

| Module | Repo | Status |
|---|---|---|
| Video Engine | ijk-player | ✅ Integrated |
| Feed Buffer | compose-reels | ✅ Logic ported |
| Upload | ctus + tus clients | 🔲 Next |
| WebSocket/Chat | IXWebSocket + tinode | 🔲 Next |
| Storage/Cache | toro + SQLite | 🔲 Next |
