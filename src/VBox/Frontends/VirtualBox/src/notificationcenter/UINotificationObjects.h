/* $Id$ */
/** @file
 * VBox Qt GUI - Various UINotificationObjects declarations.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjects_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjects_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QUuid>

/* GUI includes: */
#include "UINotificationObject.h"

/* COM includes: */
#include "COMEnums.h"
#include "CAppliance.h"
#include "CCloudClient.h"
#include "CCloudMachine.h"
#include "CExtPackFile.h"
#include "CExtPackManager.h"
#include "CGuest.h"
#include "CHost.h"
#include "CHostNetworkInterface.h"
#include "CMachine.h"
#include "CMedium.h"
#include "CSession.h"
#include "CSnapshot.h"
#include "CVirtualSystemDescription.h"

/* Forward declarations: */
class CAudioAdapter;
class CConsole;
class CEmulatedUSB;
class CNetworkAdapter;
class CVirtualBox;
class CVirtualBoxErrorInfo;
class CVRDEServer;

/** UINotificationObject extension for message functionality. */
class SHARED_LIBRARY_STUFF UINotificationMessage : public UINotificationSimple
{
    Q_OBJECT;

public:

    /** Notifies about inability to mount image.
      * @param  strMachineName  Brings the machine name.
      * @param  strMediumName   Brings the medium name. */
    static void cannotMountImage(const QString &strMachineName, const QString &strMediumName);
    /** Notifies about inability to send ACPI shutdown. */
    static void cannotSendACPIToMachine();

    /** Reminds about keyboard auto capturing. */
    static void remindAboutAutoCapture();
    /** Reminds about mouse integration.
      * @param  fSupportsAbsolute  Brings whether mouse supports absolute pointing. */
    static void remindAboutMouseIntegration(bool fSupportsAbsolute);
    /** Reminds about paused VM input. */
    static void remindAboutPausedVMInput();
    /** Revokes message about paused VM input. */
    static void forgetAboutPausedVMInput();
    /** Reminds about wrong color depth.
      * @param  uRealBPP    Brings real bit per pixel value.
      * @param  uWantedBPP  Brings wanted bit per pixel value. */
    static void remindAboutWrongColorDepth(ulong uRealBPP, ulong uWantedBPP);
    /** Revokes message about wrong color depth. */
    static void forgetAboutWrongColorDepth();
    /** Reminds about GA not affected. */
    static void remindAboutGuestAdditionsAreNotActive();

    /** Notifies about inability to attach USB device.
      * @param  comConsole  Brings console USB device belongs to.
      * @param  strDevice   Brings the device name. */
    static void cannotAttachUSBDevice(const CConsole &comConsole, const QString &strDevice);
    /** Notifies about inability to attach USB device.
      * @param  comErrorInfo    Brings info about error happened.
      * @param  strDevice       Brings the device name.
      * @param  strMachineName  Brings the machine name. */
    static void cannotAttachUSBDevice(const CVirtualBoxErrorInfo &comErrorInfo,
                                      const QString &strDevice, const QString &strMachineName);
    /** Notifies about inability to detach USB device.
      * @param  comConsole  Brings console USB device belongs to.
      * @param  strDevice   Brings the device name. */
    static void cannotDetachUSBDevice(const CConsole &comConsole, const QString &strDevice);
    /** Notifies about inability to detach USB device.
      * @param  comErrorInfo    Brings info about error happened.
      * @param  strDevice       Brings the device name.
      * @param  strMachineName  Brings the machine name. */
    static void cannotDetachUSBDevice(const CVirtualBoxErrorInfo &comErrorInfo,
                                      const QString &strDevice, const QString &strMachineName);
    /** Notifies about inability to attach webcam.
      * @param  comDispatcher   Brings emulated USB dispatcher webcam being attached to.
      * @param  strWebCamName   Brings the webcam name.
      * @param  strMachineName  Brings the machine name. */
    static void cannotAttachWebCam(const CEmulatedUSB &comDispatcher,
                                   const QString &strWebCamName, const QString &strMachineName);
    /** Notifies about inability to detach webcam.
      * @param  comDispatcher   Brings emulated USB dispatcher webcam being detached from.
      * @param  strWebCamName   Brings the webcam name.
      * @param  strMachineName  Brings the machine name. */
    static void cannotDetachWebCam(const CEmulatedUSB &comDispatcher,
                                   const QString &strWebCamName, const QString &strMachineName);
    /** Notifies about inability to open medium.
      * @param  comVBox      Brings common VBox object trying to open medium.
      * @param  strLocation  Brings the medium location. */
    static void cannotOpenMedium(const CVirtualBox &comVBox, const QString &strLocation);
    /** Notifies about inability to save machine settings.
      * @param  comMachine  Brings the machine trying to save settings. */
    static void cannotSaveMachineSettings(const CMachine &comMachine);
    /** Notifies about inability to toggle audio input.
      * @param  comAdapter      Brings the adapter input being toggled for.
      * @param  strMachineName  Brings the machine name.
      * @param  fEnable         Brings whether adapter input is enabled or not. */
    static void cannotToggleAudioInput(const CAudioAdapter &comAdapter,
                                       const QString &strMachineName, bool fEnable);
    /** Notifies about inability to toggle audio output.
      * @param  comAdapter      Brings the adapter output being toggled for.
      * @param  strMachineName  Brings the machine name.
      * @param  fEnable         Brings whether adapter output is enabled or not. */
    static void cannotToggleAudioOutput(const CAudioAdapter &comAdapter,
                                        const QString &strMachineName, bool fEnable);
    /** Notifies about inability to toggle network cable.
      * @param  comAdapter      Brings the adapter network cable being toggled for.
      * @param  strMachineName  Brings the machine name.
      * @param  fConnect        Brings whether network cable is connected or not. */
    static void cannotToggleNetworkCable(const CNetworkAdapter &comAdapter,
                                         const QString &strMachineName, bool fConnect);
    /** Notifies about inability to toggle recording.
      * @param  comMachine  Brings the machine recording being toggled for.
      * @param  fEnable     Brings whether recording is enabled or not. */
    static void cannotToggleRecording(const CMachine &comMachine, bool fEnable);
    /** Notifies about inability to toggle VRDE server.
      * @param  comServer       Brings the server being toggled.
      * @param  strMachineName  Brings the machine name.
      * @param  fEnable         Brings whether server is enabled or not. */
    static void cannotToggleVRDEServer(const CVRDEServer &comServer,
                                       const QString &strMachineName, bool fEnable);

protected:

    /** Constructs message notification-object.
      * @param  strName          Brings the message name.
      * @param  strDetails       Brings the message details.
      * @param  strInternalName  Brings the message internal name. */
    UINotificationMessage(const QString &strName,
                          const QString &strDetails,
                          const QString &strInternalName);
    /** Destructs message notification-object. */
    virtual ~UINotificationMessage() /* override final */;

private:

    /** Creates message.
      * @param  strName          Brings the message name.
      * @param  strDetails       Brings the message details.
      * @param  strInternalName  Brings the message internal name. */
    static void createMessage(const QString &strName,
                              const QString &strDetails,
                              const QString &strInternalName = QString());
    /** Destroys message.
      * @param  strInternalName  Brings the message internal name. */
    static void destroyMessage(const QString &strInternalName);

    /** Holds the IDs of messages registered. */
    static QMap<QString, QUuid>  m_messages;

    /** Holds the message name. */
    QString  m_strName;
    /** Holds the message details. */
    QString  m_strDetails;
    /** Holds the message internal name. */
    QString  m_strInternalName;
};

/** UINotificationProgress extension for medium create functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumCreate : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a comMedium was created. */
    void sigMediumCreated(const CMedium &comMedium);

public:

    /** Constructs medium create notification-progress.
      * @param  comTarget  Brings the medium being the target.
      * @param  uSize      Brings the target medium size.
      * @param  variants   Brings the target medium options. */
    UINotificationProgressMediumCreate(const CMedium &comTarget,
                                       qulonglong uSize,
                                       const QVector<KMediumVariant> &variants);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the medium being the target. */
    CMedium                  m_comTarget;
    /** Holds the target location. */
    QString                  m_strLocation;
    /** Holds the target medium size. */
    qulonglong               m_uSize;
    /** Holds the target medium options. */
    QVector<KMediumVariant>  m_variants;
};

/** UINotificationProgress extension for medium copy functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumCopy : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a comMedium was copied. */
    void sigMediumCopied(const CMedium &comMedium);

public:

    /** Constructs medium copy notification-progress.
      * @param  comSource  Brings the medium being copied.
      * @param  comTarget  Brings the medium being the target.
      * @param  variants   Brings the target medium options. */
    UINotificationProgressMediumCopy(const CMedium &comSource,
                                     const CMedium &comTarget,
                                     const QVector<KMediumVariant> &variants);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the medium being copied. */
    CMedium                  m_comSource;
    /** Holds the medium being the target. */
    CMedium                  m_comTarget;
    /** Holds the source location. */
    QString                  m_strSourceLocation;
    /** Holds the target location. */
    QString                  m_strTargetLocation;
    /** Holds the target medium options. */
    QVector<KMediumVariant>  m_variants;
};

/** UINotificationProgress extension for medium move functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumMove : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs medium move notification-progress.
      * @param  comMedium    Brings the medium being moved.
      * @param  strLocation  Brings the desired location. */
    UINotificationProgressMediumMove(const CMedium &comMedium,
                                     const QString &strLocation);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the medium being moved. */
    CMedium  m_comMedium;
    /** Holds the initial location. */
    QString  m_strFrom;
    /** Holds the desired location. */
    QString  m_strTo;
};

/** UINotificationProgress extension for medium resize functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumResize : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs medium resize notification-progress.
      * @param  comMedium  Brings the medium being resized.
      * @param  uSize      Brings the desired size. */
    UINotificationProgressMediumResize(const CMedium &comMedium,
                                       qulonglong uSize);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the medium being resized. */
    CMedium     m_comMedium;
    /** Holds the initial size. */
    qulonglong  m_uFrom;
    /** Holds the desired size. */
    qulonglong  m_uTo;
};

/** UINotificationProgress extension for deleting medium storage functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumDeletingStorage : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a comMedium storage was deleted. */
    void sigMediumStorageDeleted(const CMedium &comMedium);

public:

    /** Constructs deleting medium storage notification-progress.
      * @param  comMedium  Brings the medium which storage being deleted. */
    UINotificationProgressMediumDeletingStorage(const CMedium &comMedium);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the medium which storage being deleted. */
    CMedium  m_comMedium;
    /** Holds the medium location. */
    QString  m_strLocation;
};

/** UINotificationProgress extension for machine copy functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMachineCopy : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a comMachine was copied. */
    void sigMachineCopied(const CMachine &comMachine);

public:

    /** Constructs machine copy notification-progress.
      * @param  comSource     Brings the machine being copied.
      * @param  comTarget     Brings the machine being the target.
      * @param  enmCloneMode  Brings the cloning mode.
      * @param  options       Brings the cloning options. */
    UINotificationProgressMachineCopy(const CMachine &comSource,
                                      const CMachine &comTarget,
                                      const KCloneMode &enmCloneMode,
                                      const QVector<KCloneOptions> &options);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine being copied. */
    CMachine                m_comSource;
    /** Holds the machine being the target. */
    CMachine                m_comTarget;
    /** Holds the source name. */
    QString                 m_strSourceName;
    /** Holds the target name. */
    QString                 m_strTargetName;
    /** Holds the machine cloning mode. */
    KCloneMode              m_enmCloneMode;
    /** Holds the target machine options. */
    QVector<KCloneOptions>  m_options;
};

/** UINotificationProgress extension for machine move functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMachineMove : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs machine move notification-progress.
      * @param  uId             Brings the machine id.
      * @param  strDestination  Brings the move destination.
      * @param  strType         Brings the moving type. */
    UINotificationProgressMachineMove(const QUuid &uId,
                                      const QString &strDestination,
                                      const QString &strType);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine id. */
    QUuid     m_uId;
    /** Holds the session being opened. */
    CSession  m_comSession;
    /** Holds the machine source. */
    QString   m_strSource;
    /** Holds the machine destination. */
    QString   m_strDestination;
    /** Holds the moving type. */
    QString   m_strType;
};

/** UINotificationProgress extension for machine save-state functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMachineSaveState : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs machine save-state notification-progress.
      * @param  uId  Brings the machine id. */
    UINotificationProgressMachineSaveState(const QUuid &uId);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine id. */
    QUuid     m_uId;
    /** Holds the session being opened. */
    CSession  m_comSession;
    /** Holds the machine name. */
    QString   m_strName;
};

/** UINotificationProgress extension for machine power-down functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMachinePowerDown : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs machine power-down notification-progress.
      * @param  uId  Brings the machine id. */
    UINotificationProgressMachinePowerDown(const QUuid &uId);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine id. */
    QUuid     m_uId;
    /** Holds the session being opened. */
    CSession  m_comSession;
    /** Holds the machine name. */
    QString   m_strName;
};

/** UINotificationProgress extension for machine media remove functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMachineMediaRemove : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs machine media remove notification-progress.
      * @param  comMachine  Brings the machine being removed.
      * @param  media       Brings the machine media being removed. */
    UINotificationProgressMachineMediaRemove(const CMachine &comMachine,
                                             const CMediumVector &media);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the machine being removed. */
    CMachine       m_comMachine;
    /** Holds the machine name. */
    QString        m_strName;
    /** Holds the machine media being removed. */
    CMediumVector  m_media;
};

/** UINotificationProgress extension for cloud machine add functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachineAdd : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about cloud @a comMachine was added.
      * @param  strProviderShortName  Brings the short provider name.
      * @param  strProfileName        Brings the profile name. */
    void sigCloudMachineAdded(const QString &strProviderShortName,
                              const QString &strProfileName,
                              const CCloudMachine &comMachine);

public:

    /** Constructs cloud machine add notification-progress.
      * @param  comClient             Brings the cloud client being adding machine.
      * @param  comMachine            Brings the cloud machine being added.
      * @param  strInstanceName       Brings the instance name.
      * @param  strProviderShortName  Brings the short provider name.
      * @param  strProfileName        Brings the profile name. */
    UINotificationProgressCloudMachineAdd(const CCloudClient &comClient,
                                          const CCloudMachine &comMachine,
                                          const QString &strInstanceName,
                                          const QString &strProviderShortName,
                                          const QString &strProfileName);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the cloud client being adding machine. */
    CCloudClient   m_comClient;
    /** Holds the cloud machine being added. */
    CCloudMachine  m_comMachine;
    /** Holds the instance name. */
    QString        m_strInstanceName;
    /** Holds the short provider name. */
    QString        m_strProviderShortName;
    /** Holds the profile name. */
    QString        m_strProfileName;
};

/** UINotificationProgress extension for cloud machine create functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachineCreate : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about cloud @a comMachine was created.
      * @param  strProviderShortName  Brings the short provider name.
      * @param  strProfileName        Brings the profile name. */
    void sigCloudMachineCreated(const QString &strProviderShortName,
                                const QString &strProfileName,
                                const CCloudMachine &comMachine);

public:

    /** Constructs cloud machine create notification-progress.
      * @param  comClient             Brings the cloud client being adding machine.
      * @param  comMachine            Brings the cloud machine being added.
      * @param  comVSD                Brings the virtual system description.
      * @param  strProviderShortName  Brings the short provider name.
      * @param  strProfileName        Brings the profile name. */
    UINotificationProgressCloudMachineCreate(const CCloudClient &comClient,
                                             const CCloudMachine &comMachine,
                                             const CVirtualSystemDescription &comVSD,
                                             const QString &strProviderShortName,
                                             const QString &strProfileName);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the cloud client being adding machine. */
    CCloudClient               m_comClient;
    /** Holds the cloud machine being added. */
    CCloudMachine              m_comMachine;
    /** Holds the the virtual system description. */
    CVirtualSystemDescription  m_comVSD;
    /** Holds the cloud machine name. */
    QString                    m_strName;
    /** Holds the short provider name. */
    QString                    m_strProviderShortName;
    /** Holds the profile name. */
    QString                    m_strProfileName;
};

/** UINotificationProgress extension for cloud machine remove functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachineRemove : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about cloud machine was removed.
      * @param  strProviderShortName  Brings the short provider name.
      * @param  strProfileName        Brings the profile name.
      * @param  strName               Brings the machine name. */
    void sigCloudMachineRemoved(const QString &strProviderShortName,
                                const QString &strProfileName,
                                const QString &strName);

public:

    /** Constructs cloud machine remove notification-progress.
      * @param  comMachine    Brings the cloud machine being removed.
      * @param  fFullRemoval  Brings whether cloud machine should be removed fully. */
    UINotificationProgressCloudMachineRemove(const CCloudMachine &comMachine,
                                             bool fFullRemoval,
                                             const QString &strProviderShortName,
                                             const QString &strProfileName);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the cloud machine being removed. */
    CCloudMachine  m_comMachine;
    /** Holds the cloud machine name. */
    QString        m_strName;
    /** Holds whether cloud machine should be removed fully. */
    bool           m_fFullRemoval;
    /** Holds the short provider name. */
    QString        m_strProviderShortName;
    /** Holds the profile name. */
    QString        m_strProfileName;
};

/** UINotificationProgress extension for cloud machine power-up functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachinePowerUp : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs cloud machine power-up notification-progress.
      * @param  comMachine  Brings the machine being powered-up. */
    UINotificationProgressCloudMachinePowerUp(const CCloudMachine &comMachine);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the machine being powered-up. */
    CCloudMachine  m_comMachine;
    /** Holds the machine name. */
    QString        m_strName;
};

/** UINotificationProgress extension for cloud machine power-down functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachinePowerDown : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs cloud machine power-down notification-progress.
      * @param  comMachine  Brings the machine being powered-down. */
    UINotificationProgressCloudMachinePowerDown(const CCloudMachine &comMachine);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the machine being powered-down. */
    CCloudMachine  m_comMachine;
    /** Holds the machine name. */
    QString        m_strName;
};

/** UINotificationProgress extension for cloud machine shutdown functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachineShutdown : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs cloud machine shutdown notification-progress.
      * @param  comMachine  Brings the machine being shutdown. */
    UINotificationProgressCloudMachineShutdown(const CCloudMachine &comMachine);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the machine being shutdown. */
    CCloudMachine  m_comMachine;
    /** Holds the machine name. */
    QString        m_strName;
};

/** UINotificationProgress extension for cloud machine terminate functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudMachineTerminate : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs cloud machine terminate notification-progress.
      * @param  comMachine  Brings the machine being terminate. */
    UINotificationProgressCloudMachineTerminate(const CCloudMachine &comMachine);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the machine being terminated. */
    CCloudMachine  m_comMachine;
    /** Holds the machine name. */
    QString        m_strName;
};

/** UINotificationProgress extension for cloud console connection create functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudConsoleConnectionCreate : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs cloud console connection create notification-progress.
      * @param  comMachine    Brings the cloud machine for which console connection being created.
      * @param  strPublicKey  Brings the public key used for console connection being created. */
    UINotificationProgressCloudConsoleConnectionCreate(const CCloudMachine &comMachine,
                                                       const QString &strPublicKey);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the cloud machine for which console connection being created. */
    CCloudMachine  m_comMachine;
    /** Holds the cloud machine name. */
    QString        m_strName;
    /** Holds the public key used for console connection being created. */
    QString        m_strPublicKey;
};

/** UINotificationProgress extension for cloud console connection delete functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressCloudConsoleConnectionDelete : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs cloud console connection delete notification-progress.
      * @param  comMachine  Brings the cloud machine for which console connection being deleted. */
    UINotificationProgressCloudConsoleConnectionDelete(const CCloudMachine &comMachine);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the cloud machine for which console connection being deleted. */
    CCloudMachine  m_comMachine;
    /** Holds the cloud machine name. */
    QString        m_strName;
};

/** UINotificationProgress extension for snapshot take functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressSnapshotTake : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs snapshot take notification-progress.
      * @param  comMachine              Brings the machine we are taking snapshot for.
      * @param  strSnapshotName         Brings the name of snapshot being taken.
      * @param  strSnapshotDescription  Brings the description of snapshot being taken. */
    UINotificationProgressSnapshotTake(const CMachine &comMachine,
                                       const QString &strSnapshotName,
                                       const QString &strSnapshotDescription);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine we are taking snapshot for. */
    CMachine  m_comMachine;
    /** Holds the name of snapshot being taken. */
    QString   m_strSnapshotName;
    /** Holds the description of snapshot being taken. */
    QString   m_strSnapshotDescription;
    /** Holds the machine name. */
    QString   m_strMachineName;
    /** Holds the session being opened. */
    CSession  m_comSession;
};

/** UINotificationProgress extension for snapshot restore functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressSnapshotRestore : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs snapshot restore notification-progress.
      * @param  comMachine   Brings the machine we are restoring snapshot for.
      * @param  comSnapshot  Brings the snapshot being restored. */
    UINotificationProgressSnapshotRestore(const CMachine &comMachine,
                                          const CSnapshot &comSnapshot);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine we are restoring snapshot for. */
    CMachine   m_comMachine;
    /** Holds the snapshot being restored. */
    CSnapshot  m_comSnapshot;
    /** Holds the machine name. */
    QString    m_strMachineName;
    /** Holds the snapshot name. */
    QString    m_strSnapshotName;
    /** Holds the session being opened. */
    CSession   m_comSession;
};

/** UINotificationProgress extension for snapshot delete functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressSnapshotDelete : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs snapshot delete notification-progress.
      * @param  comMachine   Brings the machine we are deleting snapshot from.
      * @param  uSnapshotId  Brings the ID of snapshot being deleted. */
    UINotificationProgressSnapshotDelete(const CMachine &comMachine,
                                         const QUuid &uSnapshotId);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the machine we are deleting snapshot from. */
    CMachine  m_comMachine;
    /** Holds the ID of snapshot being deleted. */
    QUuid     m_uSnapshotId;
    /** Holds the machine name. */
    QString   m_strMachineName;
    /** Holds the snapshot name. */
    QString   m_strSnapshotName;
    /** Holds the session being opened. */
    CSession  m_comSession;
};

/** UINotificationProgress extension for export appliance functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressApplianceExport : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs export appliance notification-progress.
      * @param  comAppliance  Brings the appliance being exported.
      * @param  strFormat     Brings the appliance format.
      * @param  options       Brings the export options to be taken into account.
      * @param  strPath       Brings the appliance path. */
    UINotificationProgressApplianceExport(const CAppliance &comAppliance,
                                          const QString &strFormat,
                                          const QVector<KExportOptions> &options,
                                          const QString &strPath);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the appliance being exported. */
    CAppliance               m_comAppliance;
    /** Holds the appliance format. */
    QString                  m_strFormat;
    /** Holds the export options to be taken into account. */
    QVector<KExportOptions>  m_options;
    /** Holds the appliance path. */
    QString                  m_strPath;
};

/** UINotificationProgress extension for import appliance functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressApplianceImport : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs import appliance notification-progress.
      * @param  comAppliance  Brings the appliance being imported.
      * @param  options       Brings the import options to be taken into account. */
    UINotificationProgressApplianceImport(const CAppliance &comAppliance,
                                          const QVector<KImportOptions> &options);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the appliance being imported. */
    CAppliance               m_comAppliance;
    /** Holds the import options to be taken into account. */
    QVector<KImportOptions>  m_options;
};

/** UINotificationProgress extension for extension pack install functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressExtensionPackInstall : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about extension pack installed.
      * @param  strExtensionPackName  Brings extension pack name. */
    void sigExtensionPackInstalled(const QString &strExtensionPackName);

public:

    /** Constructs extension pack install notification-progress.
      * @param  comExtPackFile        Brings the extension pack file to install.
      * @param  fReplace              Brings whether extension pack should be replaced.
      * @param  strExtensionPackName  Brings the extension pack name.
      * @param  strDisplayInfo        Brings the display info. */
    UINotificationProgressExtensionPackInstall(const CExtPackFile &comExtPackFile,
                                               bool fReplace,
                                               const QString &strExtensionPackName,
                                               const QString &strDisplayInfo);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the extension pack file to install. */
    CExtPackFile     m_comExtPackFile;
    /** Holds whether extension pack should be replaced. */
    bool             m_fReplace;
    /** Holds the extension pack name. */
    QString          m_strExtensionPackName;
    /** Holds the display info. */
    QString          m_strDisplayInfo;
};

/** UINotificationProgress extension for extension pack uninstall functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressExtensionPackUninstall : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about extension pack uninstalled.
      * @param  strExtensionPackName  Brings extension pack name. */
    void sigExtensionPackUninstalled(const QString &strExtensionPackName);

public:

    /** Constructs extension pack uninstall notification-progress.
      * @param  comExtPackManager     Brings the extension pack manager.
      * @param  strExtensionPackName  Brings the extension pack name.
      * @param  strDisplayInfo        Brings the display info. */
    UINotificationProgressExtensionPackUninstall(const CExtPackManager &comExtPackManager,
                                                 const QString &strExtensionPackName,
                                                 const QString &strDisplayInfo);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the extension pack manager. */
    CExtPackManager  m_comExtPackManager;
    /** Holds the extension pack name. */
    QString          m_strExtensionPackName;
    /** Holds the display info. */
    QString          m_strDisplayInfo;
};

/** UINotificationProgress extension for guest additions install functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressGuestAdditionsInstall : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about guest additions installation failed.
      * @param  strSource  Brings the guest additions file path. */
    void sigGuestAdditionsInstallationFailed(const QString &strSource);

public:

    /** Constructs guest additions install notification-progress.
      * @param  comGuest   Brings the guest additions being installed to.
      * @param  strSource  Brings the guest additions file path. */
    UINotificationProgressGuestAdditionsInstall(const CGuest &comGuest,
                                                const QString &strSource);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the guest additions being installed to. */
    CGuest   m_comGuest;
    /** Holds the guest additions file path. */
    QString  m_strSource;
};

/** UINotificationProgress extension for host-only network interface create functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressHostOnlyNetworkInterfaceCreate : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about host-only network interface created.
      * @param  comInterface  Brings network interface created. */
    void sigHostOnlyNetworkInterfaceCreated(const CHostNetworkInterface &comInterface);

public:

    /** Constructs host-only network interface create notification-progress.
      * @param  comHost       Brings the host network interface being created for.
      * @param  comInterface  Brings the network interface being created. */
    UINotificationProgressHostOnlyNetworkInterfaceCreate(const CHost &comHost,
                                                         const CHostNetworkInterface &comInterface);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the host network interface being created for. */
    CHost                  m_comHost;
    /** Holds the network interface being created. */
    CHostNetworkInterface  m_comInterface;
};

/** UINotificationProgress extension for host-only network interface remove functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressHostOnlyNetworkInterfaceRemove : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about host-only network interface removed.
      * @param  strInterfaceName  Brings the name of network interface removed. */
    void sigHostOnlyNetworkInterfaceRemoved(const QString &strInterfaceName);

public:

    /** Constructs host-only network interface remove notification-progress.
      * @param  comHost       Brings the host network interface being removed for.
      * @param  uInterfaceId  Brings the ID of network interface being removed. */
    UINotificationProgressHostOnlyNetworkInterfaceRemove(const CHost &comHost,
                                                         const QUuid &uInterfaceId);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the host network interface being removed for. */
    CHost    m_comHost;
    /** Holds the ID of network interface being removed. */
    QUuid    m_uInterfaceId;
    /** Holds the network interface name. */
    QString  m_strInterfaceName;
};

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
/** UINotificationDownloader extension for extension pack downloading functionality. */
class SHARED_LIBRARY_STUFF UINotificationDownloaderExtensionPack : public UINotificationDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about extension pack downloaded.
      * @param  strSource  Brings the EP source.
      * @param  strTarget  Brings the EP target.
      * @param  strDigest  Brings the EP digest. */
    void sigExtensionPackDownloaded(const QString &strSource,
                                    const QString &strTarget,
                                    const QString &strDigest);

public:

    /** Returns singleton instance, creates if necessary.
      * @param  strPackName  Brings the package name. */
    static UINotificationDownloaderExtensionPack *instance(const QString &strPackName);
    /** Returns whether singleton instance already created. */
    static bool exists();

    /** Destructs extension pack downloading notification-downloader.
      * @note  Notification-center can destroy us at any time. */
    virtual ~UINotificationDownloaderExtensionPack() /* override final */;

protected:

    /** Constructs extension pack downloading notification-downloader.
      * @param  strPackName  Brings the package name. */
    UINotificationDownloaderExtensionPack(const QString &strPackName);

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started downloader. */
    virtual UIDownloader *createDownloader() /* override final */;

private:

    /** Holds the singleton instance. */
    static UINotificationDownloaderExtensionPack *s_pInstance;

    /** Holds the name of pack being dowloaded. */
    QString  m_strPackName;
};

/** UINotificationDownloader extension for guest additions downloading functionality. */
class SHARED_LIBRARY_STUFF UINotificationDownloaderGuestAdditions : public UINotificationDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about guest additions downloaded.
      * @param  strLocation  Brings the UM location. */
    void sigGuestAdditionsDownloaded(const QString &strLocation);

public:

    /** Returns singleton instance, creates if necessary.
      * @param  strFileName  Brings the file name. */
    static UINotificationDownloaderGuestAdditions *instance(const QString &strFileName);
    /** Returns whether singleton instance already created. */
    static bool exists();

    /** Destructs guest additions downloading notification-downloader.
      * @note  Notification-center can destroy us at any time. */
    virtual ~UINotificationDownloaderGuestAdditions() /* override final */;

protected:

    /** Constructs guest additions downloading notification-downloader.
      * @param  strFileName  Brings the file name. */
    UINotificationDownloaderGuestAdditions(const QString &strFileName);

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started downloader. */
    virtual UIDownloader *createDownloader() /* override final */;

private:

    /** Holds the singleton instance. */
    static UINotificationDownloaderGuestAdditions *s_pInstance;

    /** Holds the name of file being dowloaded. */
    QString  m_strFileName;
};

/** UINotificationDownloader extension for user manual downloading functionality. */
class SHARED_LIBRARY_STUFF UINotificationDownloaderUserManual : public UINotificationDownloader
{
    Q_OBJECT;

signals:

    /** Notifies listeners about user manual downloaded.
      * @param  strLocation  Brings the UM location. */
    void sigUserManualDownloaded(const QString &strLocation);

public:

    /** Returns singleton instance, creates if necessary.
      * @param  strFileName  Brings the file name. */
    static UINotificationDownloaderUserManual *instance(const QString &strFileName);
    /** Returns whether singleton instance already created. */
    static bool exists();

    /** Destructs user manual downloading notification-downloader.
      * @note  Notification-center can destroy us at any time. */
    virtual ~UINotificationDownloaderUserManual() /* override final */;

protected:

    /** Constructs user manual downloading notification-downloader.
      * @param  strFileName  Brings the file name. */
    UINotificationDownloaderUserManual(const QString &strFileName);

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started downloader. */
    virtual UIDownloader *createDownloader() /* override final */;

private:

    /** Holds the singleton instance. */
    static UINotificationDownloaderUserManual *s_pInstance;

    /** Holds the name of file being dowloaded. */
    QString  m_strFileName;
};

/** UINotificationNewVersionChecker extension for VirtualBox new version check functionality. */
class SHARED_LIBRARY_STUFF UINotificationNewVersionCheckerVirtualBox : public UINotificationNewVersionChecker
{
    Q_OBJECT;

public:

    /** Returns singleton instance, creates if necessary.
      * @param  fForcedCall  Brings whether this customer has forced privelegies. */
    static UINotificationNewVersionCheckerVirtualBox *instance(bool fForcedCall);
    /** Returns whether singleton instance already created. */
    static bool exists();

    /** Destructs VirtualBox notification-new-version-checker.
      * @note  Notification-center can destroy us at any time. */
    virtual ~UINotificationNewVersionCheckerVirtualBox() /* override final */;

protected:

    /** Constructs VirtualBox notification-new-version-checker.
      * @param  fForcedCall  Brings whether this customer has forced privelegies. */
    UINotificationNewVersionCheckerVirtualBox(bool fForcedCall);

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started new-version-checker. */
    virtual UINewVersionChecker *createChecker() /* override final */;

private:

    /** Holds the singleton instance. */
    static UINotificationNewVersionCheckerVirtualBox *s_pInstance;

    /** Holds whether this customer has forced privelegies. */
    bool  m_fForcedCall;

    /** Holds the url. */
    QString  m_strUrl;
};
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjects_h */
