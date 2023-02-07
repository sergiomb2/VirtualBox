/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachine class implementation.
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

/* Qt includes: */
#ifdef VBOX_WS_WIN
# include <QBitmap>
#endif
#ifdef VBOX_WS_MAC
# include <QMenuBar>
# include <QTimer>
#endif

/* GUI includes: */
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMachine.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#ifdef VBOX_WS_MAC
# include "UICocoaApplication.h"
# include "VBoxUtils-darwin.h"
#endif

/* COM includes: */
#include "CAudioAdapter.h"
#include "CAudioSettings.h"
#include "CConsole.h"
#include "CGraphicsAdapter.h"
#include "CHostVideoInputDevice.h"
#include "CMachine.h"
#include "CNetworkAdapter.h"
#include "CProgress.h"
#include "CRecordingSettings.h"
#include "CSession.h"
#include "CSnapshot.h"
#include "CSystemProperties.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"
#include "CVRDEServer.h"


#ifdef VBOX_WS_MAC
/**
 * MacOS X: Application Services: Core Graphics: Display reconfiguration callback.
 *
 * Notifies UIMachine about @a display configuration change.
 * Corresponding change described by Core Graphics @a flags.
 * Uses UIMachine @a pHandler to process this change.
 *
 * @note Last argument (@a pHandler) must always be valid pointer to UIMachine object.
 * @note Calls for UIMachine::sltHandleHostDisplayAboutToChange() slot if display configuration changed.
 */
void cgDisplayReconfigurationCallback(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *pHandler)
{
    /* Which flags we are handling? */
    int iHandledFlags = kCGDisplayAddFlag     /* display added */
                        | kCGDisplayRemoveFlag  /* display removed */
                        | kCGDisplaySetModeFlag /* display mode changed */;

    /* Handle 'display-add' case: */
    if (flags & kCGDisplayAddFlag)
        LogRelFlow(("GUI: UIMachine::cgDisplayReconfigurationCallback: Display added.\n"));
    /* Handle 'display-remove' case: */
    else if (flags & kCGDisplayRemoveFlag)
        LogRelFlow(("GUI: UIMachine::cgDisplayReconfigurationCallback: Display removed.\n"));
    /* Handle 'mode-set' case: */
    else if (flags & kCGDisplaySetModeFlag)
        LogRelFlow(("GUI: UIMachine::cgDisplayReconfigurationCallback: Display mode changed.\n"));

    /* Ask handler to process our callback: */
    if (flags & iHandledFlags)
        QTimer::singleShot(0, static_cast<UIMachine*>(pHandler),
                           SLOT(sltHandleHostDisplayAboutToChange()));

    Q_UNUSED(display);
}
#endif /* VBOX_WS_MAC */


/* static */
UIMachine *UIMachine::s_pInstance = 0;

/* static */
bool UIMachine::startMachine(const QUuid &uID)
{
    /* Make sure machine is not created: */
    AssertReturn(!s_pInstance, false);

    /* Restore current snapshot if requested: */
    if (uiCommon().shouldRestoreCurrentSnapshot())
    {
        /* Create temporary session: */
        CSession session = uiCommon().openSession(uID, KLockType_VM);
        if (session.isNull())
            return false;

        /* Which VM we operate on? */
        CMachine machine = session.GetMachine();
        /* Which snapshot we are restoring? */
        CSnapshot snapshot = machine.GetCurrentSnapshot();

        /* Prepare restore-snapshot progress: */
        CProgress progress = machine.RestoreSnapshot(snapshot);
        if (!machine.isOk())
            return msgCenter().cannotRestoreSnapshot(machine, snapshot.GetName(), machine.GetName());

        /* Show the snapshot-discarding progress: */
        msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_snapshot_discard_90px.png");
        if (progress.GetResultCode() != 0)
            return msgCenter().cannotRestoreSnapshot(progress, snapshot.GetName(), machine.GetName());

        /* Unlock session finally: */
        session.UnlockMachine();

        /* Clear snapshot-restoring request: */
        uiCommon().setShouldRestoreCurrentSnapshot(false);
    }

    /* For separate process we should launch VM before UI: */
    if (uiCommon().isSeparateProcess())
    {
        /* Get corresponding machine: */
        CMachine machine = uiCommon().virtualBox().FindMachine(uiCommon().managedVMUuid().toString());
        AssertMsgReturn(!machine.isNull(), ("UICommon::managedVMUuid() should have filter that case before!\n"), false);

        /* Try to launch corresponding machine: */
        if (!UICommon::launchMachine(machine, UILaunchMode_Separate))
            return false;
    }

    /* Try to create machine UI: */
    return create();
}

/* static */
bool UIMachine::create()
{
    /* Make sure machine is not created: */
    AssertReturn(!s_pInstance, false);

    /* Create machine UI: */
    new UIMachine;
    /* Make sure it's prepared: */
    if (!s_pInstance->prepare())
    {
        /* Destroy machine UI otherwise: */
        destroy();
        /* False in that case: */
        return false;
    }
    /* True by default: */
    return true;
}

/* static */
void UIMachine::destroy()
{
    /* Make sure machine is created: */
    if (!s_pInstance)
        return;

    /* Protect versus recursive call: */
    UIMachine *pInstance = s_pInstance;
    s_pInstance = 0;
    /* Cleanup machine UI: */
    pInstance->cleanup();
    /* Destroy machine UI: */
    delete pInstance;
}

QWidget* UIMachine::activeWindow() const
{
    return   machineLogic() && machineLogic()->activeMachineWindow()
           ? machineLogic()->activeMachineWindow()
           : 0;
}

void UIMachine::asyncChangeVisualState(UIVisualStateType visualState)
{
    emit sigRequestAsyncVisualStateChange(visualState);
}

void UIMachine::setRequestedVisualState(UIVisualStateType visualStateType)
{
    /* Remember requested visual state: */
    m_enmRequestedVisualState = visualStateType;

    /* Save only if it's different from Invalid and from current one: */
    if (   m_enmRequestedVisualState != UIVisualStateType_Invalid
        && gEDataManager->requestedVisualState(uiCommon().managedVMUuid()) != m_enmRequestedVisualState)
        gEDataManager->setRequestedVisualState(m_enmRequestedVisualState, uiCommon().managedVMUuid());
}

UIVisualStateType UIMachine::requestedVisualState() const
{
    return m_enmRequestedVisualState;
}

QString UIMachine::machineName() const
{
    return uisession()->machineName();
}

void UIMachine::updateStateAdditionsActions()
{
    /* Make sure action-pool knows whether GA supports graphics: */
    actionPool()->toRuntime()->setGuestSupportsGraphics(isGuestSupportsGraphics());
    /* Enable/Disable Upgrade Additions action depending on feature status: */
    actionPool()->action(UIActionIndexRT_M_Devices_S_UpgradeGuestAdditions)->setEnabled(uisession()->guestAdditionsUpgradable());
}

void UIMachine::updateStateAudioActions()
{
    /* Make sure Audio adapter is present: */
    const CAudioSettings comAudioSettings = uisession()->machine().GetAudioSettings();
    AssertMsgReturnVoid(uisession()->machine().isOk() && comAudioSettings.isNotNull(),
                        ("Audio audio settings should NOT be null!\n"));
    const CAudioAdapter comAdapter = comAudioSettings.GetAdapter();
    AssertMsgReturnVoid(comAudioSettings.isOk() && comAdapter.isNotNull(),
                        ("Audio audio adapter should NOT be null!\n"));

    /* Check/Uncheck Audio adapter output/input actions depending on features status: */
    actionPool()->action(UIActionIndexRT_M_Devices_M_Audio_T_Output)->blockSignals(true);
    actionPool()->action(UIActionIndexRT_M_Devices_M_Audio_T_Output)->setChecked(comAdapter.GetEnabledOut());
    actionPool()->action(UIActionIndexRT_M_Devices_M_Audio_T_Output)->blockSignals(false);
    actionPool()->action(UIActionIndexRT_M_Devices_M_Audio_T_Input)->blockSignals(true);
    actionPool()->action(UIActionIndexRT_M_Devices_M_Audio_T_Input)->setChecked(comAdapter.GetEnabledIn());
    actionPool()->action(UIActionIndexRT_M_Devices_M_Audio_T_Input)->blockSignals(false);
}

void UIMachine::updateStateRecordingAction()
{
    /* Make sure recording settingss present: */
    const CRecordingSettings comRecordingSettings = uisession()->machine().GetRecordingSettings();
    AssertMsgReturnVoid(uisession()->machine().isOk() && comRecordingSettings.isNotNull(),
                        ("Recording settings can't be null!\n"));

    /* Check/Uncheck Capture action depending on feature status: */
    actionPool()->action(UIActionIndexRT_M_View_M_Recording_T_Start)->blockSignals(true);
    actionPool()->action(UIActionIndexRT_M_View_M_Recording_T_Start)->setChecked(comRecordingSettings.GetEnabled());
    actionPool()->action(UIActionIndexRT_M_View_M_Recording_T_Start)->blockSignals(false);
}

void UIMachine::updateStateVRDEServerAction()
{
    /* Make sure VRDE server present: */
    const CVRDEServer comServer = uisession()->machine().GetVRDEServer();
    AssertMsgReturnVoid(uisession()->machine().isOk() && comServer.isNotNull(),
                        ("VRDE server can't be null!\n"));

    /* Check/Uncheck VRDE Server action depending on feature status: */
    actionPool()->action(UIActionIndexRT_M_View_T_VRDEServer)->blockSignals(true);
    actionPool()->action(UIActionIndexRT_M_View_T_VRDEServer)->setChecked(comServer.GetEnabled());
    actionPool()->action(UIActionIndexRT_M_View_T_VRDEServer)->blockSignals(false);
}

KMachineState UIMachine::machineState() const
{
    return uisession()->machineState();
}

void UIMachine::forgetPreviousMachineState()
{
    uisession()->forgetPreviousMachineState();
}

bool UIMachine::isTurnedOff() const
{
    return uisession()->isTurnedOff();
}

bool UIMachine::isPaused() const
{
    return uisession()->isPaused();
}

bool UIMachine::wasPaused() const
{
    return uisession()->wasPaused();
}

bool UIMachine::isRunning() const
{
    return uisession()->isRunning();
}

bool UIMachine::isStuck() const
{
    return uisession()->isStuck();
}

bool UIMachine::isGuestScreenUnDrawable() const
{
    return uisession()->isGuestScreenUnDrawable();
}

bool UIMachine::pause()
{
    return uisession()->pause();
}

bool UIMachine::unpause()
{
    return uisession()->unpause();
}

bool UIMachine::setPause(bool fPause)
{
    return uisession()->setPause(fPause);
}

bool UIMachine::isScreenVisibleHostDesires(ulong uScreenId) const
{
    /* Make sure index feats the bounds: */
    AssertReturn(uScreenId < (ulong)m_monitorVisibilityVectorHostDesires.size(), false);

    /* Return 'actual' (host-desire) visibility status: */
    return m_monitorVisibilityVectorHostDesires.value((int)uScreenId);
}

void UIMachine::setScreenVisibleHostDesires(ulong uScreenId, bool fIsMonitorVisible)
{
    /* Make sure index feats the bounds: */
    AssertReturnVoid(uScreenId < (ulong)m_monitorVisibilityVectorHostDesires.size());

    /* Remember 'actual' (host-desire) visibility status: */
    m_monitorVisibilityVectorHostDesires[(int)uScreenId] = fIsMonitorVisible;

    /* And remember the request in extra data for guests with VMSVGA: */
    /* This should be done before the actual hint is sent in case the guest overrides it. */
    gEDataManager->setLastGuestScreenVisibilityStatus(uScreenId, fIsMonitorVisible, uiCommon().managedVMUuid());
}

bool UIMachine::isScreenVisible(ulong uScreenId) const
{
    /* Make sure index feats the bounds: */
    AssertReturn(uScreenId < (ulong)m_monitorVisibilityVector.size(), false);

    /* Return 'actual' visibility status: */
    return m_monitorVisibilityVector.value((int)uScreenId);
}

void UIMachine::setScreenVisible(ulong uScreenId, bool fIsMonitorVisible)
{
    /* Make sure index feats the bounds: */
    AssertReturnVoid(uScreenId < (ulong)m_monitorVisibilityVector.size());

    /* Remember 'actual' visibility status: */
    m_monitorVisibilityVector[(int)uScreenId] = fIsMonitorVisible;
    /* Remember 'desired' visibility status: */
    // See note in UIMachineView::sltHandleNotifyChange() regarding the graphics controller check. */
    if (uisession()->machine().GetGraphicsAdapter().GetGraphicsControllerType() != KGraphicsControllerType_VMSVGA)
        gEDataManager->setLastGuestScreenVisibilityStatus(uScreenId, fIsMonitorVisible, uiCommon().managedVMUuid());

    /* Make sure action-pool knows guest-screen visibility status: */
    actionPool()->toRuntime()->setGuestScreenVisible(uScreenId, fIsMonitorVisible);
}

int UIMachine::countOfVisibleWindows()
{
    int cCountOfVisibleWindows = 0;
    for (int i = 0; i < m_monitorVisibilityVector.size(); ++i)
        if (m_monitorVisibilityVector[i])
            ++cCountOfVisibleWindows;
    return cCountOfVisibleWindows;
}

QList<int> UIMachine::listOfVisibleWindows() const
{
    QList<int> visibleWindows;
    for (int i = 0; i < m_monitorVisibilityVector.size(); ++i)
        if (m_monitorVisibilityVector.at(i))
            visibleWindows.push_back(i);
    return visibleWindows;
}

QSize UIMachine::guestScreenSize(ulong uScreenId) const
{
    return uisession()->frameBufferSize(uScreenId);
}

QSize UIMachine::lastFullScreenSize(ulong uScreenId) const
{
    /* Make sure index fits the bounds: */
    AssertReturn(uScreenId < (ulong)m_monitorLastFullScreenSizeVector.size(), QSize(-1, -1));

    /* Return last full-screen size: */
    return m_monitorLastFullScreenSizeVector.value((int)uScreenId);
}

void UIMachine::setLastFullScreenSize(ulong uScreenId, QSize size)
{
    /* Make sure index fits the bounds: */
    AssertReturnVoid(uScreenId < (ulong)m_monitorLastFullScreenSizeVector.size());

    /* Remember last full-screen size: */
    m_monitorLastFullScreenSizeVector[(int)uScreenId] = size;
}

bool UIMachine::isGuestAdditionsActive() const
{
    return uisession()->isGuestAdditionsActive();
}

bool UIMachine::isGuestSupportsGraphics() const
{
    return uisession()->isGuestSupportsGraphics();
}

bool UIMachine::isGuestSupportsSeamless() const
{
    return uisession()->isGuestSupportsSeamless();
}

void UIMachine::acquireDeviceActivity(const QVector<KDeviceType> &deviceTypes, QVector<KDeviceActivity> &states)
{
    uisession()->acquireDeviceActivity(deviceTypes, states);
}

void UIMachine::acquireHardDiskStatusInfo(QString &strInfo, bool &fAttachmentsPresent)
{
    uisession()->acquireHardDiskStatusInfo(strInfo, fAttachmentsPresent);
}

void UIMachine::acquireOpticalDiskStatusInfo(QString &strInfo, bool &fAttachmentsPresent, bool &fAttachmentsMounted)
{
    uisession()->acquireOpticalDiskStatusInfo(strInfo, fAttachmentsPresent, fAttachmentsMounted);
}

void UIMachine::acquireFloppyDiskStatusInfo(QString &strInfo, bool &fAttachmentsPresent, bool &fAttachmentsMounted)
{
    uisession()->acquireFloppyDiskStatusInfo(strInfo, fAttachmentsPresent, fAttachmentsMounted);
}

void UIMachine::acquireAudioStatusInfo(QString &strInfo, bool &fAudioEnabled, bool &fEnabledOutput, bool &fEnabledInput)
{
    uisession()->acquireAudioStatusInfo(strInfo, fAudioEnabled, fEnabledOutput, fEnabledInput);
}

void UIMachine::acquireDisplayStatusInfo(QString &strInfo, bool &fAcceleration3D)
{
    uisession()->acquireDisplayStatusInfo(strInfo, fAcceleration3D);
}

void UIMachine::detachUi()
{
    /* Manually close Runtime UI: */
    LogRel(("GUI: Detaching UI..\n"));
    closeRuntimeUI();
}

void UIMachine::saveState()
{
    /* Prepare VM to be saved: */
    if (!uisession()->prepareToBeSaved())
        return;

    /* Enable 'manual-override',
     * preventing automatic Runtime UI closing: */
    setManualOverrideMode(true);

    /* Now, do the magic: */
    LogRel(("GUI: Saving VM state..\n"));
    UINotificationProgressMachineSaveState *pNotification =
        new UINotificationProgressMachineSaveState(uisession()->machine());
    connect(pNotification, &UINotificationProgressMachineSaveState::sigMachineStateSaved,
            this, &UIMachine::sltHandleMachineStateSaved);
    gpNotificationCenter->append(pNotification);
}

void UIMachine::shutdown()
{
    /* Prepare VM to be shutdowned: */
    if (!uisession()->prepareToBeShutdowned())
        return;

    /* Now, do the magic: */
    LogRel(("GUI: Sending ACPI shutdown signal..\n"));
    CConsole comConsole = uisession()->console();
    comConsole.PowerButton();
    if (!comConsole.isOk())
        UINotificationMessage::cannotACPIShutdownMachine(uisession()->console());
}

void UIMachine::powerOff(bool fIncludingDiscard)
{
    /* Enable 'manual-override',
     * preventing automatic Runtime UI closing: */
    setManualOverrideMode(true);

    /* Now, do the magic: */
    LogRel(("GUI: Powering VM off..\n"));
    UINotificationProgressMachinePowerOff *pNotification =
        new UINotificationProgressMachinePowerOff(uisession()->machine(),
                                                  uisession()->console(),
                                                  fIncludingDiscard);
    connect(pNotification, &UINotificationProgressMachinePowerOff::sigMachinePoweredOff,
            this, &UIMachine::sltHandleMachinePoweredOff);
    gpNotificationCenter->append(pNotification);
}

void UIMachine::closeRuntimeUI()
{
    /* First, we have to hide any opened modal/popup widgets.
     * They then should unlock their event-loops asynchronously.
     * If all such loops are unlocked, we can close Runtime UI. */
    QWidget *pWidget = QApplication::activeModalWidget()
                     ? QApplication::activeModalWidget()
                     : QApplication::activePopupWidget()
                     ? QApplication::activePopupWidget()
                     : 0;
    if (pWidget)
    {
        /* First we should try to close this widget: */
        pWidget->close();
        /* If widget rejected the 'close-event' we can
         * still hide it and hope it will behave correctly
         * and unlock his event-loop if any: */
        if (!pWidget->isHidden())
            pWidget->hide();
        /* Asynchronously restart this slot: */
        QMetaObject::invokeMethod(this, "closeRuntimeUI", Qt::QueuedConnection);
        return;
    }

    /* Asynchronously ask QApplication to quit: */
    LogRel(("GUI: Request for async QApp quit.\n"));
    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
}

void UIMachine::sltChangeVisualState(UIVisualStateType visualState)
{
    /* Create new machine-logic: */
    UIMachineLogic *pMachineLogic = UIMachineLogic::create(this, visualState);

    /* First we have to check if the selected machine-logic is available at all.
     * Only then we delete the old machine-logic and switch to the new one. */
    if (pMachineLogic->checkAvailability())
    {
        /* Delete previous machine-logic if exists: */
        if (m_pMachineLogic)
        {
            m_pMachineLogic->cleanup();
            UIMachineLogic::destroy(m_pMachineLogic);
            m_pMachineLogic = 0;
        }

        /* Set the new machine-logic as current one: */
        m_pMachineLogic = pMachineLogic;
        m_pMachineLogic->prepare();

        /* Remember new visual state: */
        m_visualState = visualState;

        /* Save requested visual state: */
        gEDataManager->setRequestedVisualState(m_visualState, uiCommon().managedVMUuid());
    }
    else
    {
        /* Delete temporary created machine-logic: */
        pMachineLogic->cleanup();
        UIMachineLogic::destroy(pMachineLogic);
    }

    /* Make sure machine-logic exists: */
    if (!m_pMachineLogic)
    {
        /* Reset initial visual state  to normal: */
        m_initialVisualState = UIVisualStateType_Normal;
        /* Enter initial visual state again: */
        enterInitialVisualState();
    }
}

void UIMachine::sltHandleAdditionsActualChange()
{
    updateStateAdditionsActions();
    emit sigAdditionsStateChange();
}

void UIMachine::sltHandleAudioAdapterChange()
{
    updateStateAudioActions();
    emit sigAudioAdapterChange();
}

void UIMachine::sltHandleRecordingChange()
{
    updateStateRecordingAction();
    emit sigRecordingChange();
}

void UIMachine::sltHandleStorageDeviceChange(const CMediumAttachment &comAttachment, bool fRemoved, bool fSilent)
{
    updateActionRestrictions();
    emit sigStorageDeviceChange(comAttachment, fRemoved, fSilent);
}

void UIMachine::sltHandleVRDEChange()
{
    updateStateVRDEServerAction();
    emit sigVRDEChange();
}

#ifdef VBOX_WS_MAC
void UIMachine::sltHandleMenuBarConfigurationChange(const QUuid &uMachineID)
{
    /* Skip unrelated machine IDs: */
    if (uiCommon().managedVMUuid() != uMachineID)
        return;

    /* Update Mac OS X menu-bar: */
    updateMenu();
}
#endif /* VBOX_WS_MAC */

void UIMachine::sltHandleHostScreenCountChange()
{
    LogRelFlow(("GUI: UIMachine: Host-screen count changed.\n"));

    /* Recache display data: */
    updateHostScreenData();

    /* Notify current machine-logic: */
    emit sigHostScreenCountChange();
}

void UIMachine::sltHandleHostScreenGeometryChange()
{
    LogRelFlow(("GUI: UIMachine: Host-screen geometry changed.\n"));

    /* Recache display data: */
    updateHostScreenData();

    /* Notify current machine-logic: */
    emit sigHostScreenGeometryChange();
}

void UIMachine::sltHandleHostScreenAvailableAreaChange()
{
    LogRelFlow(("GUI: UIMachine: Host-screen available-area changed.\n"));

    /* Notify current machine-logic: */
    emit sigHostScreenAvailableAreaChange();
}

#ifdef VBOX_WS_MAC
void UIMachine::sltHandleHostDisplayAboutToChange()
{
    LogRelFlow(("GUI: UIMachine::sltHandleHostDisplayAboutToChange()\n"));

    if (m_pWatchdogDisplayChange->isActive())
        m_pWatchdogDisplayChange->stop();
    m_pWatchdogDisplayChange->setProperty("tryNumber", 1);
    m_pWatchdogDisplayChange->start();
}

void UIMachine::sltCheckIfHostDisplayChanged()
{
    LogRelFlow(("GUI: UIMachine::sltCheckIfHostDisplayChanged()\n"));

    /* Check if display count changed: */
    if (UIDesktopWidgetWatchdog::screenCount() != m_hostScreens.size())
    {
        /* Reset watchdog: */
        m_pWatchdogDisplayChange->setProperty("tryNumber", 0);
        /* Notify listeners about screen-count changed: */
        return sltHandleHostScreenCountChange();
    }
    else
    {
        /* Check if at least one display geometry changed: */
        for (int iScreenIndex = 0; iScreenIndex < UIDesktopWidgetWatchdog::screenCount(); ++iScreenIndex)
        {
            if (gpDesktop->screenGeometry(iScreenIndex) != m_hostScreens.at(iScreenIndex))
            {
                /* Reset watchdog: */
                m_pWatchdogDisplayChange->setProperty("tryNumber", 0);
                /* Notify listeners about screen-geometry changed: */
                return sltHandleHostScreenGeometryChange();
            }
        }
    }

    /* Check if watchdog expired, restart if not: */
    int cTryNumber = m_pWatchdogDisplayChange->property("tryNumber").toInt();
    if (cTryNumber > 0 && cTryNumber < 40)
    {
        /* Restart watchdog again: */
        m_pWatchdogDisplayChange->setProperty("tryNumber", ++cTryNumber);
        m_pWatchdogDisplayChange->start();
    }
    else
    {
        /* Reset watchdog: */
        m_pWatchdogDisplayChange->setProperty("tryNumber", 0);
    }
}
#endif /* VBOX_WS_MAC */

void UIMachine::sltHandleGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo)
{
    /* Ignore KGuestMonitorChangedEventType_NewOrigin change event: */
    if (changeType == KGuestMonitorChangedEventType_NewOrigin)
        return;
    /* Ignore KGuestMonitorChangedEventType_Disabled event for primary screen: */
    AssertMsg(countOfVisibleWindows() > 0, ("All machine windows are hidden!"));
    if (changeType == KGuestMonitorChangedEventType_Disabled && uScreenId == 0)
        return;

    /* Process KGuestMonitorChangedEventType_Enabled change event: */
    if (   !isScreenVisible(uScreenId)
        && changeType == KGuestMonitorChangedEventType_Enabled)
        setScreenVisible(uScreenId, true);
    /* Process KGuestMonitorChangedEventType_Disabled change event: */
    else if (   isScreenVisible(uScreenId)
             && changeType == KGuestMonitorChangedEventType_Disabled)
        setScreenVisible(uScreenId, false);

    /* Notify listeners about the change: */
    emit sigGuestMonitorChange(changeType, uScreenId, screenGeo);
}

void UIMachine::sltHandleKeyboardLedsChange(bool fNumLock, bool fCapsLock, bool fScrollLock)
{
    /* Check if something had changed: */
    if (   m_fNumLock != fNumLock
        || m_fCapsLock != fCapsLock
        || m_fScrollLock != fScrollLock)
    {
        /* Store new num lock data: */
        if (m_fNumLock != fNumLock)
        {
            m_fNumLock = fNumLock;
            m_uNumLockAdaptionCnt = 2;
        }

        /* Store new caps lock data: */
        if (m_fCapsLock != fCapsLock)
        {
            m_fCapsLock = fCapsLock;
            m_uCapsLockAdaptionCnt = 2;
        }

        /* Store new scroll lock data: */
        if (m_fScrollLock != fScrollLock)
        {
            m_fScrollLock = fScrollLock;
        }

        /* Notify listeners: */
        emit sigKeyboardLedsChange();
    }
}

void UIMachine::sltMousePointerShapeChange(const UIMousePointerShapeData &shapeData)
{
    LogRelFlow(("GUI: UIMachine::sltMousePointerShapeChange: "
                "Is visible: %s, Has alpha: %s, "
                "Hot spot: %dx%d, Shape size: %dx%d, "
                "Shape data: %s\n",
                shapeData.isVisible() ? "TRUE" : "FALSE",
                shapeData.hasAlpha() ? "TRUE" : "FALSE",
                shapeData.hotSpot().x(), shapeData.hotSpot().y(),
                shapeData.shapeSize().width(), shapeData.shapeSize().height(),
                shapeData.shape().isEmpty() ? "EMPTY" : "PRESENT"));

    /* In case if shape itself is present: */
    if (shapeData.shape().size() > 0)
    {
        /* We are ignoring visibility flag: */
        m_fIsHidingHostPointer = false;

        /* And updating current shape data: */
        m_shapeData = shapeData;
        updateMousePointerShape();
    }
    /* In case if shape itself is NOT present: */
    else
    {
        /* Remember if we should hide the cursor: */
        m_fIsHidingHostPointer = !shapeData.isVisible();
    }

    /* Notify listeners: */
    emit sigMousePointerShapeChange();
}

void UIMachine::sltMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative,
                                         bool fSupportsTouchScreen, bool fSupportsTouchPad,
                                         bool fNeedsHostCursor)
{
    LogRelFlow(("GUI: UIMachine::sltMouseCapabilityChange: "
                "Supports absolute: %s, Supports relative: %s, "
                "Supports touchscreen: %s, Supports touchpad: %s, "
                "Needs host cursor: %s\n",
                fSupportsAbsolute ? "TRUE" : "FALSE", fSupportsRelative ? "TRUE" : "FALSE",
                fSupportsTouchScreen ? "TRUE" : "FALSE", fSupportsTouchPad ? "TRUE" : "FALSE",
                fNeedsHostCursor ? "TRUE" : "FALSE"));

    /* Check if something had changed: */
    if (   m_fIsMouseSupportsAbsolute != fSupportsAbsolute
        || m_fIsMouseSupportsRelative != fSupportsRelative
        || m_fIsMouseSupportsTouchScreen != fSupportsTouchScreen
        || m_fIsMouseSupportsTouchPad != fSupportsTouchPad
        || m_fIsMouseHostCursorNeeded != fNeedsHostCursor)
    {
        /* Store new data: */
        m_fIsMouseSupportsAbsolute = fSupportsAbsolute;
        m_fIsMouseSupportsRelative = fSupportsRelative;
        m_fIsMouseSupportsTouchScreen = fSupportsTouchScreen;
        m_fIsMouseSupportsTouchPad = fSupportsTouchPad;
        m_fIsMouseHostCursorNeeded = fNeedsHostCursor;

        /* Notify listeners: */
        emit sigMouseCapabilityChange();
    }
}

void UIMachine::sltCursorPositionChange(bool fContainsData, unsigned long uX, unsigned long uY)
{
    LogRelFlow(("GUI: UIMachine::sltCursorPositionChange: "
                "Cursor position valid: %d, Cursor position: %dx%d\n",
                fContainsData ? "TRUE" : "FALSE", uX, uY));

    /* Check if something had changed: */
    if (   m_fIsValidCursorPositionPresent != fContainsData
        || m_cursorPosition.x() != (int)uX
        || m_cursorPosition.y() != (int)uY)
    {
        /* Store new data: */
        m_fIsValidCursorPositionPresent = fContainsData;
        m_cursorPosition = QPoint(uX, uY);

        /* Notify listeners: */
        emit sigCursorPositionChange();
    }
}

void UIMachine::sltHandleMachineStateSaved(bool fSuccess)
{
    /* Disable 'manual-override' finally: */
    setManualOverrideMode(false);

    /* Close Runtime UI if state was saved: */
    if (fSuccess)
        closeRuntimeUI();
}

void UIMachine::sltHandleMachinePoweredOff(bool fSuccess, bool fIncludingDiscard)
{
    /* Disable 'manual-override' finally: */
    setManualOverrideMode(false);

    /* Do we have other tasks? */
    if (fSuccess)
    {
        if (fIncludingDiscard)
        {
            /* Now, do more magic! */
            UINotificationProgressSnapshotRestore *pNotification =
                new UINotificationProgressSnapshotRestore(uiCommon().managedVMUuid());
            connect(pNotification, &UINotificationProgressSnapshotRestore::sigSnapshotRestored,
                    this, &UIMachine::sltHandleSnapshotRestored);
            gpNotificationCenter->append(pNotification);
        }
        else
            closeRuntimeUI();
    }
}

void UIMachine::sltHandleSnapshotRestored(bool)
{
    /* Close Runtime UI independent of snapshot restoring state: */
    closeRuntimeUI();
}

UIMachine::UIMachine()
    : QObject(0)
    , m_fInitialized(false)
    , m_pSession(0)
    , m_allowedVisualStates(UIVisualStateType_Invalid)
    , m_initialVisualState(UIVisualStateType_Normal)
    , m_visualState(UIVisualStateType_Invalid)
    , m_enmRequestedVisualState(UIVisualStateType_Invalid)
    , m_pMachineLogic(0)
    , m_pMachineWindowIcon(0)
    , m_pActionPool(0)
#ifdef VBOX_WS_MAC
    , m_pMenuBar(0)
#endif
#ifdef VBOX_WS_MAC
    , m_pWatchdogDisplayChange(0)
#endif
    , m_fIsGuestResizeIgnored(false)
    , m_fNumLock(false)
    , m_fCapsLock(false)
    , m_fScrollLock(false)
    , m_uNumLockAdaptionCnt(2)
    , m_uCapsLockAdaptionCnt(2)
    , m_fIsHidLedsSyncEnabled(false)
    , m_fIsAutoCaptureDisabled(false)
    , m_iKeyboardState(0)
    , m_fIsHidingHostPointer(true)
    , m_fIsValidPointerShapePresent(false)
    , m_fIsValidCursorPositionPresent(false)
    , m_fIsMouseSupportsAbsolute(false)
    , m_fIsMouseSupportsRelative(false)
    , m_fIsMouseSupportsTouchScreen(false)
    , m_fIsMouseSupportsTouchPad(false)
    , m_fIsMouseHostCursorNeeded(false)
    , m_fIsMouseCaptured(false)
    , m_fIsMouseIntegrated(true)
    , m_iMouseState(0)
    , m_enmVMExecutionEngine(KVMExecutionEngine_NotSet)
    , m_fIsHWVirtExNestedPagingEnabled(false)
    , m_fIsHWVirtExUXEnabled(false)
    , m_enmParavirtProvider(KParavirtProvider_None)
    , m_fIsManualOverride(false)
    , m_defaultCloseAction(MachineCloseAction_Invalid)
    , m_restrictedCloseActions(MachineCloseAction_Invalid)
{
    s_pInstance = this;
}

UIMachine::~UIMachine()
{
    s_pInstance = 0;
}

bool UIMachine::prepare()
{
    /* Try to create session UI: */
    if (!UISession::create(m_pSession, this))
        return false;
    AssertPtrReturn(uisession(), false);

    /* Prepare stuff: */
    prepareBranding();
    prepareSessionConnections();
    prepareActions();
    prepareScreens();
    prepareKeyboard();
    prepareClose();
    prepareMachineLogic();

    /* Try to initialize session UI: */
    if (!uisession()->initialize())
        return false;

    /* Update stuff which doesn't send events on init: */
    updateVirtualizationState();
    updateStateAudioActions();
    updateMouseState();

    /* Warn listeners about we are initialized: */
    m_fInitialized = true;
    emit sigInitialized();

    /* True by default: */
    return true;
}

void UIMachine::prepareBranding()
{
    /* Acquire user machine-window icon: */
    QIcon icon = generalIconPool().userMachineIcon(uisession()->machine());
    /* Use the OS type icon if user one was not set: */
    if (icon.isNull())
        icon = generalIconPool().guestOSTypeIcon(uisession()->machine().GetOSTypeId());
    /* Use the default icon if nothing else works: */
    if (icon.isNull())
        icon = QIcon(":/VirtualBox_48px.png");
    /* Store the icon dynamically: */
    m_pMachineWindowIcon = new QIcon(icon);

#ifndef VBOX_WS_MAC
    /* Load user's machine-window name postfix: */
    const QUuid uMachineID = uiCommon().managedVMUuid();
    m_strMachineWindowNamePostfix = gEDataManager->machineWindowNamePostfix(uMachineID);
#endif /* !VBOX_WS_MAC */
}

void UIMachine::prepareSessionConnections()
{
    /* Console events stuff: */
    connect(uisession(), &UISession::sigAudioAdapterChange,
            this, &UIMachine::sltHandleAudioAdapterChange);
    connect(uisession(), &UISession::sigAdditionsStateChange,
            this, &UIMachine::sigAdditionsStateChange);
    connect(uisession(), &UISession::sigAdditionsStateActualChange,
            this, &UIMachine::sltHandleAdditionsActualChange);
    connect(uisession(), &UISession::sigClipboardModeChange,
            this, &UIMachine::sigClipboardModeChange);
    connect(uisession(), &UISession::sigCPUExecutionCapChange,
            this, &UIMachine::sigCPUExecutionCapChange);
    connect(uisession(), &UISession::sigDnDModeChange,
            this, &UIMachine::sigDnDModeChange);
    connect(uisession(), &UISession::sigGuestMonitorChange,
            this, &UIMachine::sltHandleGuestMonitorChange);
    connect(uisession(), &UISession::sigMachineStateChange,
            this, &UIMachine::sigMachineStateChange);
    connect(uisession(), &UISession::sigMediumChange,
            this, &UIMachine::sigMediumChange);
    connect(uisession(), &UISession::sigNetworkAdapterChange,
            this, &UIMachine::sigNetworkAdapterChange);
    connect(uisession(), &UISession::sigRecordingChange,
            this, &UIMachine::sltHandleRecordingChange);
    connect(uisession(), &UISession::sigSharedFolderChange,
            this, &UIMachine::sigSharedFolderChange);
    connect(uisession(), &UISession::sigStorageDeviceChange,
            this, &UIMachine::sltHandleStorageDeviceChange);
    connect(uisession(), &UISession::sigUSBControllerChange,
            this, &UIMachine::sigUSBControllerChange);
    connect(uisession(), &UISession::sigUSBDeviceStateChange,
            this, &UIMachine::sigUSBDeviceStateChange);
    connect(uisession(), &UISession::sigVRDEChange,
            this, &UIMachine::sltHandleVRDEChange);
    connect(uisession(), &UISession::sigRuntimeError,
            this, &UIMachine::sigRuntimeError);
#ifdef VBOX_WS_MAC
    connect(uisession(), &UISession::sigShowWindows,
            this, &UIMachine::sigShowWindows);
#endif

    /* Keyboard stuff: */
    connect(uisession(), &UISession::sigKeyboardLedsChange,
            this, &UIMachine::sltHandleKeyboardLedsChange);

    /* Mouse stuff: */
    connect(uisession(), &UISession::sigMousePointerShapeChange,
            this, &UIMachine::sltMousePointerShapeChange);
    connect(uisession(), &UISession::sigMouseCapabilityChange,
            this, &UIMachine::sltMouseCapabilityChange);
    connect(uisession(), &UISession::sigCursorPositionChange,
            this, &UIMachine::sltCursorPositionChange);
}

void UIMachine::prepareActions()
{
    /* Create action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Runtime);
    if (actionPool())
    {
        /* Make sure action-pool knows guest-screen count: */
        actionPool()->toRuntime()->setGuestScreenCount(uisession()->frameBuffers().size());
        /* Update action restrictions: */
        updateActionRestrictions();

#ifdef VBOX_WS_MAC
        /* Create Mac OS X menu-bar: */
        m_pMenuBar = new QMenuBar;
        if (m_pMenuBar)
        {
            /* Configure Mac OS X menu-bar: */
            connect(gEDataManager, &UIExtraDataManager::sigMenuBarConfigurationChange,
                    this, &UIMachine::sltHandleMenuBarConfigurationChange);
            /* Update Mac OS X menu-bar: */
            updateMenu();
        }
#endif /* VBOX_WS_MAC */

        /* Get machine ID: */
        const QUuid uMachineID = uiCommon().managedVMUuid();
        Q_UNUSED(uMachineID);

#ifdef VBOX_WS_MAC
        /* User-element (Menu-bar and Dock) options: */
        {
            const bool fDisabled = gEDataManager->guiFeatureEnabled(GUIFeatureType_NoUserElements);
            if (fDisabled)
                UICocoaApplication::instance()->hideUserElements();
        }
#else /* !VBOX_WS_MAC */
        /* Menu-bar options: */
        {
            const bool fEnabledGlobally = !gEDataManager->guiFeatureEnabled(GUIFeatureType_NoMenuBar);
            const bool fEnabledForMachine = gEDataManager->menuBarEnabled(uMachineID);
            const bool fEnabled = fEnabledGlobally && fEnabledForMachine;
            actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_S_Settings)->setEnabled(fEnabled);
            actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->blockSignals(true);
            actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->setChecked(fEnabled);
            actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->blockSignals(false);
        }
#endif /* !VBOX_WS_MAC */

        /* View options: */
        const bool fGuestScreenAutoresize = gEDataManager->guestScreenAutoResizeEnabled(uMachineID);
        actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize)->blockSignals(true);
        actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize)->setChecked(fGuestScreenAutoresize);
        actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize)->blockSignals(false);

        /* Input options: */
        const bool fMouseIntegrated = isMouseIntegrated(); // no e-data for now ..
        actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration)->blockSignals(true);
        actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration)->setChecked(fMouseIntegrated);
        actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration)->blockSignals(false);

        /* Device options: */
        actionPool()->action(UIActionIndexRT_M_Devices_S_UpgradeGuestAdditions)->setEnabled(false);

        /* Status-bar options: */
        {
            const bool fEnabledGlobally = !gEDataManager->guiFeatureEnabled(GUIFeatureType_NoStatusBar);
            const bool fEnabledForMachine = gEDataManager->statusBarEnabled(uMachineID);
            const bool fEnabled = fEnabledGlobally && fEnabledForMachine;
            actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings)->setEnabled(fEnabled);
            actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->blockSignals(true);
            actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->setChecked(fEnabled);
            actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->blockSignals(false);
        }
    }
}

void UIMachine::prepareScreens()
{
    /* Recache display data: */
    updateHostScreenData();

#ifdef VBOX_WS_MAC
    /* Prepare display-change watchdog: */
    m_pWatchdogDisplayChange = new QTimer(this);
    {
        m_pWatchdogDisplayChange->setInterval(500);
        m_pWatchdogDisplayChange->setSingleShot(true);
        connect(m_pWatchdogDisplayChange, &QTimer::timeout,
                this, &UIMachine::sltCheckIfHostDisplayChanged);
    }
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WS_MAC
    /* Install native display reconfiguration callback: */
    CGDisplayRegisterReconfigurationCallback(cgDisplayReconfigurationCallback, this);
#else /* !VBOX_WS_MAC */
    /* Install Qt display reconfiguration callbacks: */
    connect(gpDesktop, &UIDesktopWidgetWatchdog::sigHostScreenCountChanged,
            this, &UIMachine::sltHandleHostScreenCountChange);
    connect(gpDesktop, &UIDesktopWidgetWatchdog::sigHostScreenResized,
            this, &UIMachine::sltHandleHostScreenGeometryChange);
# if defined(VBOX_WS_X11) && !defined(VBOX_GUI_WITH_CUSTOMIZATIONS1)
    connect(gpDesktop, &UIDesktopWidgetWatchdog::sigHostScreenWorkAreaRecalculated,
            this, &UIMachine::sltHandleHostScreenAvailableAreaChange);
# else /* !VBOX_WS_X11 || VBOX_GUI_WITH_CUSTOMIZATIONS1 */
    connect(gpDesktop, &UIDesktopWidgetWatchdog::sigHostScreenWorkAreaResized,
            this, &UIMachine::sltHandleHostScreenAvailableAreaChange);
# endif /* !VBOX_WS_X11 || VBOX_GUI_WITH_CUSTOMIZATIONS1 */
#endif /* !VBOX_WS_MAC */

    /* Prepare initial screen visibility status: */
    m_monitorVisibilityVector.resize(uisession()->machine().GetGraphicsAdapter().GetMonitorCount());
    m_monitorVisibilityVector.fill(false);
    m_monitorVisibilityVector[0] = true;

    /* Prepare empty last full-screen size vector: */
    m_monitorLastFullScreenSizeVector.resize(uisession()->machine().GetGraphicsAdapter().GetMonitorCount());
    m_monitorLastFullScreenSizeVector.fill(QSize(-1, -1));

    /* If machine is in 'saved' state: */
    if (uisession()->isSaved())
    {
        /* Update screen visibility status from saved-state: */
        for (int iScreenIndex = 0; iScreenIndex < m_monitorVisibilityVector.size(); ++iScreenIndex)
        {
            BOOL fEnabled = true;
            ULONG uGuestOriginX = 0, uGuestOriginY = 0, uGuestWidth = 0, uGuestHeight = 0;
            uisession()->machine().QuerySavedGuestScreenInfo(iScreenIndex,
                                                             uGuestOriginX, uGuestOriginY,
                                                             uGuestWidth, uGuestHeight, fEnabled);
            m_monitorVisibilityVector[iScreenIndex] = fEnabled;
        }
        /* And make sure at least one of them is visible (primary if others are hidden): */
        if (countOfVisibleWindows() < 1)
            m_monitorVisibilityVector[0] = true;
    }
    else if (uiCommon().isSeparateProcess())
    {
        /* Update screen visibility status from display directly: */
        for (int iScreenIndex = 0; iScreenIndex < m_monitorVisibilityVector.size(); ++iScreenIndex)
        {
            KGuestMonitorStatus enmStatus = KGuestMonitorStatus_Disabled;
            ULONG uGuestWidth = 0, uGuestHeight = 0, uBpp = 0;
            LONG iGuestOriginX = 0, iGuestOriginY = 0;
            uisession()->display().GetScreenResolution(iScreenIndex,
                                                       uGuestWidth, uGuestHeight, uBpp,
                                                       iGuestOriginX, iGuestOriginY, enmStatus);
            m_monitorVisibilityVector[iScreenIndex] = (   enmStatus == KGuestMonitorStatus_Enabled
                                                       || enmStatus == KGuestMonitorStatus_Blank);
        }
        /* And make sure at least one of them is visible (primary if others are hidden): */
        if (countOfVisibleWindows() < 1)
            m_monitorVisibilityVector[0] = true;
    }

    /* Prepare initial screen visibility status of host-desires (same as facts): */
    m_monitorVisibilityVectorHostDesires.resize(uisession()->machine().GetGraphicsAdapter().GetMonitorCount());
    for (int iScreenIndex = 0; iScreenIndex < m_monitorVisibilityVector.size(); ++iScreenIndex)
        m_monitorVisibilityVectorHostDesires[iScreenIndex] = m_monitorVisibilityVector[iScreenIndex];

    /* Make sure action-pool knows guest-screen visibility status: */
    for (int iScreenIndex = 0; iScreenIndex < m_monitorVisibilityVector.size(); ++iScreenIndex)
        actionPool()->toRuntime()->setGuestScreenVisible(iScreenIndex, m_monitorVisibilityVector.at(iScreenIndex));
}

void UIMachine::prepareKeyboard()
{
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    /* Load extra-data value: */
    m_fIsHidLedsSyncEnabled = gEDataManager->hidLedsSyncState(uiCommon().managedVMUuid());
    /* Connect to extra-data changes to be able to enable/disable feature dynamically: */
    connect(gEDataManager, &UIExtraDataManager::sigHidLedsSyncStateChange,
            this, &UIMachine::sltHidLedsSyncStateChanged);
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */
}

void UIMachine::prepareClose()
{
    /* What is the default close action and the restricted are? */
    const QUuid uMachineID = uiCommon().managedVMUuid();
    m_defaultCloseAction = gEDataManager->defaultMachineCloseAction(uMachineID);
    m_restrictedCloseActions = gEDataManager->restrictedMachineCloseActions(uMachineID);
}

void UIMachine::prepareMachineLogic()
{
    /* Prepare async visual state type change handler: */
    qRegisterMetaType<UIVisualStateType>();
    connect(this, &UIMachine::sigRequestAsyncVisualStateChange,
            this, &UIMachine::sltChangeVisualState,
            Qt::QueuedConnection);

    /* Load restricted visual states: */
    UIVisualStateType restrictedVisualStates = gEDataManager->restrictedVisualStates(uiCommon().managedVMUuid());
    /* Acquire allowed visual states: */
    m_allowedVisualStates = static_cast<UIVisualStateType>(UIVisualStateType_All ^ restrictedVisualStates);

    /* Load requested visual state, it can override initial one: */
    m_enmRequestedVisualState = gEDataManager->requestedVisualState(uiCommon().managedVMUuid());
    /* Check if requested visual state is allowed: */
    if (isVisualStateAllowed(m_enmRequestedVisualState))
    {
        switch (m_enmRequestedVisualState)
        {
            /* Direct transition allowed to scale/fullscreen modes only: */
            case UIVisualStateType_Scale:      m_initialVisualState = UIVisualStateType_Scale; break;
            case UIVisualStateType_Fullscreen: m_initialVisualState = UIVisualStateType_Fullscreen; break;
            default: break;
        }
    }

    /* Enter initial visual state: */
    enterInitialVisualState();
}

void UIMachine::cleanupMachineLogic()
{
    /* Destroy machine-logic if exists: */
    if (m_pMachineLogic)
    {
        m_pMachineLogic->cleanup();
        UIMachineLogic::destroy(m_pMachineLogic);
        m_pMachineLogic = 0;
    }
}

void UIMachine::cleanupScreens()
{
#ifdef VBOX_WS_MAC
    /* Remove display reconfiguration callback: */
    CGDisplayRemoveReconfigurationCallback(cgDisplayReconfigurationCallback, this);
#endif /* VBOX_WS_MAC */
}

void UIMachine::cleanupActions()
{
#ifdef VBOX_WS_MAC
    /* Destroy Mac OS X menu-bar: */
    delete m_pMenuBar;
    m_pMenuBar = 0;
#endif /* VBOX_WS_MAC */

    /* Destroy action-pool if necessary: */
    if (actionPool())
        UIActionPool::destroy(actionPool());
}

void UIMachine::cleanupBranding()
{
    /* Cleanup machine-window icon: */
    delete m_pMachineWindowIcon;
    m_pMachineWindowIcon = 0;
}

void UIMachine::cleanupSession()
{
    /* Destroy session UI if exists: */
    if (uisession())
        UISession::destroy(m_pSession);
}

void UIMachine::cleanup()
{
    /* Preprocess all the meta-events: */
    QApplication::sendPostedEvents(0, QEvent::MetaCall);

    /* Cleanup stuff: */
    cleanupMachineLogic();
    cleanupScreens();
    cleanupActions();
    cleanupBranding();

    /* Cleanup session UI: */
    cleanupSession();
}

void UIMachine::enterInitialVisualState()
{
    sltChangeVisualState(m_initialVisualState);
}

void UIMachine::updateActionRestrictions()
{
    /* Get host and prepare restrictions: */
    const CHost comHost = uiCommon().host();
    UIExtraDataMetaDefs::RuntimeMenuMachineActionType restrictionForMachine =
        UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Invalid;
    UIExtraDataMetaDefs::RuntimeMenuViewActionType restrictionForView =
        UIExtraDataMetaDefs::RuntimeMenuViewActionType_Invalid;
    UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restrictionForDevices =
        UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Invalid;

    /* Separate process stuff: */
    {
        /* Initialize 'Machine' menu: */
        if (!uiCommon().isSeparateProcess())
            restrictionForMachine = (UIExtraDataMetaDefs::RuntimeMenuMachineActionType)
                                    (restrictionForMachine | UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Detach);
    }

    /* VRDE server stuff: */
    {
        /* Initialize 'View' menu: */
        const CVRDEServer comServer = uisession()->machine().GetVRDEServer();
        if (comServer.isNull())
            restrictionForView = (UIExtraDataMetaDefs::RuntimeMenuViewActionType)
                                 (restrictionForView | UIExtraDataMetaDefs::RuntimeMenuViewActionType_VRDEServer);
    }

    /* Storage stuff: */
    {
        /* Initialize CD/FD menus: */
        int iDevicesCountCD = 0;
        int iDevicesCountFD = 0;
        foreach (const CMediumAttachment &comAttachment, uisession()->machine().GetMediumAttachments())
        {
            if (comAttachment.GetType() == KDeviceType_DVD)
                ++iDevicesCountCD;
            if (comAttachment.GetType() == KDeviceType_Floppy)
                ++iDevicesCountFD;
        }
        QAction *pOpticalDevicesMenu = actionPool()->action(UIActionIndexRT_M_Devices_M_OpticalDevices);
        QAction *pFloppyDevicesMenu = actionPool()->action(UIActionIndexRT_M_Devices_M_FloppyDevices);
        pOpticalDevicesMenu->setData(iDevicesCountCD);
        pFloppyDevicesMenu->setData(iDevicesCountFD);
        if (!iDevicesCountCD)
            restrictionForDevices = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
                                    (restrictionForDevices | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_OpticalDevices);
        if (!iDevicesCountFD)
            restrictionForDevices = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
                                    (restrictionForDevices | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_FloppyDevices);
    }

    /* Audio stuff: */
    {
        /* Check whether audio controller is enabled. */
        const CAudioSettings comAudioSettings = uisession()->machine().GetAudioSettings();
        const CAudioAdapter comAudioAdapter = comAudioSettings.GetAdapter();
        if (comAudioAdapter.isNull() || !comAudioAdapter.GetEnabled())
            restrictionForDevices = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
                                    (restrictionForDevices | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Audio);
    }

    /* Network stuff: */
    {
        /* Initialize Network menu: */
        bool fAtLeastOneAdapterActive = false;
        const KChipsetType enmChipsetType = uisession()->machine().GetChipsetType();
        ULONG uSlots = uiCommon().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(enmChipsetType);
        for (ULONG uSlot = 0; uSlot < uSlots; ++uSlot)
        {
            const CNetworkAdapter &comNetworkAdapter = uisession()->machine().GetNetworkAdapter(uSlot);
            if (comNetworkAdapter.GetEnabled())
            {
                fAtLeastOneAdapterActive = true;
                break;
            }
        }
        if (!fAtLeastOneAdapterActive)
            restrictionForDevices = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
                                    (restrictionForDevices | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Network);
    }

    /* USB stuff: */
    {
        /* Check whether there is at least one USB controller with an available proxy. */
        const bool fUSBEnabled =    !uisession()->machine().GetUSBDeviceFilters().isNull()
                                 && !uisession()->machine().GetUSBControllers().isEmpty()
                                 && uisession()->machine().GetUSBProxyAvailable();
        if (!fUSBEnabled)
            restrictionForDevices = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
                                    (restrictionForDevices | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_USBDevices);
    }

    /* WebCams stuff: */
    {
        /* Check whether there is an accessible video input devices pool: */
        comHost.GetVideoInputDevices();
        const bool fWebCamsEnabled = comHost.isOk() && !uisession()->machine().GetUSBControllers().isEmpty();
        if (!fWebCamsEnabled)
            restrictionForDevices = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)
                                    (restrictionForDevices | UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_WebCams);
    }

    /* Apply cumulative restriction for 'Machine' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuMachine(UIActionRestrictionLevel_Session, restrictionForMachine);
    /* Apply cumulative restriction for 'View' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuView(UIActionRestrictionLevel_Session, restrictionForView);
    /* Apply cumulative restriction for 'Devices' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuDevices(UIActionRestrictionLevel_Session, restrictionForDevices);
}

#ifdef VBOX_WS_MAC
void UIMachine::updateMenu()
{
    /* Rebuild Mac OS X menu-bar: */
    m_pMenuBar->clear();
    foreach (QMenu *pMenu, actionPool()->menus())
    {
        UIMenu *pMenuUI = qobject_cast<UIMenu*>(pMenu);
        if (!pMenuUI->isConsumable() || !pMenuUI->isConsumed())
            m_pMenuBar->addMenu(pMenuUI);
        if (pMenuUI->isConsumable() && !pMenuUI->isConsumed())
            pMenuUI->setConsumed(true);
    }
    /* Update the dock menu as well: */
    if (machineLogic())
        machineLogic()->updateDock();
}
#endif /* VBOX_WS_MAC */

void UIMachine::updateHostScreenData()
{
    /* Rebuild host-screen data vector: */
    m_hostScreens.clear();
    for (int iScreenIndex = 0; iScreenIndex < UIDesktopWidgetWatchdog::screenCount(); ++iScreenIndex)
        m_hostScreens << gpDesktop->screenGeometry(iScreenIndex);

    /* Make sure action-pool knows host-screen count: */
    actionPool()->toRuntime()->setHostScreenCount(m_hostScreens.size());
}

void UIMachine::updateMousePointerShape()
{
    /* Fetch incoming shape data: */
    const bool fHasAlpha = m_shapeData.hasAlpha();
    const uint uWidth = m_shapeData.shapeSize().width();
    const uint uHeight = m_shapeData.shapeSize().height();
    const uchar *pShapeData = m_shapeData.shape().constData();
    AssertMsgReturnVoid(pShapeData, ("Shape data must not be NULL!\n"));

    /* Invalidate mouse pointer shape initially: */
    m_fIsValidPointerShapePresent = false;
    m_cursorShapePixmap = QPixmap();
    m_cursorMaskPixmap = QPixmap();

    /* Parse incoming shape data: */
    const uchar *pSrcAndMaskPtr = pShapeData;
    const uint uAndMaskSize = (uWidth + 7) / 8 * uHeight;
    const uchar *pSrcShapePtr = pShapeData + ((uAndMaskSize + 3) & ~3);

#if defined (VBOX_WS_WIN)

    /* Create an ARGB image out of the shape data: */

    // WORKAROUND:
    // Qt5 QCursor recommends 32 x 32 cursor, therefore the original data is copied to
    // a larger QImage if necessary. Cursors like 10x16 did not work correctly (Solaris 10 guest).
    // Align the cursor dimensions to 32 bit pixels, because for example a 56x56 monochrome cursor
    // did not work correctly on Windows host.
    const uint uCursorWidth = RT_ALIGN_32(uWidth, 32);
    const uint uCursorHeight = RT_ALIGN_32(uHeight, 32);

    if (fHasAlpha)
    {
        QImage image(uCursorWidth, uCursorHeight, QImage::Format_ARGB32);
        memset(image.bits(), 0, image.byteCount());

        const uint32_t *pu32SrcShapeScanline = (uint32_t *)pSrcShapePtr;
        for (uint y = 0; y < uHeight; ++y, pu32SrcShapeScanline += uWidth)
            memcpy(image.scanLine(y), pu32SrcShapeScanline, uWidth * sizeof(uint32_t));

        m_cursorShapePixmap = QPixmap::fromImage(image);
    }
    else
    {
        if (isPointer1bpp(pSrcShapePtr, uWidth, uHeight))
        {
            // Incoming data consist of 32 bit BGR XOR mask and 1 bit AND mask.
            // XOR pixels contain either 0x00000000 or 0x00FFFFFF.
            //
            // Originally intended result (F denotes 0x00FFFFFF):
            // XOR AND
            //   0   0 black
            //   F   0 white
            //   0   1 transparent
            //   F   1 xor'd
            //
            // Actual Qt5 result for color table 0:0xFF000000, 1:0xFFFFFFFF
            // (tested on Windows 7 and 10 64 bit hosts):
            // Bitmap Mask
            //  0   0 black
            //  1   0 white
            //  0   1 xor
            //  1   1 transparent

            QVector<QRgb> colors(2);
            colors[0] = UINT32_C(0xFF000000);
            colors[1] = UINT32_C(0xFFFFFFFF);

            QImage bitmap(uCursorWidth, uCursorHeight, QImage::Format_Mono);
            bitmap.setColorTable(colors);
            memset(bitmap.bits(), 0xFF, bitmap.byteCount());

            QImage mask(uCursorWidth, uCursorHeight, QImage::Format_Mono);
            mask.setColorTable(colors);
            memset(mask.bits(), 0xFF, mask.byteCount());

            const uint8_t *pu8SrcAndScanline = pSrcAndMaskPtr;
            const uint32_t *pu32SrcShapeScanline = (uint32_t *)pSrcShapePtr;
            for (uint y = 0; y < uHeight; ++y)
            {
                for (uint x = 0; x < uWidth; ++x)
                {
                    const uint8_t u8Bit = (uint8_t)(1 << (7 - x % 8));

                    const uint8_t u8SrcMaskByte = pu8SrcAndScanline[x / 8];
                    const uint8_t u8SrcMaskBit = u8SrcMaskByte & u8Bit;
                    const uint32_t u32SrcPixel = pu32SrcShapeScanline[x] & UINT32_C(0xFFFFFF);

                    uint8_t *pu8DstMaskByte = &mask.scanLine(y)[x / 8];
                    uint8_t *pu8DstBitmapByte = &bitmap.scanLine(y)[x / 8];

                    if (u8SrcMaskBit == 0)
                    {
                        if (u32SrcPixel == 0)
                        {
                            /* Black: Qt Bitmap = 0, Mask = 0 */
                            *pu8DstMaskByte &= ~u8Bit;
                            *pu8DstBitmapByte &= ~u8Bit;
                        }
                        else
                        {
                            /* White: Qt Bitmap = 1, Mask = 0 */
                            *pu8DstMaskByte &= ~u8Bit;
                            *pu8DstBitmapByte |= u8Bit;
                        }
                    }
                    else
                    {
                        if (u32SrcPixel == 0)
                        {
                            /* Transparent: Qt Bitmap = 1, Mask = 1 */
                            *pu8DstMaskByte |= u8Bit;
                            *pu8DstBitmapByte |= u8Bit;
                        }
                        else
                        {
                            /* Xor'ed: Qt Bitmap = 0, Mask = 1 */
                            *pu8DstMaskByte |= u8Bit;
                            *pu8DstBitmapByte &= ~u8Bit;
                        }
                    }
                }

                pu8SrcAndScanline += (uWidth + 7) / 8;
                pu32SrcShapeScanline += uWidth;
            }

            m_cursorShapePixmap = QBitmap::fromImage(bitmap);
            m_cursorMaskPixmap = QBitmap::fromImage(mask);
        }
        else
        {
            /* Assign alpha channel values according to the AND mask: 1 -> 0x00, 0 -> 0xFF: */
            QImage image(uCursorWidth, uCursorHeight, QImage::Format_ARGB32);
            memset(image.bits(), 0, image.byteCount());

            const uint8_t *pu8SrcAndScanline = pSrcAndMaskPtr;
            const uint32_t *pu32SrcShapeScanline = (uint32_t *)pSrcShapePtr;

            for (uint y = 0; y < uHeight; ++y)
            {
                uint32_t *pu32DstPixel = (uint32_t *)image.scanLine(y);

                for (uint x = 0; x < uWidth; ++x)
                {
                    const uint8_t u8Bit = (uint8_t)(1 << (7 - x % 8));
                    const uint8_t u8SrcMaskByte = pu8SrcAndScanline[x / 8];

                    if (u8SrcMaskByte & u8Bit)
                        *pu32DstPixel++ = pu32SrcShapeScanline[x] & UINT32_C(0x00FFFFFF);
                    else
                        *pu32DstPixel++ = pu32SrcShapeScanline[x] | UINT32_C(0xFF000000);
                }

                pu32SrcShapeScanline += uWidth;
                pu8SrcAndScanline += (uWidth + 7) / 8;
            }

            m_cursorShapePixmap = QPixmap::fromImage(image);
        }
    }

    /* Mark mouse pointer shape valid: */
    m_fIsValidPointerShapePresent = true;

#elif defined(VBOX_WS_X11) || defined(VBOX_WS_MAC)

    /* Create an ARGB image out of the shape data: */
    QImage image(uWidth, uHeight, QImage::Format_ARGB32);

    if (fHasAlpha)
    {
        memcpy(image.bits(), pSrcShapePtr, uHeight * uWidth * 4);
    }
    else
    {
        renderCursorPixels((uint32_t *)pSrcShapePtr, pSrcAndMaskPtr,
                           uWidth, uHeight,
                           (uint32_t *)image.bits(), uHeight * uWidth * 4);
    }

    /* Create cursor-pixmap from the image: */
    m_cursorShapePixmap = QPixmap::fromImage(image);

    /* Mark mouse pointer shape valid: */
    m_fIsValidPointerShapePresent = true;

#else

# warning "port me"

#endif

    /* Cache cursor pixmap size and hotspot: */
    m_cursorSize = m_cursorShapePixmap.size();
    m_cursorHotspot = m_shapeData.hotSpot();
}

void UIMachine::updateMouseState()
{
    m_fIsMouseSupportsAbsolute = uisession()->mouse().GetAbsoluteSupported();
    m_fIsMouseSupportsRelative = uisession()->mouse().GetRelativeSupported();
    m_fIsMouseSupportsTouchScreen = uisession()->mouse().GetTouchScreenSupported();
    m_fIsMouseSupportsTouchPad = uisession()->mouse().GetTouchPadSupported();
    m_fIsMouseHostCursorNeeded = uisession()->mouse().GetNeedsHostCursor();
}

#if defined(VBOX_WS_X11) || defined(VBOX_WS_MAC)
/* static */
void UIMachine::renderCursorPixels(const uint32_t *pu32XOR, const uint8_t *pu8AND,
                                   uint32_t u32Width, uint32_t u32Height,
                                   uint32_t *pu32Pixels, uint32_t cbPixels)
{
    /* Output pixels set to 0 which allow to not write transparent pixels anymore. */
    memset(pu32Pixels, 0, cbPixels);

    const uint32_t *pu32XORSrc = pu32XOR;  /* Iterator for source XOR pixels. */
    const uint8_t *pu8ANDSrcLine = pu8AND; /* The current AND mask scanline. */
    uint32_t *pu32Dst = pu32Pixels;        /* Iterator for all destination BGRA pixels. */

    /* Some useful constants. */
    const int cbANDLine = ((int)u32Width + 7) / 8;

    int y;
    for (y = 0; y < (int)u32Height; ++y)
    {
        int x;
        for (x = 0; x < (int)u32Width; ++x)
        {
            const uint32_t u32Pixel = *pu32XORSrc; /* Current pixel at (x,y) */
            const uint8_t *pu8ANDSrc = pu8ANDSrcLine + x / 8; /* Byte which containt current AND bit. */

            if ((*pu8ANDSrc << (x % 8)) & 0x80)
            {
                if (u32Pixel)
                {
                    const uint32_t u32PixelInverted = ~u32Pixel;

                    /* Scan neighbor pixels and assign them if they are transparent. */
                    int dy;
                    for (dy = -1; dy <= 1; ++dy)
                    {
                        const int yn = y + dy;
                        if (yn < 0 || yn >= (int)u32Height)
                            continue; /* Do not cross the bounds. */

                        int dx;
                        for (dx = -1; dx <= 1; ++dx)
                        {
                            const int xn = x + dx;
                            if (xn < 0 || xn >= (int)u32Width)
                                continue;  /* Do not cross the bounds. */

                            if (dx != 0 || dy != 0)
                            {
                                /* Check if the neighbor pixel is transparent. */
                                const uint32_t *pu32XORNeighborSrc = &pu32XORSrc[dy * (int)u32Width + dx];
                                const uint8_t *pu8ANDNeighborSrc = pu8ANDSrcLine + dy * cbANDLine + xn / 8;
                                if (   *pu32XORNeighborSrc == 0
                                    && ((*pu8ANDNeighborSrc << (xn % 8)) & 0x80) != 0)
                                {
                                    /* Transparent neighbor pixels are replaced with the source pixel value. */
                                    uint32_t *pu32PixelNeighborDst = &pu32Dst[dy * (int)u32Width + dx];
                                    *pu32PixelNeighborDst = u32Pixel | 0xFF000000;
                                }
                            }
                            else
                            {
                                /* The pixel itself is replaced with inverted value. */
                                *pu32Dst = u32PixelInverted | 0xFF000000;
                            }
                        }
                    }
                }
                else
                {
                    /* The pixel does not affect the screen.
                     * Do nothing. Do not touch destination which can already contain generated pixels.
                     */
                }
            }
            else
            {
                /* AND bit is 0, the pixel will be just drawn. */
                *pu32Dst = u32Pixel | 0xFF000000;
            }

            ++pu32XORSrc; /* Next source pixel. */
            ++pu32Dst;    /* Next destination pixel. */
        }

        /* Next AND scanline. */
        pu8ANDSrcLine += cbANDLine;
    }
}
#endif /* VBOX_WS_X11 || VBOX_WS_MAC */

#ifdef VBOX_WS_WIN
/* static */
bool UIMachine::isPointer1bpp(const uint8_t *pu8XorMask,
                              uint uWidth,
                              uint uHeight)
{
    /* Check if the pointer has only 0 and 0xFFFFFF pixels, ignoring the alpha channel. */
    const uint32_t *pu32Src = (uint32_t *)pu8XorMask;

    uint y;
    for (y = 0; y < uHeight ; ++y)
    {
        uint x;
        for (x = 0; x < uWidth; ++x)
        {
            const uint32_t u32Pixel = pu32Src[x] & UINT32_C(0xFFFFFF);
            if (u32Pixel != 0 && u32Pixel != UINT32_C(0xFFFFFF))
                return false;
        }

        pu32Src += uWidth;
    }

    return true;
}
#endif /* VBOX_WS_WIN */

void UIMachine::updateVirtualizationState()
{
    m_enmVMExecutionEngine = uisession()->debugger().GetExecutionEngine();
    m_fIsHWVirtExNestedPagingEnabled = uisession()->debugger().GetHWVirtExNestedPagingEnabled();
    m_fIsHWVirtExUXEnabled = uisession()->debugger().GetHWVirtExUXEnabled();
    m_enmParavirtProvider = uisession()->machine().GetEffectiveParavirtProvider();
}
