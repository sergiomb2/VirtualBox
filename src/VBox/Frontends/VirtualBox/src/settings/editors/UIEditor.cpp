/* $Id$ */
/** @file
 * VBox Qt GUI - UIEditor class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

/* Qt includes: */
#include <QAbstractButton>
#include <QLabel>
#include <QRegularExpression>
#include <QTabWidget>

/* GUI includes: */
#include "UIEditor.h"


UIEditor::UIEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
{
}

void UIEditor::filterOut(const QString &strFilter)
{
    /* Make sure the editor is visible in case if filter is empty: */
    bool fVisible = strFilter.isEmpty();
    if (!fVisible)
    {
        /* Otherwise we'll have to walk through all the
         * descriptions to check whether filter is suitable: */
        foreach (const QString &strDescription, description())
            if (strDescription.contains(strFilter, Qt::CaseInsensitive))
            {
                fVisible = true;
                break;
            }
    }
    /* We'll show whole the editor if filter is suitable: */
    setVisible(fVisible);
}

QStringList UIEditor::description() const
{
    QStringList result;
    /* Clean <html> tags and &mpersands: */
    QRegularExpression re("<[^>]*>|&");

    /* Adding all the buddy labels we have: */
    foreach (QLabel *pLabel, findChildren<QLabel*>())
        if (pLabel && pLabel->buddy())
            result << pLabel->text().remove(re);

    /* Adding all the button sub-types: */
    foreach (QAbstractButton *pButton, findChildren<QAbstractButton*>())
        if (pButton)
            result << pButton->text().remove(re);

    /* Adding all the tab-widget tabs: */
    foreach (QTabWidget *pTabWidget, findChildren<QTabWidget*>())
        if (pTabWidget)
            for (int i = 0; i < pTabWidget->count(); ++i)
                result << pTabWidget->tabText(i).remove(re);

    return result;
}
