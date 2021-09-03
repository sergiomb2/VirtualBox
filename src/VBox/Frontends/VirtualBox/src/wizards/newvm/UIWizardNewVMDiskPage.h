/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMDiskPage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPage_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "QIFileDialog.h"
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QButtonGroup;
class QCheckBox;
class QRadioButton;
class QLabel;
class QIRichTextLabel;
class QIToolButton;
class UIMediaComboBox;
class UIMediumSizeEditor;

namespace UIWizardNewVMDiskCommon
{
    QUuid getWithFileOpenDialog(const QString &strOSTypeID,
                                const QString &strMachineFolder,
                                const QString &strMachineBaseName,
                                QWidget *pCaller);
}


class UIWizardNewVMDiskPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMDiskPage();

protected:


private slots:

    void sltSelectedDiskSourceChanged();
    void sltMediaComboBoxIndexChanged();
    void sltGetWithFileOpenDialog();
    void sltHandleSizeEditorChange(qulonglong uSize);
    void sltFixedCheckBoxToggled(bool fChecked);

private:

    void prepare();
    void createConnections();
    QWidget *createNewDiskWidgets();
    void setEnableNewDiskWidgets(bool fEnable);
    QWidget *createDiskWidgets();
    QWidget *createMediumVariantWidgets(bool fWithLabels);

    virtual void retranslateUi() /* override final */;
    virtual void initializePage() /* override final */;
    virtual bool isComplete() const /* override final */;

    void setEnableDiskSelectionWidgets(bool fEnabled);
    void setWidgetVisibility(const CMediumFormat &mediumFormat);

    /** @name Widgets
     * @{ */
       QButtonGroup *m_pDiskSourceButtonGroup;
       QRadioButton *m_pDiskEmpty;
       QRadioButton *m_pDiskNew;
       QRadioButton *m_pDiskExisting;
       UIMediaComboBox *m_pDiskSelector;
       QIToolButton *m_pDiskSelectionButton;
       QIRichTextLabel *m_pLabel;
       QLabel          *m_pMediumSizeEditorLabel;
       UIMediumSizeEditor *m_pMediumSizeEditor;
       QIRichTextLabel *m_pDescriptionLabel;
       QIRichTextLabel *m_pDynamicLabel;
       QIRichTextLabel *m_pFixedLabel;
       QCheckBox *m_pFixedCheckBox;
    /** @} */

    /** @name Variables
      * @{ */
        QSet<QString> m_userModifiedParameters;
        bool m_fVDIFormatFound;
        qulonglong m_uMediumSizeMin;
        qulonglong m_uMediumSizeMax;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPage_h */
