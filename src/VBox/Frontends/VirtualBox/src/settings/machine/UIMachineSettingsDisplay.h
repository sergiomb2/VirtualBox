/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsDisplay class declaration.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsDisplay_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsDisplay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* COM includes: */
#include "CGuestOSType.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QStackedLayout;
class QIAdvancedSlider;
class QITabWidget;
class UIActionPool;
struct UIDataSettingsMachineDisplay;
class UIFilePathSelector;
class UIFilmContainer;
class UIGraphicsControllerEditor;
#ifdef VBOX_WITH_3D_ACCELERATION
class UIMachineDisplayScreenFeaturesEditor;
#endif
class UIMonitorCountEditor;
class UIScaleFactorEditor;
class UIVideoMemoryEditor;
typedef UISettingsCache<UIDataSettingsMachineDisplay> UISettingsCacheMachineDisplay;

/** Machine settings: Display page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsDisplay : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs Display settings page. */
    UIMachineSettingsDisplay();
    /** Destructs Display settings page. */
    virtual ~UIMachineSettingsDisplay() RT_OVERRIDE;

    /** Defines @a comGuestOSType. */
    void setGuestOSType(CGuestOSType comGuestOSType);

#ifdef VBOX_WITH_3D_ACCELERATION
    /** Returns whether 3D Acceleration is enabled. */
    bool isAcceleration3DSelected() const;
#endif

    /** Returns recommended graphics controller type. */
    KGraphicsControllerType graphicsControllerTypeRecommended() const;
    /** Returns current graphics controller type. */
    KGraphicsControllerType graphicsControllerTypeCurrent() const;

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const RT_OVERRIDE;

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) RT_OVERRIDE;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() RT_OVERRIDE;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() RT_OVERRIDE;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
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

    /** Handles monitor count change. */
    void sltHandleMonitorCountChange();
    /** Handles Graphics Controller combo change. */
    void sltHandleGraphicsControllerComboChange();
#ifdef VBOX_WITH_3D_ACCELERATION
    /** Handles 3D Acceleration feature state change. */
    void sltHandle3DAccelerationFeatureStateChange();
#endif

    /** Handles recording toggle. */
    void sltHandleRecordingCheckboxToggle();
    /** Handles recording frame size change. */
    void sltHandleRecordingVideoFrameSizeComboboxChange();
    /** Handles recording frame width change. */
    void sltHandleRecordingVideoFrameWidthEditorChange();
    /** Handles recording frame height change. */
    void sltHandleRecordingVideoFrameHeightEditorChange();
    /** Handles recording frame rate slider change. */
    void sltHandleRecordingVideoFrameRateSliderChange();
    /** Handles recording frame rate editor change. */
    void sltHandleRecordingVideoFrameRateEditorChange();
    /** Handles recording quality slider change. */
    void sltHandleRecordingVideoQualitySliderChange();
    /** Handles recording bit-rate editor change. */
    void sltHandleRecordingVideoBitRateEditorChange();
    void sltHandleRecordingComboBoxChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares 'Screen' tab. */
    void prepareTabScreen();
    /** Prepares 'Remote Display' tab. */
    void prepareTabRemoteDisplay();
    /** Prepares 'Recording' tab. */
    void prepareTabRecording();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Repopulates auth type combo-box. */
    void repopulateComboAuthType();

    /** Returns whether the VRAM requirements are important. */
    bool shouldWeWarnAboutLowVRAM();

    /** Searches for corresponding frame size preset. */
    void lookForCorrespondingFrameSizePreset();
    /** Updates guest-screen count. */
    void updateGuestScreenCount();
    /** Updates recording file size hint. */
    void updateRecordingFileSizeHint();
    /** Searches for the @a data field in corresponding @a pComboBox. */
    static void lookForCorrespondingPreset(QComboBox *pComboBox, const QVariant &data);
    /** Calculates recording video bit-rate for passed @a iFrameWidth, @a iFrameHeight, @a iFrameRate and @a iQuality. */
    static int calculateBitRate(int iFrameWidth, int iFrameHeight, int iFrameRate, int iQuality);
    /** Calculates recording video quality for passed @a iFrameWidth, @a iFrameHeight, @a iFrameRate and @a iBitRate. */
    static int calculateQuality(int iFrameWidth, int iFrameHeight, int iFrameRate, int iBitRate);
    /** Saves existing data from cache. */
    bool saveData();
    /** Saves existing 'Screen' data from cache. */
    bool saveScreenData();
    /** Saves existing 'Remote Display' data from cache. */
    bool saveRemoteDisplayData();
    /** Saves existing 'Recording' data from cache. */
    bool saveRecordingData();
    /** Decide which of the recording related widgets are to be disabled/enabled. */
    void enableDisableRecordingWidgets();

    /** Holds the guest OS type ID. */
    CGuestOSType  m_comGuestOSType;
#ifdef VBOX_WITH_3D_ACCELERATION
    /** Holds whether the guest OS supports WDDM. */
    bool          m_fWddmModeSupported;
#endif
    /** Holds recommended graphics controller type. */
    KGraphicsControllerType  m_enmGraphicsControllerTypeRecommended;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineDisplay *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the tab-widget instance. */
        QITabWidget *m_pTabWidget;

        /** Holds the 'Screen' tab instance. */
        QWidget                              *m_pTabScreen;
        /** Holds the video memory size editor instance. */
        UIVideoMemoryEditor                  *m_pEditorVideoMemorySize;
        /** Holds the monitor count spinbox instance. */
        UIMonitorCountEditor                 *m_pEditorMonitorCount;
        /** Holds the scale factor editor instance. */
        UIScaleFactorEditor                  *m_pEditorScaleFactor;
        /** Holds the graphics controller editor instance. */
        UIGraphicsControllerEditor           *m_pEditorGraphicsController;
#ifdef VBOX_WITH_3D_ACCELERATION
        /** Holds the display screen features editor instance. */
        UIMachineDisplayScreenFeaturesEditor *m_pEditorDisplayScreenFeatures;
#endif

        /** Holds the 'Remote Display' tab instance. */
        QWidget   *m_pTabRemoteDisplay;
        /** Holds the remote display check-box instance. */
        QCheckBox *m_pCheckboxRemoteDisplay;
        /** Holds the remote display settings widget instance. */
        QWidget   *m_pWidgetRemoteDisplaySettings;
        /** Holds the remote display port label instance. */
        QLabel    *m_pLabelRemoteDisplayPort;
        /** Holds the remote display port editor instance. */
        QLineEdit *m_pEditorRemoteDisplayPort;
        /** Holds the remote display port auth method label instance. */
        QLabel    *m_pLabelRemoteDisplayAuthMethod;
        /** Holds the remote display port auth method combo instance. */
        QComboBox *m_pComboRemoteDisplayAuthMethod;
        /** Holds the remote display timeout label instance. */
        QLabel    *m_pLabelRemoteDisplayTimeout;
        /** Holds the remote display timeout editor instance. */
        QLineEdit *m_pEditorRemoteDisplayTimeout;
        /** Holds the remote display options label instance. */
        QLabel    *m_pLabelRemoteDisplayOptions;
        /** Holds the remote display multiple connection check-box instance. */
        QCheckBox *m_pCheckboxMultipleConn;

        /** Holds the 'Recording' tab instance. */
        QWidget            *m_pTabRecording;
        /** Holds the recording check-box instance. */
        QCheckBox          *m_pCheckboxRecording;
        /** Holds the recording settings widget instance. */
        QWidget            *m_pWidgetRecordingSettings;
        /** Holds the recording mode label instance. */
        QLabel             *m_pLabelRecordingMode;
        /** Holds the recording mode combo instance. */
        QComboBox          *m_pComboRecordingMode;
        /** Holds the recording file path label instance. */
        QLabel             *m_pLabelRecordingFilePath;
        /** Holds the recording file path editor instance. */
        UIFilePathSelector *m_pEditorRecordingFilePath;
        /** Holds the recording frame size label instance. */
        QLabel             *m_pLabelRecordingFrameSize;
        /** Holds the recording frame size combo instance. */
        QComboBox          *m_pComboRecordingFrameSize;
        /** Holds the recording frame width spinbox instance. */
        QSpinBox           *m_pSpinboxRecordingFrameWidth;
        /** Holds the recording frame height spinbox instance. */
        QSpinBox           *m_pSpinboxRecordingFrameHeight;
        /** Holds the recording frame rate label instance. */
        QLabel             *m_pLabelRecordingFrameRate;
        /** Holds the recording frame rate settings widget instance. */
        QWidget            *m_pWidgetRecordingFrameRateSettings;
        /** Holds the recording frame rate slider instance. */
        QIAdvancedSlider   *m_pSliderRecordingFrameRate;
        /** Holds the recording frame rate spinbox instance. */
        QSpinBox           *m_pSpinboxRecordingFrameRate;
        /** Holds the recording frame rate min label instance. */
        QLabel             *m_pLabelRecordingFrameRateMin;
        /** Holds the recording frame rate max label instance. */
        QLabel             *m_pLabelRecordingFrameRateMax;
        /** Holds the recording video quality label instance. */
        QLabel             *m_pLabelRecordingVideoQuality;
        /** Holds the recording video quality settings widget instance. */
        QWidget            *m_pWidgetRecordingVideoQualitySettings;
        /** Holds the recording video quality slider instance. */
        QIAdvancedSlider   *m_pSliderRecordingVideoQuality;
        /** Holds the recording video quality spinbox instance. */
        QSpinBox           *m_pSpinboxRecordingVideoQuality;
        /** Holds the recording video quality min label instance. */
        QLabel             *m_pLabelRecordingVideoQualityMin;
        /** Holds the recording video quality med label instance. */
        QLabel             *m_pLabelRecordingVideoQualityMed;
        /** Holds the recording video quality max label instance. */
        QLabel             *m_pLabelRecordingVideoQualityMax;
        /** Holds the recording audio quality label instance. */
        QLabel             *m_pLabelRecordingAudioQuality;
        /** Holds the recording audio quality settings widget instance. */
        QWidget            *m_pWidgetRecordingAudioQualitySettings;
        /** Holds the recording audio quality slider instance. */
        QIAdvancedSlider   *m_pSliderRecordingAudioQuality;
        /** Holds the recording audio quality min label instance. */
        QLabel             *m_pLabelRecordingAudioQualityMin;
        /** Holds the recording audio quality med label instance. */
        QLabel             *m_pLabelRecordingAudioQualityMed;
        /** Holds the recording audio quality max label instance. */
        QLabel             *m_pLabelRecordingAudioQualityMax;
        /** Holds the recording size hint label instance. */
        QLabel             *m_pLabelRecordingSizeHint;
        /** Holds the recording screens label instance. */
        QLabel             *m_pLabelRecordingScreens;
        /** Holds the recording screens scroller instance. */
        UIFilmContainer    *m_pScrollerRecordingScreens;
   /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsDisplay_h */
