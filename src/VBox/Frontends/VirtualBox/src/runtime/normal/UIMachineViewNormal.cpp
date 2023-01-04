/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineViewNormal class implementation.
 */

/*
 * Copyright (C) 2010-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QScrollBar>
#include <QTimer>

/* GUI includes: */
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineViewNormal.h"
#include "UIFrameBuffer.h"
#include "UIExtraDataManager.h"
#include "UIDesktopWidgetWatchdog.h"

/* Other VBox includes: */
#include "VBox/log.h"


UIMachineViewNormal::UIMachineViewNormal(UIMachineWindow *pMachineWindow, ulong uScreenId)
    : UIMachineView(pMachineWindow, uScreenId)
    , m_fGuestAutoresizeEnabled(actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize)->isChecked())
{
}

void UIMachineViewNormal::sltAdditionsStateChanged()
{
    adjustGuestScreenSize();
}

bool UIMachineViewNormal::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched != 0 && pWatched == machineWindow())
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Recalculate maximum guest size: */
                setMaximumGuestSize();
                /* And resize guest to current window size: */
                if (m_fGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
                    QTimer::singleShot(300, this, SLOT(sltPerformGuestResize()));
                break;
            }
            default:
                break;
        }
    }

    /* For scroll-bars of the machine-view: */
    if (   pWatched == verticalScrollBar()
        || pWatched == horizontalScrollBar())
    {
        switch (pEvent->type())
        {
            /* On show/hide event: */
            case QEvent::Show:
            case QEvent::Hide:
            {
                /* Set maximum-size to size-hint: */
                setMaximumSize(sizeHint());
                break;
            }
            default:
                break;
        }
    }

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewNormal::prepareCommon()
{
    /* Base class common settings: */
    UIMachineView::prepareCommon();

    /* Setup size-policy: */
    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
    /* Set maximum-size to size-hint: */
    setMaximumSize(sizeHint());
}

void UIMachineViewNormal::prepareFilters()
{
    /* Base class filters: */
    UIMachineView::prepareFilters();

    /* Install scroll-bars event-filters: */
    verticalScrollBar()->installEventFilter(this);
    horizontalScrollBar()->installEventFilter(this);

#ifdef VBOX_WS_WIN
    /* Install menu-bar event-filter: */
    machineWindow()->menuBar()->installEventFilter(this);
#endif /* VBOX_WS_WIN */
}

void UIMachineViewNormal::prepareConsoleConnections()
{
    /* Base class connections: */
    UIMachineView::prepareConsoleConnections();

    /* Guest additions state-change updater: */
    connect(uisession(), &UISession::sigAdditionsStateActualChange, this, &UIMachineViewNormal::sltAdditionsStateChanged);
}

void UIMachineViewNormal::setGuestAutoresizeEnabled(bool fEnabled)
{
    if (m_fGuestAutoresizeEnabled != fEnabled)
    {
        m_fGuestAutoresizeEnabled = fEnabled;

        if (m_fGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
            sltPerformGuestResize();
    }
}

void UIMachineViewNormal::resendSizeHint()
{
    /* Skip if another visual representation mode requested: */
    if (uisession()->requestedVisualState() == UIVisualStateType_Seamless) // Seamless only for now.
        return;

    /* Get the last guest-screen size-hint, taking the scale factor into account. */
    const QSize sizeHint = scaledBackward(storedGuestScreenSizeHint());
    LogRel(("GUI: UIMachineViewNormal::resendSizeHint: Restoring guest size-hint for screen %d to %dx%d\n",
            (int)screenId(), sizeHint.width(), sizeHint.height()));

    /* Expand current limitations: */
    setMaximumGuestSize(sizeHint);

    /* Temporarily restrict the size to prevent a brief resize to the
     * frame-buffer dimensions when we exit full-screen.  This is only
     * applied if the frame-buffer is at full-screen dimensions and
     * until the first machine view resize. */
    m_sizeHintOverride = QSize(800, 600).expandedTo(sizeHint);

    /* Restore saved monitor information to the guest.  The guest may not respond
     * until a suitable driver or helper is enabled (or at all).  We do not notify
     * the guest (aNotify == false), because there is technically no change (same
     * hardware as before shutdown), and notifying would interfere with the Windows
     * guest driver which saves the video mode to the registry on shutdown. */
    uisession()->setScreenVisibleHostDesires(screenId(), guestScreenVisibilityStatus());
    display().SetVideoModeHint(screenId(),
                               guestScreenVisibilityStatus(),
                               false, 0, 0, sizeHint.width(), sizeHint.height(), 0, false);
}

void UIMachineViewNormal::adjustGuestScreenSize()
{
    LogRel(("GUI: UIMachineViewNormal::adjustGuestScreenSize: Adjust guest-screen size if necessary\n"));

    /* Get last monitor size set, if any: */
    BOOL fEnabled, fChangeOrigin;
    LONG iOriginX, iOriginY;
    ULONG uWidth, uHeight, uBitsPerPixel;
    display().GetVideoModeHint(screenId(), fEnabled, fChangeOrigin,
                               iOriginX, iOriginY, uWidth, uHeight, uBitsPerPixel);

    /* Acquire effective frame-buffer size otherwise: */
    if (uWidth == 0 || uHeight == 0)
    {
        uWidth = frameBuffer()->width();
        uHeight = frameBuffer()->height();
    }

    /* Compose frame-buffer size: */
    QSize frameBufferSize(uWidth, uHeight);
    /* Take the scale-factor(s) into account: */
    frameBufferSize = scaledForward(frameBufferSize);

    /* Adjust guest-screen size if the last size hint is too big for the screen: */
    const QSize maximumGuestSize = calculateMaxGuestSize();
    if (   maximumGuestSize.width() < frameBufferSize.width()
        || maximumGuestSize.height() < frameBufferSize.height())
        sltPerformGuestResize(machineWindow()->centralWidget()->size());
}

QSize UIMachineViewNormal::sizeHint() const
{
    /* Call to base-class: */
    QSize size = UIMachineView::sizeHint();

    /* If guest-screen auto-resize is not enabled
     * or the guest-additions doesn't support graphics
     * we should take scroll-bars size-hints into account: */
    if (!m_fGuestAutoresizeEnabled || !uisession()->isGuestSupportsGraphics())
    {
        if (verticalScrollBar()->isVisible())
            size += QSize(verticalScrollBar()->sizeHint().width(), 0);
        if (horizontalScrollBar()->isVisible())
            size += QSize(0, horizontalScrollBar()->sizeHint().height());
    }

    /* Return resulting size-hint finally: */
    return size;
}

QRect UIMachineViewNormal::workingArea() const
{
    return gpDesktop->availableGeometry(this);
}

QSize UIMachineViewNormal::calculateMaxGuestSize() const
{
    /* 1) The calculation below is not reliable on some (X11) platforms until we
     *    have been visible for a fraction of a second, so do the best we can
     *    otherwise.
     * 2) We also get called early before "machineWindow" has been fully
     *    initialised, at which time we can't perform the calculation. */
    if (!isVisible())
        return workingArea().size() * 0.95;
    /* The area taken up by the machine window on the desktop, including window
     * frame, title, menu bar and status bar. */
    QSize windowSize = machineWindow()->frameGeometry().size();
    /* The window shouldn't be allowed to expand beyond the working area
     * unless it already does.  In that case the guest shouldn't expand it
     * any further though. */
    QSize maximumSize = workingArea().size().expandedTo(windowSize);
    /* The current size of the machine display. */
    QSize centralWidgetSize = machineWindow()->centralWidget()->size();
    /* To work out how big the guest display can get without the window going
     * over the maximum size we calculated above, we work out how much space
     * the other parts of the window (frame, menu bar, status bar and so on)
     * take up and subtract that space from the maximum window size. The
     * central widget shouldn't be bigger than the window, but we bound it for
     * sanity (or insanity) reasons. */
    return maximumSize - (windowSize - centralWidgetSize.boundedTo(windowSize));
}
