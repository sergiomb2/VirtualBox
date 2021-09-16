/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDSizeLocationPage class implementation.
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
#include <QDir>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIWizardNewVDSizeLocationPage.h"
#include "UIWizardNewVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIWizardDiskEditors.h"

/* COM includes: */
#include "CSystemProperties.h"

UIWizardNewVDSizeLocationPage::UIWizardNewVDSizeLocationPage(const QString &strDefaultName,
                                                             const QString &strDefaultPath, qulonglong uDefaultSize)
    : m_pMediumSizePathGroup(0)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
    , m_strDefaultName(strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
{
    prepare();
}

void UIWizardNewVDSizeLocationPage::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(pMainLayout);
    m_pMediumSizePathGroup = new UIMediumSizeAndPathGroupBox(false /* fExpertMode */, 0 /* parent */, _4M /* minimum size */);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
            this, &UIWizardNewVDSizeLocationPage::sltMediumSizeChanged);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
            this, &UIWizardNewVDSizeLocationPage::sltMediumPathChanged);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
            this, &UIWizardNewVDSizeLocationPage::sltSelectLocationButtonClicked);
    pMainLayout->addWidget(m_pMediumSizePathGroup);
    pMainLayout->addStretch();
    retranslateUi();
}

void UIWizardNewVDSizeLocationPage::sltSelectLocationButtonClicked()
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard);
    QString strSelectedPath =
        UIDiskEditorGroupBox::openFileDialogForDiskFile(pWizard->mediumPath(), pWizard->mediumFormat(),
                                                                KDeviceType_HardDisk, pWizard);

    if (strSelectedPath.isEmpty())
        return;
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strSelectedPath,
                                              UIDiskEditorGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk));
    QFileInfo mediumPath(strMediumPath);
    m_pMediumSizePathGroup->setMediumFilePath(QDir::toNativeSeparators(mediumPath.absoluteFilePath()));
}

void UIWizardNewVDSizeLocationPage::sltMediumSizeChanged(qulonglong uSize)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVD>());
    m_userModifiedParameters << "MediumSize";
    wizardWindow<UIWizardNewVD>()->setMediumSize(uSize);
    emit completeChanged();
}

void UIWizardNewVDSizeLocationPage::sltMediumPathChanged(const QString &strPath)
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard);
    m_userModifiedParameters << "MediumPath";
    QString strMediumPath =
        UIDiskEditorGroupBox::appendExtension(strPath,
                                              UIDiskEditorGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk));
    pWizard->setMediumPath(strMediumPath);
    emit completeChanged();
}

void UIWizardNewVDSizeLocationPage::retranslateUi()
{
    setTitle(UIWizardNewVD::tr("File location and size"));
}

void UIWizardNewVDSizeLocationPage::initializePage()
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturnVoid(pWizard && m_pMediumSizePathGroup);

    QString strExtension = UIDiskEditorGroupBox::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk);
    QString strMediumFilePath;
    /* Initialize the medium file path with default name and path if user has not exclusively modified them yet: */
    if (!m_userModifiedParameters.contains("MediumPath"))
        strMediumFilePath =
            UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(m_strDefaultName,
                                                                                                 strExtension), m_strDefaultPath);
    /* Initialize the medium file path with file path and file name from the location editor. This part is to update the 
     * file extention correctly in case user has gone back and changed the file format after modifying medium file path: */
    else
        strMediumFilePath =
            UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(m_pMediumSizePathGroup->mediumName(),
                                                                                                 strExtension), m_pMediumSizePathGroup->mediumPath());
    m_pMediumSizePathGroup->blockSignals(true);
    m_pMediumSizePathGroup->setMediumFilePath(strMediumFilePath);
    m_pMediumSizePathGroup->blockSignals(false);
    pWizard->setMediumPath(m_pMediumSizePathGroup->mediumFilePath());

    if (!m_userModifiedParameters.contains("MediumSize"))
    {
        m_pMediumSizePathGroup->blockSignals(true);
        m_pMediumSizePathGroup->setMediumSize(m_uDefaultSize > m_uMediumSizeMin && m_uDefaultSize < m_uMediumSizeMax ? m_uDefaultSize : m_uMediumSizeMin);
        m_pMediumSizePathGroup->blockSignals(false);
        pWizard->setMediumSize(m_pMediumSizePathGroup->mediumSize());
    }
    retranslateUi();
}

bool UIWizardNewVDSizeLocationPage::isComplete() const
{
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturn(pWizard, false);
    if (pWizard->mediumPath().isEmpty())
        return false;
    if (pWizard->mediumSize() > m_uMediumSizeMax || pWizard->mediumSize() < m_uMediumSizeMin)
        return false;
    return true;
}

bool UIWizardNewVDSizeLocationPage::validatePage()
{
    bool fResult = true;
    UIWizardNewVD *pWizard = wizardWindow<UIWizardNewVD>();
    AssertReturn(pWizard, false);
    /* Make sure such file doesn't exist already: */
    const QString strMediumPath(pWizard->mediumPath());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return fResult;
    }

    /* Make sure we are passing FAT size limitation: */
    fResult = UIDiskEditorGroupBox::checkFATSizeLimitation(pWizard->mediumVariant(),
                                     pWizard->mediumPath(),
                                     pWizard->mediumSize());
    if (!fResult)
    {
        msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
        return fResult;
    }

    fResult = pWizard->createVirtualDisk();
    return fResult;
}
