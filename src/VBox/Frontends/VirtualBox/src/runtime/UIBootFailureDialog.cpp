/* $Id$ */
/** @file
 * VBox Qt GUI - UIBootTimeErrorDialog class implementation.
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
#include <QAction>
#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QPushButton>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIToolButton.h"
#include "QIRichTextLabel.h"
#include "UIBootFailureDialog.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"

/* COM includes: */
#include "CMediumAttachment.h"
#include "CStorageController.h"

UIBootFailureDialog::UIBootFailureDialog(QWidget *pParent, const CMachine &comMachine)
    :QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_pParent(pParent)
    , m_pCentralWidget(0)
    , m_pMainLayout(0)
    , m_pButtonBox(0)
    , m_pCloseButton(0)
    , m_pResetButton(0)
    , m_pLabel(0)
    , m_pBootImageSelector(0)
    , m_pBootImageLabel(0)
    , m_pIconLabel(0)
    , m_pSuppressDialogCheckBox(0)
    , m_comMachine(comMachine)
{
    configure();
}

UIBootFailureDialog::~UIBootFailureDialog()
{
    if (m_pSuppressDialogCheckBox && m_pSuppressDialogCheckBox->isChecked())
    {
        QStringList suppressedMessageList = gEDataManager->suppressedMessages();
        suppressedMessageList << gpConverter->toInternalString(UIExtraDataMetaDefs::DialogType_BootFailure);
        gEDataManager->setSuppressedMessages(suppressedMessageList);
    }
}

QString UIBootFailureDialog::bootMediumPath() const
{
    if (!m_pBootImageSelector)
        return QString();
    return m_pBootImageSelector->path();
}

void UIBootFailureDialog::retranslateUi()
{
    if (m_pCloseButton)
    {
        m_pCloseButton->setText(tr("&Close"));
        m_pCloseButton->setToolTip(tr("Closes this dialog without resetting the guest or mounting a medium"));
    }
    if (m_pResetButton)
    {
        m_pResetButton->setText(tr("&Reset VM"));
        m_pResetButton->setToolTip(tr("Mounts the selected ISO if any and restarts the vm"));
    }

    if (m_pLabel)
        m_pLabel->setText(tr("The virtual machine failed to boot. That might be caused by a missing operating system "
                             "or misconfigured boot order. Mounting an operation install DVD might solve this problem. "
                             "Selecting an ISO file will attempt to mount it after the dialog is closed."));

    if (m_pBootImageLabel)
        m_pBootImageLabel->setText(tr("Boot DVD:"));
    if (m_pSuppressDialogCheckBox)
        m_pSuppressDialogCheckBox->setText(tr("Do not show this dialog again"));
}

void UIBootFailureDialog::configure()
{
    setWindowIcon(UIIconPool::iconSetFull(":/media_manager_32px.png", ":/media_manager_16px.png"));
    setTitle();
    prepareWidgets();
    prepareConnections();
}

void UIBootFailureDialog::prepareConnections()
{
    if (m_pCloseButton)
        connect(m_pCloseButton, &QPushButton::clicked, this, &UIBootFailureDialog::sltCancel);
    if (m_pResetButton)
        connect(m_pResetButton, &QPushButton::clicked, this, &UIBootFailureDialog::sltReset);
}

void UIBootFailureDialog::prepareWidgets()
{
    m_pCentralWidget = new QWidget;
    if (!m_pCentralWidget)
        return;
    setCentralWidget(m_pCentralWidget);

    m_pMainLayout = new QVBoxLayout;
    m_pCentralWidget->setLayout(m_pMainLayout);

    if (!m_pMainLayout || !menuBar())
        return;

    QHBoxLayout *pTopLayout = new QHBoxLayout;
    pTopLayout->setContentsMargins(0, 0, 0, 0);

    m_pIconLabel = new QLabel;
    if (m_pIconLabel)
    {
        m_pIconLabel->setPixmap(iconPixmap());
        m_pIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        pTopLayout->addWidget(m_pIconLabel, Qt::AlignTop | Qt::AlignCenter);
    }

    m_pLabel = new QIRichTextLabel;
    if (m_pLabel)
        pTopLayout->addWidget(m_pLabel);

    QHBoxLayout *pSelectorLayout = new QHBoxLayout;
    pSelectorLayout->setContentsMargins(0, 0, 0, 0);
    m_pBootImageLabel = new QLabel;

    if (m_pBootImageLabel)
    {
        pSelectorLayout->addWidget(m_pBootImageLabel);
        m_pBootImageLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    }

    m_pBootImageSelector = new UIFilePathSelector;
    if (m_pBootImageSelector)
    {
        m_pBootImageSelector->setMode(UIFilePathSelector::Mode_File_Open);
        m_pBootImageSelector->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        m_pBootImageSelector->setFileDialogFilters("ISO Images(*.iso *.ISO)");
        m_pBootImageSelector->setResetEnabled(false);
        m_pBootImageSelector->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
        m_pBootImageSelector->setRecentMediaListType(UIMediumDeviceType_DVD);
        if (m_pBootImageLabel)
            m_pBootImageLabel->setBuddy(m_pBootImageSelector);
        pSelectorLayout->addWidget(m_pBootImageSelector);
        connect(m_pBootImageSelector, &UIFilePathSelector::pathChanged,
                this, &UIBootFailureDialog::sltFileSelectorPathChanged);
    }

    m_pMainLayout->addLayout(pTopLayout);
    m_pMainLayout->addLayout(pSelectorLayout);

    m_pSuppressDialogCheckBox = new QCheckBox;
    if (m_pSuppressDialogCheckBox)
        m_pMainLayout->addWidget(m_pSuppressDialogCheckBox);

    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pCloseButton = m_pButtonBox->addButton(QString(), QDialogButtonBox::RejectRole);
        m_pResetButton = m_pButtonBox->addButton(QString(), QDialogButtonBox::ActionRole);
        m_pCloseButton->setShortcut(Qt::Key_Escape);

        m_pMainLayout->addWidget(m_pButtonBox);
    }

    m_pMainLayout->addStretch();
    retranslateUi();
}

void UIBootFailureDialog::sltCancel()
{
    done(static_cast<int>(ReturnCode_Close));
}

void UIBootFailureDialog::sltReset()
{
    done(static_cast<int>(ReturnCode_Reset));
}

void UIBootFailureDialog::showEvent(QShowEvent *pEvent)
{
    if (m_pParent)
        UIDesktopWidgetWatchdog::centerWidget(this, m_pParent, false);
    QIWithRetranslateUI<QIMainDialog>::showEvent(pEvent);

}

void UIBootFailureDialog::setTitle()
{
}

void UIBootFailureDialog::sltFileSelectorPathChanged(const QString &strPath)
{
    Q_UNUSED(strPath);
    bool fISOValid = checkISOImage();
    if (m_pBootImageSelector)
    {
        m_pBootImageSelector->mark(!fISOValid, tr("The selected path is invalid."));
    }
    if (m_pResetButton)
        m_pResetButton->setEnabled(fISOValid);
}

QPixmap UIBootFailureDialog::iconPixmap()
{
    QIcon icon = UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_MessageBoxWarning);
    if (icon.isNull())
        return QPixmap();
    int iSize = QApplication::style()->pixelMetric(QStyle::PM_MessageBoxIconSize, 0, 0);
    return icon.pixmap(iSize, iSize);
}

bool UIBootFailureDialog::checkISOImage() const
{
    AssertReturn(m_pBootImageSelector, true);
    if (m_pBootImageSelector->path().isEmpty())
        return true;
    QFileInfo fileInfo(m_pBootImageSelector->path());
    if (!fileInfo.exists() || !fileInfo.isReadable())
        return false;
    return true;
}
