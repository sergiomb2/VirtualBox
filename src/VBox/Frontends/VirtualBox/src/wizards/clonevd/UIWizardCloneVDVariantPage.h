/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDVariantPage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDVariantPage_h
#define FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDVariantPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QIRichTextLabel;
class CMediumFormat;
class UIDiskVariantWidget;

class UIWizardCloneVDVariantPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs basic page. */
    UIWizardCloneVDVariantPage();

private slots:

    void sltMediumVariantChanged(qulonglong uVariant);

private:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;
    void prepare();

    /** Prepares the page. */
    virtual void initializePage() RT_OVERRIDE;

    /** Returns whether the page is complete. */
    virtual bool isComplete() const RT_OVERRIDE;
    void setWidgetVisibility(const CMediumFormat &mediumFormat);

    /** Holds the description label instance. */
    QIRichTextLabel *m_pDescriptionLabel;
    /** Holds the 'Dynamic' description label instance. */
    QIRichTextLabel *m_pDynamicLabel;
    /** Holds the 'Fixed' description label instance. */
    QIRichTextLabel *m_pFixedLabel;
    /** Holds the 'Split to 2GB files' description label instance. */
    QIRichTextLabel *m_pSplitLabel;
    UIDiskVariantWidget *m_pVariantWidget;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_clonevd_UIWizardCloneVDVariantPage_h */
