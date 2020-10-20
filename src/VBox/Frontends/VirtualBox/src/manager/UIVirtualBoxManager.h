/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManager class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIVirtualBoxManager_h
#define FEQT_INCLUDED_SRC_manager_UIVirtualBoxManager_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>
#include <QUrl>

/* GUI includes: */
#include "QIWithRestorableGeometry.h"
#include "QIWithRetranslateUI.h"
#include "UICommon.h"

/* Forward declarations: */
class QMenu;
class QIManagerDialog;
class UIAction;
class UIActionPool;
struct UIUnattendedInstallData;
class UIVirtualBoxManagerWidget;
class UIVirtualMachineItem;

/* Type definitions: */
typedef QIWithRestorableGeometry<QMainWindow> QMainWindowWithRestorableGeometry;
typedef QIWithRetranslateUI<QMainWindowWithRestorableGeometry> QMainWindowWithRestorableGeometryAndRetranslateUi;
typedef QMap<QString, QIManagerDialog*> VMLogViewerMap;

/** Singleton QMainWindow extension used as VirtualBox Manager instance. */
class UIVirtualBoxManager : public QMainWindowWithRestorableGeometryAndRetranslateUi
{
    Q_OBJECT;

    /** Pointer to menu update-handler for this class: */
    typedef void (UIVirtualBoxManager::*MenuUpdateHandler)(QMenu *pMenu);

signals:

    /** Notifies listeners about this window remapped to another screen. */
    void sigWindowRemapped();

    /** Notifies listeners about Cloud Profile Manager change. */
    void sigCloudProfileManagerChange();

public:

    /** Singleton constructor. */
    static void create();
    /** Singleton destructor. */
    static void destroy();
    /** Singleton instance provider. */
    static UIVirtualBoxManager *instance() { return s_pInstance; }

    /** Returns the action-pool instance. */
    UIActionPool *actionPool() const { return m_pActionPool; }

    /** Opens Cloud Profile Manager. */
    void openCloudProfileManager() { sltOpenCloudProfileManagerWindow(); }

protected:

    /** Constructs VirtualBox Manager. */
    UIVirtualBoxManager();
    /** Destructs VirtualBox Manager. */
    virtual ~UIVirtualBoxManager() /* override */;

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

    /** @name Event handling stuff.
      * @{ */
#ifdef VBOX_WS_MAC
        /** Mac OS X: Preprocesses any @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
#endif

        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles any Qt @a pEvent. */
        virtual bool event(QEvent *pEvent) /* override */;
        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
        /** Handles first show @a pEvent. */
        virtual void polishEvent(QShowEvent *pEvent) /* override */;
        /** Handles close @a pEvent. */
        virtual void closeEvent(QCloseEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** @name Common stuff.
      * @{ */
#if QT_VERSION == 0
        /** Stupid moc does not warn if it cannot find headers! */
        void QT_VERSION_NOT_DEFINED
#elif defined(VBOX_WS_X11)
        /** Handles host-screen available-area change. */
        void sltHandleHostScreenAvailableAreaChange();
#endif /* VBOX_WS_X11 */

        /** Handles request to update actions. */
        void sltHandleUpdateActionAppearanceRequest() { updateActionsAppearance(); }

        /** Handles signal about medium-enumeration finished. */
        void sltHandleMediumEnumerationFinish();

        /** Handles call to open a @a list of URLs. */
        void sltHandleOpenUrlCall(QList<QUrl> list = QList<QUrl>());

        /** Hnadles singal about Chooser-pane index change.  */
        void sltHandleChooserPaneIndexChange();
        /** Handles signal about group saving progress change. */
        void sltHandleGroupSavingProgressChange();
        /** Handles signal about cloud update progress change. */
        void sltHandleCloudUpdateProgressChange();

        /** Handles singal about Tool type change.  */
        void sltHandleToolTypeChange();

        /** Handles current snapshot item change. */
        void sltCurrentSnapshotItemChange();

        /** Handles state change for cloud machine with certain @a uId. */
        void sltHandleCloudMachineStateChange(const QUuid &uId);
    /** @} */

    /** @name CVirtualBox event handling stuff.
      * @{ */
        /** Handles CVirtualBox event about state change for machine with @a uID. */
        void sltHandleStateChange(const QUuid &uID);
    /** @} */

    /** @name Action-pool stuff.
      * @{ */
        /** Handle menu prepare. */
        void sltHandleMenuPrepare(int iIndex, QMenu *pMenu);
    /** @} */

    /** @name File menu stuff.
      * @{ */
        /** Handles call to open Virtual Medium Manager window. */
        void sltOpenVirtualMediumManagerWindow();
        /** Handles call to close Virtual Medium Manager window. */
        void sltCloseVirtualMediumManagerWindow();

        /** Handles call to open Host Network Manager window. */
        void sltOpenHostNetworkManagerWindow();
        /** Handles call to close Host Network Manager window. */
        void sltCloseHostNetworkManagerWindow();

        /** Handles call to open Cloud Profile Manager window. */
        void sltOpenCloudProfileManagerWindow();
        /** Handles call to close Cloud Profile Manager window. */
        void sltCloseCloudProfileManagerWindow();

        /** Handles call to open Cloud Console Manager window. */
        void sltOpenCloudConsoleManagerWindow();
        /** Handles call to close Cloud Console Manager window. */
        void sltCloseCloudConsoleManagerWindow();

        /** Handles call to open Import Appliance wizard.
          * @param strFileName can bring the name of file to import appliance from. */
        void sltOpenImportApplianceWizard(const QString &strFileName = QString());
        /** Handles call to open Import Appliance wizard the default way. */
        void sltOpenImportApplianceWizardDefault() { sltOpenImportApplianceWizard(); }
        /** Handles call to open Export Appliance wizard. */
        void sltOpenExportApplianceWizard();

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
        /** Handles call to open Extra-data Manager window. */
        void sltOpenExtraDataManagerWindow();
#endif

        /** Handles call to open Preferences dialog. */
        void sltOpenPreferencesDialog();

        /** Handles call to exit application. */
        void sltPerformExit();
    /** @} */

    /** @name Machine menu stuff.
      * @{ */
        /** Handles call to open new machine wizard. */
        void sltOpenNewMachineWizard();
        /** Handles call to open add machine dialog. */
        void sltOpenAddMachineDialog();

        /** Handles call to open group name editor. */
        void sltOpenGroupNameEditor();
        /** Handles call to disband group. */
        void sltDisbandGroup();

        /** Handles call to open Machine Settings dialog.
          * @param strCategory can bring the settings category to start from.
          * @param strControl  can bring the widget of the page to focus.
          * @param uID       can bring the ID of machine to manage. */
        void sltOpenMachineSettingsDialog(QString strCategory = QString(),
                                          QString strControl = QString(),
                                          const QUuid &uID = QUuid());
        /** Handles call to open Machine Settings dialog the default way. */
        void sltOpenMachineSettingsDialogDefault() { sltOpenMachineSettingsDialog(); }

        /** Handles call to open Clone Machine wizard. */
        void sltOpenCloneMachineWizard();

        /** Handles call to move machine. */
        void sltPerformMachineMove();

        /** Handles call to remove machine. */
        void sltPerformMachineRemove();

        /** Handles call to move machine to a new group. */
        void sltPerformMachineMoveToNewGroup();
        /** Handles call to move machine to a specific group. */
        void sltPerformMachineMoveToSpecificGroup();

        /** Handles call to start or show machine. */
        void sltPerformStartOrShowMachine();
        /** Handles call to start machine in normal mode. */
        void sltPerformStartMachineNormal();
        /** Handles call to start machine in headless mode. */
        void sltPerformStartMachineHeadless();
        /** Handles call to start machine in detachable mode. */
        void sltPerformStartMachineDetachable();

        /** Handles call to create console connection for group. */
        void sltPerformCreateConsoleConnectionForGroup();
        /** Handles call to create console connection for machine. */
        void sltPerformCreateConsoleConnectionForMachine();
        /** Handles call to delete console connection for group. */
        void sltPerformDeleteConsoleConnectionForGroup();
        /** Handles call to delete console connection for machine. */
        void sltPerformDeleteConsoleConnectionForMachine();
        /** Handles call to copy console connection key fingerprint. */
        void sltCopyConsoleConnectionFingerprint();
        /** Handles call to copy serial console command for Unix. */
        void sltPerformCopyCommandSerialUnix();
        /** Handles call to copy serial console command for Windows. */
        void sltPerformCopyCommandSerialWindows();
        /** Handles call to copy VNC console command for Unix. */
        void sltPerformCopyCommandVNCUnix();
        /** Handles call to copy VNC console command for Windows. */
        void sltPerformCopyCommandVNCWindows();
        /** Handles call to execute external application. */
        void sltExecuteExternalApplication();

        /** Handles call to discard machine state. */
        void sltPerformDiscardMachineState();

        /** Handles call to @a fPause or resume machine otherwise. */
        void sltPerformPauseOrResumeMachine(bool fPause);

        /** Handles call to reset machine. */
        void sltPerformResetMachine();

        /** Handles call to detach machine UI. */
        void sltPerformDetachMachineUI();
        /** Handles call to save machine state. */
        void sltPerformSaveMachineState();
        /** Handles call to ask machine for shutdown. */
        void sltPerformShutdownMachine();
        /** Handles call to power machine off. */
        void sltPerformPowerOffMachine();

        /** Handles call to show machine tool corresponding to passed @a pAction. */
        void sltPerformShowMachineTool(QAction *pAction);

        /** Handles call to open machine Log Viewer window. */
        void sltOpenLogViewerWindow();
        /** Handles call to close machine Log Viewer window. */
        void sltCloseLogViewerWindow();

        /** Handles call to open machine Performance Monitor window. */
        void sltOpenPerformanceMonitorWindow();

        /** Handles call to close machine Performance Monitor window. */
        void sltClosePerformanceMonitorWindow();

        /** Handles call to refresh machine. */
        void sltPerformRefreshMachine();

        /** Handles call to show machine in File Manager. */
        void sltShowMachineInFileManager();

        /** Handles call to create machine shortcut. */
        void sltPerformCreateMachineShortcut();

        /** Handles call to sort group. */
        void sltPerformGroupSorting();

        /** Handles call to toggle machine search widget visibility to be @a fVisible. */
        void sltPerformMachineSearchWidgetVisibilityToggling(bool fVisible);
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares window. */
        void prepare();
        /** Prepares icon. */
        void prepareIcon();
        /** Prepares menu-bar. */
        void prepareMenuBar();
        /** Prepares status-bar. */
        void prepareStatusBar();
        /** Prepares toolbar. */
        void prepareToolbar();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups widgets. */
        void cleanupWidgets();
        /** Cleanups menu-bar. */
        void cleanupMenuBar();
        /** Cleanups window. */
        void cleanup();
    /** @} */

    /** @name Common stuff.
      * @{ */
        /** Returns current-item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current-items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;
        /** Returns whether all items of one group is selected. */
        bool isAllItemsOfOneGroupSelected() const;
        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
        /** Returns whether single local group is selected. */
        bool isSingleLocalGroupSelected() const;
        /** Returns whether single cloud profile group is selected. */
        bool isSingleCloudProfileGroupSelected() const;

        /** Returns whether cloud update is in progress. */
        bool isCloudUpdateInProgress() const;
    /** @} */

    /** @name Various VM helpers.
      * @{ */
        /** Opens add machine dialog specifying initial name with @a strFileName. */
        void openAddMachineDialog(const QString &strFileName = QString());

        /** Creates an unattended installer and uses it to install guest os to newly created vm. */
        void startUnattendedInstall(CUnattended &comUnattendedInstaller, const UIUnattendedInstallData &unattendedData);

        /** Launches or shows virtual machines represented by passed @a items in corresponding @a enmLaunchMode (for launch). */
        void performStartOrShowVirtualMachines(const QList<UIVirtualMachineItem*> &items, UICommon::LaunchMode enmLaunchMode);

#ifndef VBOX_WS_WIN
        /** Parses serialized @a strArguments string according to shell rules. */
        QStringList parseShellArguments(const QString &strArguments);
#endif
    /** @} */

    /** @name Action update stuff.
      * @{ */
        /** Updates 'Group' menu. */
        void updateMenuGroup(QMenu *pMenu);
        /** Updates 'Machine' menu. */
        void updateMenuMachine(QMenu *pMenu);
        /** Updates 'Group' : 'Move to Group' menu. */
        void updateMenuGroupMoveToGroup(QMenu *pMenu);
        /** Updates 'Group' : 'Console' menu. */
        void updateMenuGroupConsole(QMenu *pMenu);
        /** Updates 'Group' : 'Close' menu. */
        void updateMenuGroupClose(QMenu *pMenu);
        /** Updates 'Machine' : 'Move to Group' menu. */
        void updateMenuMachineMoveToGroup(QMenu *pMenu);
        /** Updates 'Machine' : 'Console' menu. */
        void updateMenuMachineConsole(QMenu *pMenu);
        /** Updates 'Machine' : 'Close' menu. */
        void updateMenuMachineClose(QMenu *pMenu);

        /** Performs update of actions visibility. */
        void updateActionsVisibility();
        /** Performs update of actions appearance. */
        void updateActionsAppearance();

        /** Returns whether the action with @a iActionIndex is enabled.
          * @param items used to calculate verdict about should the action be enabled. */
        bool isActionEnabled(int iActionIndex, const QList<UIVirtualMachineItem*> &items);

        /** Returns whether all passed @a items are local. */
        static bool isItemsLocal(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether all passed @a items are powered off. */
        static bool isItemsPoweredOff(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is able to shutdown. */
        static bool isAtLeastOneItemAbleToShutdown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items supports shortcut creation. */
        static bool isAtLeastOneItemSupportsShortcuts(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is accessible. */
        static bool isAtLeastOneItemAccessible(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is inaccessible. */
        static bool isAtLeastOneItemInaccessible(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is removable. */
        static bool isAtLeastOneItemRemovable(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be started. */
        static bool isAtLeastOneItemCanBeStarted(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be shown. */
        static bool isAtLeastOneItemCanBeShown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be started or shown. */
        static bool isAtLeastOneItemCanBeStartedOrShown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be discarded. */
        static bool isAtLeastOneItemDiscardable(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is started. */
        static bool isAtLeastOneItemStarted(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is running. */
        static bool isAtLeastOneItemRunning(const QList<UIVirtualMachineItem*> &items);

#ifdef VBOX_WS_X11
        /** Tries to guess default X11 terminal emulator.
          * @returns Data packed into Qt pair of QString(s),
          *          which is `name` and `--execute argument`. */
        static QPair<QString, QString> defaultTerminalData();
#endif
    /** @} */

    /** Holds the static instance. */
    static UIVirtualBoxManager *s_pInstance;

    /** Holds whether the dialog is polished. */
    bool  m_fPolished                      : 1;
    /** Holds whether first medium-enumeration handled. */
    bool  m_fFirstMediumEnumerationHandled : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;
    /** Holds the map of menu update-handlers. */
    QMap<int, MenuUpdateHandler> m_menuUpdateHandlers;

    /** Holds the Virtual Media Manager window instance. */
    QIManagerDialog *m_pManagerVirtualMedia;
    /** Holds the Host Network Manager window instance. */
    QIManagerDialog *m_pManagerHostNetwork;
    /** Holds the Cloud Profile Manager window instance. */
    QIManagerDialog *m_pManagerCloudProfile;
    /** Holds the Cloud Console Manager window instance. */
    QIManagerDialog *m_pManagerCloudConsole;
    /** Holds a map of (machineUUID, UIVMLogViewerDialog). */
    VMLogViewerMap   m_logViewers;

    /** Holds the central-widget instance. */
    UIVirtualBoxManagerWidget *m_pWidget;
};

#define gpManager UIVirtualBoxManager::instance()

#endif /* !FEQT_INCLUDED_SRC_manager_UIVirtualBoxManager_h */
