/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportApp class implementation.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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
#include <QFileInfo>
#include <QPushButton>
#include <QVariant>

/* GUI includes: */
#include "UIAddDiskEncryptionPasswordDialog.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UINotificationCenter.h"
#include "UIProgressObject.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppPageExpert.h"
#include "UIWizardExportAppPageFormat.h"
#include "UIWizardExportAppPageSettings.h"
#include "UIWizardExportAppPageVMs.h"

/* COM includes: */
#include "CAppliance.h"
#include "CVFSExplorer.h"


UIWizardExportApp::UIWizardExportApp(QWidget *pParent,
                                     const QStringList &predefinedMachineNames /* = QStringList() */,
                                     bool fFastTraverToExportOCI /* = false */)
    : UINativeWizard(pParent, WizardType_ExportAppliance, WizardMode_Auto,
                     fFastTraverToExportOCI ? "cloud-export-oci" : "ovf")
    , m_predefinedMachineNames(predefinedMachineNames)
    , m_fFastTraverToExportOCI(fFastTraverToExportOCI)
    , m_fFormatCloudOne(false)
    , m_enmMACAddressExportPolicy(MACAddressExportPolicy_KeepAllMACs)
    , m_fManifestSelected(false)
    , m_fIncludeISOsSelected(false)
    , m_enmCloudExportMode(CloudExportMode_DoNotAsk)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_ovf_export.png");
#else
    /* Assign background image: */
    setPixmapName(":/wizard_ovf_export_bg.png");
#endif
}

void UIWizardExportApp::goForward()
{
    wizardButton(WizardButtonType_Next)->click();
}

void UIWizardExportApp::disableButtons()
{
    wizardButton(WizardButtonType_Expert)->setEnabled(false);
    wizardButton(WizardButtonType_Back)->setEnabled(false);
    wizardButton(WizardButtonType_Next)->setEnabled(false);
}

QString UIWizardExportApp::uri(bool fWithFile) const
{
    /* For Cloud formats: */
    if (isFormatCloudOne())
        return QString("%1://").arg(format());
    else
    {
        /* Prepare storage path: */
        QString strPath = path();
        /* Append file name if requested: */
        if (!fWithFile)
        {
            QFileInfo fi(strPath);
            strPath = fi.path();
        }

        /* Just path by default: */
        return strPath;
    }
}

bool UIWizardExportApp::exportAppliance()
{
    /* Check whether there was cloud target selected: */
    if (isFormatCloudOne())
    {
        /* Get appliance: */
        CAppliance comAppliance = cloudAppliance();
        AssertReturn(comAppliance.isNotNull(), false);

        /* Export the VMs, on success we are finished: */
        return exportVMs(comAppliance);
    }
    else
    {
        /* Get appliance: */
        CAppliance comAppliance = localAppliance();
        AssertReturn(comAppliance.isNotNull(), false);

        /* We need to know every filename which will be created, so that we can ask the user for confirmation of overwriting.
         * For that we iterating over all virtual systems & fetch all descriptions of the type HardDiskImage. Also add the
         * manifest file to the check. In the .ova case only the target file itself get checked. */

        /* Compose a list of all required files: */
        QFileInfo fi(path());
        QVector<QString> files;

        /* Add arhive itself: */
        files << fi.fileName();

        /* If archive is of .ovf type: */
        if (fi.suffix().toLower() == "ovf")
        {
            /* Add manifest file if requested: */
            if (isManifestSelected())
                files << fi.baseName() + ".mf";

            /* Add all hard disk images: */
            CVirtualSystemDescriptionVector vsds = comAppliance.GetVirtualSystemDescriptions();
            for (int i = 0; i < vsds.size(); ++i)
            {
                QVector<KVirtualSystemDescriptionType> types;
                QVector<QString> refs, origValues, configValues, extraConfigValues;
                vsds[i].GetDescriptionByType(KVirtualSystemDescriptionType_HardDiskImage, types,
                                             refs, origValues, configValues, extraConfigValues);
                foreach (const QString &strValue, origValues)
                    files << QString("%2").arg(strValue);
            }
        }

        /* Initialize VFS explorer: */
        CVFSExplorer comExplorer = comAppliance.CreateVFSExplorer(uri(false /* fWithFile */));
        if (!comAppliance.isOk())
            return msgCenter().cannotCheckFiles(comAppliance, this);

        /* Update VFS explorer: */
        UINotificationProgressVFSExplorerUpdate *pNotification =
            new UINotificationProgressVFSExplorerUpdate(comExplorer);
        if (!handleNotificationProgressNow(pNotification))
            return false;

        /* Confirm overwriting for existing files: */
        QVector<QString> exists = comExplorer.Exists(files);
        if (!msgCenter().confirmOverridingFiles(exists, this))
            return false;

        /* DELETE all the files which exists after everything is confirmed: */
        if (!exists.isEmpty())
        {
            /* Remove files with VFS explorer: */
            UINotificationProgressVFSExplorerFilesRemove *pNotification =
                new UINotificationProgressVFSExplorerFilesRemove(comExplorer, exists);
            if (!handleNotificationProgressNow(pNotification))
                return false;
        }

        /* Export the VMs, on success we are finished: */
        return exportVMs(comAppliance);
    }
}

void UIWizardExportApp::createVsdLaunchForm()
{
    /* Acquire prepared client and description: */
    CCloudClient comClient = cloudClient();
    CVirtualSystemDescription comVSD = vsd();
    AssertReturnVoid(comClient.isNotNull() && comVSD.isNotNull());

    /* Get Launch description form: */
    CVirtualSystemDescriptionForm comForm;
    CProgress comProgress = comClient.GetLaunchDescriptionForm(comVSD, comForm);
    /* Check for immediate errors: */
    if (!comClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comClient);
    else
    {
        /* Make sure progress initially valid: */
        if (!comProgress.isNull() && !comProgress.GetCompleted())
        {
            /* Create take snapshot progress object: */
            QPointer<UIProgressObject> pObject = new UIProgressObject(comProgress, this);
            if (pObject)
            {
                connect(pObject.data(), &UIProgressObject::sigProgressChange,
                        this, &UIWizardExportApp::sltHandleProgressChange);
                connect(pObject.data(), &UIProgressObject::sigProgressComplete,
                        this, &UIWizardExportApp::sltHandleProgressFinished);
                sltHandleProgressStarted();
                pObject->exec();
                if (pObject)
                    delete pObject;
                else
                {
                    // Premature application shutdown,
                    // exit immediately:
                    return;
                }
            }
        }

        /* Check for progress errors: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress);
        else
        {
            /* Check whether form really read: */
            if (comForm.isNotNull())
            {
                /* Remember Virtual System Description Form: */
                setVsdLaunchForm(comForm);
            }
        }
    }
}

bool UIWizardExportApp::createCloudVM()
{
    /* Acquire prepared client and description: */
    CCloudClient comClient = cloudClient();
    CVirtualSystemDescription comVSD = vsd();
    AssertReturn(comClient.isNotNull() && comVSD.isNotNull(), false);

    /* Initiate cloud VM creation procedure: */
    CCloudMachine comMachine;

    /* Create cloud VM: */
    UINotificationProgressCloudMachineCreate *pNotification = new UINotificationProgressCloudMachineCreate(comClient,
                                                                                                           comMachine,
                                                                                                           comVSD,
                                                                                                           format(),
                                                                                                           profileName());
    connect(pNotification, &UINotificationProgressCloudMachineCreate::sigCloudMachineCreated,
            &uiCommon(), &UICommon::sltHandleCloudMachineAdded);
    gpNotificationCenter->append(pNotification);

    /* Return result: */
    return true;
}

void UIWizardExportApp::populatePages()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            addPage(new UIWizardExportAppPageVMs(m_predefinedMachineNames, m_fFastTraverToExportOCI));
            addPage(new UIWizardExportAppPageFormat(m_fFastTraverToExportOCI));
            addPage(new UIWizardExportAppPageSettings);
            break;
        }
        case WizardMode_Expert:
        {
            addPage(new UIWizardExportAppPageExpert(m_predefinedMachineNames, m_fFastTraverToExportOCI));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

void UIWizardExportApp::retranslateUi()
{
    /* Call to base-class: */
    UINativeWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Export Virtual Appliance"));
    /// @todo implement this?
    //setButtonText(QWizard::FinishButton, tr("Export"));
}

bool UIWizardExportApp::exportVMs(CAppliance &comAppliance)
{
    /* Get the map of the password IDs: */
    EncryptedMediumMap encryptedMedia;
    foreach (const QString &strPasswordId, comAppliance.GetPasswordIds())
        foreach (const QUuid &uMediumId, comAppliance.GetMediumIdsForPasswordId(strPasswordId))
            encryptedMedia.insert(strPasswordId, uMediumId);

    /* Ask for the disk encryption passwords if necessary: */
    if (!encryptedMedia.isEmpty())
    {
        /* Modal dialog can be destroyed in own event-loop as a part of application
         * termination procedure. We have to make sure that the dialog pointer is
         * always up to date. So we are wrapping created dialog with QPointer. */
        QPointer<UIAddDiskEncryptionPasswordDialog> pDlg =
            new UIAddDiskEncryptionPasswordDialog(this,
                                                  window()->windowTitle(),
                                                  encryptedMedia);

        /* Execute the dialog: */
        if (pDlg->exec() != QDialog::Accepted)
        {
            /* Delete the dialog: */
            delete pDlg;
            return false;
        }

        /* Acquire the passwords provided: */
        const EncryptionPasswordMap encryptionPasswords = pDlg->encryptionPasswords();

        /* Delete the dialog: */
        delete pDlg;

        /* Provide appliance with passwords if possible: */
        comAppliance.AddPasswords(encryptionPasswords.keys().toVector(),
                                  encryptionPasswords.values().toVector());
        if (!comAppliance.isOk())
        {
            msgCenter().cannotAddDiskEncryptionPassword(comAppliance);
            return false;
        }
    }

    /* Prepare export options: */
    QVector<KExportOptions> options;
    switch (macAddressExportPolicy())
    {
        case MACAddressExportPolicy_StripAllNonNATMACs: options.append(KExportOptions_StripAllNonNATMACs); break;
        case MACAddressExportPolicy_StripAllMACs: options.append(KExportOptions_StripAllMACs); break;
        default: break;
    }
    if (isManifestSelected())
        options.append(KExportOptions_CreateManifest);
    if (isIncludeISOsSelected())
        options.append(KExportOptions_ExportDVDImages);

    /* Is this VM being exported to cloud? */
    if (isFormatCloudOne())
    {
        /* Prepare Export VM progress: */
        CProgress comProgress = comAppliance.Write(format(), options, uri());
        if (!comAppliance.isOk())
        {
            msgCenter().cannotExportAppliance(comAppliance, this);
            return false;
        }

        /* Show Export VM progress: */
        msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIWizardExportApp", "Exporting Appliance ..."),
                                            ":/progress_export_90px.png", this);
        if (comProgress.GetCanceled())
            return false;
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            msgCenter().cannotExportAppliance(comProgress, comAppliance.GetPath(), this);
            return false;
        }
    }
    /* Is this VM being exported locally? */
    else
    {
        /* Export appliance: */
        UINotificationProgressApplianceExport *pNotification = new UINotificationProgressApplianceExport(comAppliance,
                                                                                                         format(),
                                                                                                         options,
                                                                                                         uri());
        gpNotificationCenter->append(pNotification);
    }

    /* Success finally: */
    return true;
}
