/* $Id$ */
/** @file
 * VBox Qt GUI - UISession class declaration.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_runtime_UISession_h
#define FEQT_INCLUDED_SRC_runtime_UISession_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QCursor>
#include <QEvent>
#include <QMap>
#include <QObject>

/* GUI includes: */
#include "UIExtraDataDefs.h"
#include "UIMediumDefs.h"
#include "UIMousePointerShapeData.h"

/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CDisplay.h"
#include "CGuest.h"
#include "CKeyboard.h"
#include "CMachine.h"
#include "CMachineDebugger.h"
#include "CMouse.h"
#include "CSession.h"

/* Forward declarations: */
class QMenu;
class UIConsoleEventHandler;
class UIFrameBuffer;
class UIMachine;
class UIMachineLogic;
class UIMachineWindow;
class UIActionPool;
class CUSBDevice;
class CNetworkAdapter;
class CMediumAttachment;

/** QObject subclass implementing
  * COM related functionality for Runtime UI. */
class UISession : public QObject
{
    Q_OBJECT;

signals:

    /** @name COM events stuff.
     ** @{ */
        /** Notifies about additions state change. */
        void sigAdditionsStateChange();
        /** Notifies about additions state actually change. */
        void sigAdditionsStateActualChange();
        /** Notifies about additions state actually change. */
        void sigAudioAdapterChange();
        /** Notifies about clipboard mode change. */
        void sigClipboardModeChange(KClipboardMode enmMode);
        /** Notifies about CPU execution cap change. */
        void sigCPUExecutionCapChange();
        /** Notifies about DnD mode change. */
        void sigDnDModeChange(KDnDMode enmMode);
        /** Notifies about guest monitor change. */
        void sigGuestMonitorChange(KGuestMonitorChangedEventType enmChangeType, ulong uScreenId, QRect screenGeo);
        /** Notifies about machine change. */
        void sigMachineStateChange();
        /** Notifies about medium change. */
        void sigMediumChange(const CMediumAttachment &comMediumAttachment);
        /** Notifies about network adapter change. */
        void sigNetworkAdapterChange(const CNetworkAdapter &comNetworkAdapter);
        /** Notifies about recording change. */
        void sigRecordingChange();
        /** Notifies about shared folder change. */
        void sigSharedFolderChange();
        /** Notifies about storage device change for @a attachment, which was @a fRemoved and it was @a fSilent for guest. */
        void sigStorageDeviceChange(const CMediumAttachment &comAttachment, bool fRemoved, bool fSilent);
        /** Handles USB controller change signal. */
        void sigUSBControllerChange();
        /** Handles USB device state change signal. */
        void sigUSBDeviceStateChange(const CUSBDevice &comDevice, bool fAttached, const CVirtualBoxErrorInfo &comError);
        /** Notifies about VRDE change. */
        void sigVRDEChange();

        /** Notifies about runtime error happened. */
        void sigRuntimeError(bool fFatal, const QString &strErrorId, const QString &strMessage);

#ifdef VBOX_WS_MAC
        /** Notifies about VM window should be shown. */
        void sigShowWindows();
#endif
    /** @} */

    /** @name Keyboard stuff.
     ** @{ */
        /** Notifies about keyboard LEDs change. */
        void sigKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock);
    /** @} */

    /** @name Mouse stuff.
     ** @{ */
        /** Notifies listeners about mouse pointer shape change. */
        void sigMousePointerShapeChange(const UIMousePointerShapeData &shapeData);
        /** Notifies listeners about mouse capability change. */
        void sigMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative,
                                      bool fSupportsTouchScreen, bool fSupportsTouchPad,
                                      bool fNeedsHostCursor);
        /** Notifies listeners about cursor position change. */
        void sigCursorPositionChange(bool fContainsData, unsigned long uX, unsigned long uY);
    /** @} */

    /** @name Graphics stuff.
     ** @{ */
        /** Notifies about frame-buffer resize. */
        void sigFrameBufferResize();
    /** @} */

public:

    /** Constructs session UI passing @a pMachine to the constructor.
      * @param  pSession  Brings the pointer to the session UI being constructed.
      * @param  pMachine  Brings the machine UI reference. */
    static bool create(UISession *&pSession, UIMachine *pMachine);
    /** Destructs session UI.
      * @param  pSession  Brings the pointer to the session UI being destructed. */
    static void destroy(UISession *&pSession);

    /** @name General stuff.
     ** @{ */
        /** Performs session UI intialization. */
        bool initialize();
        /** Powers VM up. */
        bool powerUp();
    /** @} */

    /** @name COM stuff.
     ** @{ */
        /** Returns the session instance. */
        CSession &session() { return m_comSession; }
        /** Returns the session's machine instance. */
        CMachine &machine() { return m_comMachine; }
        /** Returns the session's console instance. */
        CConsole &console() { return m_comConsole; }
        /** Returns the console's display instance. */
        CDisplay &display() { return m_comDisplay; }
        /** Returns the console's guest instance. */
        CGuest &guest() { return m_comGuest; }
        /** Returns the console's mouse instance. */
        CMouse &mouse() { return m_comMouse; }
        /** Returns the console's keyboard instance. */
        CKeyboard &keyboard() { return m_comKeyboard; }
        /** Returns the console's debugger instance. */
        CMachineDebugger &debugger() { return m_comDebugger; }
    /** @} */

    /** @name General stuff.
     ** @{ */
        /** Returns the machine name. */
        QString machineName() const { return m_strMachineName; }

        /** Returns main machine-widget id. */
        WId mainMachineWindowId() const;
    /** @} */

    /** @name Machine-state stuff.
     ** @{ */
        /** Returns previous machine state. */
        KMachineState machineStatePrevious() const { return m_enmMachineStatePrevious; }
        /** Returns machine state. */
        KMachineState machineState() const { return m_enmMachineState; }

        /** Resets previous state to be the same as current one. */
        void forgetPreviousMachineState() { m_enmMachineStatePrevious = m_enmMachineState; }

        /** Returns whether VM is in one of saved states. */
        bool isSaved() const { return    machineState() == KMachineState_Saved
                                      || machineState() == KMachineState_AbortedSaved; }
        /** Returns whether VM is in one of turned off states. */
        bool isTurnedOff() const { return    machineState() == KMachineState_PoweredOff
                                          || machineState() == KMachineState_Saved
                                          || machineState() == KMachineState_Teleported
                                          || machineState() == KMachineState_Aborted
                                          || machineState() == KMachineState_AbortedSaved; }
        /** Returns whether VM is in one of paused states. */
        bool isPaused() const { return    machineState() == KMachineState_Paused
                                       || machineState() == KMachineState_TeleportingPausedVM; }
        /** Returns whether VM was in one of paused states. */
        bool wasPaused() const { return    machineStatePrevious() == KMachineState_Paused
                                        || machineStatePrevious() == KMachineState_TeleportingPausedVM; }
        /** Returns whether VM is in one of running states. */
        bool isRunning() const { return    machineState() == KMachineState_Running
                                        || machineState() == KMachineState_Teleporting
                                        || machineState() == KMachineState_LiveSnapshotting; }
        /** Returns whether VM is in one of stuck states. */
        bool isStuck() const { return machineState() == KMachineState_Stuck; }
        /** Returns whether VM is one of states where guest-screen is undrawable. */
        bool isGuestScreenUnDrawable() const { return    machineState() == KMachineState_Stopping
                                                      || machineState() == KMachineState_Saving; }

        /** Performes VM pausing. */
        bool pause() { return setPause(true); }
        /** Performes VM resuming. */
        bool unpause() { return setPause(false); }
        /** Performes VM pausing/resuming depending on @a fPause state. */
        bool setPause(bool fPause);
    /** @} */

    /** @name Guest additions stuff.
     ** @{ */
        /** Returns whether guest additions is active. */
        bool isGuestAdditionsActive() const { return (m_ulGuestAdditionsRunLevel > KAdditionsRunLevelType_None); }
        /** Returns whether guest additions supports graphics. */
        bool isGuestSupportsGraphics() const { return m_fIsGuestSupportsGraphics; }
        /** Returns whether guest additions supports seamless.
          * @note The double check below is correct, even though it is an implementation
          *       detail of the Additions which the GUI should not ideally have to know. */
        bool isGuestSupportsSeamless() const { return isGuestSupportsGraphics() && m_fIsGuestSupportsSeamless; }
        /** Returns whether GA can be upgraded. */
        bool guestAdditionsUpgradable();
    /** @} */

    /** @name Graphics stuff.
     ** @{ */
        /** Returns existing framebuffer for the screen with given @a uScreenId;
          * @returns 0 (asserts) if uScreenId attribute is out of bounds. */
        UIFrameBuffer *frameBuffer(ulong uScreenId) const;
        /** Sets framebuffer for the screen with given @a uScreenId;
          * Ignores (asserts) if screen-number attribute is out of bounds. */
        void setFrameBuffer(ulong uScreenId, UIFrameBuffer *pFrameBuffer);
        /** Returns existing frame-buffer vector. */
        const QVector<UIFrameBuffer*> &frameBuffers() const { return m_frameBufferVector; }
        /** Returns frame-buffer size for screen with index @a uScreenId. */
        QSize frameBufferSize(ulong uScreenId) const;
    /** @} */

    /** @name Status-bar stuff.
     ** @{ */
        /** Acquires device activity composing a vector of current @a states for device with @a deviceTypes specified. */
        void acquireDeviceActivity(const QVector<KDeviceType> &deviceTypes, QVector<KDeviceActivity> &states);
    /** @} */

    /** @name Close stuff.
     ** @{ */
        /** Prepares VM to be saved. */
        bool prepareToBeSaved();
        /** Returns whether VM can be shutdowned. */
        bool prepareToBeShutdowned();
    /** @} */

public slots:

    /** @name Guest additions stuff.
     ** @{ */
        /** Handles request to install guest additions image.
          * @param  strSource  Brings the source of image being installed. */
        void sltInstallGuestAdditionsFrom(const QString &strSource);
        /** Mounts DVD adhoc.
          * @param  strSource  Brings the source of image being mounted. */
        void sltMountDVDAdHoc(const QString &strSource);
    /** @} */

private slots:

    /** @name COM stuff.
     ** @{ */
        /** Detaches COM. */
        void sltDetachCOM();
    /** @} */

    /** @name Machine-state stuff.
     ** @{ */
        /** Handles event about VM @a enmState change. */
        void sltStateChange(KMachineState enmState);
    /** @} */

    /** @name Guest additions stuff.
     ** @{ */
        /** Handles event about guest additions change. */
        void sltAdditionsChange();
    /** @} */

private:

    /** Constructs session UI passing @a pMachine to the base-class.
      * @param  pMachine  Brings the machine UI reference. */
    UISession(UIMachine *pMachine);
    /** Destructs session UI. */
    virtual ~UISession() RT_OVERRIDE;

    /** @name Prepare/cleanup cascade.
     ** @{ */
        /** Prepares everything. */
        bool prepare();
        /** Prepares COM session. */
        bool prepareSession();
        /** Prepares notification-center. */
        void prepareNotificationCenter();
        /** Prepares console event-handler. */
        void prepareConsoleEventHandlers();
        /** Prepares frame-buffers. */
        void prepareFramebuffers();
        /** Prepares connections. */
        void prepareConnections();
        /** Prepares signal handling. */
        void prepareSignalHandling();

        /** Cleanups frame-buffers. */
        void cleanupFramebuffers();
        /** Cleanups console event-handler. */
        void cleanupConsoleEventHandlers();
        /** Cleanups notification-center. */
        void cleanupNotificationCenter();
        /** Cleanups COM session. */
        void cleanupSession();
    /** @} */

    /** @name General stuff.
     ** @{ */
        /** Returns the machine UI reference. */
        UIMachine *uimachine() const { return m_pMachine; }
        /** Returns the machine-logic reference. */
        UIMachineLogic *machineLogic() const;
        /** Returns main machine-window reference. */
        UIMachineWindow *activeMachineWindow() const;
        /** Returns main machine-widget reference. */
        QWidget *mainMachineWindow() const;

        /** Preprocess initialization. */
        bool preprocessInitialization();

        /** Mounts medium adhoc.
          * @param  enmDeviceType  Brings device type.
          * @param  enmMediumType  Brings medium type.
          * @param  strMediumName  Brings medium name. */
        bool mountAdHocImage(KDeviceType enmDeviceType, UIMediumDeviceType enmMediumType, const QString &strMediumName);

        /** Recaches media attached to the machine. */
        void recacheMachineMedia();
    /** @} */

    /** @name General stuff.
     ** @{ */
        /** Holds the machine UI reference. */
        UIMachine *m_pMachine;

        /** Holds the machine name. */
        QString  m_strMachineName;
    /** @} */

    /** @name COM stuff.
     ** @{ */
        /** Holds the CConsole event handler instance. */
        UIConsoleEventHandler *m_pConsoleEventhandler;

        /** Holds the session instance. */
        CSession          m_comSession;
        /** Holds the session's machine instance. */
        CMachine          m_comMachine;
        /** Holds the session's console instance. */
        CConsole          m_comConsole;
        /** Holds the console's display instance. */
        CDisplay          m_comDisplay;
        /** Holds the console's guest instance. */
        CGuest            m_comGuest;
        /** Holds the console's mouse instance. */
        CMouse            m_comMouse;
        /** Holds the console's keyboard instance. */
        CKeyboard         m_comKeyboard;
        /** Holds the console's debugger instance. */
        CMachineDebugger  m_comDebugger;
    /** @} */

    /** @name Machine-state stuff.
     ** @{ */
        /** Holds the previous machine state. */
        KMachineState  m_enmMachineStatePrevious;
        /** Holds the actual machine state. */
        KMachineState  m_enmMachineState;
    /** @} */

    /** @name Guest additions stuff.
     ** @{ */
        /** Holds the guest-additions run level. */
        ULONG  m_ulGuestAdditionsRunLevel;
        /** Holds whether guest-additions supports graphics. */
        bool   m_fIsGuestSupportsGraphics;
        /** Holds whether guest-additions supports seamless. */
        bool   m_fIsGuestSupportsSeamless;
    /** @} */

    /** @name Machine-state stuff.
     ** @{ */
        /** Holds the frame-buffer vector. */
        QVector<UIFrameBuffer*>  m_frameBufferVector;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_runtime_UISession_h */
