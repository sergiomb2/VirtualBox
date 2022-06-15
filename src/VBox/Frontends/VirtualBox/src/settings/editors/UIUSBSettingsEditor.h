/* $Id$ */
/** @file
 * VBox Qt GUI - UIUSBSettingsEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIUSBSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIUSBSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"
#include "UIUSBFiltersEditor.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QCheckBox;
class UIUSBControllerEditor;

/** QWidget subclass used as a USB settings editor. */
class SHARED_LIBRARY_STUFF UIUSBSettingsEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about value change. */
    void sigValueChanged();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIUSBSettingsEditor(QWidget *pParent = 0);

    /** @name General stuff
     * @{ */
        /** Defines whether feature is @a fEnabled. */
        void setFeatureEnabled(bool fEnabled);
        /** Returns whether feature is enabled. */
        bool isFeatureEnabled() const;

        /** Defines whether feature @a fAvailable. */
        void setFeatureAvailable(bool fAvailable);
    /** @} */

    /** @name USB controller editor stuff
     * @{ */
        /** Defines USB controller @a enmType. */
        void setUsbControllerType(KUSBControllerType enmType);
        /** Returns USB controller type. */
        KUSBControllerType usbControllerType() const;

        /** Defines whether USB controller option @a fAvailable. */
        void setUsbControllerOptionAvailable(bool fAvailable);
    /** @} */

    /** @name USB filters editor stuff
     * @{ */
        /** Defines a list of USB @a filters. */
        void setUsbFilters(const QList<UIDataUSBFilter> &filters);
        /** Returns a list of USB filters. */
        QList<UIDataUSBFilter> usbFilters() const;

        /** Defines whether USB filters option @a fAvailable. */
        void setUsbFiltersOptionAvailable(bool fAvailable);
    /** @} */

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles feature toggling. */
    void sltHandleFeatureToggled();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Updates feature availability. */
    void updateFeatureAvailability();

    /** @name Values
     * @{ */
        /** Holds whether feature is enabled. */
        bool  m_fFeatureEnabled;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the feature check-box instance. */
        QCheckBox             *m_pCheckboxFeature;
        /** Holds the settings widget instance. */
        QWidget               *m_pWidgetSettings;
        /** Holds the controller editor instance. */
        UIUSBControllerEditor *m_pEditorController;
        /** Holds the filters editor instance. */
        UIUSBFiltersEditor    *m_pEditorFilters;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIUSBSettingsEditor_h */
