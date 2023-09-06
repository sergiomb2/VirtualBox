﻿/* $Id$ */
/** @file
 * VBox Qt GUI - UIAdvancedSettingsDialog class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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
#include <QCloseEvent>
#include <QGridLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QVariant>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIAdvancedSettingsDialog.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIPopupCenter.h"
#include "UISettingsPage.h"
#include "UISettingsPageValidator.h"
#include "UISettingsSelector.h"
#include "UISettingsSerializer.h"
#include "UISettingsWarningPane.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils.h"
#endif

#ifdef VBOX_WS_MAC
# define VBOX_GUI_WITH_TOOLBAR_SETTINGS
#endif

#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
# include "QIToolBar.h"
#endif


/** QScrollArea extension to be used for
  * advanced settings dialog. The idea is to make
  * vertical scroll-bar always visible, keeping
  * horizontal scroll-bar always hidden. */
class UIVerticalScrollArea : public QScrollArea
{
    Q_OBJECT;

public:

    /** Constructs vertical scroll-area passing @a pParent to the base-class. */
    UIVerticalScrollArea(QWidget *pParent);

protected:

    /** Holds the minimum widget size. */
    virtual QSize minimumSizeHint() const RT_OVERRIDE;
};


/*********************************************************************************************************************************
*   Class UIVerticalScrollArea implementation.                                                                                   *
*********************************************************************************************************************************/

UIVerticalScrollArea::UIVerticalScrollArea(QWidget *pParent)
    : QScrollArea(pParent)
{
    /* Make vertical scroll-bar always visible. */
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

QSize UIVerticalScrollArea::minimumSizeHint() const
{
    /* To make horizontal scroll-bar always hidden we'll
     * have to make sure minimum size-hint updated accordingly. */
    const int iMinWidth = viewportSizeHint().width()
                        + verticalScrollBar()->sizeHint().width()
                        + frameWidth() * 2;
    const int iMinHeight = qMax(QScrollArea::minimumSizeHint().height(),
                                (int)(iMinWidth / 1.6));
    return QSize(iMinWidth, iMinHeight);
}


/*********************************************************************************************************************************
*   Class UIAdvancedSettingsDialog implementation.                                                                               *
*********************************************************************************************************************************/

UIAdvancedSettingsDialog::UIAdvancedSettingsDialog(QWidget *pParent,
                                                   const QString &strCategory,
                                                   const QString &strControl)
    : QIWithRetranslateUI<QMainWindow>(pParent)
    , m_strCategory(strCategory)
    , m_strControl(strControl)
    , m_pSelector(0)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_pSerializeProcess(0)
    , m_fPolished(false)
    , m_fSerializationIsInProgress(false)
    , m_fSerializationClean(false)
    , m_fClosed(false)
    , m_pStatusBar(0)
    , m_pProcessBar(0)
    , m_pWarningPane(0)
    , m_fValid(true)
    , m_fSilent(true)
    , m_pLayoutMain(0)
    , m_pScrollArea(0)
    , m_pScrollViewport(0)
    , m_pButtonBox(0)
{
    prepare();
}

UIAdvancedSettingsDialog::~UIAdvancedSettingsDialog()
{
    cleanup();
}

void UIAdvancedSettingsDialog::accept()
{
    /* Save data: */
    save();

    /* Close if there is no ongoing serialization: */
    if (!isSerializationInProgress())
        close();
}

void UIAdvancedSettingsDialog::reject()
{
    /* Close if there is no ongoing serialization: */
    if (!isSerializationInProgress())
        close();
}

void UIAdvancedSettingsDialog::sltCategoryChanged(int cId)
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle(title());
#endif

    /* Let's calculate scroll-bar shift: */
    int iShift = 0;
    /* We'll have to take upper content's margin into account: */
    int iL, iT, iR, iB;
    m_pScrollViewport->layout()->getContentsMargins(&iL, &iT, &iR, &iB);
    iShift -= iT;
    /* And actual page position according to parent: */
    const QPoint pnt = m_frames.value(cId)->pos();
    iShift += pnt.y();
    /* Make sure corresponding page is visible: */
    m_pScrollArea->verticalScrollBar()->setValue(iShift);

#ifndef VBOX_WS_MAC
    uiCommon().setHelpKeyword(m_pButtonBox->button(QDialogButtonBox::Help), m_pageHelpKeywords.value(cId));
#endif
}

void UIAdvancedSettingsDialog::sltHandleSerializationStarted()
{
    m_pProcessBar->setValue(0);
    m_pStatusBar->setCurrentWidget(m_pProcessBar);
}

void UIAdvancedSettingsDialog::sltHandleSerializationProgressChange(int iValue)
{
    m_pProcessBar->setValue(iValue);
    if (m_pProcessBar->value() == m_pProcessBar->maximum())
    {
        if (!m_fValid || !m_fSilent)
            m_pStatusBar->setCurrentWidget(m_pWarningPane);
        else
            m_pStatusBar->setCurrentIndex(0);
    }
}

void UIAdvancedSettingsDialog::sltHandleSerializationFinished()
{
    /* Delete serializer if exists: */
    delete m_pSerializeProcess;
    m_pSerializeProcess = 0;

    /* Mark serialization finished: */
    m_fSerializationIsInProgress = false;
}

void UIAdvancedSettingsDialog::retranslateUi()
{
    /* Translate warning-pane stuff: */
    m_pWarningPane->setWarningLabelText(tr("Invalid settings detected"));

    /* Translate page-frames: */
    foreach (int cId, m_frames.keys())
        m_frames.value(cId)->setName(m_pSelector->itemText(cId));

    /* Retranslate all validators: */
    foreach (UISettingsPageValidator *pValidator, findChildren<UISettingsPageValidator*>())
        pValidator->setTitlePrefix(m_pSelector->itemTextByPage(pValidator->page()));
    revalidate();
}

void UIAdvancedSettingsDialog::showEvent(QShowEvent *pEvent)
{
    /* Polish stuff: */
    if (!m_fPolished)
    {
        m_fPolished = true;
        polishEvent();
    }

    /* Call to base-class: */
    QIWithRetranslateUI<QMainWindow>::showEvent(pEvent);
}

void UIAdvancedSettingsDialog::polishEvent()
{
    /* Resize to minimum size: */
    resize(minimumSizeHint());

    /* Choose page/tab finally: */
    choosePageAndTab();

    /* Explicit centering according to our parent: */
    gpDesktop->centerWidget(this, parentWidget(), false);
}

void UIAdvancedSettingsDialog::closeEvent(QCloseEvent *pEvent)
{
    /* Ignore event initially: */
    pEvent->ignore();

    /* Check whether serialization was clean (save)
     * or there are no unsaved settings to be lost (cancel): */
    if (   m_fSerializationClean
        || !isSettingsChanged()
        || msgCenter().confirmSettingsDiscarding(this))
    {
        /* Tell the listener to close us (once): */
        if (!m_fClosed)
        {
            m_fClosed = true;
            emit sigClose();
        }
    }
}

void UIAdvancedSettingsDialog::choosePageAndTab(bool fKeepPreviousByDefault /* = false */)
{
    /* Setup settings window: */
    if (!m_strCategory.isNull())
    {
        m_pSelector->selectByLink(m_strCategory);
        /* Search for a widget with the given name: */
        if (!m_strControl.isNull())
        {
            if (QWidget *pWidget = m_pScrollViewport->findChild<QWidget*>(m_strControl))
            {
                QList<QWidget*> parents;
                QWidget *pParentWidget = pWidget;
                while ((pParentWidget = pParentWidget->parentWidget()) != 0)
                {
                    if (QTabWidget *pTabWidget = qobject_cast<QTabWidget*>(pParentWidget))
                    {
                        // WORKAROUND:
                        // The tab contents widget is two steps down
                        // (QTabWidget -> QStackedWidget -> QWidget).
                        QWidget *pTabPage = parents[parents.count() - 1];
                        if (pTabPage)
                            pTabPage = parents[parents.count() - 2];
                        if (pTabPage)
                            pTabWidget->setCurrentWidget(pTabPage);
                    }
                    parents.append(pParentWidget);
                }
                pWidget->setFocus();
            }
        }
    }
    /* First item as default (if previous is not guarded): */
    else if (!fKeepPreviousByDefault)
        m_pSelector->selectById(1);
}

void UIAdvancedSettingsDialog::loadData(QVariant &data)
{
    /* Mark serialization started: */
    m_fSerializationIsInProgress = true;

    /* Create settings loader: */
    m_pSerializeProcess = new UISettingsSerializer(this, UISettingsSerializer::Load,
                                                   data, m_pSelector->settingPages());
    if (m_pSerializeProcess)
    {
        /* Configure settings loader: */
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessStarted,
                this, &UIAdvancedSettingsDialog::sltHandleSerializationStarted);
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessProgressChanged,
                this, &UIAdvancedSettingsDialog::sltHandleSerializationProgressChange);
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessFinished,
                this, &UIAdvancedSettingsDialog::sltHandleSerializationFinished);

        /* Raise current page priority: */
        m_pSerializeProcess->raisePriorityOfPage(m_pSelector->currentId());

        /* Start settings loader: */
        m_pSerializeProcess->start();

        /* Upload data finally: */
        data = m_pSerializeProcess->data();
    }
}

void UIAdvancedSettingsDialog::saveData(QVariant &data)
{
    /* Mark serialization started: */
    m_fSerializationIsInProgress = true;

    /* Create the 'settings saver': */
    QPointer<UISettingsSerializerProgress> pDlgSerializeProgress =
        new UISettingsSerializerProgress(this, UISettingsSerializer::Save,
                                         data, m_pSelector->settingPages());
    if (pDlgSerializeProgress)
    {
        /* Make the 'settings saver' temporary parent for all sub-dialogs: */
        windowManager().registerNewParent(pDlgSerializeProgress, windowManager().realParentWindow(this));

        /* Execute the 'settings saver': */
        pDlgSerializeProgress->exec();

        /* Any modal dialog can be destroyed in own event-loop
         * as a part of application termination procedure..
         * We have to check if the dialog still valid. */
        if (pDlgSerializeProgress)
        {
            /* Remember whether the serialization was clean: */
            m_fSerializationClean = pDlgSerializeProgress->isClean();

            /* Upload 'settings saver' data: */
            data = pDlgSerializeProgress->data();

            /* Delete the 'settings saver': */
            delete pDlgSerializeProgress;
        }
    }
}

void UIAdvancedSettingsDialog::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    /* Make sure something changed: */
    if (m_enmConfigurationAccessLevel == enmConfigurationAccessLevel)
        return;

    /* Apply new configuration access level: */
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;

    /* And propagate it to settings-page(s): */
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
        pPage->setConfigurationAccessLevel(configurationAccessLevel());
}

void UIAdvancedSettingsDialog::addItem(const QString &strBigIcon,
                                       const QString &strMediumIcon,
                                       const QString &strSmallIcon,
                                       int cId,
                                       const QString &strLink,
                                       UISettingsPage *pSettingsPage /* = 0 */,
                                       int iParentId /* = -1 */)
{
    /* Add new selector item: */
    if (m_pSelector->addItem(strBigIcon, strMediumIcon, strSmallIcon,
                             cId, strLink, pSettingsPage, iParentId))
    {
        /* Create frame with page inside: */
        UISettingsPageFrame *pFrame = new UISettingsPageFrame(pSettingsPage, m_pScrollViewport);
        if (pFrame)
        {
            /* Add frame to scroll-viewport: */
            m_pScrollViewport->layout()->addWidget(pFrame);

            /* Remember page-frame for referencing: */
            m_frames[cId] = pFrame;
        }
    }

    /* Assign validator if necessary: */
    if (pSettingsPage)
    {
        pSettingsPage->setId(cId);

        /* Create validator: */
        UISettingsPageValidator *pValidator = new UISettingsPageValidator(this, pSettingsPage);
        connect(pValidator, &UISettingsPageValidator::sigValidityChanged, this, &UIAdvancedSettingsDialog::sltHandleValidityChange);
        pSettingsPage->setValidator(pValidator);
        m_pWarningPane->registerValidator(pValidator);

        /* Update navigation (tab-order): */
        pSettingsPage->setOrderAfter(m_pSelector->widget());
    }
}

void UIAdvancedSettingsDialog::addPageHelpKeyword(int iPageType, const QString &strHelpKeyword)
{
    m_pageHelpKeywords[iPageType] = strHelpKeyword;
}

void UIAdvancedSettingsDialog::revalidate()
{
    /* Perform dialog revalidation: */
    m_fValid = true;
    m_fSilent = true;

    /* Enumerating all the validators we have: */
    foreach (UISettingsPageValidator *pValidator, findChildren<UISettingsPageValidator*>())
    {
        /* Is current validator have something to say? */
        if (!pValidator->lastMessage().isEmpty())
        {
            /* What page is it related to? */
            UISettingsPage *pFailedSettingsPage = pValidator->page();
            LogRelFlow(("Settings Dialog:  Dialog validation FAILED: Page *%s*\n",
                        pFailedSettingsPage->internalName().toUtf8().constData()));

            /* Show error first: */
            if (!pValidator->isValid())
                m_fValid = false;
            /* Show warning if message is not an error: */
            else
                m_fSilent = false;

            /* Stop dialog revalidation on first error/warning: */
            break;
        }
    }

    /* Update warning-pane visibility: */
    m_pWarningPane->setWarningLabelVisible(!m_fValid || !m_fSilent);

    /* Make sure warning-pane visible if necessary: */
    if ((!m_fValid || !m_fSilent) && m_pStatusBar->currentIndex() == 0)
        m_pStatusBar->setCurrentWidget(m_pWarningPane);
    /* Make sure empty-pane visible otherwise: */
    else if (m_fValid && m_fSilent && m_pStatusBar->currentWidget() == m_pWarningPane)
        m_pStatusBar->setCurrentIndex(0);

    /* Lock/unlock settings-page OK button according global validity status: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_fValid);
}

bool UIAdvancedSettingsDialog::isSettingsChanged()
{
    bool fIsSettingsChanged = false;
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
    {
        pPage->putToCache();
        if (!fIsSettingsChanged && pPage->changed())
            fIsSettingsChanged = true;
    }
    return fIsSettingsChanged;
}

void UIAdvancedSettingsDialog::sltHandleValidityChange(UISettingsPageValidator *pValidator)
{
    /* Determine which settings-page had called for revalidation: */
    if (UISettingsPage *pSettingsPage = pValidator->page())
    {
        /* Determine settings-page name: */
        const QString strPageName(pSettingsPage->internalName());

        LogRelFlow(("Settings Dialog: %s Page: Revalidation in progress..\n",
                    strPageName.toUtf8().constData()));

        /* Perform page revalidation: */
        pValidator->revalidate();
        /* Perform inter-page recorrelation: */
        recorrelate(pSettingsPage);
        /* Perform dialog revalidation: */
        revalidate();

        LogRelFlow(("Settings Dialog: %s Page: Revalidation complete.\n",
                    strPageName.toUtf8().constData()));
    }
}

void UIAdvancedSettingsDialog::sltHandleWarningPaneHovered(UISettingsPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon hovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Show corresponding popup: */
    if (!m_fValid || !m_fSilent)
        popupCenter().popup(m_pScrollArea, "SettingsDialogWarning",
                            pValidator->lastMessage());
}

void UIAdvancedSettingsDialog::sltHandleWarningPaneUnhovered(UISettingsPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon unhovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Recall corresponding popup: */
    popupCenter().recall(m_pScrollArea, "SettingsDialogWarning");
}

void UIAdvancedSettingsDialog::prepare()
{
    /* Prepare central-widget: */
    setCentralWidget(new QWidget);
    if (centralWidget())
    {
        /* Prepare main layout: */
        m_pLayoutMain = new QGridLayout(centralWidget());
        if (m_pLayoutMain)
        {
            /* Prepare widgets: */
            prepareSelector();
            prepareScrollArea();
            prepareButtonBox();
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIAdvancedSettingsDialog::prepareSelector()
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* Prepare modern tool-bar selector: */
    m_pSelector = new UISettingsSelectorToolBar(this);
    if (m_pSelector)
    {
        static_cast<QIToolBar*>(m_pSelector->widget())->enableMacToolbar();
        addToolBar(qobject_cast<QToolBar*>(m_pSelector->widget()));
    }

    /* No title in this mode, we change the title of the window: */
    m_pLayoutMain->setColumnMinimumWidth(0, 0);
    m_pLayoutMain->setHorizontalSpacing(0);

#else /* !VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Prepare classical tree-view selector: */
    m_pSelector = new UISettingsSelectorTreeWidget(centralWidget());
    if (m_pSelector)
    {
        m_pLayoutMain->addWidget(m_pSelector->widget(), 0, 0, 2, 1);
        m_pSelector->widget()->setFocus();
    }
#endif /* !VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Configure selector created above: */
    if (m_pSelector)
        connect(m_pSelector, &UISettingsSelectorTreeWidget::sigCategoryChanged,
                this, &UIAdvancedSettingsDialog::sltCategoryChanged);
}

void UIAdvancedSettingsDialog::prepareScrollArea()
{
    /* Prepare scroll-area: */
    m_pScrollArea = new UIVerticalScrollArea(centralWidget());
    if (m_pScrollArea)
    {
        /* Configure popup-stack: */
        popupCenter().setPopupStackOrientation(m_pScrollArea, UIPopupStackOrientation_Bottom);

        m_pScrollArea->setWidgetResizable(true);
        m_pScrollArea->setFrameShape(QFrame::NoFrame);

        /* Prepare scroll-viewport: */
        m_pScrollViewport = new QWidget(m_pScrollArea);
        if (m_pScrollViewport)
        {
            QVBoxLayout *pLayout = new QVBoxLayout(m_pScrollViewport);
            if (pLayout)
            {
                pLayout->setAlignment(Qt::AlignTop);
                int iL, iT, iR, iB;
                pLayout->getContentsMargins(&iL, &iT, &iR, &iB);
                pLayout->setContentsMargins(0, 0, iR, 0);
                int iSpacing = pLayout->spacing();
                pLayout->setSpacing(2 * iSpacing);
            }
            m_pScrollArea->setWidget(m_pScrollViewport);
        }

        /* Add scroll-area into main layout: */
        m_pLayoutMain->addWidget(m_pScrollArea, 1, 1);
    }
}

void UIAdvancedSettingsDialog::prepareButtonBox()
{
    /* Prepare button-box: */
    m_pButtonBox = new QIDialogButtonBox(centralWidget());
    if (m_pButtonBox)
    {
#ifndef VBOX_WS_MAC
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::NoButton | QDialogButtonBox::Help);
        m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);
#else
        // WORKAROUND:
        // No Help button on macOS for now, conflict with old Qt.
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::NoButton);
#endif
        m_pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(Qt::Key_Return);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIAdvancedSettingsDialog::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIAdvancedSettingsDialog::accept);
#ifndef VBOX_WS_MAC
        connect(m_pButtonBox->button(QDialogButtonBox::Help), &QAbstractButton::pressed,
                m_pButtonBox, &QIDialogButtonBox::sltHandleHelpRequest);
#endif

        /* Prepare status-bar: */
        m_pStatusBar = new QStackedWidget(m_pButtonBox);
        if (m_pStatusBar)
        {
            /* Add empty widget: */
            m_pStatusBar->addWidget(new QWidget);

            /* Prepare process-bar: */
            m_pProcessBar = new QProgressBar(m_pStatusBar);
            if (m_pProcessBar)
            {
                m_pProcessBar->setMinimum(0);
                m_pProcessBar->setMaximum(100);
                m_pStatusBar->addWidget(m_pProcessBar);
            }

            /* Prepare warning-pane: */
            m_pWarningPane = new UISettingsWarningPane(m_pStatusBar);
            if (m_pWarningPane)
            {
                connect(m_pWarningPane, &UISettingsWarningPane::sigHoverEnter,
                        this, &UIAdvancedSettingsDialog::sltHandleWarningPaneHovered);
                connect(m_pWarningPane, &UISettingsWarningPane::sigHoverLeave,
                        this, &UIAdvancedSettingsDialog::sltHandleWarningPaneUnhovered);
                m_pStatusBar->addWidget(m_pWarningPane);
            }

            /* Add status-bar to button-box: */
            m_pButtonBox->addExtraWidget(m_pStatusBar);
        }

        /* Add button-box into main layout: */
        m_pLayoutMain->addWidget(m_pButtonBox, 2, 0, 1, 2);
    }
}

void UIAdvancedSettingsDialog::cleanup()
{
    /* Delete serializer if exists: */
    delete m_pSerializeProcess;
    m_pSerializeProcess = 0;

    /* Recall popup-pane if any: */
    popupCenter().recall(m_pScrollArea, "SettingsDialogWarning");

    /* Delete selector early! */
    delete m_pSelector;
}

#include "UIAdvancedSettingsDialog.moc"
