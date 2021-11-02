/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudNetworkingStuff namespace implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIErrorString.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAppliance.h"
#include "CForm.h"
#include "CProgress.h"
#include "CStringArray.h"
#include "CVirtualBox.h"
#include "CVirtualBoxErrorInfo.h"
#include "CVirtualSystemDescription.h"


CCloudProviderManager UICloudNetworkingStuff::cloudProviderManager(QWidget *pParent /* = 0 */)
{
    /* Acquire VBox: */
    const CVirtualBox comVBox = uiCommon().virtualBox();
    if (comVBox.isNotNull())
    {
        /* Acquire cloud provider manager: */
        CCloudProviderManager comProviderManager = comVBox.GetCloudProviderManager();
        if (!comVBox.isOk())
            msgCenter().cannotAcquireCloudProviderManager(comVBox, pParent);
        else
            return comProviderManager;
    }
    /* Null by default: */
    return CCloudProviderManager();
}

CCloudProviderManager UICloudNetworkingStuff::cloudProviderManager(QString &strErrorMessage)
{
    /* Acquire VBox: */
    const CVirtualBox comVBox = uiCommon().virtualBox();
    if (comVBox.isNotNull())
    {
        /* Acquire cloud provider manager: */
        CCloudProviderManager comProviderManager = comVBox.GetCloudProviderManager();
        if (!comVBox.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comVBox);
        else
            return comProviderManager;
    }
    /* Null by default: */
    return CCloudProviderManager();
}

CCloudProvider UICloudNetworkingStuff::cloudProviderByShortName(const QString &strProviderShortName,
                                                                QWidget *pParent /* = 0 */)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager(pParent);
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud provider: */
        CCloudProvider comProvider = comProviderManager.GetProviderByShortName(strProviderShortName);
        if (!comProviderManager.isOk())
            msgCenter().cannotAcquireCloudProviderManagerParameter(comProviderManager, pParent);
        else
            return comProvider;
    }
    /* Null by default: */
    return CCloudProvider();
}

CCloudProvider UICloudNetworkingStuff::cloudProviderByShortName(const QString &strProviderShortName,
                                                                QString &strErrorMessage)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager(strErrorMessage);
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud provider: */
        CCloudProvider comProvider = comProviderManager.GetProviderByShortName(strProviderShortName);
        if (!comProviderManager.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comProviderManager);
        else
            return comProvider;
    }
    /* Null by default: */
    return CCloudProvider();
}

CCloudProfile UICloudNetworkingStuff::cloudProfileByName(const QString &strProviderShortName,
                                                         const QString &strProfileName,
                                                         QWidget *pParent /* = 0 */)
{
    /* Acquire cloud provider: */
    CCloudProvider comProvider = cloudProviderByShortName(strProviderShortName, pParent);
    if (comProvider.isNotNull())
    {
        /* Acquire cloud profile: */
        CCloudProfile comProfile = comProvider.GetProfileByName(strProfileName);
        if (!comProvider.isOk())
            msgCenter().cannotFindCloudProfile(comProvider, strProfileName, pParent);
        else
            return comProfile;
    }
    /* Null by default: */
    return CCloudProfile();
}

CCloudProfile UICloudNetworkingStuff::cloudProfileByName(const QString &strProviderShortName,
                                                         const QString &strProfileName,
                                                         QString &strErrorMessage)
{
    /* Acquire cloud provider: */
    CCloudProvider comProvider = cloudProviderByShortName(strProviderShortName, strErrorMessage);
    if (comProvider.isNotNull())
    {
        /* Acquire cloud profile: */
        CCloudProfile comProfile = comProvider.GetProfileByName(strProfileName);
        if (!comProvider.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comProvider);
        else
            return comProfile;
    }
    /* Null by default: */
    return CCloudProfile();
}

CCloudClient UICloudNetworkingStuff::cloudClient(CCloudProfile comProfile,
                                                 QWidget *pParent /* = 0 */)
{
    /* Create cloud client: */
    CCloudClient comClient = comProfile.CreateCloudClient();
    if (!comProfile.isOk())
        msgCenter().cannotCreateCloudClient(comProfile, pParent);
    else
        return comClient;
    /* Null by default: */
    return CCloudClient();
}

CCloudClient UICloudNetworkingStuff::cloudClient(CCloudProfile comProfile,
                                                 QString &strErrorMessage)
{
    /* Create cloud client: */
    CCloudClient comClient = comProfile.CreateCloudClient();
    if (!comProfile.isOk())
        strErrorMessage = UIErrorString::formatErrorInfo(comProfile);
    else
        return comClient;
    /* Null by default: */
    return CCloudClient();
}

CCloudClient UICloudNetworkingStuff::cloudClientByName(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       QWidget *pParent /* = 0 */)
{
    /* Acquire cloud profile: */
    CCloudProfile comProfile = cloudProfileByName(strProviderShortName, strProfileName, pParent);
    if (comProfile.isNotNull())
        return cloudClient(comProfile, pParent);
    /* Null by default: */
    return CCloudClient();
}

CCloudClient UICloudNetworkingStuff::cloudClientByName(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       QString &strErrorMessage)
{
    /* Acquire cloud profile: */
    CCloudProfile comProfile = cloudProfileByName(strProviderShortName, strProfileName, strErrorMessage);
    if (comProfile.isNotNull())
        return cloudClient(comProfile, strErrorMessage);
    /* Null by default: */
    return CCloudClient();
}

CVirtualSystemDescription UICloudNetworkingStuff::createVirtualSystemDescription(QWidget *pParent /* = 0 */)
{
    /* Acquire VBox: */
    CVirtualBox comVBox = uiCommon().virtualBox();
    if (comVBox.isNotNull())
    {
        /* Create appliance: */
        CAppliance comAppliance = comVBox.CreateAppliance();
        if (!comVBox.isOk())
            msgCenter().cannotCreateAppliance(comVBox, pParent);
        else
        {
            /* Append it with one (1) description we need: */
            comAppliance.CreateVirtualSystemDescriptions(1);
            if (!comAppliance.isOk())
                msgCenter().cannotCreateVirtualSystemDescription(comAppliance, pParent);
            else
            {
                /* Get received description: */
                const QVector<CVirtualSystemDescription> descriptions = comAppliance.GetVirtualSystemDescriptions();
                AssertReturn(!descriptions.isEmpty(), CVirtualSystemDescription());
                return descriptions.at(0);
            }
        }
    }
    /* Null by default: */
    return CVirtualSystemDescription();
}

QVector<CCloudProvider> UICloudNetworkingStuff::listCloudProviders(QWidget *pParent /* = 0 */)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager();
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud providers: */
        QVector<CCloudProvider> providers = comProviderManager.GetProviders();
        if (!comProviderManager.isOk())
            msgCenter().cannotAcquireCloudProviderManagerParameter(comProviderManager, pParent);
        else
            return providers;
    }
    /* Return empty list by default: */
    return QVector<CCloudProvider>();
}

bool UICloudNetworkingStuff::cloudProviderId(const CCloudProvider &comCloudProvider,
                                             QUuid &uResult,
                                             QWidget *pParent /* = 0 */)
{
    const QUuid uId = comCloudProvider.GetId();
    if (!comCloudProvider.isOk())
        msgCenter().cannotAcquireCloudProviderParameter(comCloudProvider, pParent);
    else
    {
        uResult = uId;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudProviderShortName(const CCloudProvider &comCloudProvider,
                                                    QString &strResult,
                                                    QWidget *pParent /* = 0 */)
{
    const QString strShortName = comCloudProvider.GetShortName();
    if (!comCloudProvider.isOk())
        msgCenter().cannotAcquireCloudProviderParameter(comCloudProvider, pParent);
    else
    {
        strResult = strShortName;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudProviderName(const CCloudProvider &comCloudProvider,
                                               QString &strResult,
                                               QWidget *pParent /* = 0 */)
{
    const QString strName = comCloudProvider.GetName();
    if (!comCloudProvider.isOk())
        msgCenter().cannotAcquireCloudProviderParameter(comCloudProvider, pParent);
    else
    {
        strResult = strName;
        return true;
    }
    return false;
}

QVector<CCloudProfile> UICloudNetworkingStuff::listCloudProfiles(CCloudProvider comCloudProvider,
                                                                 QWidget *pParent /* = 0 */)
{
    /* Check cloud provider: */
    if (comCloudProvider.isNotNull())
    {
        /* Acquire cloud providers: */
        QVector<CCloudProfile> profiles = comCloudProvider.GetProfiles();
        if (!comCloudProvider.isOk())
            msgCenter().cannotAcquireCloudProviderParameter(comCloudProvider, pParent);
        else
            return profiles;
    }
    /* Return empty list by default: */
    return QVector<CCloudProfile>();
}

bool UICloudNetworkingStuff::cloudProfileName(const CCloudProfile &comCloudProfile,
                                              QString &strResult,
                                              QWidget *pParent /* = 0 */)
{
    const QString strName = comCloudProfile.GetName();
    if (!comCloudProfile.isOk())
        msgCenter().cannotAcquireCloudProfileParameter(comCloudProfile, pParent);
    else
    {
        strResult = strName;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudProfileProperties(const CCloudProfile &comCloudProfile,
                                                    QVector<QString> &keys,
                                                    QVector<QString> &values,
                                                    QWidget *pParent /* = 0 */)
{
    QVector<QString> aKeys;
    QVector<QString> aValues;
    aValues = comCloudProfile.GetProperties(QString(), aKeys);
    if (!comCloudProfile.isOk())
        msgCenter().cannotAcquireCloudProfileParameter(comCloudProfile, pParent);
    else
    {
        aValues.resize(aKeys.size());
        keys = aKeys;
        values = aValues;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::listCloudImages(const CCloudClient &comCloudClient,
                                             CStringArray &comNames,
                                             CStringArray &comIDs,
                                             QWidget *pParent /* = 0 */)
{
    /* Currently we are interested in Available images only: */
    const QVector<KCloudImageState> cloudImageStates  = QVector<KCloudImageState>()
                                                     << KCloudImageState_Available;
    /* Execute ListImages async method: */
    CProgress comProgress = comCloudClient.ListImages(cloudImageStates, comNames, comIDs);
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
    else
    {
        /* Show "Acquire cloud images" progress: */
        msgCenter().showModalProgressDialog(comProgress,
                                            QString(),
                                            ":/progress_reading_appliance_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
        else
            return true;
    }
    /* Return false by default: */
    return false;
}

bool UICloudNetworkingStuff::listCloudSourceBootVolumes(const CCloudClient &comCloudClient,
                                                        CStringArray &comNames,
                                                        CStringArray &comIDs,
                                                        QWidget *pParent /* = 0 */)
{
    /* Execute ListSourceBootVolumes async method: */
    CProgress comProgress = comCloudClient.ListSourceBootVolumes(comNames, comIDs);
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
    else
    {
        /* Show "Acquire cloud source boot volumes" progress: */
        msgCenter().showModalProgressDialog(comProgress,
                                            QString(),
                                            ":/progress_reading_appliance_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
        else
            return true;
    }
    /* Return false by default: */
    return false;
}

bool UICloudNetworkingStuff::listCloudSourceInstances(const CCloudClient &comCloudClient,
                                                      CStringArray &comNames,
                                                      CStringArray &comIDs,
                                                      QWidget *pParent /* = 0 */)
{
    /* Execute ListSourceInstances async method: */
    CProgress comProgress = comCloudClient.ListSourceInstances(comNames, comIDs);
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
    else
    {
        /* Show "Acquire cloud source instances" progress: */
        msgCenter().showModalProgressDialog(comProgress,
                                            QString(),
                                            ":/progress_reading_appliance_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
        else
            return true;
    }
    /* Return false by default: */
    return false;
}

bool UICloudNetworkingStuff::exportDescriptionForm(CCloudClient comCloudClient,
                                                   CVirtualSystemDescription comDescription,
                                                   CVirtualSystemDescriptionForm &comResult,
                                                   QWidget *pParent /* = 0 */)
{
    /* Execute GetExportDescriptionForm async method: */
    CProgress comProgress = comCloudClient.GetExportDescriptionForm(comDescription, comResult);
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
    else
    {
        /* Show "Get Export Description Form" progress: */
        msgCenter().showModalProgressDialog(comProgress,
                                            QString(),
                                            ":/progress_refresh_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress);
        else
            return true;
    }
    /* False by default: */
    return false;
}

bool UICloudNetworkingStuff::importDescriptionForm(CCloudClient comCloudClient,
                                                   CVirtualSystemDescription comDescription,
                                                   CVirtualSystemDescriptionForm &comResult,
                                                   QWidget *pParent /* = 0 */)
{
    /* Execute GetImportDescriptionForm async method: */
    CProgress comProgress = comCloudClient.GetImportDescriptionForm(comDescription, comResult);
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient);
    else
    {
        /* Show "Get Import Description Form" progress: */
        msgCenter().showModalProgressDialog(comProgress,
                                            QString(),
                                            ":/progress_refresh_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress);
        else
            return true;
    }
    /* False by default: */
    return false;
}

bool UICloudNetworkingStuff::cloudMachineId(const CCloudMachine &comCloudMachine,
                                            QUuid &uResult,
                                            QWidget *pParent /* = 0 */)
{
    const QUuid uId = comCloudMachine.GetId();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        uResult = uId;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineAccessible(const CCloudMachine &comCloudMachine,
                                                    bool &fResult,
                                                    QWidget *pParent /* = 0 */)
{
    const bool fAccessible = comCloudMachine.GetAccessible();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        fResult = fAccessible;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineAccessError(const CCloudMachine &comCloudMachine,
                                                     CVirtualBoxErrorInfo &comResult,
                                                     QWidget *pParent /* = 0 */)
{
    const CVirtualBoxErrorInfo comAccessError = comCloudMachine.GetAccessError();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        comResult = comAccessError;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineName(const CCloudMachine &comCloudMachine,
                                              QString &strResult,
                                              QWidget *pParent /* = 0 */)
{
    const QString strName = comCloudMachine.GetName();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        strResult = strName;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineOSTypeId(const CCloudMachine &comCloudMachine,
                                                  QString &strResult,
                                                  QWidget *pParent /* = 0 */)
{
    const QString strOSTypeId = comCloudMachine.GetOSTypeId();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        strResult = strOSTypeId;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineState(const CCloudMachine &comCloudMachine,
                                               KCloudMachineState &enmResult,
                                               QWidget *pParent /* = 0 */)
{
    const KCloudMachineState enmState = comCloudMachine.GetState();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        enmResult = enmState;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineConsoleConnectionFingerprint(const CCloudMachine &comCloudMachine,
                                                                      QString &strResult,
                                                                      QWidget *pParent /* = 0 */)
{
    const QString strConsoleConnectionFingerprint = comCloudMachine.GetConsoleConnectionFingerprint();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        strResult = strConsoleConnectionFingerprint;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                      CForm &comResult,
                                                      QWidget *pParent /* = 0 */)
{
    /* Acquire machine name first: */
    QString strMachineName;
    if (!cloudMachineName(comCloudMachine, strMachineName))
        return false;

    /* Prepare settings form: */
    CForm comForm;

    /* Now execute GetSettingsForm async method: */
    CProgress comProgress = comCloudMachine.GetSettingsForm(comForm);
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }

    /* Show "Get settings form" progress: */
    msgCenter().showModalProgressDialog(comProgress,
                                        strMachineName,
                                        ":/progress_settings_90px.png", pParent, 0);
    if (comProgress.GetCanceled())
        return false;
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
        return false;
    }

    /* Return result: */
    comResult = comForm;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                      CForm &comResult,
                                                      QString &strErrorMessage)
{
    /* Prepare settings form: */
    CForm comForm;

    /* Now execute GetSettingsForm async method: */
    CProgress comProgress = comCloudMachine.GetSettingsForm(comForm);
    if (!comCloudMachine.isOk())
    {
        strErrorMessage = UIErrorString::formatErrorInfo(comCloudMachine);
        return false;
    }

    /* Wait for "Get settings form" progress: */
    comProgress.WaitForCompletion(-1);
    if (comProgress.GetCanceled())
        return false;
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        return false;
    }

    /* Return result: */
    comResult = comForm;
    return true;
}

bool UICloudNetworkingStuff::applyCloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                           CForm comForm,
                                                           QWidget *pParent /* = 0 */)
{
    /* Acquire machine name first: */
    QString strMachineName;
    if (!cloudMachineName(comCloudMachine, strMachineName))
        return false;

    /* Now execute Apply async method: */
    CProgress comProgress = comForm.Apply();
    if (!comForm.isOk())
    {
        msgCenter().cannotApplyCloudMachineFormSettings(comForm, strMachineName, pParent);
        return false;
    }

    /* Show "Apply" progress: */
    msgCenter().showModalProgressDialog(comProgress,
                                        strMachineName,
                                        ":/progress_settings_90px.png", pParent, 0);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotApplyCloudMachineFormSettings(comProgress, strMachineName, pParent);
        return false;
    }

    /* Return result: */
    return true;
}

QMap<QString, QString> UICloudNetworkingStuff::listInstances(const CCloudClient &comCloudClient,
                                                             QWidget *pParent /* = 0 */)
{
    /* Prepare VM names, ids and states.
     * Currently we are interested in Running and Stopped VMs only. */
    CStringArray comNames;
    CStringArray comIDs;
    const QVector<KCloudMachineState> cloudMachineStates  = QVector<KCloudMachineState>()
                                                         << KCloudMachineState_Running
                                                         << KCloudMachineState_Stopped;

    /* Now execute ListInstances async method: */
    CProgress comProgress = comCloudClient.ListInstances(cloudMachineStates, comNames, comIDs);
    if (!comCloudClient.isOk())
    {
        if (pParent)
            msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
    }
    else
    {
        /* Show "Acquire cloud instances" progress: */
        if (pParent)
            msgCenter().showModalProgressDialog(comProgress,
                                                QString(),
                                                ":/progress_reading_appliance_90px.png", pParent, 0);
        else
            comProgress.WaitForCompletion(-1);
        if (!comProgress.GetCanceled())
        {
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            {
                if (pParent)
                    msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
            }
            else
            {
                /* Fetch acquired objects to map: */
                const QVector<QString> instanceIds = comIDs.GetValues();
                const QVector<QString> instanceNames = comNames.GetValues();
                QMap<QString, QString> resultMap;
                for (int i = 0; i < instanceIds.size(); ++i)
                    resultMap[instanceIds.at(i)] = instanceNames.at(i);
                return resultMap;
            }
        }
    }

    /* Return empty map by default: */
    return QMap<QString, QString>();
}
