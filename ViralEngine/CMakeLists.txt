cmake_minimum_required(VERSION 3.18)
project(viralengine VERSION 1.0.0 LANGUAGES C CXX)

# ─────────────────────────────────────────
# PATHS — adjust to your viral1/ directory
# ─────────────────────────────────────────
set(VIRAL1_ROOT      "${CMAKE_SOURCE_DIR}/../..")
set(IJKPLAYER_ROOT   "${VIRAL1_ROOT}/ijk-player")
set(IJKMEDIA_ROOT    "${IJKPLAYER_ROOT}/ijkmedia")
set(IXWEBSOCKET_ROOT "${VIRAL1_ROOT}/IXWebSocket-master")
set(CTUS_ROOT        "${VIRAL1_ROOT}/ctus-main")

# ─────────────────────────────────────────
# VIRAL ENGINE CORE
# ─────────────────────────────────────────
set(VIRAL_CORE_SOURCES
    core/ViralEngine.c
)

# ─────────────────────────────────────────
# ANDROID BUILD
# ─────────────────────────────────────────
if(ANDROID)
    add_library(viralengine SHARED
        ${VIRAL_CORE_SOURCES}
        bridge/android/ViralEngine_jni.c
    )

    target_include_directories(viralengine PRIVATE
        ${IJKMEDIA_ROOT}
        ${IJKMEDIA_ROOT}/ijkplayer
        ${IJKMEDIA_ROOT}/ijksdl
        ${IXWEBSOCKET_ROOT}
        core/
    )

    # Link against ijkplayer prebuilt .so and Android libs
    target_link_libraries(viralengine
        ijkffmpeg
        ijkplayer
        ijksdl
        android
        log
        OpenSLES
        EGL
        GLESv2
    )

    target_compile_definitions(viralengine PRIVATE __ANDROID__)
endif()

# ─────────────────────────────────────────
# iOS BUILD (via Xcode — run cmake -G Xcode)
# ─────────────────────────────────────────
if(IOS)
    add_library(viralengine STATIC
        ${VIRAL_CORE_SOURCES}
        bridge/ios/ViralEngine_iOS.m
    )

    target_include_directories(viralengine PRIVATE
        ${IJKMEDIA_ROOT}
        ${IJKMEDIA_ROOT}/ijkplayer
        ${IJKMEDIA_ROOT}/ijksdl
        core/
    )

    find_library(FOUNDATION_LIB       Foundation)
    find_library(AVFOUNDATION_LIB     AVFoundation)
    find_library(QUARTZCORE_LIB       QuartzCore)
    find_library(COREMEDIA_LIB        CoreMedia)
    find_library(COREVIDEO_LIB        CoreVideo)
    find_library(AUDIOTOOLBOX_LIB     AudioToolbox)
    find_library(VIDEOTOOLBOX_LIB     VideoToolbox)
    find_library(OPENGLES_LIB         OpenGLES)

    target_link_libraries(viralengine
        ${FOUNDATION_LIB}
        ${AVFOUNDATION_LIB}
        ${QUARTZCORE_LIB}
        ${COREMEDIA_LIB}
        ${COREVIDEO_LIB}
        ${AUDIOTOOLBOX_LIB}
        ${VIDEOTOOLBOX_LIB}
        ${OPENGLES_LIB}
    )

    set_target_properties(viralengine PROPERTIES
        FRAMEWORK TRUE
        MACOSX_FRAMEWORK_IDENTIFIER com.viral.engine
    )
endif()

# ─────────────────────────────────────────
# INSTALL
# ─────────────────────────────────────────
install(FILES core/ViralEngine.h DESTINATION include)
install(TARGETS viralengine DESTINATION lib)
