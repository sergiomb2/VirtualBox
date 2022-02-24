/* $Id$ */
/** @file
 * VBox Qt GUI - UIDefaultMachineFolderEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIDefaultMachineFolderEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIDefaultMachineFolderEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class UIFilePathSelector;

/** QWidget subclass used as a default machine folder editor. */
class SHARED_LIBRARY_STUFF UIDefaultMachineFolderEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a enmValue change. */
    void sigValueChanged(const QString &strValue);

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIDefaultMachineFolderEditor(QWidget *pParent = 0);

    /** Defines editor @a strValue. */
    void setValue(const QString &strValue);
    /** Returns editor value. */
    QString value() const;

    /** Returns minimum layout hint. */
    int minimumLabelHorizontalHint() const;
    /** Defines minimum layout @a iIndent. */
    void setMinimumLayoutIndent(int iIndent);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles selector path change. */
    void sltHandleSelectorPathChanged();

private:

    /** Prepares all. */
    void prepare();

    /** Holds the value to be set. */
    QString  m_strValue;

    /** Holds the main layout instance. */
    QGridLayout        *m_pLayout;
    /** Holds the label instance. */
    QLabel             *m_pLabel;
    /** Holds the selector instance. */
    UIFilePathSelector *m_pSelector;
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIDefaultMachineFolderEditor_h */
