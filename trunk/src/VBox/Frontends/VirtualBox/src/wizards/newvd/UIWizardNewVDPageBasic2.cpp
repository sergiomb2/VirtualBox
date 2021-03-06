/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageBasic2 class implementation.
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
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>

/* GUI includes: */
#include "UIWizardNewVDPageBasic2.h"
#include "UIWizardNewVD.h"
#include "QIRichTextLabel.h"

/* COM includes: */
#include "CMediumFormat.h"

UIWizardNewVDPage2::UIWizardNewVDPage2()
    : m_pFixedCheckBox(0)
    , m_pSplitBox(0)
    , m_pDescriptionLabel(0)
    , m_pDynamicLabel(0)
    , m_pFixedLabel(0)
    , m_pSplitLabel(0)
{
}

QWidget *UIWizardNewVDPage2::createMediumVariantWidgets(bool fWithLabels)
{
    QWidget *pContainerWidget = new QWidget;
    QVBoxLayout *pMainLayout = new QVBoxLayout(pContainerWidget);
    if (pMainLayout)
    {
        if (fWithLabels)
        {
            m_pDescriptionLabel = new QIRichTextLabel;
            m_pDynamicLabel = new QIRichTextLabel;
            m_pFixedLabel = new QIRichTextLabel;
            m_pSplitLabel = new QIRichTextLabel;
        }
        QVBoxLayout *pVariantLayout = new QVBoxLayout;
        if (pVariantLayout)
        {
            m_pFixedCheckBox = new QCheckBox;
            m_pSplitBox = new QCheckBox;
            pVariantLayout->addWidget(m_pFixedCheckBox);
            pVariantLayout->addWidget(m_pSplitBox);
        }
        if (fWithLabels)
        {
            pMainLayout->addWidget(m_pDescriptionLabel);
            pMainLayout->addWidget(m_pDynamicLabel);
            pMainLayout->addWidget(m_pFixedLabel);
            pMainLayout->addWidget(m_pSplitLabel);
        }
        pMainLayout->addLayout(pVariantLayout);
        pMainLayout->addStretch();
        pMainLayout->setContentsMargins(0, 0, 0, 0);
    }
    return pContainerWidget;
}

qulonglong UIWizardNewVDPage2::mediumVariant() const
{
    /* Initial value: */
    qulonglong uMediumVariant = (qulonglong)KMediumVariant_Max;

    /* Exclusive options: */
    if (m_pFixedCheckBox && m_pFixedCheckBox->isChecked())
        uMediumVariant = (qulonglong)KMediumVariant_Fixed;
    else
        uMediumVariant = (qulonglong)KMediumVariant_Standard;

    /* Additional options: */
    if (m_pSplitBox && m_pSplitBox->isChecked())
        uMediumVariant |= (qulonglong)KMediumVariant_VmdkSplit2G;

    /* Return options: */
    return uMediumVariant;
}

void UIWizardNewVDPage2::setMediumVariant(qulonglong uMediumVariant)
{
    /* Exclusive options: */
    if (uMediumVariant & (qulonglong)KMediumVariant_Fixed)
    {
        m_pFixedCheckBox->click();
        m_pFixedCheckBox->setFocus();
    }

    /* Additional options: */
    m_pSplitBox->setChecked(uMediumVariant & (qulonglong)KMediumVariant_VmdkSplit2G);
}

void UIWizardNewVDPage2::retranslateWidgets()
{
    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setText(UIWizardNewVD::tr("Pre-allocate &Full Size"));
        m_pFixedCheckBox->setToolTip(UIWizardNewVD::tr("<p>When checked, the virtual disk image will be fully allocated at "
                                                       "VM creation time, rather than being allocated dynamically at VM run-time.</p>"));
    }

    if (m_pSplitBox)
        m_pSplitBox->setText(UIWizardNewVD::tr("&Split Into Files of Less Than 2GB"));


    /* Translate rich text labels: */
    if (m_pDescriptionLabel)
        m_pDescriptionLabel->setText(UIWizardNewVD::tr("Please choose whether the new virtual hard disk file should grow as it is used "
                                                       "(dynamically allocated) or if it should be created at its maximum size (fixed size)."));
    if (m_pDynamicLabel)
        m_pDynamicLabel->setText(UIWizardNewVD::tr("<p>A <b>dynamically allocated</b> hard disk file will only use space "
                                                   "on your physical hard disk as it fills up (up to a maximum <b>fixed size</b>), "
                                                   "although it will not shrink again automatically when space on it is freed.</p>"));
    if (m_pFixedLabel)
        m_pFixedLabel->setText(UIWizardNewVD::tr("<p>A <b>fixed size</b> hard disk file may take longer to create on some "
                                                 "systems but is often faster to use.</p>"));
    if (m_pSplitLabel)
        m_pSplitLabel->setText(UIWizardNewVD::tr("<p>You can also choose to <b>split</b> the hard disk file into several files "
                                                 "of up to two gigabytes each. This is mainly useful if you wish to store the "
                                                 "virtual machine on removable USB devices or old systems, some of which cannot "
                                                 "handle very large files."));
}

void UIWizardNewVDPage2::setWidgetVisibility(CMediumFormat &mediumFormat)
{
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;
    if (m_pFixedCheckBox)
    {
        if (!fIsCreateDynamicPossible)
        {
            m_pFixedCheckBox->setChecked(true);
            m_pFixedCheckBox->setEnabled(false);
        }
        if (!fIsCreateFixedPossible)
        {
            m_pFixedCheckBox->setChecked(false);
            m_pFixedCheckBox->setEnabled(false);
        }
    }
    if (m_pDynamicLabel)
        m_pDynamicLabel->setHidden(!fIsCreateDynamicPossible);
    if (m_pFixedLabel)
        m_pFixedLabel->setHidden(!fIsCreateFixedPossible);
    if (m_pFixedCheckBox)
        m_pFixedCheckBox->setHidden(!fIsCreateFixedPossible);
    if (m_pSplitLabel)
        m_pSplitLabel->setHidden(!fIsCreateSplitPossible);
    if (m_pSplitBox)
        m_pSplitBox->setHidden(!fIsCreateSplitPossible);
}

void UIWizardNewVDPage2::updateMediumVariantWidgetsAfterFormatChange(const CMediumFormat &mediumFormat)
{
    /* Enable/disable widgets: */
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;

    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setEnabled(fIsCreateDynamicPossible || fIsCreateFixedPossible);
        if (!fIsCreateDynamicPossible)
            m_pFixedCheckBox->setChecked(true);
        if (!fIsCreateFixedPossible)
            m_pFixedCheckBox->setChecked(false);
    }
    m_pSplitBox->setEnabled(fIsCreateSplitPossible);
}

UIWizardNewVDPageBasic2::UIWizardNewVDPageBasic2()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->addWidget(createMediumVariantWidgets(true));
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pFixedCheckBox, &QAbstractButton::toggled,
            this, &UIWizardNewVDPageBasic2::completeChanged);
    connect(m_pSplitBox, &QCheckBox::stateChanged,
            this, &UIWizardNewVDPageBasic2::completeChanged);

    /* Register fields: */
    registerField("mediumVariant", this, "mediumVariant");
}

void UIWizardNewVDPageBasic2::retranslateUi()
{
    retranslateWidgets();
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("Storage on physical hard disk"));
}

void UIWizardNewVDPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    setWidgetVisibility(mediumFormat);
}

bool UIWizardNewVDPageBasic2::isComplete() const
{
    /* Make sure medium variant is correct: */
    return mediumVariant() != (qulonglong)KMediumVariant_Max;
}
