/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVMPageExpert class declaration.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QGroupBox;
class QGridLayout;
class UICloneVMAdditionalOptionsEditor;
class UICloneVMCloneModeGroupBox;
class UICloneVMCloneTypeGroupBox;
class UICloneVMNamePathEditor;


/* Expert page of the Clone Virtual Machine wizard: */
class UIWizardCloneVMPageExpert : public UINativeWizardPage
{
    Q_OBJECT;
    // Q_PROPERTY(QString cloneName READ cloneName WRITE setCloneName);
    // Q_PROPERTY(QString cloneFilePath READ cloneFilePath WRITE setCloneFilePath);
    // Q_PROPERTY(bool linkedClone READ linkedClone);
    // Q_PROPERTY(KCloneMode cloneMode READ cloneMode WRITE setCloneMode);
    // Q_PROPERTY(MACAddressClonePolicy macAddressClonePolicy READ macAddressClonePolicy WRITE setMACAddressClonePolicy);
    // Q_PROPERTY(bool keepDiskNames READ keepDiskNames WRITE setKeepDiskNames);
    // Q_PROPERTY(bool keepHWUUIDs READ keepHWUUIDs WRITE setKeepHWUUIDs);

public:

    /* Constructor: */
    UIWizardCloneVMPageExpert(const QString &strOriginalName, const QString &strDefaultPath,
                              bool fAdditionalInfo, bool fShowChildsOption, const QString &strGroup);

private slots:

    /* Button toggle handler: */
    // void sltButtonToggled(QAbstractButton *pButton, bool fChecked);
    // void sltNameChanged();
    // void sltPathChanged();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();
    void prepare(const QString &strOriginalName, const QString &strDefaultPath, bool fShowChildsOption);

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    QGridLayout *m_pMainLayout;
    UICloneVMNamePathEditor *m_pNamePathGroupBox;
    UICloneVMCloneTypeGroupBox *m_pCloneTypeGroupBox;
    UICloneVMCloneModeGroupBox *m_pCloneModeGroupBox;
    UICloneVMAdditionalOptionsEditor *m_pAdditionalOptionsroupBox;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevm_UIWizardCloneVMPageExpert_h */
