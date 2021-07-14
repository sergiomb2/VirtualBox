/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMEditors class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_editors_UIWizardNewVMEditors_h
#define FEQT_INCLUDED_SRC_wizards_editors_UIWizardNewVMEditors_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QGroupBox>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;
class QILineEdit;
class UIFilePathSelector;
class UIHostnameDomainNameEditor;
class UIPasswordLineEdit;
class UIUserNamePasswordEditor;

class UIUserNamePasswordGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:

    void sigUserNameChanged(const QString &strUserName);
    void sigPasswordChanged(const QString &strPassword);

public:

    UIUserNamePasswordGroupBox(QWidget *pParent = 0);

    /** @name Wrappers for UIUserNamePasswordEditor
      * @{ */
        QString userName() const;
        void setUserName(const QString &strUserName);

        QString password() const;
        void setPassword(const QString &strPassword);
        bool isComplete();
        void setLabelsVisible(bool fVisible);
    /** @} */

private:

    void prepare();
    virtual void retranslateUi() /* override final */;

    UIUserNamePasswordEditor *m_pUserNamePasswordEditor;
};


class UIGAInstallationGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:

    void sigPathChanged(const QString &strPath);

public:

    UIGAInstallationGroupBox(QWidget *pParent = 0);

    /** @name Wrappers for UIFilePathSelector
      * @{ */
        QString path() const;
        void setPath(const QString &strPath, bool fRefreshText = true);
        void mark(bool fError, const QString &strErrorMessage = QString());
    /** @} */

public slots:

    void sltToggleWidgetsEnabled(bool fEnabled);

private:

    virtual void retranslateUi() /* override final */;
    void prepare();

    QLabel *m_pGAISOPathLabel;
    UIFilePathSelector *m_pGAISOFilePathSelector;

};

class UIAdditionalUnattendedOptions : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:

    void sigHostnameDomainNameChanged(const QString &strHostnameDomainName);
    void sigProductKeyChanged(const QString &strHostnameDomainName);
    void sigStartHeadlessChanged(bool fChecked);

public:

    UIAdditionalUnattendedOptions(QWidget *pParent = 0);

    /** @name Wrappers for UIFilePathSelector
      * @{ */
        QString hostname() const;
        void setHostname(const QString &strHostname);
        QString domainName() const;
        void setDomainName(const QString &strDomain);
        QString hostnameDomainName() const;
        bool isComplete() const;
        void disableEnableProductKeyWidgets(bool fEnabled);
    /** @} */

private:

    void prepare();
    virtual void retranslateUi() /* override final */;

    QLabel *m_pProductKeyLabel;
    QILineEdit *m_pProductKeyLineEdit;
    UIHostnameDomainNameEditor *m_pHostnameDomainNameEditor;
    QCheckBox *m_pStartHeadlessCheckBox;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIWizardNewVMEditors_h */
