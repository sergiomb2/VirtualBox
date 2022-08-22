/* $Id$ */
/** @file
 * VBox Qt GUI - UILanguageSettingsEditor class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declartions: */
class QTreeWidgetItem;
class QILabelSeparator;
class QIRichTextLabel;
class QITreeWidget;

/** QWidget subclass used as a language settings editor. */
class SHARED_LIBRARY_STUFF UILanguageSettingsEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UILanguageSettingsEditor(QWidget *pParent = 0);

    /** Defines editor @a strValue. */
    void setValue(const QString &strValue);
    /** Returns editor value. */
    QString value() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) RT_OVERRIDE;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

private slots:

    /** Handles @a pItem painting with passed @a pPainter. */
    void sltHandleItemPainting(QTreeWidgetItem *pItem, QPainter *pPainter);

    /** Handles @a pCurrentItem change. */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Reloads language list, choosing item with @a strLanguageId as current. */
    void reloadLanguageTree(const QString &strLanguageId);

    /** Holds whether the page is polished. */
    bool  m_fPolished;

    /** Holds the value to be set. */
    QString  m_strValue;

    /** @name Widgets
     * @{ */
        /** Holds the separator label instance. */
        QILabelSeparator *m_pLabelSeparator;
        /** Holds the tree-widget instance. */
        QITreeWidget     *m_pTreeWidget;
        /** Holds the info label instance. */
        QIRichTextLabel  *m_pLabelInfo;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UILanguageSettingsEditor_h */
