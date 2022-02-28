/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneGlobal class declaration.
 */

/*
 * Copyright (C) 2017-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIToolPaneGlobal_h
#define FEQT_INCLUDED_SRC_manager_UIToolPaneGlobal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QHBoxLayout;
class QStackedLayout;
class QVBoxLayout;
class UIActionPool;
class UICloudProfileManagerWidget;
class UIExtensionPackManagerWidget;
class UIMediumManagerWidget;
class UINetworkManagerWidget;
class UIVMActivityOverviewWidget;
class UIVirtualMachineItem;
class UIWelcomePane;
class CMachine;


/** QWidget subclass representing container for tool panes. */
class UIToolPaneGlobal : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about request to switch to Activity pane of machine with @a uMachineId. */
    void sigSwitchToMachineActivityPane(const QUuid &uMachineId);

public:

    /** Constructs tools pane passing @a pParent to the base-class. */
    UIToolPaneGlobal(UIActionPool *pActionPool, QWidget *pParent = 0);
    /** Destructs tools pane. */
    virtual ~UIToolPaneGlobal() RT_OVERRIDE;

    /** Defines whether this pane is @a fActive. */
    void setActive(bool fActive);
    /** Returns whether this pane is active. */
    bool active() const { return m_fActive; }

    /** Returns type of tool currently opened. */
    UIToolType currentTool() const;
    /** Returns whether tool of particular @a enmType is opened. */
    bool isToolOpened(UIToolType enmType) const;
    /** Activates tool of passed @a enmType, creates new one if necessary. */
    void openTool(UIToolType enmType);
    /** Closes tool of passed @a enmType, deletes one if exists. */
    void closeTool(UIToolType enmType);
    /** Returns the help keyword of the current tool's widget. */
    QString currentHelpKeyword() const;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares stacked-layout. */
    void prepareStackedLayout();
    /** Cleanups all. */
    void cleanup();

    /** Handles token change. */
    void handleTokenChange();

    /** Holds the action pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the stacked-layout instance. */
    QStackedLayout               *m_pLayout;
    /** Holds the Welcome pane instance. */
    UIWelcomePane                *m_pPaneWelcome;
    /** Holds the Extension Pack Manager instance. */
    UIExtensionPackManagerWidget *m_pPaneExtensions;
    /** Holds the Virtual Media Manager instance. */
    UIMediumManagerWidget        *m_pPaneMedia;
    /** Holds the Network Manager instance. */
    UINetworkManagerWidget       *m_pPaneNetwork;
    /** Holds the Cloud Profile Manager instance. */
    UICloudProfileManagerWidget  *m_pPaneCloud;
    /** Holds the VM Activity Overview instance. */
    UIVMActivityOverviewWidget   *m_pPaneVMActivityOverview;

    /** Holds whether this pane is active. */
    bool  m_fActive;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIToolPaneGlobal_h */
