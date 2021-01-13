/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageExpert class implementation.
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
#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpacerItem>
#include <QSpinBox>
#include <QToolBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIBaseMemoryEditor.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UINameAndSystemEditor.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageExpert.h"


UIWizardNewVMPageExpert::UIWizardNewVMPageExpert(const QString &strGroup)
    : UIWizardNewVMPage1(strGroup)
    , m_pNameAndSystemContainer(0)
    , m_pToolBox(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pToolBox = new QToolBox;
        m_pNameAndSystemContainer = new QWidget(this);
        QGridLayout *pNameContainerLayout = new QGridLayout(m_pNameAndSystemContainer);
        createNameOSTypeWidgets(pNameContainerLayout, false);
        m_pGAInstallContainer = createGAInstallWidgets();
        m_pUsernameHostnameContainer = createUserNameHostNameWidgets();

        m_pToolBox->insertItem(ExpertToolboxItems_NameAndOSType, m_pNameAndSystemContainer, "");
        m_pToolBox->insertItem(ExpertToolboxItems_Disk, createDiskWidgets(), "");
        m_pToolBox->insertItem(ExpertToolboxItems_Hardware, createHardwareWidgets(), "");
        m_pToolBox->insertItem(ExpertToolboxItems_UsernameHostname, m_pUsernameHostnameContainer, "");
        m_pToolBox->insertItem(ExpertToolboxItems_GAInstall, m_pGAInstallContainer, "");
        m_pToolBox->insertItem(ExpertToolboxItems_ProductKey, createProductKeyWidgets(), "");
        pMainLayout->addWidget(m_pToolBox);
        pMainLayout->addStretch();
        updateVirtualDiskSource();
    }

    createConnections();

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    /* Register fields: */
    registerField("name*", m_pNameAndFolderEditor, "name", SIGNAL(sigNameChanged(const QString &)));
    registerField("type", m_pSystemTypeEditor, "type", SIGNAL(sigOsTypeChanged()));
    registerField("machineFilePath", this, "machineFilePath");
    registerField("machineFolder", this, "machineFolder");
    registerField("machineBaseName", this, "machineBaseName");
    registerField("baseMemory", this, "baseMemory");
    registerField("virtualDisk", this, "virtualDisk");
    registerField("virtualDiskId", this, "virtualDiskId");
    registerField("virtualDiskLocation", this, "virtualDiskLocation");
    registerField("guestOSFamiyId", this, "guestOSFamiyId");
    registerField("ISOFilePath", this, "ISOFilePath");
    registerField("isUnattendedEnabled", this, "isUnattendedEnabled");
    registerField("startHeadless", this, "startHeadless");
    registerField("detectedOSTypeId", this, "detectedOSTypeId");
    registerField("userName", this, "userName");
    registerField("password", this, "password");
    registerField("hostname", this, "hostname");
    registerField("installGuestAdditions", this, "installGuestAdditions");
    registerField("guestAdditionsISOPath", this, "guestAdditionsISOPath");
    registerField("productKey", this, "productKey");
    registerField("VCPUCount", this, "VCPUCount");

    m_pToolBox->setStyleSheet(QString::fromUtf8("QToolBox::tab {\n"
                                                "    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,\n"
                                                "                                stop: 0 #E1E1E1, stop: 0.4 #DDDDDD,\n"
                                                "                                stop: 0.5 #D8D8D8, stop: 1.0 #D3D3D3);\n"
                                                "    border-radius: 5px;\n"
                                                "    color: black;\n"
                                                "}\n"
                                                "\n"
                                                "QToolBox::tab:selected {\n"
                                                "    font: bold;\n"
                                                "}\n"
                                                "\n"
                                                "QToolBox::tab:hover {\n"
                                                "    font: bold;\n"
                                                "}\n"
                                                "QToolBox::tab:disabled {\n"
                                                "    font: italic;\n"
                                                "    color: gray;\n"
                                                "}"));

    if (m_pEnableUnattendedInstallCheckBox)
        disableEnableUnattendedRelatedWidgets(m_pEnableUnattendedInstallCheckBox->isChecked());
}

void UIWizardNewVMPageExpert::sltNameChanged(const QString &strNewText)
{
    /* Call to base-class: */
    onNameChanged(strNewText);

    composeMachineFilePath();
    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltPathChanged(const QString &strNewPath)
{
    Q_UNUSED(strNewPath);
    composeMachineFilePath();
}

void UIWizardNewVMPageExpert::sltOsTypeChanged()
{
    /* Call to base-class: */
    onOsTypeChanged();

    /* Fetch recommended RAM value: */
    CGuestOSType type = m_pSystemTypeEditor->type();
    m_pBaseMemoryEditor->setValue(type.GetRecommendedRAM());

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged()
{
    /* Call to base-class: */
    updateVirtualDiskSource();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGetWithFileOpenDialog()
{
    /* Call to base-class: */
    getWithFileOpenDialog();
}

void UIWizardNewVMPageExpert::sltUnattendedCheckBoxToggle(bool fEnabled)
{
    disableEnableUnattendedRelatedWidgets(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltISOPathChanged(const QString &strPath)
{
    determineOSType(strPath);
    setTypeByISODetectedOSType(m_strDetectedOSTypeId);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle(bool fEnabled)
{
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setEnabled(fEnabled);
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setEnabled(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGAISOPathChanged(const QString &strPath)
{
    Q_UNUSED(strPath);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltOSFamilyTypeChanged()
{
    if (m_pToolBox)
        m_pToolBox->setItemEnabled(ExpertToolboxItems_ProductKey, isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::retranslateUi()
{
    UIWizardNewVMPage1::retranslateWidgets();
    UIWizardNewVMPage2::retranslateWidgets();
    UIWizardNewVMPage3::retranslateWidgets();

    if (m_pToolBox)
    {
        m_pToolBox->setItemText(ExpertToolboxItems_NameAndOSType, QString(UIWizardNewVM::tr("Name and operating system")));
        m_pToolBox->setItemText(ExpertToolboxItems_UsernameHostname, UIWizardNewVM::tr("Username and hostname"));
        m_pToolBox->setItemText(ExpertToolboxItems_GAInstall, UIWizardNewVM::tr("Guest additions install"));
        m_pToolBox->setItemText(ExpertToolboxItems_ProductKey, UIWizardNewVM::tr("Product key"));
        m_pToolBox->setItemText(ExpertToolboxItems_Disk, UIWizardNewVM::tr("Hard disk"));
        m_pToolBox->setItemText(ExpertToolboxItems_Hardware, UIWizardNewVM::tr("Hardware"));
    }
}

void UIWizardNewVMPageExpert::createConnections()
{
    /* Connections for Name, OS Type, and unattended install stuff: */
    if (m_pNameAndFolderEditor)
    {
        connect(m_pNameAndFolderEditor, &UINameAndSystemEditor::sigNameChanged,
                this, &UIWizardNewVMPageExpert::sltNameChanged);
        connect(m_pNameAndFolderEditor, &UINameAndSystemEditor::sigPathChanged,
                this, &UIWizardNewVMPageExpert::sltPathChanged);
    }
    if (m_pSystemTypeEditor)
    {
        connect(m_pSystemTypeEditor, &UINameAndSystemEditor::sigOsTypeChanged,
                this, &UIWizardNewVMPageExpert::sltOsTypeChanged);
        connect(m_pSystemTypeEditor, &UINameAndSystemEditor::sigOSFamilyChanged,
                this, &UIWizardNewVMPageExpert::sltOSFamilyTypeChanged);
    }
    if (m_pEnableUnattendedInstallCheckBox)
        connect(m_pEnableUnattendedInstallCheckBox, &QCheckBox::clicked,
                this, &UIWizardNewVMPageExpert::sltUnattendedCheckBoxToggle);
    if (m_pISOFilePathSelector)
        connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageExpert::sltISOPathChanged);

    /* Connections for username, password, and hostname: */
    if (m_pUserNamePasswordEditor)
        connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigSomeTextChanged,
                this, &UIWizardNewVMPageExpert::completeChanged);
    if (m_pInstallGACheckBox)
        connect(m_pInstallGACheckBox, &QCheckBox::toggled, this,
                &UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle);
    if (m_pGAISOFilePathSelector)
        connect(m_pGAISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageExpert::sltGAISOPathChanged);

    /* Connections for disk and hardware stuff: */
    if (m_pDiskSkip)
        connect(m_pDiskSkip, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskCreate)
        connect(m_pDiskCreate, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskPresent)
        connect(m_pDiskPresent, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskSelector)
        connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pVMMButton)
        connect(m_pVMMButton, &QIToolButton::clicked,
                this, &UIWizardNewVMPageExpert::sltGetWithFileOpenDialog);
}

void UIWizardNewVMPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    if (!field("type").canConvert<CGuestOSType>())
        return;

    /* Get recommended 'ram' field value: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    ULONG recommendedRam = type.GetRecommendedRAM();
    m_pBaseMemoryEditor->setValue(recommendedRam);

    /* Prepare initial disk choice: */
    if (type.GetRecommendedHDD() != 0)
    {
        if (m_pDiskCreate)
        {
            m_pDiskCreate->setFocus();
            m_pDiskCreate->setChecked(true);
        }
        m_fRecommendedNoDisk = false;
    }
    else
    {
        if (m_pDiskSkip)
        {
            m_pDiskSkip->setFocus();
            m_pDiskSkip->setChecked(true);
        }
        m_fRecommendedNoDisk = true;
    }
    if (m_pDiskSelector)
        m_pDiskSelector->setCurrentIndex(0);

    if (m_pEnableUnattendedInstallCheckBox)
        disableEnableUnattendedRelatedWidgets(m_pEnableUnattendedInstallCheckBox->isChecked());
    m_pProductKeyLabel->setEnabled(isProductKeyWidgetEnabled());
    m_pProductKeyLineEdit->setEnabled(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::cleanupPage()
{
    /* Call to base-class: */
    ensureNewVirtualDiskDeleted();
    cleanupMachineFolder();
}

void UIWizardNewVMPageExpert::markWidgets() const
{
    UIWizardNewVMPage1::markWidgets();

    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->mark(isUnattendedEnabled() && !checkGAISOFile());
}

bool UIWizardNewVMPageExpert::isComplete() const
{
    markWidgets();
    bool fIsComplete = true;
    m_pToolBox->setItemIcon(ExpertToolboxItems_NameAndOSType, QIcon());
    m_pToolBox->setItemIcon(ExpertToolboxItems_Disk, QIcon());
    m_pToolBox->setItemIcon(ExpertToolboxItems_Hardware, QIcon());
    m_pToolBox->setItemIcon(ExpertToolboxItems_UsernameHostname, QIcon());
    m_pToolBox->setItemIcon(ExpertToolboxItems_GAInstall, QIcon());
    m_pToolBox->setItemIcon(ExpertToolboxItems_ProductKey, QIcon());


    if (!UIWizardPage::isComplete())
    {
        m_pToolBox->setItemIcon(ExpertToolboxItems_NameAndOSType,
                                UIIconPool::iconSet(":/status_error_16px.png"));
        fIsComplete = false;
    }

    if (!m_pDiskSkip->isChecked() && !m_pDiskPresent->isChecked() && uiCommon().medium(m_pDiskSelector->id()).isNull())
    {
        m_pToolBox->setItemIcon(ExpertToolboxItems_Disk,
                                UIIconPool::iconSet(":/status_error_16px.png"));
        fIsComplete = false;
    }
    /* Check unattended install related stuff: */
    if (isUnattendedEnabled())
    {
        /* Check the installation medium: */
        if (!isISOFileSelectorComplete())
        {
            m_pToolBox->setItemIcon(ExpertToolboxItems_NameAndOSType,
                                    UIIconPool::iconSet(":/status_error_16px.png"));
            fIsComplete = false;
        }
        /* Check the GA installation medium: */
        if (!checkGAISOFile())
        {
            m_pToolBox->setItemIcon(ExpertToolboxItems_GAInstall,
                                    UIIconPool::iconSet(":/status_error_16px.png"));
            fIsComplete = false;
        }
        if (m_pUserNamePasswordEditor)
        {
            if (!m_pUserNamePasswordEditor->isComplete())
            {
                m_pToolBox->setItemIcon(ExpertToolboxItems_UsernameHostname,
                                        UIIconPool::iconSet(":/status_error_16px.png"));
                fIsComplete = false;
            }
        }
    }
    return fIsComplete;
}

bool UIWizardNewVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to create machine folder: */
    if (fResult)
        fResult = createMachineFolder();

    /* Try to assign boot virtual-disk: */
    if (fResult)
    {
        /* Ensure there is no virtual-disk created yet: */
        Assert(m_virtualDisk.isNull());
        if (fResult)
        {
            if (m_pDiskCreate->isChecked())
            {
                /* Show the New Virtual Hard Drive wizard if necessary: */
                fResult = getWithNewVirtualDiskWizard();
            }
        }
    }

    /* Try to create VM: */
    if (fResult)
        fResult = qobject_cast<UIWizardNewVM*>(wizard())->createVM();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

bool UIWizardNewVMPageExpert::isProductKeyWidgetEnabled() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedInstallEnabled() || !pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}

void UIWizardNewVMPageExpert::disableEnableUnattendedRelatedWidgets(bool fEnabled)
{
    if (m_pToolBox)
    {
        m_pToolBox->setItemEnabled(ExpertToolboxItems_UsernameHostname, fEnabled);
        m_pToolBox->setItemEnabled(ExpertToolboxItems_GAInstall, fEnabled);
        m_pToolBox->setItemEnabled(ExpertToolboxItems_ProductKey, fEnabled);
    }
    if (m_pISOSelectorLabel)
        m_pISOSelectorLabel->setEnabled(fEnabled);
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setEnabled(fEnabled);
    if (m_pStartHeadlessCheckBox)
        m_pStartHeadlessCheckBox->setEnabled(fEnabled);
}
