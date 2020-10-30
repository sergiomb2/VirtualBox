/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressObject class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIProgressObject_h
#define FEQT_INCLUDED_SRC_globals_UIProgressObject_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QEventLoop>
#include <QObject>
#include <QPointer>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class UIProgressEventHandler;
class CProgress;

/** QObject reimplementation allowing to effectively track the CProgress object completion
  * (w/o using CProgress::waitForCompletion() and w/o blocking the calling thread in any other way for too long).
  * @note The CProgress instance is passed as a non-const reference to the constructor
  *       (to memorize COM errors if they happen), and therefore must not be destroyed
  *       before the created UIProgressObject instance is destroyed. */
class SHARED_LIBRARY_STUFF UIProgressObject : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about wrapped CProgress change.
      * @param  iOperations   Brings the number of operations CProgress have.
      * @param  strOperation  Brings the description of the current CProgress operation.
      * @param  iOperation    Brings the index of the current CProgress operation.
      * @param  iPercent      Brings the percentage of the current CProgress operation. */
    void sigProgressChange(ulong iOperations, QString strOperation,
                           ulong iOperation, ulong iPercent);

    /** Notifies listeners about particular COM error.
      * @param strErrorInfo holds the details of the error happened. */
    void sigProgressError(QString strErrorInfo);

    /** Notifies listeners about wrapped CProgress complete. */
    void sigProgressComplete();

    /** Notifies listeners about CProgress event handling finished. */
    void sigProgressEventHandlingFinished();

public:

    /** Constructs progress-object passing @a pParent to the base-class.
      * @param  comProgress   Brings the progress reference. */
    UIProgressObject(CProgress &comProgress, QObject *pParent = 0);
    /** Destructs progress handler. */
    virtual ~UIProgressObject() /* override */;

    /** Executes the progress within local event-loop. */
    void exec();
    /** Cancels the progress within local event-loop. */
    void cancel();

private slots:

    /** Handles percentage changed event for progress with @a uProgressId to @a iPercent. */
    void sltHandleProgressPercentageChange(const QUuid &uProgressId, const int iPercent);
    /** Handles task completed event for progress with @a uProgressId. */
    void sltHandleProgressTaskComplete(const QUuid &uProgressId);

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Holds the progress reference. */
    CProgress &m_comProgress;

    /** Holds the progress event handler instance. */
    UIProgressEventHandler *m_pEventHandler;

    /** Holds the exec event-loop instance. */
    QPointer<QEventLoop>  m_pEventLoopExec;
    /** Holds the cancel event-loop instance. */
    QPointer<QEventLoop>  m_pEventLoopCancel;
};

#endif /* !FEQT_INCLUDED_SRC_globals_UIProgressObject_h */
