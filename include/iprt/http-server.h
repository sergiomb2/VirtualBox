/* $Id$ */
/** @file
 * Header file for HTTP server implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef IPRT_INCLUDED_http_server_h
#define IPRT_INCLUDED_http_server_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/http-common.h>
#include <iprt/types.h>
#include <iprt/fs.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_httpserver  RTHttpServer - HTTP server implementation.
 * @ingroup grp_rt
 * @{
 */

/** @todo the following three definitions may move the iprt/types.h later. */
/** HTTP server handle. */
typedef R3PTRTYPE(struct RTHTTPSERVERINTERNAL *) RTHTTPSERVER;
/** Pointer to a HTTP server handle. */
typedef RTHTTPSERVER                            *PRTHTTPSERVER;
/** Nil HTTP client handle. */
#define NIL_RTHTTPSERVER                         ((RTHTTPSERVER)0)

/**
 * Structure for maintaining a HTTP client request.
 */
typedef struct RTHTTPSERVERREQ
{
    /** Request URL. */
    char            *pszUrl;
    /** Request method. */
    RTHTTPMETHOD     enmMethod;
    /** Request header list. */
    RTHTTPHEADERLIST hHdrLst;
    /** Request body data. */
    RTHTTPBODY       Body;
} RTHTTPSERVERREQ;
/** Pointer to a HTTP client request. */
typedef RTHTTPSERVERREQ *PRTHTTPSERVERREQ;

/**
 * Structure for maintaining a HTTP server response.
 */
typedef struct RTHTTPSERVERRESP
{
    /** HTTP status to send. */
    RTHTTPSTATUS     enmSts;
    /** List of headers to send. */
    RTHTTPHEADERLIST hHdrLst;
    /** Body data to send. */
    RTHTTPBODY       Body;
} RTHTTPSERVERRESP;
/** Pointer to a HTTP server response. */
typedef RTHTTPSERVERRESP *PRTHTTPSERVERRESP;

RTR3DECL(int)  RTHttpServerResponseInitEx(PRTHTTPSERVERRESP pResp, size_t cbBody);
RTR3DECL(int)  RTHttpServerResponseInit(PRTHTTPSERVERRESP pResp);
RTR3DECL(void) RTHttpServerResponseDestroy(PRTHTTPSERVERRESP pResp);

/**
 * Structure for maintaining a HTTP server client state.
 *
 * Note: The HTTP protocol itself is stateless, but we want to have to possibility to store
 *       some state stuff here nevertheless.
 */
typedef struct RTHTTPSERVERCLIENTSTATE
{
    /** If non-zero, the time (in ms) to keep a client connection alive.
     *  Requested via client header, but set and controlled by the server in the end. */
    RTMSINTERVAL msKeepAlive;
} RTHTTPSERVERCLIENTSTATE;
/** Pointer to a FTP server client state. */
typedef RTHTTPSERVERCLIENTSTATE *PRTHTTPSERVERCLIENTSTATE;

/**
 * Structure for storing HTTP server callback data.
 */
typedef struct RTHTTPCALLBACKDATA
{
    /** Pointer to the client state. */
    PRTHTTPSERVERCLIENTSTATE pClient;
    /** Saved user pointer. */
    void                    *pvUser;
    /** Size (in bytes) of data at user pointer. */
    size_t                   cbUser;
} RTHTTPCALLBACKDATA;
/** Pointer to HTTP server callback data. */
typedef RTHTTPCALLBACKDATA *PRTHTTPCALLBACKDATA;

/**
 * Function callback table for the HTTP server implementation.
 *
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct RTHTTPSERVERCALLBACKS
{
    /**
     * Called before a given URL will be retrieved by the GET method.
     *
     * Note: High level function, not being called when pfnOnGetRequest is implemented.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     * @param   pReq            Pointer to request to handle.
     * @param   ppvHandle       Where to return the pointer to the opaque handle used for object identification.
     */
    DECLCALLBACKMEMBER(int, pfnOpen,(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq, void **ppvHandle));
    /**
     * Called when a given URL will be retrieved by the GET method.
     *
     * Note:  High level function, not being called when pfnOnGetRequest is implemented.
     * Note2: Can be called multiple times, based on the body size to send.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     * @param   pvHandle        Opaque handle for object identification.
     * @param   pvBuf           Pointer to buffer where to store the read data.
     * @param   cbBuf           Size (in bytes) of the buffer where to store the read data.
     * @param   pcbRead         Where to return the amount (in bytes) of read data. Optional and can be NULL.
     */
    DECLCALLBACKMEMBER(int, pfnRead,(PRTHTTPCALLBACKDATA pData, void *pvHandle, void *pvBuf, size_t cbBuf, size_t *pcbRead));
    /**
     * Called when a given URL is done retrieving by the GET method.
     *
     * Note: High level function, not being called when pfnOnGetRequest is implemented.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     * @param   pszUrl          URL to handle.
     * @param   pvHandle        Opaque handle for object identification.
     */
    DECLCALLBACKMEMBER(int, pfnClose,(PRTHTTPCALLBACKDATA pData, void *pvHandle));
    /**
     * Queries information about a given URL.
     *
     * Will be called with GET or HEAD request.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     * @param   pReq            Pointer to request to handle.
     * @param   pObjInfo        Where to store the queried file information on success.
     * @param   ppszMIMEHint    Where to return an allocated MIME type hint on success.
     *                          Must be free'd by the caller using RTStrFree().
     */
    DECLCALLBACKMEMBER(int, pfnQueryInfo,(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq, PRTFSOBJINFO pObjInfo, char **ppszMIMEHint));
    /**
     * Low-level handler for a GET method request.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     * @param   pReq            Pointer to request to handle.
     */
    DECLCALLBACKMEMBER(int, pfnOnGetRequest,(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq));
    /**
     * Low-level handler for a HEAD method request.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     * @param   pReq            Pointer to request to handle.
     */
    DECLCALLBACKMEMBER(int, pfnOnHeadRequest,(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq));
    /**
     * Called before the HTTP server will be destroyed.
     *
     * @returns VBox status code.
     * @param   pData           Pointer to HTTP callback data.
     */
    DECLCALLBACKMEMBER(int, pfnDestroy,(PRTHTTPCALLBACKDATA pData));
} RTHTTPSERVERCALLBACKS;
/** Pointer to a HTTP server callback data table. */
typedef RTHTTPSERVERCALLBACKS *PRTHTTPSERVERCALLBACKS;

/** Maximum length (in bytes) a single client request can have. */
#define RTHTTPSERVER_MAX_REQ_LEN        _8K
/** EOL string according to the HTTP 1.1 specs.
 *  See https://tools.ietf.org/html/rfc2616#section-2.2 */
#define RTHTTPSERVER_HTTP11_EOL_STR     "\r\n"

/**
 * Creates a HTTP server instance.
 *
 * @returns IPRT status code.
 * @param   phHttpServer        Where to store the HTTP server handle.
 * @param   pcszAddress         The address for creating a listening socket.
 *                              If NULL or empty string the server is bound to all interfaces.
 * @param   uPort               The port for creating a listening socket.
 * @param   pCallbacks          Callback table to use.
 * @param   pvUser              Pointer to user-specific data. Optional.
 * @param   cbUser              Size of user-specific data. Optional.
 */
RTR3DECL(int) RTHttpServerCreate(PRTHTTPSERVER phHttpServer, const char *pcszAddress, uint16_t uPort,
                                 PRTHTTPSERVERCALLBACKS pCallbacks, void *pvUser, size_t cbUser);

/**
 * Destroys a HTTP server instance.
 *
 * @returns IPRT status code.
 * @param   hHttpServer          Handle to the HTTP server handle.
 */
RTR3DECL(int) RTHttpServerDestroy(RTHTTPSERVER hHttpServer);

/** @} */
RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_http_server_h */

