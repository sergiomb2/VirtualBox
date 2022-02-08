/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMExpertPage class implementation.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
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
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

/* GUI includes: */
#include "QILineEdit.h"
#include "UICommon.h"
#include "UIFilePathSelector.h"
#include "UIWizardCloneVMExpertPage.h"
#include "UIWizardCloneVM.h"
#include "UIWizardCloneVMNamePathPage.h"

/* COM includes: */
#include "CSystemProperties.h"


UIWizardCloneVMExpertPage::UIWizardCloneVMExpertPage(const QString &strOriginalName, const QString &strDefaultPath,
                                                     bool /*fAdditionalInfo*/, bool fShowChildsOption, const QString &strGroup)
    : m_pNamePathGroupBox(0)
    , m_pCloneTypeGroupBox(0)
    , m_pCloneModeGroupBox(0)
    , m_pAdditionalOptionsGroupBox(0)
    , m_strGroup(strGroup)
{
    prepare(strOriginalName, strDefaultPath, fShowChildsOption);
}

void UIWizardCloneVMExpertPage::prepare(const QString &strOriginalName, const QString &strDefaultPath, bool fShowChildsOption)
{
    QGridLayout *pMainLayout = new QGridLayout(this);
    AssertReturnVoid(pMainLayout);
    m_pNamePathGroupBox = new UICloneVMNamePathEditor(strOriginalName, strDefaultPath);
    if (m_pNamePathGroupBox)
    {
        pMainLayout->addWidget(m_pNamePathGroupBox, 0, 0, 3, 2);
        connect(m_pNamePathGroupBox, &UICloneVMNamePathEditor::sigCloneNameChanged,
                this, &UIWizardCloneVMExpertPage::sltCloneNameChanged);
        connect(m_pNamePathGroupBox, &UICloneVMNamePathEditor::sigClonePathChanged,
                this, &UIWizardCloneVMExpertPage::sltClonePathChanged);
    }

    m_pCloneTypeGroupBox = new UICloneVMCloneTypeGroupBox;
    if (m_pCloneTypeGroupBox)
        pMainLayout->addWidget(m_pCloneTypeGroupBox, 3, 0, 2, 1);

    m_pCloneModeGroupBox = new UICloneVMCloneModeGroupBox(fShowChildsOption);
    if (m_pCloneModeGroupBox)
        pMainLayout->addWidget(m_pCloneModeGroupBox, 3, 1, 2, 1);

    m_pAdditionalOptionsGroupBox = new UICloneVMAdditionalOptionsEditor;
    if (m_pAdditionalOptionsGroupBox)
    {
        pMainLayout->addWidget(m_pAdditionalOptionsGroupBox, 5, 0, 2, 2);
        connect(m_pAdditionalOptionsGroupBox, &UICloneVMAdditionalOptionsEditor::sigMACAddressClonePolicyChanged,
                this, &UIWizardCloneVMExpertPage::sltMACAddressClonePolicyChanged);
        connect(m_pAdditionalOptionsGroupBox, &UICloneVMAdditionalOptionsEditor::sigKeepDiskNamesToggled,
                this, &UIWizardCloneVMExpertPage::sltKeepDiskNamesToggled);
        connect(m_pAdditionalOptionsGroupBox, &UICloneVMAdditionalOptionsEditor::sigKeepHardwareUUIDsToggled,
                this, &UIWizardCloneVMExpertPage::sltKeepHardwareUUIDsToggled);
    }
    retranslateUi();
}

void UIWizardCloneVMExpertPage::retranslateUi()
{
    /* Translate widgets: */
    if (m_pNamePathGroupBox)
        m_pNamePathGroupBox->setTitle(UIWizardCloneVM::tr("New machine &name and path"));
    if (m_pCloneTypeGroupBox)
        m_pCloneTypeGroupBox->setTitle(UIWizardCloneVM::tr("Clone type"));
    if (m_pCloneModeGroupBox)
        m_pCloneModeGroupBox->setTitle(UIWizardCloneVM::tr("Snapshots"));
    if (m_pAdditionalOptionsGroupBox)
        m_pAdditionalOptionsGroupBox->setTitle(UIWizardCloneVM::tr("Additional options"));
}

void UIWizardCloneVMExpertPage::initializePage()
{
    UIWizardCloneVM *pWizard = wizardWindow<UIWizardCloneVM>();
    AssertReturnVoid(pWizard);
    if (m_pNamePathGroupBox)
    {
        m_pNamePathGroupBox->setFocus();
        pWizard->setCloneName(m_pNamePathGroupBox->cloneName());
        pWizard->setCloneFilePath(
                                 UIWizardCloneVMNamePathCommon::composeCloneFilePath(m_pNamePathGroupBox->cloneName(), m_strGroup, m_pNamePathGroupBox->clonePath()));
    }
    if (m_pAdditionalOptionsGroupBox)
    {
        pWizard->setMacAddressPolicy(m_pAdditionalOptionsGroupBox->macAddressClonePolicy());
        pWizard->setKeepDiskNames(m_pAdditionalOptionsGroupBox->keepDiskNames());
        pWizard->setKeepHardwareUUIDs(m_pAdditionalOptionsGroupBox->keepHardwareUUIDs());
    }
    if (m_pCloneTypeGroupBox)
        pWizard->setLinkedClone(!m_pCloneTypeGroupBox->isFullClone());
    if (m_pCloneModeGroupBox)
        pWizard->setCloneMode(m_pCloneModeGroupBox->cloneMode());

    if (m_pCloneModeGroupBox)
        m_pCloneModeGroupBox->setEnabled(pWizard->machineHasSnapshot());

    retranslateUi();
}

bool UIWizardCloneVMExpertPage::isComplete() const
{
    return m_pNamePathGroupBox && m_pNamePathGroupBox->isComplete(m_strGroup);
}

bool UIWizardCloneVMExpertPage::validatePage()
{
    AssertReturn(wizardWindow<UIWizardCloneVM>(), false);
    return wizardWindow<UIWizardCloneVM>()->cloneVM();
}

void UIWizardCloneVMExpertPage::sltCloneNameChanged(const QString &strCloneName)
{
    UIWizardCloneVM *pWizard = wizardWindow<UIWizardCloneVM>();
    AssertReturnVoid(pWizard);
    AssertReturnVoid(m_pNamePathGroupBox);
    pWizard->setCloneName(strCloneName);
    pWizard->setCloneFilePath(
                             UIWizardCloneVMNamePathCommon::composeCloneFilePath(strCloneName, m_strGroup, m_pNamePathGroupBox->clonePath()));
    emit completeChanged();
}

void UIWizardCloneVMExpertPage::sltClonePathChanged(const QString &strClonePath)
{
    AssertReturnVoid(m_pNamePathGroupBox && wizardWindow<UIWizardCloneVM>());
    wizardWindow<UIWizardCloneVM>()->setCloneFilePath(
                             UIWizardCloneVMNamePathCommon::composeCloneFilePath(m_pNamePathGroupBox->cloneName(), m_strGroup, strClonePath));
    emit completeChanged();
}

void UIWizardCloneVMExpertPage::sltMACAddressClonePolicyChanged(MACAddressClonePolicy enmMACAddressClonePolicy)
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVM>());
    wizardWindow<UIWizardCloneVM>()->setMacAddressPolicy(enmMACAddressClonePolicy);
}

void UIWizardCloneVMExpertPage::sltKeepDiskNamesToggled(bool fKeepDiskNames)
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVM>());
    wizardWindow<UIWizardCloneVM>()->setKeepDiskNames(fKeepDiskNames);
}

void UIWizardCloneVMExpertPage::sltKeepHardwareUUIDsToggled(bool fKeepHardwareUUIDs)
{
    AssertReturnVoid(wizardWindow<UIWizardCloneVM>());
    wizardWindow<UIWizardCloneVM>()->setKeepHardwareUUIDs(fKeepHardwareUUIDs);
}
