/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMUnattendedPage class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMUnattendedPage_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMUnattendedPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class UIAdditionalUnattendedOptions;
class UIGAInstallationGroupBox;
class UIUserNamePasswordGroupBox;
class UIWindowsISOImageSelector;

namespace UIWizardNewVMUnattendedCommon
{
    /** Returns false if ISO path selector is non empty but has invalid file path. */
    bool checkGAISOFile(const QString &strPatho);
}

class UIWizardNewVMUnattendedPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMUnattendedPage();

private slots:

    void sltInstallGACheckBoxToggle(bool fChecked);
    void sltGAISOPathChanged(const QString &strPath);
    void sltPasswordChanged(const QString &strPassword);
    void sltUserNameChanged(const QString &strUserName);
    void sltHostnameDomainNameChanged(const QString &strHostnameDomainName);
    void sltProductKeyChanged(const QString &strProductKey);
    void sltStartHeadlessChanged(bool fStartHeadless);
    void sltSelectedWindowsImageChanged(ulong uImageIndex);

private:

    void prepare();
    void createConnections();

    void retranslateUi();
    void initializePage();
    bool isComplete() const;
    /** Returns true if we show the widgets for guest os product key. */
    bool isProductKeyWidgetEnabled() const;
    void markWidgets() const;

    /** @name Widgets
      * @{ */
        QIRichTextLabel *m_pLabel;
        UIAdditionalUnattendedOptions *m_pAdditionalOptionsContainer;
        UIGAInstallationGroupBox *m_pGAInstallationISOContainer;
        UIUserNamePasswordGroupBox *m_pUserNamePasswordGroupBox;
        UIWindowsISOImageSelector *m_pWindowsISOImageSelector;
    /** @} */
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMUnattendedPage_h */
