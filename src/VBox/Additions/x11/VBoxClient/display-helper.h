/* $Id$ */
/** @file
 * Guest Additions - Definitions for Desktop Environment helpers.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_x11_VBoxClient_display_helper_h
#define GA_INCLUDED_SRC_x11_VBoxClient_display_helper_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "display-ipc.h"

/** Environment variable which contains information about currently running Desktop Environment. */
#define VBCL_HLP_ENV_XDG_CURRENT_DESKTOP    "XDG_CURRENT_DESKTOP"

/**
 * Display offsets change notification callback.
 *
 * @returns IPRT status code.
 * @param   cDisplays   Number of displays which have changed their offset.
 * @param   aDisplays   Displays offset data.
 */
typedef DECLCALLBACKTYPE(int, FNDISPLAYOFFSETCHANGE, (uint32_t cDisplays, struct VBOX_DRMIPC_VMWRECT *aDisplays));

/**
 * Desktop Environment helper definition structure.
 */
typedef struct
{
    /** A short helper name. 16 chars maximum (RTTHREAD_NAME_LEN). */
    const char *pszName;

    /**
     * Probing callback.
     *
     * Called in attempt to detect if user is currently running Desktop Environment
     * which is compatible with the helper.
     *
     * @returns IPRT status code.
     */
    DECLCALLBACKMEMBER(int, pfnProbe, (void));

    /**
     * Initialization callback.
     *
     * @returns IPRT status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit, (void));

    /**
     * Termination callback.
     *
     * @returns IPRT status code.
     */
    DECLCALLBACKMEMBER(int, pfnTerm, (void));

    /**
     * Set primary display in Desktop Environment specific way.
     *
     * @returns IPRT status code.
     * @param   idDisplay   Display ID which should be set as primary.
     */
    DECLCALLBACKMEMBER(int, pfnSetPrimaryDisplay, (uint32_t idDisplay));

    /**
     * Register notification callback for display offsets change event.
     *
     * @param   pfnCb   Notification callback.
     */
    DECLCALLBACKMEMBER(void, pfnSubscribeDisplayOffsetChangeNotification, (FNDISPLAYOFFSETCHANGE *pfnCb));

    /**
     * Unregister notification callback for display offsets change event.
     */
    DECLCALLBACKMEMBER(void, pfnUnsubscribeDisplayOffsetChangeNotification, (void));

} VBCLDISPLAYHELPER;

/**
 * Initialization callback for generic Desktop Environment helper.
 *
 * @returns IPRT status code.
 */
RTDECL(int) vbcl_hlp_generic_init(void);

/**
 * Termination callback for generic Desktop Environment helper.
 *
 * @returns IPRT status code.
 */
RTDECL(int) vbcl_hlp_generic_term(void);

/**
 * Subscribe to display offset change notifications emitted by Generic Desktop Environment helper.
 *
 * @param   pfnCb   A pointer to callback function which will be triggered when event arrives.
 */
RTDECL(void) vbcl_hlp_generic_subscribe_display_offset_changed(FNDISPLAYOFFSETCHANGE *pfnCb);

/**
 * Unsubscribe from display offset change notifications emitted by Generic Desktop Environment helper.
 */
RTDECL(void) vbcl_hlp_generic_unsubscribe_display_offset_changed(void);

/** GNOME3 helper private data. */
extern const VBCLDISPLAYHELPER g_DisplayHelperGnome3;
/** Generic helper private data. */
extern const VBCLDISPLAYHELPER g_DisplayHelperGeneric;

#endif /* !GA_INCLUDED_SRC_x11_VBoxClient_display_helper_h */
