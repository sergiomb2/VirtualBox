/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewerDialog class implementation.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
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
#if defined(RT_OS_SOLARIS)
# include <QFontDatabase>
#endif
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIVMLogViewerDialog.h"
#include "UIVMLogViewerWidget.h"
#include "UICommon.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif


/*********************************************************************************************************************************
*   Class UIVMLogViewerDialogFactory implementation.                                                                             *
*********************************************************************************************************************************/

UIVMLogViewerDialogFactory::UIVMLogViewerDialogFactory(UIActionPool *pActionPool /* = 0 */,
                                                       const CMachine &comMachine /* = CMachine() */)
    : m_pActionPool(pActionPool)
    , m_comMachine(comMachine)
{
}

void UIVMLogViewerDialogFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UIVMLogViewerDialog(pCenterWidget, m_pActionPool, m_comMachine);
}


/*********************************************************************************************************************************
*   Class UIVMLogViewerDialog implementation.                                                                                    *
*********************************************************************************************************************************/

UIVMLogViewerDialog::UIVMLogViewerDialog(QWidget *pCenterWidget, UIActionPool *pActionPool, const CMachine &comMachine)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
    , m_comMachine(comMachine)
    , m_iGeometrySaveTimerId(-1)
{
}

UIVMLogViewerDialog::~UIVMLogViewerDialog()
{
}

void UIVMLogViewerDialog::setSelectedVMListItems(const QList<UIVirtualMachineItem*> &items)
{
    Q_UNUSED(items);
    UIVMLogViewerWidget *pLogViewerWidget = qobject_cast<UIVMLogViewerWidget*>(widget());
    if (pLogViewerWidget)
        pLogViewerWidget->setSelectedVMListItems(items);
}

void UIVMLogViewerDialog::addSelectedVMListItems(const QList<UIVirtualMachineItem*> &items)
{
    Q_UNUSED(items);
    UIVMLogViewerWidget *pLogViewerWidget = qobject_cast<UIVMLogViewerWidget*>(widget());
    if (pLogViewerWidget)
        pLogViewerWidget->addSelectedVMListItems(items);
}

void UIVMLogViewerDialog::retranslateUi()
{
    /* Translate window title: */
    if (!m_comMachine.isNull())
        setWindowTitle(tr("%1 - Log Viewer").arg(m_comMachine.GetName()));
    else
        setWindowTitle(UIVMLogViewerWidget::tr("Log Viewer"));

    /* Translate buttons: */
    button(ButtonType_Close)->setText(UIVMLogViewerWidget::tr("Close"));
    button(ButtonType_Help)->setText(UIVMLogViewerWidget::tr("Help"));
    button(ButtonType_Close)->setStatusTip(UIVMLogViewerWidget::tr("Close dialog"));
    button(ButtonType_Help)->setStatusTip(UIVMLogViewerWidget::tr("Show dialog help"));
    button(ButtonType_Close)->setShortcut(Qt::Key_Escape);
    button(ButtonType_Help)->setShortcut(QKeySequence::HelpContents);
    button(ButtonType_Close)->setToolTip(UIVMLogViewerWidget::tr("Close Window (%1)").arg(button(ButtonType_Close)->shortcut().toString()));
    button(ButtonType_Help)->setToolTip(UIVMLogViewerWidget::tr("Show Help (%1)").arg(button(ButtonType_Help)->shortcut().toString()));
}

bool UIVMLogViewerDialog::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        case QEvent::Move:
        {
            if (m_iGeometrySaveTimerId != -1)
                killTimer(m_iGeometrySaveTimerId);
            m_iGeometrySaveTimerId = startTimer(300);
            break;
        }
        case QEvent::Timer:
        {
            QTimerEvent *pTimerEvent = static_cast<QTimerEvent*>(pEvent);
            if (pTimerEvent->timerId() == m_iGeometrySaveTimerId)
            {
                killTimer(m_iGeometrySaveTimerId);
                m_iGeometrySaveTimerId = -1;
                saveDialogGeometry();
            }
            break;
        }
        default:
            break;
    }
    return QIWithRetranslateUI<QIManagerDialog>::event(pEvent);
}

void UIVMLogViewerDialog::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/vm_show_logs_32px.png", ":/vm_show_logs_16px.png"));
}

void UIVMLogViewerDialog::configureCentralWidget()
{
    /* Create widget: */
    UIVMLogViewerWidget *pWidget = new UIVMLogViewerWidget(EmbedTo_Dialog, m_pActionPool, true /* show toolbar */, m_comMachine, this);
    /* Release the CMachine reference as we don't need it anymore. Doing it during dtor causes problems since xcom might be gone already: */
    m_comMachine.detach();
    if (pWidget)
    {
        /* Configure widget: */
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        connect(pWidget, &UIVMLogViewerWidget::sigSetCloseButtonShortCut,
                this, &UIVMLogViewerDialog::sltSetCloseButtonShortCut);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UIVMLogViewerDialog::finalize()
{
    /* Apply language settings: */
    retranslateUi();
    manageEscapeShortCut();
    loadDialogGeometry();
}

void UIVMLogViewerDialog::loadDialogGeometry()
{

    const QRect availableGeo = gpDesktop->availableGeometry(this);
    int iDefaultWidth = availableGeo.width() / 2;
    int iDefaultHeight = availableGeo.height() * 3 / 4;
    /* Try obtain the default width of the current logviewer: */
    const UIVMLogViewerWidget *pWidget = qobject_cast<const UIVMLogViewerWidget*>(widget());
    if (pWidget)
    {
        const int iWidth = pWidget->defaultLogPageWidth();
        if (iWidth != 0)
            iDefaultWidth = iWidth;
    }
    QRect defaultGeo(0, 0, iDefaultWidth, iDefaultHeight);

    /* Load geometry from extradata: */
    const QRect geo = gEDataManager->logWindowGeometry(this, centerWidget(), defaultGeo);
    LogRel2(("GUI: UIVMLogViewerDialog: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    restoreGeometry(geo);
}

void UIVMLogViewerDialog::saveDialogGeometry()
{
    /* Save geometry to extradata: */
    const QRect geo = currentGeometry();
    LogRel2(("GUI: UIVMLogViewerDialog: Saving geometry as: Origin=%dx%d, Size=%dx%d\n",
             geo.x(), geo.y(), geo.width(), geo.height()));
    gEDataManager->setLogWindowGeometry(geo, isCurrentlyMaximized());
}

bool UIVMLogViewerDialog::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIVMLogViewerDialog::sltSetCloseButtonShortCut(QKeySequence shortcut)
{
    if (!closeEmitted() &&  button(ButtonType_Close))
        button(ButtonType_Close)->setShortcut(shortcut);
}

void UIVMLogViewerDialog::manageEscapeShortCut()
{
    UIVMLogViewerWidget *pWidget = qobject_cast<UIVMLogViewerWidget*>(widget());
    if (!pWidget)
        return;
    pWidget->manageEscapeShortCut();
}
