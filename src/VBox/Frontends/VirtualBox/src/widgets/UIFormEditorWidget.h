/* $Id$ */
/** @file
 * VBox Qt GUI - UIFormEditorWidget class declaration.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIFormEditorWidget_h
#define FEQT_INCLUDED_SRC_widgets_UIFormEditorWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QPointer>
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CFormValue.h"

/* Forward declarations: */
class QHeaderView;
class UIFormEditorModel;
class UIFormEditorView;
class CForm;
class CVirtualSystemDescriptionForm;

/** QWidget subclass representing model/view Form Editor widget. */
class UIFormEditorWidget : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about progress has started. */
    void sigProgressStarted();
    /** Notifies listeners about progress has changed.
      * @param  uPercent  Brings the progress percentage. */
    void sigProgressChange(ulong uPercent);
    /** Notifies listeners about progress has finished. */
    void sigProgressFinished();

public:

    /** Constructs Form Editor widget passing @a pParent to the base-class. */
    UIFormEditorWidget(QWidget *pParent = 0);

    /** Returns horizontal header reference. */
    QHeaderView *horizontalHeader() const;
    /** Returns vertical header reference. */
    QHeaderView *verticalHeader() const;

    /** Defines @a values to be edited. */
    void setValues(const QVector<CFormValue> &values);
    /** Defines @a comForm to be edited. */
    void setForm(const CForm &comForm);
    /** Defines virtual system description @a comForm to be edited. */
    void setVirtualSystemDescriptionForm(const CVirtualSystemDescriptionForm &comForm);

    /** Makes sure current editor data committed. */
    void makeSureEditorDataCommitted();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Adjusts table column sizes. */
    void adjustTable();

    /** Holds the table-view instance. */
    UIFormEditorView  *m_pTableView;
    /** Holds the table-model instance. */
    UIFormEditorModel *m_pTableModel;
};

/** Safe pointer to Form Editor widget. */
typedef QPointer<UIFormEditorWidget> UIFormEditorWidgetPointer;

#endif /* !FEQT_INCLUDED_SRC_widgets_UIFormEditorWidget_h */
