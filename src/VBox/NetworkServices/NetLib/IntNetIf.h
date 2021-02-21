/* $Id$ */
/** @file
 * IntNetIf - Convenience class implementing an IntNet connection.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_NetLib_IntNetIf_h
#define VBOX_INCLUDED_SRC_NetLib_IntNetIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

#include <iprt/initterm.h>
#include <iprt/cpp/ministring.h>

#include <VBox/sup.h>
#include <VBox/vmm/vmm.h>
#include <VBox/intnet.h>


/**
 * Convenience class implementing an IntNet connection.
 */
class IntNetIf
{
public:
    /**
     * User input callback function.
     *
     * @param pvUser    The user specified argument.
     * @param pvFrame   The pointer to the frame data.
     * @param cbFrame   The length of the frame data.
     */
    typedef DECLCALLBACKTYPE(void, FNINPUT,(void *pvUser, void *pvFrame, uint32_t cbFrame));

    /** Pointer to the user input callback function. */
    typedef FNINPUT *PFNINPUT;

    /**
     * User GSO input callback function.
     *
     * @param pvUser    The user specified argument.
     * @param pcGso     The pointer to the GSO context.
     * @param cbFrame   The length of the GSO data.
     */
    typedef DECLCALLBACKTYPE(void, FNINPUTGSO,(void *pvUser, PCPDMNETWORKGSO pcGso, uint32_t cbFrame));

    /** Pointer to the user GSO input callback function. */
    typedef FNINPUTGSO *PFNINPUTGSO;


    /**
     * An output frame in the send ring buffer.
     *
     * Obtained with getOutputFrame().  Caller should copy frame
     * contents to pvFrame and pass the frame structure to ifOutput()
     * to be sent to the network.
     */
    struct Frame {
        PINTNETHDR pHdr;
        void *pvFrame;
    };


private:
    PSUPDRVSESSION m_pSession;
    INTNETIFHANDLE m_hIf;
    PINTNETBUF m_pIfBuf;

    PFNINPUT m_pfnInput;
    void *m_pvUser;

    PFNINPUTGSO m_pfnInputGSO;
    void *m_pvUserGSO;

public:
    IntNetIf();
    ~IntNetIf();

    int init(const RTCString &strNetwork,
             INTNETTRUNKTYPE enmTrunkType = kIntNetTrunkType_WhateverNone,
             const RTCString &strTrunk = RTCString());
    void uninit();

    int setInputCallback(PFNINPUT pfnInput, void *pvUser);
    int setInputGSOCallback(PFNINPUTGSO pfnInputGSO, void *pvUser);

    int ifPump();
    int ifAbort();

    int getOutputFrame(Frame &rFrame, size_t cbFrame);
    int ifOutput(Frame &rFrame);

    int ifClose();

private:
    int r3Init();
    void r3Fini();

    int vmmInit();

    int ifOpen(const RTCString &strNetwork,
               INTNETTRUNKTYPE enmTrunkType,
               const RTCString &strTrunk);
    int ifGetBuf();
    int ifActivate();

    int ifWait(uint32_t cMillies = RT_INDEFINITE_WAIT);
    int ifProcessInput();

    int ifFlush();
};

#endif /* !VBOX_INCLUDED_SRC_NetLib_IntNetIf_h */
