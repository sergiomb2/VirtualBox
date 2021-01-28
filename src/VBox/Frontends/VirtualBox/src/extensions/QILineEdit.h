/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILineEdit class declaration.
 */

/*
 * Copyright (C) 2008-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_extensions_QILineEdit_h
#define FEQT_INCLUDED_SRC_extensions_QILineEdit_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes */
#include <QLineEdit>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QLineEdit extension with advanced functionality. */
class SHARED_LIBRARY_STUFF QILineEdit : public QLineEdit
{
    Q_OBJECT;

public:

    /** Constructs line-edit passing @a pParent to the base-class. */
    QILineEdit(QWidget *pParent = 0);
    /** Constructs line-edit passing @a pParent to the base-class.
      * @param  strText  Brings the line-edit text. */
    QILineEdit(const QString &strText, QWidget *pParent = 0);

    /** Forces line-edit to adjust minimum width acording to passed @a strText. */
    void setMinimumWidthByText(const QString &strText);
    /** Forces line-edit to adjust fixed width acording to passed @a strText. */
    void setFixedWidthByText(const QString &strText);

    /** Sets the color to some reddish color when @p fError is true. Usually used to indicate some error. */
    void mark(bool fError);

private:

    /** Prepares all. */
    void prepare();

    /** Calculates suitable @a strText size. */
    QSize featTextWidth(const QString &strText) const;

    /** The original background base color. Used when marking/unmarking the combo box. */
    QColor  m_originalBaseColor;
};

#endif /* !FEQT_INCLUDED_SRC_extensions_QILineEdit_h */
