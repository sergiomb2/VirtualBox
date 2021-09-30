/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageProperties class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageProperties_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageProperties_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class UIFormEditorWidget;
class UIWizardNewCloudVM;
class CVirtualSystemDescriptionForm;

/** Namespace for properties page of the New Cloud VM wizard. */
namespace UIWizardNewCloudVMProperties
{
    /** Refreshes @a pFormEditor on the basis of comForm specified. */
    void refreshFormPropertiesTable(UIFormEditorWidget *pFormEditor, const CVirtualSystemDescriptionForm &comForm);
}

/** UINativeWizardPage extension for properties page of the New Cloud VM wizard,
  * based on UIWizardNewCloudVMProperties namespace functions. */
class UIWizardNewCloudVMPageProperties : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs properties basic page. */
    UIWizardNewCloudVMPageProperties();

protected:

    /** Returns wizard this page belongs to. */
    UIWizardNewCloudVM *wizard() const;

    /** Handles translation event. */
    virtual void retranslateUi() /* override final */;

    /** Performs page initialization. */
    virtual void initializePage() /* override final */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override final */;

    /** Performs page validation. */
    virtual bool validatePage() /* override final */;

private slots:

    /** Initializes short wizard form. */
    void sltInitShortWizardForm();

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;

    /** Holds the Form Editor widget instance. */
    UIFormEditorWidget *m_pFormEditor;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageProperties_h */
