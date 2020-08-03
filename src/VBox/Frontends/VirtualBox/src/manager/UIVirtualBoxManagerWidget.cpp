/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManagerWidget class implementation.
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

/* Qt includes: */
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QISplitter.h"
#include "UIActionPoolManager.h"
#include "UIExtraDataManager.h"
#include "UIChooser.h"
#include "UIVirtualBoxManager.h"
#include "UIVirtualBoxManagerWidget.h"
#include "UITabBar.h"
#include "UIToolBar.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItemCloud.h"
#include "UIVirtualMachineItemLocal.h"
#include "UITools.h"
#ifndef VBOX_WS_MAC
# include "UIMenuBar.h"
#endif


UIVirtualBoxManagerWidget::UIVirtualBoxManagerWidget(UIVirtualBoxManager *pParent)
    : m_pActionPool(pParent->actionPool())
    , m_pSplitter(0)
    , m_pToolBar(0)
    , m_pPaneChooser(0)
    , m_pStackedWidget(0)
    , m_pPaneToolsGlobal(0)
    , m_pPaneToolsMachine(0)
    , m_pSlidingAnimation(0)
    , m_pPaneTools(0)
    , m_enmSelectionType(SelectionType_Invalid)
    , m_fSelectedMachineItemAccessible(false)
{
    prepare();
}

UIVirtualBoxManagerWidget::~UIVirtualBoxManagerWidget()
{
    cleanup();
}

UIVirtualMachineItem *UIVirtualBoxManagerWidget::currentItem() const
{
    return m_pPaneChooser->currentItem();
}

QList<UIVirtualMachineItem*> UIVirtualBoxManagerWidget::currentItems() const
{
    return m_pPaneChooser->currentItems();
}

bool UIVirtualBoxManagerWidget::isGroupItemSelected() const
{
    return m_pPaneChooser->isGroupItemSelected();
}

bool UIVirtualBoxManagerWidget::isGlobalItemSelected() const
{
    return m_pPaneChooser->isGlobalItemSelected();
}

bool UIVirtualBoxManagerWidget::isMachineItemSelected() const
{
    return m_pPaneChooser->isMachineItemSelected();
}

bool UIVirtualBoxManagerWidget::isSingleGroupSelected() const
{
    return m_pPaneChooser->isSingleGroupSelected();
}

bool UIVirtualBoxManagerWidget::isSingleLocalGroupSelected() const
{
    return m_pPaneChooser->isSingleLocalGroupSelected();
}

bool UIVirtualBoxManagerWidget::isSingleCloudProfileGroupSelected() const
{
    return m_pPaneChooser->isSingleCloudProfileGroupSelected();
}

bool UIVirtualBoxManagerWidget::isAllItemsOfOneGroupSelected() const
{
    return m_pPaneChooser->isAllItemsOfOneGroupSelected();
}

QString UIVirtualBoxManagerWidget::fullGroupName() const
{
    return m_pPaneChooser->fullGroupName();
}

bool UIVirtualBoxManagerWidget::isGroupSavingInProgress() const
{
    return m_pPaneChooser->isGroupSavingInProgress();
}

void UIVirtualBoxManagerWidget::openGroupNameEditor()
{
    m_pPaneChooser->openGroupNameEditor();
}

void UIVirtualBoxManagerWidget::disbandGroup()
{
    m_pPaneChooser->disbandGroup();
}

void UIVirtualBoxManagerWidget::removeMachine()
{
    m_pPaneChooser->removeMachine();
}

void UIVirtualBoxManagerWidget::moveMachineToGroup(const QString &strName /* = QString() */)
{
    m_pPaneChooser->moveMachineToGroup(strName);
}

QStringList UIVirtualBoxManagerWidget::possibleGroupsForMachineToMove(const QUuid &uId)
{
    return m_pPaneChooser->possibleGroupsForMachineToMove(uId);
}

QStringList UIVirtualBoxManagerWidget::possibleGroupsForGroupToMove(const QString &strFullName)
{
    return m_pPaneChooser->possibleGroupsForGroupToMove(strFullName);
}

void UIVirtualBoxManagerWidget::refreshMachine()
{
    m_pPaneChooser->refreshMachine();
}

void UIVirtualBoxManagerWidget::sortGroup()
{
    m_pPaneChooser->sortGroup();
}

void UIVirtualBoxManagerWidget::setMachineSearchWidgetVisibility(bool fVisible)
{
    m_pPaneChooser->setMachineSearchWidgetVisibility(fVisible);
}

void UIVirtualBoxManagerWidget::setToolsType(UIToolType enmType)
{
    m_pPaneTools->setToolsType(enmType);
}

UIToolType UIVirtualBoxManagerWidget::toolsType() const
{
    return m_pPaneTools ? m_pPaneTools->toolsType() : UIToolType_Invalid;
}

UIToolType UIVirtualBoxManagerWidget::currentGlobalTool() const
{
    return m_pPaneToolsGlobal ? m_pPaneToolsGlobal->currentTool() : UIToolType_Invalid;
}

UIToolType UIVirtualBoxManagerWidget::currentMachineTool() const
{
    return m_pPaneToolsMachine ? m_pPaneToolsMachine->currentTool() : UIToolType_Invalid;
}

bool UIVirtualBoxManagerWidget::isGlobalToolOpened(UIToolType enmType) const
{
    return m_pPaneToolsGlobal ? m_pPaneToolsGlobal->isToolOpened(enmType) : false;
}

bool UIVirtualBoxManagerWidget::isMachineToolOpened(UIToolType enmType) const
{
    return m_pPaneToolsMachine ? m_pPaneToolsMachine->isToolOpened(enmType) : false;
}

void UIVirtualBoxManagerWidget::switchToGlobalTool(UIToolType enmType)
{
    /* Open corresponding tool: */
    m_pPaneToolsGlobal->openTool(enmType);

    /* Let the parent know: */
    emit sigToolTypeChange();

    /* Update toolbar: */
    updateToolbar();
}

void UIVirtualBoxManagerWidget::switchToMachineTool(UIToolType enmType)
{
    /* Open corresponding tool: */
    m_pPaneToolsMachine->openTool(enmType);

    /* Let the parent know: */
    emit sigToolTypeChange();

    /* Update toolbar: */
    updateToolbar();
}

void UIVirtualBoxManagerWidget::closeGlobalTool(UIToolType enmType)
{
    m_pPaneToolsGlobal->closeTool(enmType);
}

void UIVirtualBoxManagerWidget::closeMachineTool(UIToolType enmType)
{
    m_pPaneToolsMachine->closeTool(enmType);
}

bool UIVirtualBoxManagerWidget::isCurrentStateItemSelected() const
{
    return m_pPaneToolsMachine->isCurrentStateItemSelected();
}

void UIVirtualBoxManagerWidget::sltHandleToolBarContextMenuRequest(const QPoint &position)
{
    /* Populate toolbar actions: */
    QList<QAction*> actions;
    /* Add 'Show Toolbar Text' action: */
    QAction *pShowToolBarText = new QAction(UIVirtualBoxManager::tr("Show Toolbar Text"), 0);
    AssertPtrReturnVoid(pShowToolBarText);
    {
        /* Configure action: */
        pShowToolBarText->setCheckable(true);
        pShowToolBarText->setChecked(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);

        /* Add into action list: */
        actions << pShowToolBarText;
    }

    /* Prepare the menu position: */
    QPoint globalPosition = position;
    QWidget *pSender = static_cast<QWidget*>(sender());
    if (pSender)
        globalPosition = pSender->mapToGlobal(position);

    /* Execute the menu: */
    QAction *pResult = QMenu::exec(actions, globalPosition);

    /* Handle the menu execution result: */
    if (pResult == pShowToolBarText)
    {
        m_pToolBar->setToolButtonStyle(  pResult->isChecked()
                                       ? Qt::ToolButtonTextUnderIcon
                                       : Qt::ToolButtonIconOnly);
    }
}

void UIVirtualBoxManagerWidget::retranslateUi()
{
    /* Make sure chosen item fetched: */
    sltHandleChooserPaneIndexChange();

#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    m_pToolBar->updateLayout();
#endif
}

void UIVirtualBoxManagerWidget::sltHandleStateChange(const QUuid &)
{
    /* Recache current item info if machine or group item selected: */
    if (isMachineItemSelected() || isGroupItemSelected())
        recacheCurrentItemInformation();
}

void UIVirtualBoxManagerWidget::sltHandleToolBarResize(const QSize &newSize)
{
    emit sigToolBarHeightChange(newSize.height());
}

void UIVirtualBoxManagerWidget::sltHandleChooserPaneIndexChange()
{
    /* Let the parent know: */
    emit sigChooserPaneIndexChange();

    /* If global item is selected and we are on machine tools pane => switch to global tools pane: */
    if (   isGlobalItemSelected()
        && m_pStackedWidget->currentWidget() != m_pPaneToolsGlobal)
    {
        /* Just start animation and return, do nothing else.. */
        m_pStackedWidget->setCurrentWidget(m_pPaneToolsGlobal); // rendering w/a
        m_pStackedWidget->setCurrentWidget(m_pSlidingAnimation);
        m_pSlidingAnimation->animate(SlidingDirection_Reverse);
        return;
    }

    else

    /* If machine or group item is selected and we are on global tools pane => switch to machine tools pane: */
    if (   (isMachineItemSelected() || isGroupItemSelected())
        && m_pStackedWidget->currentWidget() != m_pPaneToolsMachine)
    {
        /* Just start animation and return, do nothing else.. */
        m_pStackedWidget->setCurrentWidget(m_pPaneToolsMachine); // rendering w/a
        m_pStackedWidget->setCurrentWidget(m_pSlidingAnimation);
        m_pSlidingAnimation->animate(SlidingDirection_Forward);
        return;
    }

    /* Recache current item info if machine or group item selected: */
    if (isMachineItemSelected() || isGroupItemSelected())
        recacheCurrentItemInformation();

    /* Calculate selection type: */
    const SelectionType enmSelectedItemType = isSingleGroupSelected()
                                            ? SelectionType_SingleGroupItem
                                            : isGlobalItemSelected()
                                            ? SelectionType_FirstIsGlobalItem
                                            : isMachineItemSelected()
                                            ? SelectionType_FirstIsMachineItem
                                            : SelectionType_Invalid;
    /* Acquire current item: */
    UIVirtualMachineItem *pItem = currentItem();
    const bool fCurrentItemIsOk = pItem && pItem->accessible();

    /* Update toolbar if selection type or item accessibility got changed: */
    if (   m_enmSelectionType != enmSelectedItemType
        || m_fSelectedMachineItemAccessible != fCurrentItemIsOk)
        updateToolbar();

    /* Remember the last selection type: */
    m_enmSelectionType = enmSelectedItemType;
    /* Remember whether the last selected item was accessible: */
    m_fSelectedMachineItemAccessible = fCurrentItemIsOk;
}

void UIVirtualBoxManagerWidget::sltHandleSlidingAnimationComplete(SlidingDirection enmDirection)
{
    /* First switch the panes: */
    switch (enmDirection)
    {
        case SlidingDirection_Forward:
        {
            m_pPaneTools->setToolsClass(UIToolClass_Machine);
            m_pStackedWidget->setCurrentWidget(m_pPaneToolsMachine);
            m_pPaneToolsGlobal->setActive(false);
            m_pPaneToolsMachine->setActive(true);
            break;
        }
        case SlidingDirection_Reverse:
        {
            m_pPaneTools->setToolsClass(UIToolClass_Global);
            m_pStackedWidget->setCurrentWidget(m_pPaneToolsGlobal);
            m_pPaneToolsMachine->setActive(false);
            m_pPaneToolsGlobal->setActive(true);
            break;
        }
    }
    /* Then handle current item change (again!): */
    sltHandleChooserPaneIndexChange();
}

void UIVirtualBoxManagerWidget::sltHandleCloudMachineStateChange(const QUuid &uId)
{
    /* Not for global items: */
    if (!isGlobalItemSelected())
    {
        /* Acquire current item: */
        UIVirtualMachineItem *pItem = currentItem();
        const bool fCurrentItemIsOk = pItem && pItem->accessible();

        /* If current item is Ok: */
        if (fCurrentItemIsOk)
        {
            /* If Error-pane is chosen currently => open tool currently chosen in Tools-pane: */
            if (m_pPaneToolsMachine->currentTool() == UIToolType_Error)
                sltHandleToolsPaneIndexChange();

            /* If we still have same item selected: */
            if (pItem && pItem->id() == uId)
            {
                /* Propagate current items to update the Details-pane: */
                m_pPaneToolsMachine->setItems(currentItems());
                /* Repeat the task a bit delayed: */
                pItem->toCloud()->updateInfoAsync(true /* delayed? */);
            }
        }
        else
        {
            /* Make sure Error pane raised: */
            if (m_pPaneToolsMachine->currentTool() != UIToolType_Error)
                m_pPaneToolsMachine->openTool(UIToolType_Error);

            /* If we still have same item selected: */
            if (pItem && pItem->id() == uId)
            {
                /* Propagate current items to update the Details-pane (in any case): */
                m_pPaneToolsMachine->setItems(currentItems());
                /* Propagate last access error to update the Error-pane (if machine selected but inaccessible): */
                m_pPaneToolsMachine->setErrorDetails(pItem->accessError());
            }
        }

        /* Pass the signal further: */
        emit sigCloudMachineStateChange(uId);
    }
}

void UIVirtualBoxManagerWidget::sltHandleToolMenuRequested(UIToolClass enmClass, const QPoint &position)
{
    /* Define current tools class: */
    m_pPaneTools->setToolsClass(enmClass);

    /* Move, resize and show: */
    m_pPaneTools->move(position);
    m_pPaneTools->show();
    // WORKAROUND:
    // Don't want even to think why, but for Qt::Popup resize to
    // smaller size often being ignored until it is actually shown.
    m_pPaneTools->resize(m_pPaneTools->minimumSizeHint());
}

void UIVirtualBoxManagerWidget::sltHandleToolsPaneIndexChange()
{
    /* Acquire current class/type: */
    const UIToolClass enmCurrentClass = m_pPaneTools->toolsClass();
    const UIToolType enmCurrentType = m_pPaneTools->toolsType();

    /* Invent default for fallback case: */
    const UIToolType enmDefaultType = enmCurrentClass == UIToolClass_Global ? UIToolType_Welcome
                                    : enmCurrentClass == UIToolClass_Machine ? UIToolType_Details
                                    : UIToolType_Invalid;
    AssertReturnVoid(enmDefaultType != UIToolType_Invalid);

    /* Calculate new type to choose: */
    const UIToolType enmNewType = UIToolStuff::isTypeOfClass(enmCurrentType, enmCurrentClass)
                                ? enmCurrentType : enmDefaultType;

    /* Choose new type: */
    switch (m_pPaneTools->toolsClass())
    {
        case UIToolClass_Global: switchToGlobalTool(enmNewType); break;
        case UIToolClass_Machine: switchToMachineTool(enmNewType); break;
        default: break;
    }
}

void UIVirtualBoxManagerWidget::sltSwitchToMachinePerformancePane(const QUuid &uMachineId)
{
    AssertPtrReturnVoid(m_pPaneChooser);
    AssertPtrReturnVoid(m_pPaneTools);
    m_pPaneChooser->setCurrentMachine(uMachineId);
    m_pPaneTools->setToolsType(UIToolType_Performance);
}

void UIVirtualBoxManagerWidget::prepare()
{
    /* Prepare everything: */
    preparePalette();
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    retranslateUi();

    /* Make sure current Chooser-pane index fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIVirtualBoxManagerWidget::preparePalette()
{
    setAutoFillBackground(true);
    QPalette pal = palette();
#ifdef VBOX_WS_MAC
    const QColor color = pal.color(QPalette::Active, QPalette::Mid).lighter(145);
#else
    const QColor color = pal.color(QPalette::Active, QPalette::Mid).lighter(160);
#endif
    pal.setColor(QPalette::Window, color);
    setPalette(pal);
}

void UIVirtualBoxManagerWidget::prepareWidgets()
{
    /* Create main-layout: */
    QHBoxLayout *pLayoutMain = new QHBoxLayout(this);
    if (pLayoutMain)
    {
        /* Configure layout: */
        pLayoutMain->setSpacing(0);
        pLayoutMain->setContentsMargins(0, 0, 0, 0);

        /* Create splitter: */
        m_pSplitter = new QISplitter(Qt::Horizontal, QISplitter::Flat);
        if (m_pSplitter)
        {
            /* Configure splitter: */
            m_pSplitter->setHandleWidth(1);

            /* Create Chooser-pane: */
            m_pPaneChooser = new UIChooser(this, actionPool());
            if (m_pPaneChooser)
            {
                /* Add into splitter: */
                m_pSplitter->addWidget(m_pPaneChooser);
            }

            /* Create right widget: */
            QWidget *pWidgetRight = new QWidget;
            if (pWidgetRight)
            {
                /* Create right-layout: */
                QVBoxLayout *pLayoutRight = new QVBoxLayout(pWidgetRight);
                if(pLayoutRight)
                {
                    /* Configure layout: */
                    pLayoutRight->setSpacing(0);
                    pLayoutRight->setContentsMargins(0, 0, 0, 0);

                    /* Create Main toolbar: */
                    m_pToolBar = new UIToolBar;
                    if (m_pToolBar)
                    {
                        /* Configure toolbar: */
                        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
                        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
                        m_pToolBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                        m_pToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
                        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
#ifdef VBOX_WS_MAC
                        m_pToolBar->emulateMacToolbar();
#endif

                        /* Add tool-bar into layout: */
                        pLayoutRight->addWidget(m_pToolBar);
                    }

                    /* Create stacked-widget: */
                    m_pStackedWidget = new QStackedWidget;
                    if (m_pStackedWidget)
                    {
                        /* Create Global Tools-pane: */
                        m_pPaneToolsGlobal = new UIToolPaneGlobal(actionPool());
                        if (m_pPaneToolsGlobal)
                        {
                            if (m_pPaneChooser->isGlobalItemSelected())
                                m_pPaneToolsGlobal->setActive(true);
                            connect(m_pPaneToolsGlobal, &UIToolPaneGlobal::sigCloudProfileManagerChange,
                                    this, &UIVirtualBoxManagerWidget::sigCloudProfileManagerChange);
                            connect(m_pPaneToolsGlobal, &UIToolPaneGlobal::sigSwitchToMachinePerformancePane,
                                    this, &UIVirtualBoxManagerWidget::sltSwitchToMachinePerformancePane);

                            /* Add into stack: */
                            m_pStackedWidget->addWidget(m_pPaneToolsGlobal);
                        }

                        /* Create Machine Tools-pane: */
                        m_pPaneToolsMachine = new UIToolPaneMachine(actionPool());
                        if (m_pPaneToolsMachine)
                        {
                            if (!m_pPaneChooser->isGlobalItemSelected())
                                m_pPaneToolsMachine->setActive(true);
                            connect(m_pPaneToolsMachine, &UIToolPaneMachine::sigCurrentSnapshotItemChange,
                                    this, &UIVirtualBoxManagerWidget::sigCurrentSnapshotItemChange);

                            /* Add into stack: */
                            m_pStackedWidget->addWidget(m_pPaneToolsMachine);
                        }

                        /* Create sliding-widget: */
                        // Reverse initial animation direction if group or machine selected!
                        const bool fReverse = !m_pPaneChooser->isGlobalItemSelected();
                        m_pSlidingAnimation = new UISlidingAnimation(Qt::Vertical, fReverse);
                        if (m_pSlidingAnimation)
                        {
                            /* Add first/second widgets into sliding animation: */
                            m_pSlidingAnimation->setWidgets(m_pPaneToolsGlobal, m_pPaneToolsMachine);
                            connect(m_pSlidingAnimation, &UISlidingAnimation::sigAnimationComplete,
                                    this, &UIVirtualBoxManagerWidget::sltHandleSlidingAnimationComplete);

                            /* Add into stack: */
                            m_pStackedWidget->addWidget(m_pSlidingAnimation);
                        }

                        /* Choose which pane should be active initially: */
                        if (m_pPaneChooser->isGlobalItemSelected())
                            m_pStackedWidget->setCurrentWidget(m_pPaneToolsGlobal);
                        else
                            m_pStackedWidget->setCurrentWidget(m_pPaneToolsMachine);

                        /* Add into layout: */
                        pLayoutRight->addWidget(m_pStackedWidget, 1);
                    }
                }

                /* Add into splitter: */
                m_pSplitter->addWidget(pWidgetRight);
            }

            /* Adjust splitter colors according to main widgets it splits: */
            m_pSplitter->configureColor(palette().color(QPalette::Active, QPalette::Midlight).darker(110));
            /* Set the initial distribution. The right site is bigger. */
            m_pSplitter->setStretchFactor(0, 2);
            m_pSplitter->setStretchFactor(1, 3);

            /* Add into layout: */
            pLayoutMain->addWidget(m_pSplitter);
        }

        /* Create Tools-pane: */
        m_pPaneTools = new UITools(this);
        if (m_pPaneTools)
        {
            /* Choose which pane should be active initially: */
            if (m_pPaneChooser->isGlobalItemSelected())
                m_pPaneTools->setToolsClass(UIToolClass_Global);
            else
                m_pPaneTools->setToolsClass(UIToolClass_Machine);
        }
    }

    /* Update toolbar finally: */
    updateToolbar();

    /* Bring the VM list to the focus: */
    m_pPaneChooser->setFocus();
}

void UIVirtualBoxManagerWidget::prepareConnections()
{
    /* Global VBox event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIVirtualBoxManagerWidget::sltHandleStateChange);

    /* Tool-bar connections: */
    connect(m_pToolBar, &UIToolBar::customContextMenuRequested,
            this, &UIVirtualBoxManagerWidget::sltHandleToolBarContextMenuRequest);
    connect(m_pToolBar, &UIToolBar::sigResized,
            this, &UIVirtualBoxManagerWidget::sltHandleToolBarResize);

    /* Chooser-pane connections: */
    connect(this, &UIVirtualBoxManagerWidget::sigToolBarHeightChange,
            m_pPaneChooser, &UIChooser::setGlobalItemHeightHint);
    connect(m_pPaneChooser, &UIChooser::sigSelectionChanged,
            this, &UIVirtualBoxManagerWidget::sltHandleChooserPaneIndexChange);
    connect(m_pPaneChooser, &UIChooser::sigSelectionInvalidated,
            this, &UIVirtualBoxManagerWidget::sltHandleChooserPaneSelectionInvalidated);
    connect(m_pPaneChooser, &UIChooser::sigToggleStarted,
            m_pPaneToolsMachine, &UIToolPaneMachine::sigToggleStarted);
    connect(m_pPaneChooser, &UIChooser::sigToggleFinished,
            m_pPaneToolsMachine, &UIToolPaneMachine::sigToggleFinished);
    connect(m_pPaneChooser, &UIChooser::sigGroupSavingStateChanged,
            this, &UIVirtualBoxManagerWidget::sigGroupSavingStateChanged);
    connect(m_pPaneChooser, &UIChooser::sigToolMenuRequested,
            this, &UIVirtualBoxManagerWidget::sltHandleToolMenuRequested);
    connect(m_pPaneChooser, &UIChooser::sigCloudMachineStateChange,
            this, &UIVirtualBoxManagerWidget::sltHandleCloudMachineStateChange);
    connect(m_pPaneChooser, &UIChooser::sigStartOrShowRequest,
            this, &UIVirtualBoxManagerWidget::sigStartOrShowRequest);
    connect(m_pPaneChooser, &UIChooser::sigMachineSearchWidgetVisibilityChanged,
            this, &UIVirtualBoxManagerWidget::sigMachineSearchWidgetVisibilityChanged);

    /* Details-pane connections: */
    connect(m_pPaneToolsMachine, &UIToolPaneMachine::sigLinkClicked,
            this, &UIVirtualBoxManagerWidget::sigMachineSettingsLinkClicked);

    /* Tools-pane connections: */
    connect(m_pPaneTools, &UITools::sigSelectionChanged,
            this, &UIVirtualBoxManagerWidget::sltHandleToolsPaneIndexChange);
}

void UIVirtualBoxManagerWidget::loadSettings()
{
    /* Restore splitter handle position: */
    {
        /* Read splitter hints: */
        QList<int> sizes = gEDataManager->selectorWindowSplitterHints();
        /* If both hints are zero, we have the 'default' case: */
        if (sizes[0] == 0 && sizes[1] == 0)
        {
            /* Propose some 'default' based on current dialog width: */
            sizes[0] = (int)(width() * .9 * (1.0 / 3));
            sizes[1] = (int)(width() * .9 * (2.0 / 3));
        }
        /* Pass hints to the splitter: */
        m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar settings: */
    {
        m_pToolBar->setToolButtonStyle(  gEDataManager->selectorWindowToolBarTextVisible()
                                       ? Qt::ToolButtonTextUnderIcon
                                       : Qt::ToolButtonIconOnly);
    }

    /* Open tools last chosen in Tools-pane: */
    switchToGlobalTool(m_pPaneTools->lastSelectedToolGlobal());
    switchToMachineTool(m_pPaneTools->lastSelectedToolMachine());
}

void UIVirtualBoxManagerWidget::updateToolbar()
{
    /* Make sure toolbar exists: */
    AssertPtrReturnVoid(m_pToolBar);

    /* Clear initially: */
    m_pToolBar->clear();

    /* Basic action set: */
    switch (m_pPaneTools->toolsClass())
    {
        /* Global toolbar: */
        case UIToolClass_Global:
        {
            switch (currentGlobalTool())
            {
                case UIToolType_Welcome:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_File_S_ImportAppliance));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_File_S_ExportAppliance));
                    //m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_File_S_NewCloudVM)); // later
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Welcome_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Welcome_S_Add));
                    break;
                }
                case UIToolType_Media:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Add));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Create));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Copy));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Move));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Remove));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Release));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_T_Search));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_T_Details));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Refresh));
                    break;
                }
                case UIToolType_Network:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_S_Create));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_S_Remove));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_T_Details));
                    //m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_S_Refresh));
                    break;
                }
                case UIToolType_Cloud:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Add));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Import));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Remove));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_T_Details));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_TryPage));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Help));
                    break;
                }
                case UIToolType_Resources:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_VMResourceMonitor_M_Columns));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_VMResourceMonitor_S_SwitchToMachinePerformance));
                    QToolButton *pButton =
                        qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexMN_M_VMResourceMonitor_M_Columns)));
                    if (pButton)
                    {
                        pButton->setPopupMode(QToolButton::InstantPopup);
                        pButton->setAutoRaise(true);
                    }
                    break;
                }

                default:
                    break;
            }
            break;
        }
        /* Machine toolbar: */
        case UIToolClass_Machine:
        {
            switch (currentMachineTool())
            {
                case UIToolType_Details:
                {
                    if (isSingleGroupSelected())
                    {
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_S_New));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_S_Add));
                        m_pToolBar->addSeparator();
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_S_Discard));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_M_StartOrShow));
                    }
                    else
                    {
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_New));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Add));
                        m_pToolBar->addSeparator();
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    }
                    break;
                }
                case UIToolType_Snapshots:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Take));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Delete));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Restore));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_T_Properties));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Clone));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_Logs:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Save));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Find));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Filter));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Bookmark));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Options));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Refresh));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_Performance:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Performance_S_Export));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_Error:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Add));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Refresh));
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // Actually Qt should do that itself but by some unknown reason it sometimes
    // forget to update toolbar after changing its actions on Cocoa platform.
    connect(actionPool()->action(UIActionIndexMN_M_Machine_S_New), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
    connect(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
    connect(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
    connect(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow), &UIAction::changed,
            m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));

    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    m_pToolBar->updateLayout();
#endif /* VBOX_WS_MAC */
}

void UIVirtualBoxManagerWidget::saveSettings()
{
    /* Save toolbar visibility: */
    {
        gEDataManager->setSelectorWindowToolBarVisible(!m_pToolBar->isHidden());
        gEDataManager->setSelectorWindowToolBarTextVisible(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);
    }

    /* Save splitter handle position: */
    {
        gEDataManager->setSelectorWindowSplitterHints(m_pSplitter->sizes());
    }
}

void UIVirtualBoxManagerWidget::cleanupConnections()
{
    /* Tool-bar connections: */
    disconnect(m_pToolBar, &UIToolBar::customContextMenuRequested,
               this, &UIVirtualBoxManagerWidget::sltHandleToolBarContextMenuRequest);
    disconnect(m_pToolBar, &UIToolBar::sigResized,
               this, &UIVirtualBoxManagerWidget::sltHandleToolBarResize);

    /* Chooser-pane connections: */
    disconnect(this, &UIVirtualBoxManagerWidget::sigToolBarHeightChange,
               m_pPaneChooser, &UIChooser::setGlobalItemHeightHint);
    disconnect(m_pPaneChooser, &UIChooser::sigSelectionChanged,
               this, &UIVirtualBoxManagerWidget::sltHandleChooserPaneIndexChange);
    disconnect(m_pPaneChooser, &UIChooser::sigSelectionInvalidated,
               this, &UIVirtualBoxManagerWidget::sltHandleChooserPaneSelectionInvalidated);
    disconnect(m_pPaneChooser, &UIChooser::sigToggleStarted,
               m_pPaneToolsMachine, &UIToolPaneMachine::sigToggleStarted);
    disconnect(m_pPaneChooser, &UIChooser::sigToggleFinished,
               m_pPaneToolsMachine, &UIToolPaneMachine::sigToggleFinished);
    disconnect(m_pPaneChooser, &UIChooser::sigGroupSavingStateChanged,
               this, &UIVirtualBoxManagerWidget::sigGroupSavingStateChanged);
    disconnect(m_pPaneChooser, &UIChooser::sigToolMenuRequested,
               this, &UIVirtualBoxManagerWidget::sltHandleToolMenuRequested);
    disconnect(m_pPaneChooser, &UIChooser::sigCloudMachineStateChange,
               this, &UIVirtualBoxManagerWidget::sltHandleCloudMachineStateChange);
    disconnect(m_pPaneChooser, &UIChooser::sigStartOrShowRequest,
               this, &UIVirtualBoxManagerWidget::sigStartOrShowRequest);
    disconnect(m_pPaneChooser, &UIChooser::sigMachineSearchWidgetVisibilityChanged,
               this, &UIVirtualBoxManagerWidget::sigMachineSearchWidgetVisibilityChanged);

    /* Details-pane connections: */
    disconnect(m_pPaneToolsMachine, &UIToolPaneMachine::sigLinkClicked,
               this, &UIVirtualBoxManagerWidget::sigMachineSettingsLinkClicked);

    /* Tools-pane connections: */
    disconnect(m_pPaneTools, &UITools::sigSelectionChanged,
               this, &UIVirtualBoxManagerWidget::sltHandleToolsPaneIndexChange);
}

void UIVirtualBoxManagerWidget::cleanup()
{
    /* Save settings: */
    saveSettings();

    /* Cleanup everything: */
    cleanupConnections();
}

void UIVirtualBoxManagerWidget::recacheCurrentItemInformation(bool fDontRaiseErrorPane /* = false */)
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    const bool fCurrentItemIsOk = pItem && pItem->accessible();

    /* Update machine tools restrictions: */
    QList<UIToolType> retrictedTypes;
    if (pItem && pItem->itemType() != UIVirtualMachineItemType_Local)
        retrictedTypes << UIToolType_Snapshots << UIToolType_Logs << UIToolType_Performance;
    else if (pItem && !pItem->isItemStarted())
        retrictedTypes << UIToolType_Performance;
    if (retrictedTypes.contains(m_pPaneTools->toolsType()))
        m_pPaneTools->setToolsType(UIToolType_Details);
    m_pPaneTools->setRestrictedToolTypes(retrictedTypes);
    /* Update machine tools availability: */
    m_pPaneTools->setToolsEnabled(UIToolClass_Machine, fCurrentItemIsOk);

    /* Propagate current item anyway: */
    m_pPaneToolsMachine->setCurrentItem(pItem);

    /* If current item is Ok: */
    if (fCurrentItemIsOk)
    {
        /* If Error-pane is chosen currently => open tool currently chosen in Tools-pane: */
        if (m_pPaneToolsMachine->currentTool() == UIToolType_Error)
            sltHandleToolsPaneIndexChange();

        /* Propagate current items to update the Details-pane: */
        m_pPaneToolsMachine->setItems(currentItems());
        /* Propagate current machine to update the Snapshots-pane or/and Logviewer-pane: */
        if (pItem->itemType() == UIVirtualMachineItemType_Local)
            m_pPaneToolsMachine->setMachine(pItem->toLocal()->machine());
        /* Update current cloud machine state: */
        if (pItem->itemType() == UIVirtualMachineItemType_CloudReal)
            pItem->toCloud()->updateInfoAsync(false /* delayed? */);
    }
    else
    {
        /* If we were not asked separately: */
        if (!fDontRaiseErrorPane)
        {
            /* Make sure Error pane raised: */
            m_pPaneToolsMachine->openTool(UIToolType_Error);

            /* Propagate last access error to update the Error-pane (if machine selected but inaccessible): */
            if (pItem)
                m_pPaneToolsMachine->setErrorDetails(pItem->accessError());
        }

        /* Propagate current items to update the Details-pane (in any case): */
        m_pPaneToolsMachine->setItems(currentItems());
        /* Propagate current machine to update the Snapshots-pane or/and Logviewer-pane (in any case): */
        m_pPaneToolsMachine->setMachine(CMachine());
    }
}
