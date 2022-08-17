/* $Id$ */
/** @file
 * Recording stream code header.
 */

/*
 * Copyright (C) 2012-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_RecordingStream_h
#define MAIN_INCLUDED_RecordingStream_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <map>
#include <vector>

#include <iprt/critsect.h>

#include "RecordingInternals.h"

class WebMWriter;
class RecordingContext;

/** Structure for queuing all blocks bound to a single timecode.
 *  This can happen if multiple tracks are being involved. */
struct RecordingBlocks
{
    virtual ~RecordingBlocks()
    {
        Clear();
    }

    /**
     * Resets a recording block list by removing (destroying)
     * all current elements.
     */
    void Clear()
    {
        while (!List.empty())
        {
            RecordingBlock *pBlock = List.front();
            List.pop_front();
            delete pBlock;
        }

        Assert(List.size() == 0);
    }

    /** The actual block list for this timecode. */
    RecordingBlockList List;
};

/** A block map containing all currently queued blocks.
 *  The key specifies a unique timecode, whereas the value
 *  is a list of blocks which all correlate to the same key (timecode). */
typedef std::map<uint64_t, RecordingBlocks *> RecordingBlockMap;

/**
 * Structure for holding a set of recording (data) blocks.
 */
struct RecordingBlockSet
{
    virtual ~RecordingBlockSet()
    {
        Clear();
    }

    /**
     * Resets a recording block set by removing (destroying)
     * all current elements.
     */
    void Clear(void)
    {
        RecordingBlockMap::iterator it = Map.begin();
        while (it != Map.end())
        {
            it->second->Clear();
            delete it->second;
            Map.erase(it);
            it = Map.begin();
        }

        Assert(Map.size() == 0);
    }

    /** Timestamp (in ms) when this set was last processed. */
    uint64_t         tsLastProcessedMs;
    /** All blocks related to this block set. */
    RecordingBlockMap Map;
};

/**
 * Class for managing a recording stream.
 *
 * A recording stream represents one entity to record (e.g. on screen / monitor),
 * so there is a 1:1 mapping (stream <-> monitors).
 */
class RecordingStream
{
public:

    RecordingStream(RecordingContext *pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings);

    virtual ~RecordingStream(void);

public:

    int Init(RecordingContext *pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &Settings);
    int Uninit(void);

    int Process(RecordingBlockMap &mapBlocksCommon);
    int SendAudioFrame(const void *pvData, size_t cbData, uint64_t msTimestamp);
    int SendVideoFrame(uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBPP, uint32_t uBytesPerLine,
                       uint32_t uSrcWidth, uint32_t uSrcHeight, uint8_t *puSrcData, uint64_t msTimestamp);

    const settings::RecordingScreenSettings &GetConfig(void) const;
    uint16_t GetID(void) const { return this->uScreenID; };
#ifdef VBOX_WITH_AUDIO_RECORDING
    PRECORDINGCODEC GetAudioCodec(void) { return this->pCodecAudio; };
#endif
    PRECORDINGCODEC GetVideoCodec(void) { return &this->CodecVideo; };
    bool IsLimitReached(uint64_t msTimestamp) const;
    bool IsReady(void) const;

public:

    static DECLCALLBACK(int) codecWriteDataCallback(PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, uint64_t msAbsPTS, uint32_t uFlags, void *pvUser);

protected:

    int open(const settings::RecordingScreenSettings &screenSettings);
    int close(void);

    int initInternal(RecordingContext *pCtx, uint32_t uScreen, const settings::RecordingScreenSettings &screenSettings);
    int uninitInternal(void);

    int initVideo(const settings::RecordingScreenSettings &screenSettings);
    int unitVideo(void);

    bool isLimitReachedInternal(uint64_t msTimestamp) const;
    int iterateInternal(uint64_t msTimestamp);

    int codecWriteToWebM(PRECORDINGCODEC pCodec, const void *pvData, size_t cbData, uint64_t msAbsPTS, uint32_t uFlags);

    void lock(void);
    void unlock(void);

protected:

    /**
     * Enumeration for a recording stream state.
     */
    enum RECORDINGSTREAMSTATE
    {
        /** Stream not initialized. */
        RECORDINGSTREAMSTATE_UNINITIALIZED = 0,
        /** Stream was initialized. */
        RECORDINGSTREAMSTATE_INITIALIZED   = 1,
        /** The usual 32-bit hack. */
        RECORDINGSTREAMSTATE_32BIT_HACK    = 0x7fffffff
    };

    /** Recording context this stream is associated to. */
    RecordingContext       *m_pCtx;
    /** The current state. */
    RECORDINGSTREAMSTATE    enmState;
    struct
    {
        /** File handle to use for writing. */
        RTFILE              hFile;
        /** Pointer to WebM writer instance being used. */
        WebMWriter         *pWEBM;
    } File;
    bool                fEnabled;
    /** Track number of audio stream.
     *  Set to UINT8_MAX if not being used. */
    uint8_t             uTrackAudio;
    /** Track number of video stream.
     *  Set to UINT8_MAX if not being used. */
    uint8_t             uTrackVideo;
    /** Screen ID. */
    uint16_t            uScreenID;
    /** Critical section to serialize access. */
    RTCRITSECT          CritSect;
    /** Timestamp (in ms) of when recording has been started. */
    uint64_t            tsStartMs;
#ifdef VBOX_WITH_AUDIO_RECORDING
    /** Pointer to audio codec instance data to use.
     *
     *  We multiplex audio data from the recording context to all streams,
     *  to avoid encoding the same audio data for each stream. We ASSUME that
     *  all audio data of a VM will be the same for each stream at a given
     *  point in time.
     *
     *  Might be NULL if not being used. */
    PRECORDINGCODEC     pCodecAudio;
#endif /* VBOX_WITH_AUDIO_RECORDING */
    /** Video codec instance data to use. */
    RECORDINGCODEC      CodecVideo;
    /** Screen settings to use. */
    settings::RecordingScreenSettings
                        ScreenSettings;
    /** Common set of recording (data) blocks, needed for
     *  multiplexing to all recording streams. */
    RecordingBlockSet   Blocks;
};

/** Vector of recording streams. */
typedef std::vector <RecordingStream *> RecordingStreams;

#endif /* !MAIN_INCLUDED_RecordingStream_h */

