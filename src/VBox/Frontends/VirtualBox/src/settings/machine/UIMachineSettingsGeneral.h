/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsGeneral class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsGeneral_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsGeneral_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QITabWidget;
class UIDiskEncryptionSettingsEditor;
class UIDragAndDropEditor;
class UIMachineDescriptionEditor;
class UINameAndSystemEditor;
class UISharedClipboardEditor;
class UISnapshotFolderEditor;
struct UIDataSettingsMachineGeneral;
typedef UISettingsCache<UIDataSettingsMachineGeneral> UISettingsCacheMachineGeneral;

/** Machine settings: General page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsGeneral : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs General settings page. */
    UIMachineSettingsGeneral();
    /** Destructs General settings page. */
    virtual ~UIMachineSettingsGeneral() RT_OVERRIDE;

    /** Returns the VM OS type ID. */
    CGuestOSType guestOSType() const;
    /** Returns whether 64bit OS type ID is selected. */
    bool is64BitOSTypeSelected() const;

    /** Defines whether HW virtualization extension is enabled. */
    void setHWVirtExEnabled(bool fEnabled);

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const RT_OVERRIDE;

    /** Loads data into the cache from the corresponding external object(s).
      * @note This task COULD be performed in other than GUI thread. */
    virtual void loadToCacheFrom(QVariant &data) RT_OVERRIDE;
    /** Loads data into the corresponding widgets from cache,
      * @note This task SHOULD be performed in GUI thread only! */
    virtual void getFromCache() RT_OVERRIDE;

    /** Saves the data from the corresponding widgets into the cache,
      * @note This task SHOULD be performed in GUI thread only! */
    virtual void putToCache() RT_OVERRIDE;
    /** Save data from cache into the corresponding external object(s).
      * @note This task COULD be performed in other than GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) RT_OVERRIDE;

    /** Defines TAB order for passed @a pWidget. */
    virtual void setOrderAfter(QWidget *pWidget) RT_OVERRIDE;

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

    /** Performs final page polishing. */
    virtual void polishPage() RT_OVERRIDE;

private slots:

    /** Handles encryption cipher change. */
    void sltHandleEncryptionCipherChanged();
    /** Handles encryption password change. */
    void sltHandleEncryptionPasswordChanged();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares 'Basic' tab. */
    void prepareTabBasic();
    /** Prepares 'Advanced' tab. */
    void prepareTabAdvanced();
    /** Prepares 'Description' tab. */
    void prepareTabDescription();
    /** Prepares 'Encryption' tab. */
    void prepareTabEncryption();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing general data from cache. */
    bool saveData();
    /** Saves existing 'Basic' data from cache. */
    bool saveBasicData();
    /** Saves existing 'Advanced' data from cache. */
    bool saveAdvancedData();
    /** Saves existing 'Description' data from cache. */
    bool saveDescriptionData();
    /** Saves existing 'Encryption' data from cache. */
    bool saveEncryptionData();

    /** Holds whether HW virtualization extension is enabled. */
    bool  m_fHWVirtExEnabled;

    /** Holds whether the encryption cipher was changed.
      * We are holding that argument here because we do not know
      * the old <i>cipher</i> for sure to compare the new one with. */
    bool  m_fEncryptionCipherChanged;
    /** Holds whether the encryption password was changed.
      * We are holding that argument here because we do not know
      * the old <i>password</i> at all to compare the new one with. */
    bool  m_fEncryptionPasswordChanged;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineGeneral *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the tab-widget instance. */
        QITabWidget *m_pTabWidget;

        /** Holds the 'Basic' tab instance. */
        QWidget               *m_pTabBasic;
        /** Holds the name and system editor instance. */
        UINameAndSystemEditor *m_pEditorNameAndSystem;

        /** Holds the 'Advanced' tab instance. */
        QWidget                 *m_pTabAdvanced;
        /** Holds the snapshot folder editor instance. */
        UISnapshotFolderEditor  *m_pEditorSnapshotFolder;
        /** Holds the shared clipboard editor instance. */
        UISharedClipboardEditor *m_pEditorClipboard;
        /** Holds the drag and drop editor instance. */
        UIDragAndDropEditor     *m_pEditorDragAndDrop;

        /** Holds the 'Description' tab instance. */
        QWidget                    *m_pTabDescription;
        /** Holds the description editor instance. */
        UIMachineDescriptionEditor *m_pEditorDescription;

        /** Holds the 'Encryption' tab instance. */
        QWidget                        *m_pTabEncryption;
        /** Holds the cipher settings editor instance. */
        UIDiskEncryptionSettingsEditor *m_pEditorDiskEncryptionSettings;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsGeneral_h */
