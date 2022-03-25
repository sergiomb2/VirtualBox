/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsDisplay class implementation.
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

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStackedLayout>

/* GUI includes: */
#include "QIAdvancedSlider.h"
#include "QITabWidget.h"
#include "QIWidgetValidator.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIErrorString.h"
#include "UIExtraDataManager.h"
#include "UIFilePathSelector.h"
#include "UIFilmContainer.h"
#include "UIGraphicsControllerEditor.h"
#ifdef VBOX_WITH_3D_ACCELERATION
# include "UIMachineDisplayScreenFeaturesEditor.h"
#endif
#include "UIMachineSettingsDisplay.h"
#include "UIMonitorCountEditor.h"
#include "UIScaleFactorEditor.h"
#include "UITranslator.h"
#include "UIVideoMemoryEditor.h"
#include "UIVRDESettingsEditor.h"

/* COM includes: */
#include "CGraphicsAdapter.h"
#include "CRecordingSettings.h"
#include "CRecordingScreenSettings.h"
#include "CExtPack.h"
#include "CExtPackManager.h"
#include "CVRDEServer.h"


#define VIDEO_CAPTURE_BIT_RATE_MIN 32
#define VIDEO_CAPTURE_BIT_RATE_MAX 2048


/** Machine settings: Display page data structure. */
struct UIDataSettingsMachineDisplay
{
    /** Constructs data. */
    UIDataSettingsMachineDisplay()
        : m_iCurrentVRAM(0)
        , m_cGuestScreenCount(0)
        , m_graphicsControllerType(KGraphicsControllerType_Null)
#ifdef VBOX_WITH_3D_ACCELERATION
        , m_f3dAccelerationEnabled(false)
#endif
        , m_fRemoteDisplayServerSupported(false)
        , m_fRemoteDisplayServerEnabled(false)
        , m_strRemoteDisplayPort(QString())
        , m_remoteDisplayAuthType(KAuthType_Null)
        , m_uRemoteDisplayTimeout(0)
        , m_fRemoteDisplayMultiConnAllowed(false)
        , m_fRecordingEnabled(false)
        , m_strRecordingFolder(QString())
        , m_strRecordingFilePath(QString())
        , m_iRecordingVideoFrameWidth(0)
        , m_iRecordingVideoFrameHeight(0)
        , m_iRecordingVideoFrameRate(0)
        , m_iRecordingVideoBitRate(0)
        , m_strRecordingVideoOptions(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineDisplay &other) const
    {
        return true
               && (m_iCurrentVRAM == other.m_iCurrentVRAM)
               && (m_cGuestScreenCount == other.m_cGuestScreenCount)
               && (m_scaleFactors == other.m_scaleFactors)
               && (m_graphicsControllerType == other.m_graphicsControllerType)
#ifdef VBOX_WITH_3D_ACCELERATION
               && (m_f3dAccelerationEnabled == other.m_f3dAccelerationEnabled)
#endif
               && (m_fRemoteDisplayServerSupported == other.m_fRemoteDisplayServerSupported)
               && (m_fRemoteDisplayServerEnabled == other.m_fRemoteDisplayServerEnabled)
               && (m_strRemoteDisplayPort == other.m_strRemoteDisplayPort)
               && (m_remoteDisplayAuthType == other.m_remoteDisplayAuthType)
               && (m_uRemoteDisplayTimeout == other.m_uRemoteDisplayTimeout)
               && (m_fRemoteDisplayMultiConnAllowed == other.m_fRemoteDisplayMultiConnAllowed)
               && (m_fRecordingEnabled == other.m_fRecordingEnabled)
               && (m_strRecordingFilePath == other.m_strRecordingFilePath)
               && (m_iRecordingVideoFrameWidth == other.m_iRecordingVideoFrameWidth)
               && (m_iRecordingVideoFrameHeight == other.m_iRecordingVideoFrameHeight)
               && (m_iRecordingVideoFrameRate == other.m_iRecordingVideoFrameRate)
               && (m_iRecordingVideoBitRate == other.m_iRecordingVideoBitRate)
               && (m_vecRecordingScreens == other.m_vecRecordingScreens)
               && (m_strRecordingVideoOptions == other.m_strRecordingVideoOptions)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineDisplay &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineDisplay &other) const { return !equal(other); }

    /** Recording options. */
    enum RecordingOption
    {
        RecordingOption_Unknown,
        RecordingOption_AC,
        RecordingOption_VC,
        RecordingOption_AC_Profile
    };

    /** Returns enum value corresponding to passed @a strKey. */
    static RecordingOption toRecordingOptionKey(const QString &strKey)
    {
        /* Compare case-sensitive: */
        QMap<QString, RecordingOption> keys;
        keys["ac_enabled"] = RecordingOption_AC;
        keys["vc_enabled"] = RecordingOption_VC;
        keys["ac_profile"] = RecordingOption_AC_Profile;
        /* Return known value or RecordingOption_Unknown otherwise: */
        return keys.value(strKey, RecordingOption_Unknown);
    }

    /** Returns string representation for passed enum @a enmKey. */
    static QString fromRecordingOptionKey(RecordingOption enmKey)
    {
        /* Compare case-sensitive: */
        QMap<RecordingOption, QString> values;
        values[RecordingOption_AC] = "ac_enabled";
        values[RecordingOption_VC] = "vc_enabled";
        values[RecordingOption_AC_Profile] = "ac_profile";
        /* Return known value or QString() otherwise: */
        return values.value(enmKey);
    }

    /** Parses recording options. */
    static void parseRecordingOptions(const QString &strOptions,
                                      QList<RecordingOption> &outKeys,
                                      QStringList &outValues)
    {
        outKeys.clear();
        outValues.clear();
        const QStringList aPairs = strOptions.split(',');
        foreach (const QString &strPair, aPairs)
        {
            const QStringList aPair = strPair.split('=');
            if (aPair.size() != 2)
                continue;
            const RecordingOption enmKey = toRecordingOptionKey(aPair.value(0));
            if (enmKey == RecordingOption_Unknown)
                continue;
            outKeys << enmKey;
            outValues << aPair.value(1);
        }
    }

    /** Serializes recording options. */
    static void serializeRecordingOptions(const QList<RecordingOption> &inKeys,
                                          const QStringList &inValues,
                                          QString &strOptions)
    {
        QStringList aPairs;
        for (int i = 0; i < inKeys.size(); ++i)
        {
            QStringList aPair;
            aPair << fromRecordingOptionKey(inKeys.value(i));
            aPair << inValues.value(i);
            aPairs << aPair.join('=');
        }
        strOptions = aPairs.join(',');
    }

    /** Returns whether passed Recording @a enmOption is enabled. */
    static bool isRecordingOptionEnabled(const QString &strOptions,
                                         RecordingOption enmOption)
    {
        QList<RecordingOption> aKeys;
        QStringList aValues;
        parseRecordingOptions(strOptions, aKeys, aValues);
        int iIndex = aKeys.indexOf(enmOption);
        if (iIndex == -1)
            return false; /* If option is missing, assume disabled (false). */
        if (aValues.value(iIndex).compare("true", Qt::CaseInsensitive) == 0)
            return true;
        return false;
    }

    /** Searches for ac_profile and return 1 for "low", 2 for "med", and 3 for "high". Returns 2
        if ac_profile is missing */
    static int getAudioQualityFromOptions(const QString &strOptions)
    {
        QList<RecordingOption> aKeys;
        QStringList aValues;
        parseRecordingOptions(strOptions, aKeys, aValues);
        int iIndex = aKeys.indexOf(RecordingOption_AC_Profile);
        if (iIndex == -1)
            return 2;
        if (aValues.value(iIndex).compare("low", Qt::CaseInsensitive) == 0)
            return 1;
        if (aValues.value(iIndex).compare("high", Qt::CaseInsensitive) == 0)
            return 3;
        return 2;
    }

    /** Sets the video recording options for @a enmOptions to @a values. */
    static QString setRecordingOptions(const QString &strOptions,
                                       const QVector<RecordingOption> &enmOptions,
                                       const QStringList &values)
    {
        if (enmOptions.size() != values.size())
            return QString();
        QList<RecordingOption> aKeys;
        QStringList aValues;
        parseRecordingOptions(strOptions, aKeys, aValues);
        for(int i = 0; i < values.size(); ++i)
        {
            QString strValue = values[i];
            int iIndex = aKeys.indexOf(enmOptions[i]);
            if (iIndex == -1)
            {
                aKeys << enmOptions[i];
                aValues << strValue;
            }
            else
            {
                aValues[iIndex] = strValue;
            }
        }
        QString strResult;
        serializeRecordingOptions(aKeys, aValues, strResult);
        return strResult;
    }

    /** Holds the video RAM amount. */
    int                      m_iCurrentVRAM;
    /** Holds the guest screen count. */
    int                      m_cGuestScreenCount;
    /** Holds the guest screen scale-factor. */
    QList<double>            m_scaleFactors;
    /** Holds the graphics controller type. */
    KGraphicsControllerType  m_graphicsControllerType;
#ifdef VBOX_WITH_3D_ACCELERATION
    /** Holds whether the 3D acceleration is enabled. */
    bool                     m_f3dAccelerationEnabled;
#endif
    /** Holds whether the remote display server is supported. */
    bool                     m_fRemoteDisplayServerSupported;
    /** Holds whether the remote display server is enabled. */
    bool                     m_fRemoteDisplayServerEnabled;
    /** Holds the remote display server port. */
    QString                  m_strRemoteDisplayPort;
    /** Holds the remote display server auth type. */
    KAuthType                m_remoteDisplayAuthType;
    /** Holds the remote display server timeout. */
    ulong                    m_uRemoteDisplayTimeout;
    /** Holds whether the remote display server allows multiple connections. */
    bool                     m_fRemoteDisplayMultiConnAllowed;

    /** Holds whether recording is enabled. */
    bool m_fRecordingEnabled;
    /** Holds the recording folder. */
    QString m_strRecordingFolder;
    /** Holds the recording file path. */
    QString m_strRecordingFilePath;
    /** Holds the recording frame width. */
    int m_iRecordingVideoFrameWidth;
    /** Holds the recording frame height. */
    int m_iRecordingVideoFrameHeight;
    /** Holds the recording frame rate. */
    int m_iRecordingVideoFrameRate;
    /** Holds the recording bit rate. */
    int m_iRecordingVideoBitRate;
    /** Holds which of the guest screens should be recorded. */
    QVector<BOOL> m_vecRecordingScreens;
    /** Holds the video recording options. */
    QString m_strRecordingVideoOptions;
};


UIMachineSettingsDisplay::UIMachineSettingsDisplay()
    : m_comGuestOSType(CGuestOSType())
#ifdef VBOX_WITH_3D_ACCELERATION
    , m_fWddmModeSupported(false)
#endif
    , m_enmGraphicsControllerTypeRecommended(KGraphicsControllerType_Null)
    , m_pCache(0)
    , m_pTabWidget(0)
    , m_pTabScreen(0)
    , m_pEditorVideoMemorySize(0)
    , m_pEditorMonitorCount(0)
    , m_pEditorScaleFactor(0)
    , m_pEditorGraphicsController(0)
#ifdef VBOX_WITH_3D_ACCELERATION
    , m_pEditorDisplayScreenFeatures(0)
#endif
    , m_pTabRemoteDisplay(0)
    , m_pEditorVRDESettings(0)
    , m_pTabRecording(0)
    , m_pCheckboxRecording(0)
    , m_pWidgetRecordingSettings(0)
    , m_pLabelRecordingMode(0)
    , m_pComboRecordingMode(0)
    , m_pLabelRecordingFilePath(0)
    , m_pEditorRecordingFilePath(0)
    , m_pLabelRecordingFrameSize(0)
    , m_pComboRecordingFrameSize(0)
    , m_pSpinboxRecordingFrameWidth(0)
    , m_pSpinboxRecordingFrameHeight(0)
    , m_pLabelRecordingFrameRate(0)
    , m_pWidgetRecordingFrameRateSettings(0)
    , m_pSliderRecordingFrameRate(0)
    , m_pSpinboxRecordingFrameRate(0)
    , m_pLabelRecordingFrameRateMin(0)
    , m_pLabelRecordingFrameRateMax(0)
    , m_pLabelRecordingVideoQuality(0)
    , m_pWidgetRecordingVideoQualitySettings(0)
    , m_pSliderRecordingVideoQuality(0)
    , m_pSpinboxRecordingVideoQuality(0)
    , m_pLabelRecordingVideoQualityMin(0)
    , m_pLabelRecordingVideoQualityMed(0)
    , m_pLabelRecordingVideoQualityMax(0)
    , m_pLabelRecordingAudioQuality(0)
    , m_pWidgetRecordingAudioQualitySettings(0)
    , m_pSliderRecordingAudioQuality(0)
    , m_pLabelRecordingAudioQualityMin(0)
    , m_pLabelRecordingAudioQualityMed(0)
    , m_pLabelRecordingAudioQualityMax(0)
    , m_pLabelRecordingSizeHint(0)
    , m_pLabelRecordingScreens(0)
    , m_pScrollerRecordingScreens(0)
{
    prepare();
}

UIMachineSettingsDisplay::~UIMachineSettingsDisplay()
{
    cleanup();
}

void UIMachineSettingsDisplay::setGuestOSType(CGuestOSType comGuestOSType)
{
    /* Check if guest OS type changed: */
    if (m_comGuestOSType == comGuestOSType)
        return;

    /* Remember new guest OS type: */
    m_comGuestOSType = comGuestOSType;
    m_pEditorVideoMemorySize->setGuestOSType(m_comGuestOSType);

#ifdef VBOX_WITH_3D_ACCELERATION
    /* Check if WDDM mode supported by the guest OS type: */
    const QString strGuestOSTypeId = m_comGuestOSType.isNotNull() ? m_comGuestOSType.GetId() : QString();
    m_fWddmModeSupported = UICommon::isWddmCompatibleOsType(strGuestOSTypeId);
    m_pEditorVideoMemorySize->set3DAccelerationSupported(m_fWddmModeSupported);
#endif
    /* Acquire recommended graphics controller type: */
    m_enmGraphicsControllerTypeRecommended = m_comGuestOSType.GetRecommendedGraphicsController();

    /* Revalidate: */
    revalidate();
}

#ifdef VBOX_WITH_3D_ACCELERATION
bool UIMachineSettingsDisplay::isAcceleration3DSelected() const
{
    return m_pEditorDisplayScreenFeatures->isEnabled3DAcceleration();
}
#endif /* VBOX_WITH_3D_ACCELERATION */

KGraphicsControllerType UIMachineSettingsDisplay::graphicsControllerTypeRecommended() const
{
    return   m_pEditorGraphicsController->supportedValues().contains(m_enmGraphicsControllerTypeRecommended)
           ? m_enmGraphicsControllerTypeRecommended
           : graphicsControllerTypeCurrent();
}

KGraphicsControllerType UIMachineSettingsDisplay::graphicsControllerTypeCurrent() const
{
    return m_pEditorGraphicsController->value();
}

bool UIMachineSettingsDisplay::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsDisplay::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old display data: */
    UIDataSettingsMachineDisplay oldDisplayData;

    /* Check whether graphics adapter is valid: */
    const CGraphicsAdapter &comGraphics = m_machine.GetGraphicsAdapter();
    if (!comGraphics.isNull())
    {
        /* Gather old 'Screen' data: */
        oldDisplayData.m_iCurrentVRAM = comGraphics.GetVRAMSize();
        oldDisplayData.m_cGuestScreenCount = comGraphics.GetMonitorCount();
        oldDisplayData.m_scaleFactors = gEDataManager->scaleFactors(m_machine.GetId());
        oldDisplayData.m_graphicsControllerType = comGraphics.GetGraphicsControllerType();
#ifdef VBOX_WITH_3D_ACCELERATION
        oldDisplayData.m_f3dAccelerationEnabled = comGraphics.GetAccelerate3DEnabled();
#endif
    }

    /* Check whether remote display server is valid: */
    const CVRDEServer &vrdeServer = m_machine.GetVRDEServer();
    oldDisplayData.m_fRemoteDisplayServerSupported = !vrdeServer.isNull();
    if (!vrdeServer.isNull())
    {
        /* Gather old 'Remote Display' data: */
        oldDisplayData.m_fRemoteDisplayServerEnabled = vrdeServer.GetEnabled();
        oldDisplayData.m_strRemoteDisplayPort = vrdeServer.GetVRDEProperty("TCP/Ports");
        oldDisplayData.m_remoteDisplayAuthType = vrdeServer.GetAuthType();
        oldDisplayData.m_uRemoteDisplayTimeout = vrdeServer.GetAuthTimeout();
        oldDisplayData.m_fRemoteDisplayMultiConnAllowed = vrdeServer.GetAllowMultiConnection();
    }

    /* Gather old 'Recording' data: */
    CRecordingSettings recordingSettings = m_machine.GetRecordingSettings();
    Assert(recordingSettings.isNotNull());
    oldDisplayData.m_fRecordingEnabled = recordingSettings.GetEnabled();

    /* For now we're using the same settings for all screens; so get settings from screen 0 and work with that. */
    CRecordingScreenSettings recordingScreen0Settings = recordingSettings.GetScreenSettings(0);
    if (!recordingScreen0Settings.isNull())
    {
        oldDisplayData.m_strRecordingFolder = QFileInfo(m_machine.GetSettingsFilePath()).absolutePath();
        oldDisplayData.m_strRecordingFilePath = recordingScreen0Settings.GetFilename();
        oldDisplayData.m_iRecordingVideoFrameWidth = recordingScreen0Settings.GetVideoWidth();
        oldDisplayData.m_iRecordingVideoFrameHeight = recordingScreen0Settings.GetVideoHeight();
        oldDisplayData.m_iRecordingVideoFrameRate = recordingScreen0Settings.GetVideoFPS();
        oldDisplayData.m_iRecordingVideoBitRate = recordingScreen0Settings.GetVideoRate();
        oldDisplayData.m_strRecordingVideoOptions = recordingScreen0Settings.GetOptions();
    }

    CRecordingScreenSettingsVector recordingScreenSettingsVector = recordingSettings.GetScreens();
    oldDisplayData.m_vecRecordingScreens.resize(recordingScreenSettingsVector.size());
    for (int iScreenIndex = 0; iScreenIndex < recordingScreenSettingsVector.size(); ++iScreenIndex)
    {
        CRecordingScreenSettings recordingScreenSettings = recordingScreenSettingsVector.at(iScreenIndex);
        if (!recordingScreenSettings.isNull())
            oldDisplayData.m_vecRecordingScreens[iScreenIndex] = recordingScreenSettings.GetEnabled();
    }

    /* Cache old display data: */
    m_pCache->cacheInitialData(oldDisplayData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsDisplay::getFromCache()
{
    /* Get old display data from cache: */
    const UIDataSettingsMachineDisplay &oldDisplayData = m_pCache->base();

    /* Load old 'Screen' data from cache: */
    m_pEditorMonitorCount->setValue(oldDisplayData.m_cGuestScreenCount);
    m_pEditorScaleFactor->setScaleFactors(oldDisplayData.m_scaleFactors);
    m_pEditorScaleFactor->setMonitorCount(oldDisplayData.m_cGuestScreenCount);
    m_pEditorGraphicsController->setValue(oldDisplayData.m_graphicsControllerType);
#ifdef VBOX_WITH_3D_ACCELERATION
    m_pEditorDisplayScreenFeatures->setEnable3DAcceleration(oldDisplayData.m_f3dAccelerationEnabled);
#endif
    /* Push required value to m_pEditorVideoMemorySize: */
    sltHandleMonitorCountChange();
    sltHandleGraphicsControllerComboChange();
#ifdef VBOX_WITH_3D_ACCELERATION
    sltHandle3DAccelerationFeatureStateChange();
#endif
    // Should be the last one for this tab, since it depends on some of others:
    m_pEditorVideoMemorySize->setValue(oldDisplayData.m_iCurrentVRAM);

    /* If remote display server is supported: */
    if (oldDisplayData.m_fRemoteDisplayServerSupported)
    {
        /* Load old 'Remote Display' data from cache: */
        m_pEditorVRDESettings->setFeatureEnabled(oldDisplayData.m_fRemoteDisplayServerEnabled);
        m_pEditorVRDESettings->setPort(oldDisplayData.m_strRemoteDisplayPort);
        m_pEditorVRDESettings->setAuthType(oldDisplayData.m_remoteDisplayAuthType);
        m_pEditorVRDESettings->setTimeout(QString::number(oldDisplayData.m_uRemoteDisplayTimeout));
        m_pEditorVRDESettings->setMultipleConnectionsAllowed(oldDisplayData.m_fRemoteDisplayMultiConnAllowed);
    }

    /* Load old 'Recording' data from cache: */
    m_pCheckboxRecording->setChecked(oldDisplayData.m_fRecordingEnabled);
    m_pEditorRecordingFilePath->setInitialPath(oldDisplayData.m_strRecordingFolder);
    m_pEditorRecordingFilePath->setPath(oldDisplayData.m_strRecordingFilePath);
    m_pSpinboxRecordingFrameWidth->setValue(oldDisplayData.m_iRecordingVideoFrameWidth);
    m_pSpinboxRecordingFrameHeight->setValue(oldDisplayData.m_iRecordingVideoFrameHeight);
    m_pSpinboxRecordingFrameRate->setValue(oldDisplayData.m_iRecordingVideoFrameRate);
    m_pSpinboxRecordingVideoQuality->setValue(oldDisplayData.m_iRecordingVideoBitRate);
    m_pScrollerRecordingScreens->setValue(oldDisplayData.m_vecRecordingScreens);

    /* Load data from old 'Recording option': */
    bool fRecordAudio = UIDataSettingsMachineDisplay::isRecordingOptionEnabled(oldDisplayData.m_strRecordingVideoOptions,
                                                                                UIDataSettingsMachineDisplay::RecordingOption_AC);
    bool fRecordVideo = UIDataSettingsMachineDisplay::isRecordingOptionEnabled(oldDisplayData.m_strRecordingVideoOptions,
                                                                                UIDataSettingsMachineDisplay::RecordingOption_VC);
    if (fRecordAudio && fRecordVideo)
        m_pComboRecordingMode->setCurrentIndex(m_pComboRecordingMode->findText(gpConverter->toString(UISettingsDefs::RecordingMode_VideoAudio)));
    else if (fRecordAudio && !fRecordVideo)
        m_pComboRecordingMode->setCurrentIndex(m_pComboRecordingMode->findText(gpConverter->toString(UISettingsDefs::RecordingMode_AudioOnly)));
    else
        m_pComboRecordingMode->setCurrentIndex(m_pComboRecordingMode->findText(gpConverter->toString(UISettingsDefs::RecordingMode_VideoOnly)));

    m_pSliderRecordingAudioQuality->setValue(UIDataSettingsMachineDisplay::getAudioQualityFromOptions(oldDisplayData.m_strRecordingVideoOptions));

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsDisplay::putToCache()
{
    /* Prepare new display data: */
    UIDataSettingsMachineDisplay newDisplayData;

    /* Gather new 'Screen' data: */
    newDisplayData.m_iCurrentVRAM = m_pEditorVideoMemorySize->value();
    newDisplayData.m_cGuestScreenCount = m_pEditorMonitorCount->value();
    newDisplayData.m_scaleFactors = m_pEditorScaleFactor->scaleFactors();
    newDisplayData.m_graphicsControllerType = m_pEditorGraphicsController->value();
#ifdef VBOX_WITH_3D_ACCELERATION
    newDisplayData.m_f3dAccelerationEnabled = m_pEditorDisplayScreenFeatures->isEnabled3DAcceleration();
#endif
    /* If remote display server is supported: */
    newDisplayData.m_fRemoteDisplayServerSupported = m_pCache->base().m_fRemoteDisplayServerSupported;
    if (newDisplayData.m_fRemoteDisplayServerSupported)
    {
        /* Gather new 'Remote Display' data: */
        newDisplayData.m_fRemoteDisplayServerEnabled = m_pEditorVRDESettings->isFeatureEnabled();
        newDisplayData.m_strRemoteDisplayPort = m_pEditorVRDESettings->port();
        newDisplayData.m_remoteDisplayAuthType = m_pEditorVRDESettings->authType();
        newDisplayData.m_uRemoteDisplayTimeout = m_pEditorVRDESettings->timeout().toULong();
        newDisplayData.m_fRemoteDisplayMultiConnAllowed = m_pEditorVRDESettings->isMultipleConnectionsAllowed();
    }

    /* Gather new 'Recording' data: */
    newDisplayData.m_fRecordingEnabled = m_pCheckboxRecording->isChecked();
    newDisplayData.m_strRecordingFolder = m_pCache->base().m_strRecordingFolder;
    newDisplayData.m_strRecordingFilePath = m_pEditorRecordingFilePath->path();
    newDisplayData.m_iRecordingVideoFrameWidth = m_pSpinboxRecordingFrameWidth->value();
    newDisplayData.m_iRecordingVideoFrameHeight = m_pSpinboxRecordingFrameHeight->value();
    newDisplayData.m_iRecordingVideoFrameRate = m_pSpinboxRecordingFrameRate->value();
    newDisplayData.m_iRecordingVideoBitRate = m_pSpinboxRecordingVideoQuality->value();
    newDisplayData.m_vecRecordingScreens = m_pScrollerRecordingScreens->value();

    /* Update recording options */
    const UISettingsDefs::RecordingMode enmRecordingMode =
        gpConverter->fromString<UISettingsDefs::RecordingMode>(m_pComboRecordingMode->currentText());
    QStringList optionValues;
    /* Option value for video recording: */
    optionValues.push_back(     (enmRecordingMode == UISettingsDefs::RecordingMode_VideoAudio)
                             || (enmRecordingMode == UISettingsDefs::RecordingMode_VideoOnly)
                           ? "true" : "false");
    /* Option value for audio recording: */
    optionValues.push_back(     (enmRecordingMode == UISettingsDefs::RecordingMode_VideoAudio)
                             || (enmRecordingMode == UISettingsDefs::RecordingMode_AudioOnly)
                           ? "true" : "false");

    if (m_pSliderRecordingAudioQuality->value() == 1)
        optionValues.push_back("low");
    else if (m_pSliderRecordingAudioQuality->value() == 2)
        optionValues.push_back("med");
    else
        optionValues.push_back("high");

    QVector<UIDataSettingsMachineDisplay::RecordingOption> recordingOptionsVector;
    recordingOptionsVector.push_back(UIDataSettingsMachineDisplay::RecordingOption_VC);
    recordingOptionsVector.push_back(UIDataSettingsMachineDisplay::RecordingOption_AC);
    recordingOptionsVector.push_back(UIDataSettingsMachineDisplay::RecordingOption_AC_Profile);

    newDisplayData.m_strRecordingVideoOptions = UIDataSettingsMachineDisplay::setRecordingOptions(m_pCache->base().m_strRecordingVideoOptions,
                                                                                                   recordingOptionsVector,
                                                                                                   optionValues);

    /* Cache new display data: */
    m_pCache->cacheCurrentData(newDisplayData);
}

void UIMachineSettingsDisplay::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsDisplay::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Screen tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = UITranslator::removeAccelMark(m_pTabWidget->tabText(0));

        /* Video RAM amount test: */
        if (shouldWeWarnAboutLowVRAM() && !m_comGuestOSType.isNull())
        {
            quint64 uNeedBytes = UICommon::requiredVideoMemory(m_comGuestOSType.GetId(), m_pEditorMonitorCount->value());

            /* Basic video RAM amount test: */
            if ((quint64)m_pEditorVideoMemorySize->value() * _1M < uNeedBytes)
            {
                message.second << tr("The virtual machine is currently assigned less than <b>%1</b> of video memory "
                                     "which is the minimum amount required to switch to full-screen or seamless mode.")
                                     .arg(UITranslator::formatSize(uNeedBytes, 0, FormatSize_RoundUp));
            }
#ifdef VBOX_WITH_3D_ACCELERATION
            /* 3D acceleration video RAM amount test: */
            else if (m_pEditorDisplayScreenFeatures->isEnabled3DAcceleration() && m_fWddmModeSupported)
            {
                uNeedBytes = qMax(uNeedBytes, (quint64) 128 * _1M);
                if ((quint64)m_pEditorVideoMemorySize->value() * _1M < uNeedBytes)
                {
                    message.second << tr("The virtual machine is set up to use hardware graphics acceleration "
                                         "and the operating system hint is set to Windows Vista or later. "
                                         "For best performance you should set the machine's video memory to at least <b>%1</b>.")
                                         .arg(UITranslator::formatSize(uNeedBytes, 0, FormatSize_RoundUp));
                }
            }
#endif /* VBOX_WITH_3D_ACCELERATION */
        }

        /* Graphics controller type test: */
        if (!m_comGuestOSType.isNull())
        {
            if (graphicsControllerTypeCurrent() != graphicsControllerTypeRecommended())
            {
#ifdef VBOX_WITH_3D_ACCELERATION
                if (m_pEditorDisplayScreenFeatures->isEnabled3DAcceleration())
                    message.second << tr("The virtual machine is configured to use 3D acceleration. This will work only if you "
                                         "pick a different graphics controller (%1). Either disable 3D acceleration or switch "
                                         "to required graphics controller type. The latter will be done automatically if you "
                                         "confirm your changes.")
                                         .arg(gpConverter->toString(m_enmGraphicsControllerTypeRecommended));
                else
#endif /* VBOX_WITH_3D_ACCELERATION */
                    message.second << tr("The virtual machine is configured to use a graphics controller other than the "
                                         "recommended one (%1). Please consider switching unless you have a reason to keep the "
                                         "currently selected graphics controller.")
                                         .arg(gpConverter->toString(m_enmGraphicsControllerTypeRecommended));
            }
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Remote Display tab: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = UITranslator::removeAccelMark(m_pTabWidget->tabText(1));

        /* Extension Pack presence test: */
        if (m_pEditorVRDESettings->isFeatureEnabled())
        {
            CExtPackManager extPackManager = uiCommon().virtualBox().GetExtensionPackManager();
            if (!extPackManager.isNull() && !extPackManager.IsExtPackUsable(GUI_ExtPackName))
            {
                message.second << tr("Remote Display is currently enabled for this virtual machine. "
                                    "However, this requires the <i>%1</i> to be installed. "
                                    "Please install the Extension Pack from the VirtualBox download site as "
                                    "otherwise your VM will be started with Remote Display disabled.")
                                    .arg(GUI_ExtPackName);
            }
        }

        /* Check VRDE server port: */
        if (m_pEditorVRDESettings->port().trimmed().isEmpty())
        {
            message.second << tr("The VRDE server port value is not currently specified.");
            fPass = false;
        }

        /* Check VRDE server timeout: */
        if (m_pEditorVRDESettings->timeout().trimmed().isEmpty())
        {
            message.second << tr("The VRDE authentication timeout value is not currently specified.");
            fPass = false;
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Return result: */
    return fPass;
}

void UIMachineSettingsDisplay::setOrderAfter(QWidget *pWidget)
{
    /* Screen tab-order: */
    setTabOrder(pWidget, m_pTabWidget->focusProxy());
    setTabOrder(m_pTabWidget->focusProxy(), m_pEditorVideoMemorySize);
    setTabOrder(m_pEditorVideoMemorySize, m_pEditorMonitorCount);
    setTabOrder(m_pEditorMonitorCount, m_pEditorScaleFactor);
    setTabOrder(m_pEditorScaleFactor, m_pEditorGraphicsController);
#ifdef VBOX_WITH_3D_ACCELERATION
    setTabOrder(m_pEditorGraphicsController, m_pEditorDisplayScreenFeatures);
    setTabOrder(m_pEditorDisplayScreenFeatures, m_pEditorVRDESettings);
#else
    setTabOrder(m_pEditorGraphicsController, m_pEditorVRDESettings);
#endif

    /* Remote Display tab-order: */
    setTabOrder(m_pEditorVRDESettings, m_pCheckboxRecording);

    /* Recording tab-order: */
    setTabOrder(m_pCheckboxRecording, m_pEditorRecordingFilePath);
    setTabOrder(m_pEditorRecordingFilePath, m_pComboRecordingFrameSize);
    setTabOrder(m_pComboRecordingFrameSize, m_pSpinboxRecordingFrameWidth);
    setTabOrder(m_pSpinboxRecordingFrameWidth, m_pSpinboxRecordingFrameHeight);
    setTabOrder(m_pSpinboxRecordingFrameHeight, m_pSliderRecordingFrameRate);
    setTabOrder(m_pSliderRecordingFrameRate, m_pSpinboxRecordingFrameRate);
    setTabOrder(m_pSpinboxRecordingFrameRate, m_pSliderRecordingVideoQuality);
    setTabOrder(m_pSliderRecordingVideoQuality, m_pSpinboxRecordingVideoQuality);
}

void UIMachineSettingsDisplay::retranslateUi()
{
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabScreen), tr("&Screen"));
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabRemoteDisplay), tr("&Remote Display"));
    m_pCheckboxRecording->setToolTip(tr("When checked, VirtualBox will record the virtual machine session as a video file."));
    m_pCheckboxRecording->setText(tr("&Enable Recording"));
    m_pLabelRecordingMode->setText(tr("Recording &Mode:"));
    m_pComboRecordingMode->setToolTip(tr("Selects the recording mode."));
    m_pLabelRecordingFilePath->setText(tr("File &Path:"));
    m_pEditorRecordingFilePath->setToolTip(tr("Holds the filename VirtualBox uses to save the recorded content."));
    m_pLabelRecordingFrameSize->setText(tr("Frame Si&ze:"));
    m_pComboRecordingFrameSize->setToolTip(tr("Selects the resolution (frame size) of the recorded video."));
    m_pSpinboxRecordingFrameWidth->setToolTip(tr("Holds the <b>horizontal</b> resolution (frame width) of the recorded video."));
    m_pSpinboxRecordingFrameHeight->setToolTip(tr("Holds the <b>vertical</b> resolution (frame height) of the recorded video."));
    m_pLabelRecordingFrameRate->setText(tr("Frame R&ate:"));
    m_pSliderRecordingFrameRate->setToolTip(tr("Controls the maximum number of <b>frames per second</b>. Additional frames will "
                                               "be skipped. Reducing this value will increase the number of skipped frames and "
                                               "reduce the file size."));
    m_pSpinboxRecordingFrameRate->setToolTip(tr("Controls the maximum number of <b>frames per second</b>. Additional frames will "
                                                "be skipped. Reducing this value will increase the number of skipped frames and "
                                                "reduce the file size."));
    m_pLabelRecordingVideoQuality->setText(tr("&Video Quality:"));
    m_pSliderRecordingVideoQuality->setToolTip(tr("Controls the <b>quality</b>. Increasing this value will make the video look "
                                                  "better at the cost of an increased file size."));
    m_pSpinboxRecordingVideoQuality->setToolTip(tr("Holds the bitrate in <b>kilobits per second</b>. Increasing this value will "
                                                   "make the video look better at the cost of an increased file size."));
    m_pLabelRecordingAudioQuality->setText(tr("&Audio Quality:"));
    m_pSliderRecordingAudioQuality->setToolTip(tr("Controls the <b>quality</b>. Increasing this value will make the audio sound "
                                                  "better at the cost of an increased file size."));
    m_pLabelRecordingScreens->setText(tr("Scree&ns:"));
    m_pScrollerRecordingScreens->setToolTip(QString());
    m_pTabWidget->setTabText(m_pTabWidget->indexOf(m_pTabRecording), tr("Re&cording"));

    /* Recording stuff: */
    m_pSpinboxRecordingFrameRate->setSuffix(QString(" %1").arg(tr("fps")));
    m_pSpinboxRecordingVideoQuality->setSuffix(QString(" %1").arg(tr("kbps")));
    m_pComboRecordingFrameSize->setItemText(0, tr("User Defined"));
    m_pLabelRecordingFrameRateMin->setText(tr("%1 fps").arg(m_pSliderRecordingFrameRate->minimum()));
    m_pLabelRecordingFrameRateMax->setText(tr("%1 fps").arg(m_pSliderRecordingFrameRate->maximum()));
    m_pLabelRecordingVideoQualityMin->setText(tr("low", "quality"));
    m_pLabelRecordingVideoQualityMed->setText(tr("medium", "quality"));
    m_pLabelRecordingVideoQualityMax->setText(tr("high", "quality"));
    m_pLabelRecordingAudioQualityMin->setText(tr("low", "quality"));
    m_pLabelRecordingAudioQualityMed->setText(tr("medium", "quality"));
    m_pLabelRecordingAudioQualityMax->setText(tr("high", "quality"));

    m_pComboRecordingMode->setItemText(0, gpConverter->toString(UISettingsDefs::RecordingMode_VideoAudio));
    m_pComboRecordingMode->setItemText(1, gpConverter->toString(UISettingsDefs::RecordingMode_VideoOnly));
    m_pComboRecordingMode->setItemText(2, gpConverter->toString(UISettingsDefs::RecordingMode_AudioOnly));

    /* These editors have own labels, but we want them to be properly layouted according to each other: */
    int iMinimumLayoutHint = 0;
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorVideoMemorySize->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorMonitorCount->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorScaleFactor->minimumLabelHorizontalHint());
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorGraphicsController->minimumLabelHorizontalHint());
#ifdef VBOX_WITH_3D_ACCELERATION
    iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pEditorDisplayScreenFeatures->minimumLabelHorizontalHint());
#endif
    m_pEditorVideoMemorySize->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorMonitorCount->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorScaleFactor->setMinimumLayoutIndent(iMinimumLayoutHint);
    m_pEditorGraphicsController->setMinimumLayoutIndent(iMinimumLayoutHint);
#ifdef VBOX_WITH_3D_ACCELERATION
    m_pEditorDisplayScreenFeatures->setMinimumLayoutIndent(iMinimumLayoutHint);
#endif

    updateRecordingFileSizeHint();
}

void UIMachineSettingsDisplay::polishPage()
{
    /* Get old display data from cache: */
    const UIDataSettingsMachineDisplay &oldDisplayData = m_pCache->base();

    /* Polish 'Screen' availability: */
    m_pEditorVideoMemorySize->setEnabled(isMachineOffline());
    m_pEditorMonitorCount->setEnabled(isMachineOffline());
    m_pEditorScaleFactor->setEnabled(isMachineInValidMode());
    m_pEditorGraphicsController->setEnabled(isMachineOffline());
#ifdef VBOX_WITH_3D_ACCELERATION
    m_pEditorDisplayScreenFeatures->setEnabled(isMachineOffline());
#endif

    /* Polish 'Remote Display' availability: */
    m_pTabWidget->setTabEnabled(1, oldDisplayData.m_fRemoteDisplayServerSupported);
    m_pTabRemoteDisplay->setEnabled(isMachineInValidMode());
    m_pEditorVRDESettings->setVRDEOptionsAvailable(isMachineOffline() || isMachineSaved());

    /* Polish 'Recording' availability: */
    m_pTabRecording->setEnabled(isMachineInValidMode());
    sltHandleRecordingCheckboxToggle();
}

void UIMachineSettingsDisplay::sltHandleMonitorCountChange()
{
    /* Update recording tab screen count: */
    updateGuestScreenCount();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsDisplay::sltHandleGraphicsControllerComboChange()
{
    /* Update Video RAM requirements: */
    m_pEditorVideoMemorySize->setGraphicsControllerType(m_pEditorGraphicsController->value());

    /* Revalidate: */
    revalidate();
}

#ifdef VBOX_WITH_3D_ACCELERATION
void UIMachineSettingsDisplay::sltHandle3DAccelerationFeatureStateChange()
{
    /* Update Video RAM requirements: */
    m_pEditorVideoMemorySize->set3DAccelerationEnabled(m_pEditorDisplayScreenFeatures->isEnabled3DAcceleration());

    /* Revalidate: */
    revalidate();
}
#endif /* VBOX_WITH_3D_ACCELERATION */

void UIMachineSettingsDisplay::sltHandleRecordingCheckboxToggle()
{
    /* Recording options should be enabled only if:
     * 1. Machine is in 'offline' or 'saved' state and check-box is checked,
     * 2. Machine is in 'online' state, check-box is checked, and video recording is *disabled* currently. */
    const bool fIsRecordingOptionsEnabled = ((isMachineOffline() || isMachineSaved()) && m_pCheckboxRecording->isChecked()) ||
                                               (isMachineOnline() && !m_pCache->base().m_fRecordingEnabled && m_pCheckboxRecording->isChecked());

    m_pLabelRecordingMode->setEnabled(fIsRecordingOptionsEnabled);
    m_pComboRecordingMode->setEnabled(fIsRecordingOptionsEnabled);

    m_pLabelRecordingFilePath->setEnabled(fIsRecordingOptionsEnabled);
    m_pEditorRecordingFilePath->setEnabled(fIsRecordingOptionsEnabled);

    enableDisableRecordingWidgets();
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoFrameSizeComboboxChange()
{
    /* Get the proposed size: */
    const int iCurrentIndex = m_pComboRecordingFrameSize->currentIndex();
    const QSize videoCaptureSize = m_pComboRecordingFrameSize->itemData(iCurrentIndex).toSize();

    /* Make sure its valid: */
    if (!videoCaptureSize.isValid())
        return;

    /* Apply proposed size: */
    m_pSpinboxRecordingFrameWidth->setValue(videoCaptureSize.width());
    m_pSpinboxRecordingFrameHeight->setValue(videoCaptureSize.height());
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoFrameWidthEditorChange()
{
    /* Look for preset: */
    lookForCorrespondingFrameSizePreset();
    /* Update quality and bit-rate: */
    sltHandleRecordingVideoQualitySliderChange();
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoFrameHeightEditorChange()
{
    /* Look for preset: */
    lookForCorrespondingFrameSizePreset();
    /* Update quality and bit-rate: */
    sltHandleRecordingVideoQualitySliderChange();
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoFrameRateSliderChange()
{
    /* Apply proposed frame-rate: */
    m_pSpinboxRecordingFrameRate->blockSignals(true);
    m_pSpinboxRecordingFrameRate->setValue(m_pSliderRecordingFrameRate->value());
    m_pSpinboxRecordingFrameRate->blockSignals(false);
    /* Update quality and bit-rate: */
    sltHandleRecordingVideoQualitySliderChange();
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoFrameRateEditorChange()
{
    /* Apply proposed frame-rate: */
    m_pSliderRecordingFrameRate->blockSignals(true);
    m_pSliderRecordingFrameRate->setValue(m_pSpinboxRecordingFrameRate->value());
    m_pSliderRecordingFrameRate->blockSignals(false);
    /* Update quality and bit-rate: */
    sltHandleRecordingVideoQualitySliderChange();
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoQualitySliderChange()
{
    /* Calculate/apply proposed bit-rate: */
    m_pSpinboxRecordingVideoQuality->blockSignals(true);
    m_pSpinboxRecordingVideoQuality->setValue(calculateBitRate(m_pSpinboxRecordingFrameWidth->value(),
                                                            m_pSpinboxRecordingFrameHeight->value(),
                                                            m_pSpinboxRecordingFrameRate->value(),
                                                            m_pSliderRecordingVideoQuality->value()));
    m_pSpinboxRecordingVideoQuality->blockSignals(false);
    updateRecordingFileSizeHint();
}

void UIMachineSettingsDisplay::sltHandleRecordingVideoBitRateEditorChange()
{
    /* Calculate/apply proposed quality: */
    m_pSliderRecordingVideoQuality->blockSignals(true);
    m_pSliderRecordingVideoQuality->setValue(calculateQuality(m_pSpinboxRecordingFrameWidth->value(),
                                                            m_pSpinboxRecordingFrameHeight->value(),
                                                            m_pSpinboxRecordingFrameRate->value(),
                                                            m_pSpinboxRecordingVideoQuality->value()));
    m_pSliderRecordingVideoQuality->blockSignals(false);
    updateRecordingFileSizeHint();
}

void UIMachineSettingsDisplay::sltHandleRecordingComboBoxChange()
{
    enableDisableRecordingWidgets();
}

void UIMachineSettingsDisplay::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineDisplay;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsDisplay::prepareWidgets()
{
    /* Prepare main layout: */
    QVBoxLayout *pLayoutMain = new QVBoxLayout(this);
    if (pLayoutMain)
    {
        /* Prepare tab-widget: */
        m_pTabWidget = new QITabWidget(this);
        if (m_pTabWidget)
        {
            /* Prepare each tab separately: */
            prepareTabScreen();
            prepareTabRemoteDisplay();
            prepareTabRecording();

            pLayoutMain->addWidget(m_pTabWidget);
        }
    }
}

void UIMachineSettingsDisplay::prepareTabScreen()
{
    /* Prepare 'Screen' tab: */
    m_pTabScreen = new QWidget;
    if (m_pTabScreen)
    {
        /* Prepare 'Screen' tab layout: */
        QVBoxLayout *pLayoutScreen = new QVBoxLayout(m_pTabScreen);
        if (pLayoutScreen)
        {
            /* Prepare video memory editor: */
            m_pEditorVideoMemorySize = new UIVideoMemoryEditor(m_pTabScreen);
            if (m_pEditorVideoMemorySize)
                pLayoutScreen->addWidget(m_pEditorVideoMemorySize);

            /* Prepare monitor count editor: */
            m_pEditorMonitorCount = new UIMonitorCountEditor(m_pTabScreen);
            if (m_pEditorMonitorCount)
                pLayoutScreen->addWidget(m_pEditorMonitorCount);

            /* Prepare scale factor editor: */
            m_pEditorScaleFactor = new UIScaleFactorEditor(m_pTabScreen);
            if (m_pEditorScaleFactor)
                pLayoutScreen->addWidget(m_pEditorScaleFactor);

            /* Prepare graphics controller editor: */
            m_pEditorGraphicsController = new UIGraphicsControllerEditor(m_pTabScreen);
            if (m_pEditorGraphicsController)
                pLayoutScreen->addWidget(m_pEditorGraphicsController);

#ifdef VBOX_WITH_3D_ACCELERATION
            /* Prepare display screen features editor: */
            m_pEditorDisplayScreenFeatures = new UIMachineDisplayScreenFeaturesEditor(m_pTabScreen);
            if (m_pEditorDisplayScreenFeatures)
                pLayoutScreen->addWidget(m_pEditorDisplayScreenFeatures);
#endif /* VBOX_WITH_3D_ACCELERATION */

            pLayoutScreen->addStretch();
        }

        m_pTabWidget->addTab(m_pTabScreen, QString());
    }
}

void UIMachineSettingsDisplay::prepareTabRemoteDisplay()
{
    /* Prepare 'Remote Display' tab: */
    m_pTabRemoteDisplay = new QWidget;
    if (m_pTabRemoteDisplay)
    {
        /* Prepare 'Remote Display' tab layout: */
        QVBoxLayout *pLayoutRemoteDisplay = new QVBoxLayout(m_pTabRemoteDisplay);
        if (pLayoutRemoteDisplay)
        {
            /* Prepare remote display settings editor: */
            m_pEditorVRDESettings = new UIVRDESettingsEditor(m_pTabRemoteDisplay);
            if (m_pEditorVRDESettings)
                pLayoutRemoteDisplay->addWidget(m_pEditorVRDESettings);

            pLayoutRemoteDisplay->addStretch();
        }

        m_pTabWidget->addTab(m_pTabRemoteDisplay, QString());
    }
}

void UIMachineSettingsDisplay::prepareTabRecording()
{
    /* Prepare 'Recording' tab: */
    m_pTabRecording = new QWidget;
    if (m_pTabRecording)
    {
        /* Prepare 'Recording' tab layout: */
        QGridLayout *pLayoutRecording = new QGridLayout(m_pTabRecording);
        if (pLayoutRecording)
        {
            pLayoutRecording->setRowStretch(2, 1);

            /* Prepare recording check-box: */
            m_pCheckboxRecording = new QCheckBox(m_pWidgetRecordingSettings);
            if (m_pCheckboxRecording)
                pLayoutRecording->addWidget(m_pCheckboxRecording, 0, 0, 1, 2);

            /* Prepare 20-px shifting spacer: */
            QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
            if (pSpacerItem)
                pLayoutRecording->addItem(pSpacerItem, 1, 0);

            /* Prepare recording settings widget: */
            m_pWidgetRecordingSettings = new QWidget(m_pTabRecording);
            if (m_pWidgetRecordingSettings)
            {
                /* Prepare recording settings widget layout: */
                QGridLayout *pLayoutRecordingSettings = new QGridLayout(m_pWidgetRecordingSettings);
                if (pLayoutRecordingSettings)
                {
                    pLayoutRecordingSettings->setContentsMargins(0, 0, 0, 0);

                    /* Prepare recording mode label: */
                    m_pLabelRecordingMode = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingMode)
                    {
                        m_pLabelRecordingMode->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingMode, 0, 0);
                    }
                    /* Prepare recording mode combo: */
                    m_pComboRecordingMode = new QComboBox(m_pWidgetRecordingSettings);
                    if (m_pComboRecordingMode)
                    {
                        if (m_pLabelRecordingMode)
                            m_pLabelRecordingMode->setBuddy(m_pComboRecordingMode);
                        m_pComboRecordingMode->insertItem(0, ""); /* UISettingsDefs::RecordingMode_VideoAudio */
                        m_pComboRecordingMode->insertItem(1, ""); /* UISettingsDefs::RecordingMode_VideoOnly */
                        m_pComboRecordingMode->insertItem(2, ""); /* UISettingsDefs::RecordingMode_AudioOnly */

                        pLayoutRecordingSettings->addWidget(m_pComboRecordingMode, 0, 1, 1, 3);
                    }

                    /* Prepare recording file path label: */
                    m_pLabelRecordingFilePath = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingFilePath)
                    {
                        m_pLabelRecordingFilePath->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingFilePath, 1, 0);
                    }
                    /* Prepare recording file path editor: */
                    m_pEditorRecordingFilePath = new UIFilePathSelector(m_pWidgetRecordingSettings);
                    if (m_pEditorRecordingFilePath)
                    {
                        if (m_pLabelRecordingFilePath)
                            m_pLabelRecordingFilePath->setBuddy(m_pEditorRecordingFilePath->focusProxy());
                        m_pEditorRecordingFilePath->setEditable(false);
                        m_pEditorRecordingFilePath->setMode(UIFilePathSelector::Mode_File_Save);

                        pLayoutRecordingSettings->addWidget(m_pEditorRecordingFilePath, 1, 1, 1, 3);
                    }

                    /* Prepare recording frame size label: */
                    m_pLabelRecordingFrameSize = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingFrameSize)
                    {
                        m_pLabelRecordingFrameSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingFrameSize, 2, 0);
                    }
                    /* Prepare recording frame size combo: */
                    m_pComboRecordingFrameSize = new QComboBox(m_pWidgetRecordingSettings);
                    if (m_pComboRecordingFrameSize)
                    {
                        if (m_pLabelRecordingFrameSize)
                            m_pLabelRecordingFrameSize->setBuddy(m_pComboRecordingFrameSize);
                        m_pComboRecordingFrameSize->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
                        m_pComboRecordingFrameSize->addItem(""); /* User Defined */
                        m_pComboRecordingFrameSize->addItem("320 x 200 (16:10)",   QSize(320, 200));
                        m_pComboRecordingFrameSize->addItem("640 x 480 (4:3)",     QSize(640, 480));
                        m_pComboRecordingFrameSize->addItem("720 x 400 (9:5)",     QSize(720, 400));
                        m_pComboRecordingFrameSize->addItem("720 x 480 (3:2)",     QSize(720, 480));
                        m_pComboRecordingFrameSize->addItem("800 x 600 (4:3)",     QSize(800, 600));
                        m_pComboRecordingFrameSize->addItem("1024 x 768 (4:3)",    QSize(1024, 768));
                        m_pComboRecordingFrameSize->addItem("1152 x 864 (4:3)",    QSize(1152, 864));
                        m_pComboRecordingFrameSize->addItem("1280 x 720 (16:9)",   QSize(1280, 720));
                        m_pComboRecordingFrameSize->addItem("1280 x 800 (16:10)",  QSize(1280, 800));
                        m_pComboRecordingFrameSize->addItem("1280 x 960 (4:3)",    QSize(1280, 960));
                        m_pComboRecordingFrameSize->addItem("1280 x 1024 (5:4)",   QSize(1280, 1024));
                        m_pComboRecordingFrameSize->addItem("1366 x 768 (16:9)",   QSize(1366, 768));
                        m_pComboRecordingFrameSize->addItem("1440 x 900 (16:10)",  QSize(1440, 900));
                        m_pComboRecordingFrameSize->addItem("1440 x 1080 (4:3)",   QSize(1440, 1080));
                        m_pComboRecordingFrameSize->addItem("1600 x 900 (16:9)",   QSize(1600, 900));
                        m_pComboRecordingFrameSize->addItem("1680 x 1050 (16:10)", QSize(1680, 1050));
                        m_pComboRecordingFrameSize->addItem("1600 x 1200 (4:3)",   QSize(1600, 1200));
                        m_pComboRecordingFrameSize->addItem("1920 x 1080 (16:9)",  QSize(1920, 1080));
                        m_pComboRecordingFrameSize->addItem("1920 x 1200 (16:10)", QSize(1920, 1200));
                        m_pComboRecordingFrameSize->addItem("1920 x 1440 (4:3)",   QSize(1920, 1440));
                        m_pComboRecordingFrameSize->addItem("2880 x 1800 (16:10)", QSize(2880, 1800));

                        pLayoutRecordingSettings->addWidget(m_pComboRecordingFrameSize, 2, 1);
                    }
                    /* Prepare recording frame width spinbox: */
                    m_pSpinboxRecordingFrameWidth = new QSpinBox(m_pWidgetRecordingSettings);
                    if (m_pSpinboxRecordingFrameWidth)
                    {
                        uiCommon().setMinimumWidthAccordingSymbolCount(m_pSpinboxRecordingFrameWidth, 5);
                        m_pSpinboxRecordingFrameWidth->setMinimum(16);
                        m_pSpinboxRecordingFrameWidth->setMaximum(2880);

                        pLayoutRecordingSettings->addWidget(m_pSpinboxRecordingFrameWidth, 2, 2);
                    }
                    /* Prepare recording frame height spinbox: */
                    m_pSpinboxRecordingFrameHeight = new QSpinBox(m_pWidgetRecordingSettings);
                    if (m_pSpinboxRecordingFrameHeight)
                    {
                        uiCommon().setMinimumWidthAccordingSymbolCount(m_pSpinboxRecordingFrameHeight, 5);
                        m_pSpinboxRecordingFrameHeight->setMinimum(16);
                        m_pSpinboxRecordingFrameHeight->setMaximum(1800);

                        pLayoutRecordingSettings->addWidget(m_pSpinboxRecordingFrameHeight, 2, 3);
                    }

                    /* Prepare recording frame rate label: */
                    m_pLabelRecordingFrameRate = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingFrameRate)
                    {
                        m_pLabelRecordingFrameRate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingFrameRate, 3, 0);
                    }
                    /* Prepare recording frame rate widget: */
                    m_pWidgetRecordingFrameRateSettings = new QWidget(m_pWidgetRecordingSettings);
                    if (m_pWidgetRecordingFrameRateSettings)
                    {
                        /* Prepare recording frame rate layout: */
                        QVBoxLayout *pLayoutRecordingFrameRate = new QVBoxLayout(m_pWidgetRecordingFrameRateSettings);
                        if (pLayoutRecordingFrameRate)
                        {
                            pLayoutRecordingFrameRate->setContentsMargins(0, 0, 0, 0);

                            /* Prepare recording frame rate slider: */
                            m_pSliderRecordingFrameRate = new QIAdvancedSlider(m_pWidgetRecordingFrameRateSettings);
                            if (m_pSliderRecordingFrameRate)
                            {
                                m_pSliderRecordingFrameRate->setOrientation(Qt::Horizontal);
                                m_pSliderRecordingFrameRate->setMinimum(1);
                                m_pSliderRecordingFrameRate->setMaximum(30);
                                m_pSliderRecordingFrameRate->setPageStep(1);
                                m_pSliderRecordingFrameRate->setSingleStep(1);
                                m_pSliderRecordingFrameRate->setTickInterval(1);
                                m_pSliderRecordingFrameRate->setSnappingEnabled(true);
                                m_pSliderRecordingFrameRate->setOptimalHint(1, 25);
                                m_pSliderRecordingFrameRate->setWarningHint(25, 30);

                                pLayoutRecordingFrameRate->addWidget(m_pSliderRecordingFrameRate);
                            }
                            /* Prepare recording frame rate scale layout: */
                            QHBoxLayout *pLayoutRecordingFrameRateScale = new QHBoxLayout;
                            if (pLayoutRecordingFrameRateScale)
                            {
                                pLayoutRecordingFrameRateScale->setContentsMargins(0, 0, 0, 0);

                                /* Prepare recording frame rate min label: */
                                m_pLabelRecordingFrameRateMin = new QLabel(m_pWidgetRecordingFrameRateSettings);
                                if (m_pLabelRecordingFrameRateMin)
                                    pLayoutRecordingFrameRateScale->addWidget(m_pLabelRecordingFrameRateMin);
                                pLayoutRecordingFrameRateScale->addStretch();
                                /* Prepare recording frame rate max label: */
                                m_pLabelRecordingFrameRateMax = new QLabel(m_pWidgetRecordingFrameRateSettings);
                                if (m_pLabelRecordingFrameRateMax)
                                    pLayoutRecordingFrameRateScale->addWidget(m_pLabelRecordingFrameRateMax);

                                pLayoutRecordingFrameRate->addLayout(pLayoutRecordingFrameRateScale);
                            }
                        }

                        pLayoutRecordingSettings->addWidget(m_pWidgetRecordingFrameRateSettings, 3, 1, 2, 1);
                    }
                    /* Prepare recording frame rate spinbox: */
                    m_pSpinboxRecordingFrameRate = new QSpinBox(m_pWidgetRecordingSettings);
                    if (m_pSpinboxRecordingFrameRate)
                    {
                        if (m_pLabelRecordingFrameRate)
                            m_pLabelRecordingFrameRate->setBuddy(m_pSpinboxRecordingFrameRate);
                        uiCommon().setMinimumWidthAccordingSymbolCount(m_pSpinboxRecordingFrameRate, 3);
                        m_pSpinboxRecordingFrameRate->setMinimum(1);
                        m_pSpinboxRecordingFrameRate->setMaximum(30);

                        pLayoutRecordingSettings->addWidget(m_pSpinboxRecordingFrameRate, 3, 2, 1, 2);
                    }

                    /* Prepare recording video quality label: */
                    m_pLabelRecordingVideoQuality = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingVideoQuality)
                    {
                        m_pLabelRecordingVideoQuality->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingVideoQuality, 5, 0);
                    }
                    /* Prepare recording video quality widget: */
                    m_pWidgetRecordingVideoQualitySettings = new QWidget(m_pWidgetRecordingSettings);
                    if (m_pWidgetRecordingVideoQualitySettings)
                    {
                        /* Prepare recording video quality layout: */
                        QVBoxLayout *pLayoutRecordingVideoQuality = new QVBoxLayout(m_pWidgetRecordingVideoQualitySettings);
                        if (pLayoutRecordingVideoQuality)
                        {
                            pLayoutRecordingVideoQuality->setContentsMargins(0, 0, 0, 0);

                            /* Prepare recording video quality slider: */
                            m_pSliderRecordingVideoQuality = new QIAdvancedSlider(m_pWidgetRecordingVideoQualitySettings);
                            if (m_pSliderRecordingVideoQuality)
                            {
                                m_pSliderRecordingVideoQuality->setOrientation(Qt::Horizontal);
                                m_pSliderRecordingVideoQuality->setMinimum(1);
                                m_pSliderRecordingVideoQuality->setMaximum(10);
                                m_pSliderRecordingVideoQuality->setPageStep(1);
                                m_pSliderRecordingVideoQuality->setSingleStep(1);
                                m_pSliderRecordingVideoQuality->setTickInterval(1);
                                m_pSliderRecordingVideoQuality->setSnappingEnabled(true);
                                m_pSliderRecordingVideoQuality->setOptimalHint(1, 5);
                                m_pSliderRecordingVideoQuality->setWarningHint(5, 9);
                                m_pSliderRecordingVideoQuality->setErrorHint(9, 10);

                                pLayoutRecordingVideoQuality->addWidget(m_pSliderRecordingVideoQuality);
                            }
                            /* Prepare recording video quality scale layout: */
                            QHBoxLayout *pLayoutRecordingVideoQialityScale = new QHBoxLayout;
                            if (pLayoutRecordingVideoQialityScale)
                            {
                                pLayoutRecordingVideoQialityScale->setContentsMargins(0, 0, 0, 0);

                                /* Prepare recording video quality min label: */
                                m_pLabelRecordingVideoQualityMin = new QLabel(m_pWidgetRecordingVideoQualitySettings);
                                if (m_pLabelRecordingVideoQualityMin)
                                    pLayoutRecordingVideoQialityScale->addWidget(m_pLabelRecordingVideoQualityMin);
                                pLayoutRecordingVideoQialityScale->addStretch();
                                /* Prepare recording video quality med label: */
                                m_pLabelRecordingVideoQualityMed = new QLabel(m_pWidgetRecordingVideoQualitySettings);
                                if (m_pLabelRecordingVideoQualityMed)
                                    pLayoutRecordingVideoQialityScale->addWidget(m_pLabelRecordingVideoQualityMed);
                                pLayoutRecordingVideoQialityScale->addStretch();
                                /* Prepare recording video quality max label: */
                                m_pLabelRecordingVideoQualityMax = new QLabel(m_pWidgetRecordingVideoQualitySettings);
                                if (m_pLabelRecordingVideoQualityMax)
                                    pLayoutRecordingVideoQialityScale->addWidget(m_pLabelRecordingVideoQualityMax);

                                pLayoutRecordingVideoQuality->addLayout(pLayoutRecordingVideoQialityScale);
                            }
                        }

                        pLayoutRecordingSettings->addWidget(m_pWidgetRecordingVideoQualitySettings, 5, 1, 2, 1);
                    }
                    /* Prepare recording video quality spinbox: */
                    m_pSpinboxRecordingVideoQuality = new QSpinBox(m_pWidgetRecordingSettings);
                    if (m_pSpinboxRecordingVideoQuality)
                    {
                        if (m_pLabelRecordingVideoQuality)
                            m_pLabelRecordingVideoQuality->setBuddy(m_pSpinboxRecordingVideoQuality);
                        uiCommon().setMinimumWidthAccordingSymbolCount(m_pSpinboxRecordingVideoQuality, 5);
                        m_pSpinboxRecordingVideoQuality->setMinimum(VIDEO_CAPTURE_BIT_RATE_MIN);
                        m_pSpinboxRecordingVideoQuality->setMaximum(VIDEO_CAPTURE_BIT_RATE_MAX);

                        pLayoutRecordingSettings->addWidget(m_pSpinboxRecordingVideoQuality, 5, 2, 1, 2);
                    }

                    /* Prepare recording audio quality label: */
                    m_pLabelRecordingAudioQuality = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingAudioQuality)
                    {
                        m_pLabelRecordingAudioQuality->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingAudioQuality, 7, 0);
                    }
                    /* Prepare recording audio quality widget: */
                    m_pWidgetRecordingAudioQualitySettings = new QWidget(m_pWidgetRecordingSettings);
                    if (m_pWidgetRecordingAudioQualitySettings)
                    {
                        /* Prepare recording audio quality layout: */
                        QVBoxLayout *pLayoutRecordingAudioQuality = new QVBoxLayout(m_pWidgetRecordingAudioQualitySettings);
                        if (pLayoutRecordingAudioQuality)
                        {
                            pLayoutRecordingAudioQuality->setContentsMargins(0, 0, 0, 0);

                            /* Prepare recording audio quality slider: */
                            m_pSliderRecordingAudioQuality = new QIAdvancedSlider(m_pWidgetRecordingAudioQualitySettings);
                            if (m_pSliderRecordingAudioQuality)
                            {
                                if (m_pLabelRecordingAudioQuality)
                                    m_pLabelRecordingAudioQuality->setBuddy(m_pSliderRecordingAudioQuality);
                                m_pSliderRecordingAudioQuality->setOrientation(Qt::Horizontal);
                                m_pSliderRecordingAudioQuality->setMinimum(1);
                                m_pSliderRecordingAudioQuality->setMaximum(3);
                                m_pSliderRecordingAudioQuality->setPageStep(1);
                                m_pSliderRecordingAudioQuality->setSingleStep(1);
                                m_pSliderRecordingAudioQuality->setTickInterval(1);
                                m_pSliderRecordingAudioQuality->setSnappingEnabled(true);
                                m_pSliderRecordingAudioQuality->setOptimalHint(1, 2);
                                m_pSliderRecordingAudioQuality->setWarningHint(2, 3);

                                pLayoutRecordingAudioQuality->addWidget(m_pSliderRecordingAudioQuality);
                            }
                            /* Prepare recording audio quality scale layout: */
                            QHBoxLayout *pLayoutRecordingAudioQialityScale = new QHBoxLayout;
                            if (pLayoutRecordingAudioQialityScale)
                            {
                                pLayoutRecordingAudioQialityScale->setContentsMargins(0, 0, 0, 0);

                                /* Prepare recording audio quality min label: */
                                m_pLabelRecordingAudioQualityMin = new QLabel(m_pWidgetRecordingAudioQualitySettings);
                                if (m_pLabelRecordingAudioQualityMin)
                                    pLayoutRecordingAudioQialityScale->addWidget(m_pLabelRecordingAudioQualityMin);
                                pLayoutRecordingAudioQialityScale->addStretch();
                                /* Prepare recording audio quality med label: */
                                m_pLabelRecordingAudioQualityMed = new QLabel(m_pWidgetRecordingAudioQualitySettings);
                                if (m_pLabelRecordingAudioQualityMed)
                                    pLayoutRecordingAudioQialityScale->addWidget(m_pLabelRecordingAudioQualityMed);
                                pLayoutRecordingAudioQialityScale->addStretch();
                                /* Prepare recording audio quality max label: */
                                m_pLabelRecordingAudioQualityMax = new QLabel(m_pWidgetRecordingAudioQualitySettings);
                                if (m_pLabelRecordingAudioQualityMax)
                                    pLayoutRecordingAudioQialityScale->addWidget(m_pLabelRecordingAudioQualityMax);

                                pLayoutRecordingAudioQuality->addLayout(pLayoutRecordingAudioQialityScale);
                            }
                        }

                        pLayoutRecordingSettings->addWidget(m_pWidgetRecordingAudioQualitySettings, 7, 1, 2, 1);
                    }

                    /* Prepare recording size hint label: */
                    m_pLabelRecordingSizeHint = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingSizeHint)
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingSizeHint, 9, 1);

                    /* Prepare recording screens label: */
                    m_pLabelRecordingScreens = new QLabel(m_pWidgetRecordingSettings);
                    if (m_pLabelRecordingScreens)
                    {
                        m_pLabelRecordingScreens->setAlignment(Qt::AlignRight | Qt::AlignTop);
                        pLayoutRecordingSettings->addWidget(m_pLabelRecordingScreens, 10, 0);
                    }
                    /* Prepare recording screens scroller: */
                    m_pScrollerRecordingScreens = new UIFilmContainer(m_pWidgetRecordingSettings);
                    if (m_pScrollerRecordingScreens)
                    {
                        if (m_pLabelRecordingScreens)
                            m_pLabelRecordingScreens->setBuddy(m_pScrollerRecordingScreens);
                        pLayoutRecordingSettings->addWidget(m_pScrollerRecordingScreens, 10, 1, 1, 3);
                    }
                }

                pLayoutRecording->addWidget(m_pWidgetRecordingSettings, 1, 1);
            }
        }

        m_pTabWidget->addTab(m_pTabRecording, QString());
    }
}

void UIMachineSettingsDisplay::prepareConnections()
{
    /* Configure 'Screen' connections: */
    connect(m_pEditorVideoMemorySize, &UIVideoMemoryEditor::sigValidChanged,
            this, &UIMachineSettingsDisplay::revalidate);
    connect(m_pEditorMonitorCount, &UIMonitorCountEditor::sigValidChanged,
            this, &UIMachineSettingsDisplay::sltHandleMonitorCountChange);
    connect(m_pEditorGraphicsController, &UIGraphicsControllerEditor::sigValueChanged,
            this, &UIMachineSettingsDisplay::sltHandleGraphicsControllerComboChange);
#ifdef VBOX_WITH_3D_ACCELERATION
    connect(m_pEditorDisplayScreenFeatures, &UIMachineDisplayScreenFeaturesEditor::sig3DAccelerationFeatureStatusChange,
            this, &UIMachineSettingsDisplay::sltHandle3DAccelerationFeatureStateChange);
#endif

    /* Configure 'Remote Display' connections: */
    connect(m_pEditorVRDESettings, &UIVRDESettingsEditor::sigChanged,
            this, &UIMachineSettingsDisplay::revalidate);

    /* Configure 'Recording' connections: */
    connect(m_pCheckboxRecording, &QCheckBox::toggled,
            this, &UIMachineSettingsDisplay::sltHandleRecordingCheckboxToggle);
    connect(m_pComboRecordingMode, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIMachineSettingsDisplay::sltHandleRecordingComboBoxChange);
    connect(m_pComboRecordingFrameSize, static_cast<void(QComboBox::*)(int)>(&QComboBox:: currentIndexChanged),
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoFrameSizeComboboxChange);
    connect(m_pSpinboxRecordingFrameWidth, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoFrameWidthEditorChange);
    connect(m_pSpinboxRecordingFrameHeight, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoFrameHeightEditorChange);
    connect(m_pSliderRecordingFrameRate, &QIAdvancedSlider::valueChanged,
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoFrameRateSliderChange);
    connect(m_pSpinboxRecordingFrameRate, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoFrameRateEditorChange);
    connect(m_pSliderRecordingVideoQuality, &QIAdvancedSlider::valueChanged,
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoQualitySliderChange);
    connect(m_pSpinboxRecordingVideoQuality, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &UIMachineSettingsDisplay::sltHandleRecordingVideoBitRateEditorChange);
}

void UIMachineSettingsDisplay::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIMachineSettingsDisplay::shouldWeWarnAboutLowVRAM()
{
    bool fResult = true;

    QStringList excludingOSList = QStringList()
        << "Other" << "DOS" << "Netware" << "L4" << "QNX" << "JRockitVE";
    if (m_comGuestOSType.isNull() || excludingOSList.contains(m_comGuestOSType.GetId()))
        fResult = false;

    return fResult;
}

void UIMachineSettingsDisplay::lookForCorrespondingFrameSizePreset()
{
    /* Look for video-capture size preset: */
    lookForCorrespondingPreset(m_pComboRecordingFrameSize,
                               QSize(m_pSpinboxRecordingFrameWidth->value(),
                                     m_pSpinboxRecordingFrameHeight->value()));
}

void UIMachineSettingsDisplay::updateGuestScreenCount()
{
    /* Update copy of the cached item to get the desired result: */
    QVector<BOOL> screens = m_pCache->base().m_vecRecordingScreens;
    screens.resize(m_pEditorMonitorCount->value());
    m_pScrollerRecordingScreens->setValue(screens);
    m_pEditorScaleFactor->setMonitorCount(m_pEditorMonitorCount->value());
}

void UIMachineSettingsDisplay::updateRecordingFileSizeHint()
{
    m_pLabelRecordingSizeHint->setText(tr("<i>About %1MB per 5 minute video</i>").arg(m_pSpinboxRecordingVideoQuality->value() * 300 / 8 / 1024));
}

/* static */
void UIMachineSettingsDisplay::lookForCorrespondingPreset(QComboBox *pComboBox, const QVariant &data)
{
    /* Use passed iterator to look for corresponding preset of passed combo-box: */
    const int iLookupResult = pComboBox->findData(data);
    if (iLookupResult != -1 && pComboBox->currentIndex() != iLookupResult)
        pComboBox->setCurrentIndex(iLookupResult);
    else if (iLookupResult == -1 && pComboBox->currentIndex() != 0)
        pComboBox->setCurrentIndex(0);
}

/* static */
int UIMachineSettingsDisplay::calculateBitRate(int iFrameWidth, int iFrameHeight, int iFrameRate, int iQuality)
{
    /* Linear quality<=>bit-rate scale-factor: */
    const double dResult = (double)iQuality
                         * (double)iFrameWidth * (double)iFrameHeight * (double)iFrameRate
                         / (double)10 /* translate quality to [%] */
                         / (double)1024 /* translate bit-rate to [kbps] */
                         / (double)18.75 /* linear scale factor */;
    return (int)dResult;
}

/* static */
int UIMachineSettingsDisplay::calculateQuality(int iFrameWidth, int iFrameHeight, int iFrameRate, int iBitRate)
{
    /* Linear bit-rate<=>quality scale-factor: */
    const double dResult = (double)iBitRate
                         / (double)iFrameWidth / (double)iFrameHeight / (double)iFrameRate
                         * (double)10 /* translate quality to [%] */
                         * (double)1024 /* translate bit-rate to [kbps] */
                         * (double)18.75 /* linear scale factor */;
    return (int)dResult;
}

bool UIMachineSettingsDisplay::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save display settings from cache: */
    if (fSuccess && isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Save 'Screen' data from cache: */
        if (fSuccess)
            fSuccess = saveScreenData();
        /* Save 'Remote Display' data from cache: */
        if (fSuccess)
            fSuccess = saveRemoteDisplayData();
        /* Save 'Video Capture' data from cache: */
        if (fSuccess)
            fSuccess = saveRecordingData();
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsDisplay::saveScreenData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Screen' data from cache: */
    if (fSuccess)
    {
        /* Get old display data from cache: */
        const UIDataSettingsMachineDisplay &oldDisplayData = m_pCache->base();
        /* Get new display data from cache: */
        const UIDataSettingsMachineDisplay &newDisplayData = m_pCache->data();

        /* Get graphics adapter for further activities: */
        CGraphicsAdapter comGraphics = m_machine.GetGraphicsAdapter();
        fSuccess = m_machine.isOk() && comGraphics.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
        else
        {
            /* Save video RAM size: */
            if (fSuccess && isMachineOffline() && newDisplayData.m_iCurrentVRAM != oldDisplayData.m_iCurrentVRAM)
            {
                comGraphics.SetVRAMSize(newDisplayData.m_iCurrentVRAM);
                fSuccess = comGraphics.isOk();
            }
            /* Save guest screen count: */
            if (fSuccess && isMachineOffline() && newDisplayData.m_cGuestScreenCount != oldDisplayData.m_cGuestScreenCount)
            {
                comGraphics.SetMonitorCount(newDisplayData.m_cGuestScreenCount);
                fSuccess = comGraphics.isOk();
            }
            /* Save the Graphics Controller Type: */
            if (fSuccess && isMachineOffline() && newDisplayData.m_graphicsControllerType != oldDisplayData.m_graphicsControllerType)
            {
                comGraphics.SetGraphicsControllerType(newDisplayData.m_graphicsControllerType);
                fSuccess = comGraphics.isOk();
            }
#ifdef VBOX_WITH_3D_ACCELERATION
            /* Save whether 3D acceleration is enabled: */
            if (fSuccess && isMachineOffline() && newDisplayData.m_f3dAccelerationEnabled != oldDisplayData.m_f3dAccelerationEnabled)
            {
                comGraphics.SetAccelerate3DEnabled(newDisplayData.m_f3dAccelerationEnabled);
                fSuccess = comGraphics.isOk();
            }
#endif

            /* Get machine ID for further activities: */
            QUuid uMachineId;
            if (fSuccess)
            {
                uMachineId = m_machine.GetId();
                fSuccess = m_machine.isOk();
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));

            /* Save guest-screen scale-factor: */
            if (fSuccess && newDisplayData.m_scaleFactors != oldDisplayData.m_scaleFactors)
                /* fSuccess = */ gEDataManager->setScaleFactors(newDisplayData.m_scaleFactors, uMachineId);
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsDisplay::saveRemoteDisplayData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Remote Display' data from cache: */
    if (fSuccess)
    {
        /* Get old display data from cache: */
        const UIDataSettingsMachineDisplay &oldDisplayData = m_pCache->base();
        /* Get new display data from cache: */
        const UIDataSettingsMachineDisplay &newDisplayData = m_pCache->data();

        /* Get remote display server for further activities: */
        CVRDEServer comServer = m_machine.GetVRDEServer();
        fSuccess = m_machine.isOk() && comServer.isNotNull();

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
        else
        {
            /* Save whether remote display server is enabled: */
            if (fSuccess && newDisplayData.m_fRemoteDisplayServerEnabled != oldDisplayData.m_fRemoteDisplayServerEnabled)
            {
                comServer.SetEnabled(newDisplayData.m_fRemoteDisplayServerEnabled);
                fSuccess = comServer.isOk();
            }
            /* Save remote display server port: */
            if (fSuccess && newDisplayData.m_strRemoteDisplayPort != oldDisplayData.m_strRemoteDisplayPort)
            {
                comServer.SetVRDEProperty("TCP/Ports", newDisplayData.m_strRemoteDisplayPort);
                fSuccess = comServer.isOk();
            }
            /* Save remote display server auth type: */
            if (fSuccess && newDisplayData.m_remoteDisplayAuthType != oldDisplayData.m_remoteDisplayAuthType)
            {
                comServer.SetAuthType(newDisplayData.m_remoteDisplayAuthType);
                fSuccess = comServer.isOk();
            }
            /* Save remote display server timeout: */
            if (fSuccess && newDisplayData.m_uRemoteDisplayTimeout != oldDisplayData.m_uRemoteDisplayTimeout)
            {
                comServer.SetAuthTimeout(newDisplayData.m_uRemoteDisplayTimeout);
                fSuccess = comServer.isOk();
            }
            /* Save whether remote display server allows multiple connections: */
            if (   fSuccess
                && (isMachineOffline() || isMachineSaved())
                && (newDisplayData.m_fRemoteDisplayMultiConnAllowed != oldDisplayData.m_fRemoteDisplayMultiConnAllowed))
            {
                comServer.SetAllowMultiConnection(newDisplayData.m_fRemoteDisplayMultiConnAllowed);
                fSuccess = comServer.isOk();
            }

            /* Show error message if necessary: */
            if (!fSuccess)
                notifyOperationProgressError(UIErrorString::formatErrorInfo(comServer));
        }
    }
    /* Return result: */
    return fSuccess;
}

bool UIMachineSettingsDisplay::saveRecordingData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save 'Recording' data from cache: */
    if (fSuccess)
    {
        /* Get old display data from cache: */
        const UIDataSettingsMachineDisplay &oldDisplayData = m_pCache->base();
        /* Get new display data from cache: */
        const UIDataSettingsMachineDisplay &newDisplayData = m_pCache->data();

        CRecordingSettings recordingSettings = m_machine.GetRecordingSettings();
        Assert(recordingSettings.isNotNull());

        /** @todo r=andy Make the code below more compact -- too much redundancy here. */

        /* Save new 'Recording' data for online case: */
        if (isMachineOnline())
        {
            /* If 'Recording' was *enabled*: */
            if (oldDisplayData.m_fRecordingEnabled)
            {
                /* Save whether recording is enabled: */
                if (fSuccess && newDisplayData.m_fRecordingEnabled != oldDisplayData.m_fRecordingEnabled)
                {
                    recordingSettings.SetEnabled(newDisplayData.m_fRecordingEnabled);
                    fSuccess = recordingSettings.isOk();
                }

                // We can still save the *screens* option.
                /* Save recording screens: */
                if (fSuccess)
                {
                    CRecordingScreenSettingsVector RecordScreenSettingsVector = recordingSettings.GetScreens();
                    for (int iScreenIndex = 0; fSuccess && iScreenIndex < RecordScreenSettingsVector.size(); ++iScreenIndex)
                    {
                        if (newDisplayData.m_vecRecordingScreens[iScreenIndex] == oldDisplayData.m_vecRecordingScreens[iScreenIndex])
                            continue;

                        CRecordingScreenSettings recordingScreenSettings = RecordScreenSettingsVector.at(iScreenIndex);
                        recordingScreenSettings.SetEnabled(newDisplayData.m_vecRecordingScreens[iScreenIndex]);
                        fSuccess = recordingScreenSettings.isOk();
                    }
                }
            }
            /* If 'Recording' was *disabled*: */
            else
            {
                CRecordingScreenSettingsVector recordingScreenSettingsVector = recordingSettings.GetScreens();
                for (int iScreenIndex = 0; fSuccess && iScreenIndex < recordingScreenSettingsVector.size(); ++iScreenIndex)
                {
                    CRecordingScreenSettings recordingScreenSettings = recordingScreenSettingsVector.at(iScreenIndex);

                    // We should save all the options *before* 'Recording' activation.
                    // And finally we should *enable* Recording if necessary.
                    /* Save recording file path: */
                    if (fSuccess && newDisplayData.m_strRecordingFilePath != oldDisplayData.m_strRecordingFilePath)
                    {
                        recordingScreenSettings.SetFilename(newDisplayData.m_strRecordingFilePath);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                    /* Save recording frame width: */
                    if (fSuccess && newDisplayData.m_iRecordingVideoFrameWidth != oldDisplayData.m_iRecordingVideoFrameWidth)
                    {
                        recordingScreenSettings.SetVideoWidth(newDisplayData.m_iRecordingVideoFrameWidth);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                    /* Save recording frame height: */
                    if (fSuccess && newDisplayData.m_iRecordingVideoFrameHeight != oldDisplayData.m_iRecordingVideoFrameHeight)
                    {
                        recordingScreenSettings.SetVideoHeight(newDisplayData.m_iRecordingVideoFrameHeight);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                    /* Save recording frame rate: */
                    if (fSuccess && newDisplayData.m_iRecordingVideoFrameRate != oldDisplayData.m_iRecordingVideoFrameRate)
                    {
                        recordingScreenSettings.SetVideoFPS(newDisplayData.m_iRecordingVideoFrameRate);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                    /* Save recording frame bit rate: */
                    if (fSuccess && newDisplayData.m_iRecordingVideoBitRate != oldDisplayData.m_iRecordingVideoBitRate)
                    {
                        recordingScreenSettings.SetVideoRate(newDisplayData.m_iRecordingVideoBitRate);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                    /* Save recording options: */
                    if (fSuccess && newDisplayData.m_strRecordingVideoOptions != oldDisplayData.m_strRecordingVideoOptions)
                    {
                        recordingScreenSettings.SetOptions(newDisplayData.m_strRecordingVideoOptions);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                    /* Finally, save the screen's recording state: */
                    /* Note: Must come last, as modifying options with an enabled recording state is not possible. */
                    if (fSuccess && newDisplayData.m_vecRecordingScreens != oldDisplayData.m_vecRecordingScreens)
                    {
                        recordingScreenSettings.SetEnabled(newDisplayData.m_vecRecordingScreens[iScreenIndex]);
                        Assert(recordingScreenSettings.isOk());
                        fSuccess = recordingScreenSettings.isOk();
                    }
                }

                /* Save whether recording is enabled:
                 * Do this last, as after enabling recording no changes via API aren't allowed anymore. */
                if (fSuccess && newDisplayData.m_fRecordingEnabled != oldDisplayData.m_fRecordingEnabled)
                {
                    recordingSettings.SetEnabled(newDisplayData.m_fRecordingEnabled);
                    Assert(recordingSettings.isOk());
                    fSuccess = recordingSettings.isOk();
                }
            }
        }
        /* Save new 'Recording' data for offline case: */
        else
        {
            CRecordingScreenSettingsVector recordingScreenSettingsVector = recordingSettings.GetScreens();
            for (int iScreenIndex = 0; fSuccess && iScreenIndex < recordingScreenSettingsVector.size(); ++iScreenIndex)
            {
                CRecordingScreenSettings recordingScreenSettings = recordingScreenSettingsVector.at(iScreenIndex);

                /* Save recording file path: */
                if (fSuccess && newDisplayData.m_strRecordingFilePath != oldDisplayData.m_strRecordingFilePath)
                {
                    recordingScreenSettings.SetFilename(newDisplayData.m_strRecordingFilePath);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
                /* Save recording frame width: */
                if (fSuccess && newDisplayData.m_iRecordingVideoFrameWidth != oldDisplayData.m_iRecordingVideoFrameWidth)
                {
                    recordingScreenSettings.SetVideoWidth(newDisplayData.m_iRecordingVideoFrameWidth);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
                /* Save recording frame height: */
                if (fSuccess && newDisplayData.m_iRecordingVideoFrameHeight != oldDisplayData.m_iRecordingVideoFrameHeight)
                {
                    recordingScreenSettings.SetVideoHeight(newDisplayData.m_iRecordingVideoFrameHeight);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
                /* Save recording frame rate: */
                if (fSuccess && newDisplayData.m_iRecordingVideoFrameRate != oldDisplayData.m_iRecordingVideoFrameRate)
                {
                    recordingScreenSettings.SetVideoFPS(newDisplayData.m_iRecordingVideoFrameRate);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
                /* Save recording frame bit rate: */
                if (fSuccess && newDisplayData.m_iRecordingVideoBitRate != oldDisplayData.m_iRecordingVideoBitRate)
                {
                    recordingScreenSettings.SetVideoRate(newDisplayData.m_iRecordingVideoBitRate);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
                /* Save capture options: */
                if (fSuccess && newDisplayData.m_strRecordingVideoOptions != oldDisplayData.m_strRecordingVideoOptions)
                {
                    recordingScreenSettings.SetOptions(newDisplayData.m_strRecordingVideoOptions);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
                /* Finally, save the screen's recording state: */
                /* Note: Must come last, as modifying options with an enabled recording state is not possible. */
                if (fSuccess && newDisplayData.m_vecRecordingScreens != oldDisplayData.m_vecRecordingScreens)
                {
                    recordingScreenSettings.SetEnabled(newDisplayData.m_vecRecordingScreens[iScreenIndex]);
                    Assert(recordingScreenSettings.isOk());
                    fSuccess = recordingScreenSettings.isOk();
                }
            }

            /* Save whether recording is enabled:
             * Do this last, as after enabling recording no changes via API aren't allowed anymore. */
            if (fSuccess && newDisplayData.m_fRecordingEnabled != oldDisplayData.m_fRecordingEnabled)
            {
                recordingSettings.SetEnabled(newDisplayData.m_fRecordingEnabled);
                Assert(recordingSettings.isOk());
                fSuccess = recordingSettings.isOk();
            }
        }

        /* Show error message if necessary: */
        if (!fSuccess)
            notifyOperationProgressError(UIErrorString::formatErrorInfo(m_machine));
    }
    /* Return result: */
    return fSuccess;
}

void UIMachineSettingsDisplay::enableDisableRecordingWidgets()
{
    /* Recording options should be enabled only if:
     * 1. Machine is in 'offline' or 'saved' state and check-box is checked,
     * 2. Machine is in 'online' state, check-box is checked, and video recording is *disabled* currently. */
    const bool fIsRecordingOptionsEnabled = ((isMachineOffline() || isMachineSaved()) && m_pCheckboxRecording->isChecked()) ||
                                             (isMachineOnline() && !m_pCache->base().m_fRecordingEnabled && m_pCheckboxRecording->isChecked());

    /* Video Capture Screens option should be enabled only if:
     * Machine is in *any* valid state and check-box is checked. */
    const bool fIsVideoCaptureScreenOptionEnabled = isMachineInValidMode() && m_pCheckboxRecording->isChecked();
    const UISettingsDefs::RecordingMode enmRecordingMode =
        gpConverter->fromString<UISettingsDefs::RecordingMode>(m_pComboRecordingMode->currentText());
    const bool fRecordVideo =    enmRecordingMode == UISettingsDefs::RecordingMode_VideoOnly
                              || enmRecordingMode == UISettingsDefs::RecordingMode_VideoAudio;
    const bool fRecordAudio =    enmRecordingMode == UISettingsDefs::RecordingMode_AudioOnly
                              || enmRecordingMode == UISettingsDefs::RecordingMode_VideoAudio;

    m_pLabelRecordingFrameSize->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pComboRecordingFrameSize->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pSpinboxRecordingFrameWidth->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pSpinboxRecordingFrameHeight->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);

    m_pLabelRecordingFrameRate->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pWidgetRecordingFrameRateSettings->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pSpinboxRecordingFrameRate->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);

    m_pLabelRecordingVideoQuality->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pWidgetRecordingVideoQualitySettings->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pSpinboxRecordingVideoQuality->setEnabled(fIsRecordingOptionsEnabled && fRecordVideo);
    m_pScrollerRecordingScreens->setEnabled(fIsVideoCaptureScreenOptionEnabled && fRecordVideo);

    m_pLabelRecordingAudioQuality->setEnabled(fIsRecordingOptionsEnabled && fRecordAudio);
    m_pWidgetRecordingAudioQualitySettings->setEnabled(fIsRecordingOptionsEnabled && fRecordAudio);

    m_pLabelRecordingScreens->setEnabled(fIsVideoCaptureScreenOptionEnabled && fRecordVideo);
    m_pLabelRecordingSizeHint->setEnabled(fIsVideoCaptureScreenOptionEnabled && fRecordVideo);
}
