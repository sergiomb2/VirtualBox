/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualCPUEditor class declaration.
 */

/*
 * Copyright (C) 2019-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIVirtualCPUEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIVirtualCPUEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"


/* Forward declarations: */
class QLabel;
class QSpinBox;
class QIAdvancedSlider;

/** QWidget subclass used as a virtual cpu count editor. Includes a spinbox, a slider,
  * optional label, and max-min value labels. */
class SHARED_LIBRARY_STUFF UIVirtualCPUEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

 signals:

    void sigValueChanged(int iValue);

public:

    /** Constructs editor passing @a pParent to the base-class.
      * @param  fWithLabel  Determines whether we should add label ourselves. */
    UIVirtualCPUEditor(QWidget *pParent = 0, bool fWithLabel = false);

    /** Defines editor @a iValue. */
    void setValue(int iValue);
    /** Returns editor value. */
    int value() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles slider value changes. */
    void sltHandleSliderChange();
    /** Handles spin-box value changes. */
    void sltHandleSpinBoxChange();

private:

    /** Prepares all. */
    void prepare();


    /* @{ */
    /** Widgets */
       QLabel           *m_pLabelVCPU;
       QIAdvancedSlider *m_pSlider;
       QLabel           *m_pLabelVCPUMin;
       QLabel           *m_pLabelVCPUMax;
       QSpinBox         *m_pSpinBox;
    /** @} */

    unsigned m_uMaxVCPUCount;
    unsigned m_uMinVCPUCount;
    bool  m_fWithLabel;

};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIVirtualCPUEditor_h */
