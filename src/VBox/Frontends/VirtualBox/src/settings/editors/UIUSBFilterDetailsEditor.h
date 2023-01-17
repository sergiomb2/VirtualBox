/* $Id$ */
/** @file
 * VBox Qt GUI - UIUSBFilterDetailsEditor class declaration.
 */

/*
 * Copyright (C) 2008-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIUSBFilterDetailsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIUSBFilterDetailsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QComboBox;
class QLabel;
class QLineEdit;
class QIDialogButtonBox;

/** QIDialog subclass used as a USB filter editor. */
class SHARED_LIBRARY_STUFF UIUSBFilterDetailsEditor : public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIUSBFilterDetailsEditor(QWidget *pParent = 0);

    /** Defines @a strName. */
    void setName(const QString &strName);
    /** Returns name. */
    QString name() const;

    /** Defines @a strVendorID. */
    void setVendorID(const QString &strVendorID);
    /** Returns vendor ID. */
    QString vendorID() const;

    /** Defines @a strProductID. */
    void setProductID(const QString &strProductID);
    /** Returns product ID. */
    QString productID() const;

    /** Defines @a strRevision. */
    void setRevision(const QString &strRevision);
    /** Returns revision. */
    QString revision() const;

    /** Defines @a strManufacturer. */
    void setManufacturer(const QString &strManufacturer);
    /** Returns manufacturer. */
    QString manufacturer() const;

    /** Defines @a strProduct. */
    void setProduct(const QString &strProduct);
    /** Returns product. */
    QString product() const;

    /** Defines @a strSerialNo. */
    void setSerialNo(const QString &strSerialNo);
    /** Returns serial no. */
    QString serialNo() const;

    /** Defines @a strPort. */
    void setPort(const QString &strPort);
    /** Returns port. */
    QString port() const;

    /** Defines @a enmRemoteMode. */
    void setRemoteMode(const UIRemoteMode &enmRemoteMode);
    /** Returns port. */
    UIRemoteMode remoteMode() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Wipes out @a strString if it's empty. */
    static QString wiped(const QString &strString);

    /** @name Widgets
     * @{ */
        /** Holds the name label instance. */
        QLabel            *m_pLabelName;
        /** Holds the name editor instance. */
        QLineEdit         *m_pEditorName;
        /** Holds the vendor ID label instance. */
        QLabel            *m_pLabelVendorID;
        /** Holds the vendor ID editor instance. */
        QLineEdit         *m_pEditorVendorID;
        /** Holds the product ID label instance. */
        QLabel            *m_pLabelProductID;
        /** Holds the product ID editor instance. */
        QLineEdit         *m_pEditorProductID;
        /** Holds the revision label instance. */
        QLabel            *m_pLabelRevision;
        /** Holds the revision editor instance. */
        QLineEdit         *m_pEditorRevision;
        /** Holds the manufacturer label instance. */
        QLabel            *m_pLabelManufacturer;
        /** Holds the manufacturer editor instance. */
        QLineEdit         *m_pEditorManufacturer;
        /** Holds the product label instance. */
        QLabel            *m_pLabelProduct;
        /** Holds the product editor instance. */
        QLineEdit         *m_pEditorProduct;
        /** Holds the serial NO label instance. */
        QLabel            *m_pLabelSerialNo;
        /** Holds the serial NO editor instance. */
        QLineEdit         *m_pEditorSerialNo;
        /** Holds the port label instance. */
        QLabel            *m_pLabelPort;
        /** Holds the port editor instance. */
        QLineEdit         *m_pEditorPort;
        /** Holds the remote label instance. */
        QLabel            *m_pLabelRemote;
        /** Holds the remote combo instance. */
        QComboBox         *m_pComboRemote;
        /** Holds the button-box instance. */
        QIDialogButtonBox *m_pButtonBox;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIUSBFilterDetailsEditor_h */
