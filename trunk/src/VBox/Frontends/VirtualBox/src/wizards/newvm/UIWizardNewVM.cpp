/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVM class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QAbstractButton>
#include <QLayout>

/* GUI includes: */
#include "UICommon.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVMPageBasic3.h"
#include "UIWizardNewVMPageBasic4.h"
#include "UIWizardNewVMPageExpert.h"
#include "UIMessageCenter.h"
#include "UIMedium.h"

/* COM includes: */
#include "CAudioAdapter.h"
#include "CBIOSSettings.h"
#include "CGraphicsAdapter.h"
#include "CMediumFormat.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"
#include "CExtPackManager.h"
#include "CStorageController.h"

/* Namespaces: */
using namespace UIExtraDataDefs;


UIUnattendedInstallData::UIUnattendedInstallData()
    : m_fUnattendedEnabled(false)
    , m_fStartHeadless(false)
{
}

UIWizardNewVM::UIWizardNewVM(QWidget *pParent, const QString &strGroup /* = QString() */)
    : UIWizard(pParent, WizardType_NewVM)
    , m_strGroup(strGroup)
    , m_iIDECount(0)
    , m_iSATACount(0)
    , m_iSCSICount(0)
    , m_iFloppyCount(0)
    , m_iSASCount(0)
    , m_iUSBCount(0)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_new_welcome.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    assignBackground(":/wizard_new_welcome_bg.png");
#endif /* VBOX_WS_MAC */
    /* Register classes: */
    qRegisterMetaType<CGuestOSType>();

    connect(this, &UIWizardNewVM::rejected, this, &UIWizardNewVM::sltHandleWizardCancel);
}

void UIWizardNewVM::prepare()
{
    enableHelpButton("gui-createvm");
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            setPage(Page1, new UIWizardNewVMPageBasic1(m_strGroup));
            setPage(Page2, new UIWizardNewVMPageBasic2);
            setPage(Page3, new UIWizardNewVMPageBasic3);
            setPage(Page4, new UIWizardNewVMPageBasic4);

            setStartId(Page1);
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardNewVMPageExpert(m_strGroup));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}

bool UIWizardNewVM::createVM()
{
    /* Get VBox object: */
    CVirtualBox vbox = uiCommon().virtualBox();

    /* OS type: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    QString strTypeId = type.GetId();

    /* Create virtual machine: */
    if (m_machine.isNull())
    {
        QVector<QString> groups;
        if (!m_strGroup.isEmpty())
            groups << m_strGroup;
        m_machine = vbox.CreateMachine(field("machineFilePath").toString(),
                                       field("name").toString(),
                                       groups, strTypeId, QString());
        if (!vbox.isOk())
        {
            msgCenter().cannotCreateMachine(vbox, this);
            return false;
        }

        /* The First RUN Wizard is to be shown:
         * 1. if we don't attach any virtual hard-drive
         * 2. or attach a new (empty) one.
         * 3. and if the unattended install is not enabled
         * 4. User did not select an ISO image file
         * Usually we are assigning extra-data values through UIExtraDataManager,
         * but in that special case VM was not registered yet, so UIExtraDataManager is unaware of it: */
        if (ISOFilePath().isEmpty() &&
            !isUnattendedEnabled() &&
            !m_virtualDisk.isNull())
            m_machine.SetExtraData(GUI_FirstRun, "yes");
    }

#if 0
    /* Configure the newly created vm here in GUI by several calls to API: */
    configureVM(strTypeId, type);
#else
    /* The newer and less tested way of configuring vms: */
    m_machine.ApplyDefaults(QString());
    /* Apply user preferences again. IMachine::applyDefaults may have overwritten the user setting: */
    m_machine.SetMemorySize(field("baseMemory").toUInt());
    int iVPUCount = qMax((unsigned)1, field("VCPUCount").toUInt());
    m_machine.SetCPUCount(iVPUCount);
    /* Correct the VRAM size since API does not take fullscreen memory requirements into account: */
    CGraphicsAdapter comGraphics = m_machine.GetGraphicsAdapter();
    comGraphics.SetVRAMSize(qMax(comGraphics.GetVRAMSize(), (ULONG)(UICommon::requiredVideoMemory(strTypeId) / _1M)));
    /* Enabled I/O APIC explicitly in we have more than 1 VCPU: */
    if (iVPUCount > 1)
        m_machine.GetBIOSSettings().SetIOAPICEnabled(true);

    /* Set recommended firmware type: */
    m_machine.SetFirmwareType(getBoolFieldValue("EFIEnabled") ? KFirmwareType_EFI : KFirmwareType_BIOS);
#endif

    /* Register the VM prior to attaching hard disks: */
    vbox.RegisterMachine(m_machine);
    if (!vbox.isOk())
    {
        msgCenter().cannotRegisterMachine(vbox, m_machine.GetName(), this);
        return false;
    }
    return attachDefaultDevices(type);
}

bool UIWizardNewVM::createVirtualDisk()
{
    /* Gather attributes: */
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    /* uVariant is of type KMediumVariant*/
    qulonglong uMediumVariant = field("mediumVariant").toULongLong();
    QString strMediumPath = field("mediumPath").toString();
    qulonglong uSize = field("mediumSize").toULongLong();
    /* Check attributes: */
    AssertReturn(!strMediumPath.isNull(), false);
    AssertReturn(uSize > 0, false);

    /* Get VBox object: */
    CVirtualBox vbox = uiCommon().virtualBox();

    /* Create new virtual hard-disk: */
    CMedium newVirtualDisk = vbox.CreateMedium(mediumFormat.GetName(), strMediumPath, KAccessMode_ReadWrite, KDeviceType_HardDisk);
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateHardDiskStorage(vbox, strMediumPath, this);
        return false;
    }

    /* Compose medium-variant: */
    QVector<KMediumVariant> variants(sizeof(qulonglong)*8);
    for (int i = 0; i < variants.size(); ++i)
    {
        qulonglong temp = uMediumVariant;
        temp &= UINT64_C(1)<<i;
        variants[i] = (KMediumVariant)temp;
    }

    /* Create base storage for the new virtual-disk: */
    CProgress progress = newVirtualDisk.CreateBaseStorage(uSize, variants);
    if (!newVirtualDisk.isOk())
    {
        msgCenter().cannotCreateHardDiskStorage(newVirtualDisk, strMediumPath, this);
        return false;
    }

    /* Show creation progress: */
    msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_media_create_90px.png", this);
    if (progress.GetCanceled())
        return false;
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        msgCenter().cannotCreateHardDiskStorage(progress, strMediumPath, this);
        return false;
    }

    /* Inform UICommon about it: */
    uiCommon().createMedium(UIMedium(newVirtualDisk, UIMediumDeviceType_HardDisk, KMediumState_Created));

    /* Remember created virtual-disk: */
    m_virtualDisk = newVirtualDisk;

    return true;
}

void UIWizardNewVM::deleteVirtualDisk()
{
    /* Make sure virtual-disk valid: */
    if (m_virtualDisk.isNull())
        return;

    /* Remember virtual-disk attributes: */
    QString strLocation = m_virtualDisk.GetLocation();
    /* Prepare delete storage progress: */
    CProgress progress = m_virtualDisk.DeleteStorage();
    if (m_virtualDisk.isOk())
    {
        /* Show delete storage progress: */
        msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_media_delete_90px.png", this);
        if (!progress.isOk() || progress.GetResultCode() != 0)
            msgCenter().cannotDeleteHardDiskStorage(progress, strLocation, this);
    }
    else
        msgCenter().cannotDeleteHardDiskStorage(m_virtualDisk, strLocation, this);

    /* Detach virtual-disk anyway: */
    m_virtualDisk.detach();
}

void UIWizardNewVM::configureVM(const QString &strGuestTypeId, const CGuestOSType &comGuestType)
{
    /* Get graphics adapter: */
    CGraphicsAdapter comGraphics = m_machine.GetGraphicsAdapter();

    /* RAM size: */
    m_machine.SetMemorySize(field("baseMemory").toInt());

    /* VCPU count: */
    int iVPUCount = qMax((unsigned)1, field("VCPUCount").toUInt());
    m_machine.SetCPUCount(iVPUCount);

    /* Enabled I/O APIC explicitly in we have more than 1 VCPU: */
    if (iVPUCount > 1)
        m_machine.GetBIOSSettings().SetIOAPICEnabled(true);

    /* Graphics Controller type: */
    comGraphics.SetGraphicsControllerType(comGuestType.GetRecommendedGraphicsController());

    /* VRAM size - select maximum between recommended and minimum for fullscreen: */
    comGraphics.SetVRAMSize(qMax(comGuestType.GetRecommendedVRAM(), (ULONG)(UICommon::requiredVideoMemory(strGuestTypeId) / _1M)));

    /* Selecting recommended chipset type: */
    m_machine.SetChipsetType(comGuestType.GetRecommendedChipset());

    /* Selecting recommended Audio Controller: */
    m_machine.GetAudioAdapter().SetAudioController(comGuestType.GetRecommendedAudioController());
    /* And the Audio Codec: */
    m_machine.GetAudioAdapter().SetAudioCodec(comGuestType.GetRecommendedAudioCodec());
    /* Enabling audio by default: */
    m_machine.GetAudioAdapter().SetEnabled(true);
    m_machine.GetAudioAdapter().SetEnabledOut(true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2): */
    CUSBDeviceFilters usbDeviceFilters = m_machine.GetUSBDeviceFilters();
    bool fOhciEnabled = false;
    if (!usbDeviceFilters.isNull() && comGuestType.GetRecommendedUSB3() && m_machine.GetUSBProxyAvailable())
    {
        /* USB 3.0 is only available if the proper ExtPack is installed: */
        CExtPackManager extPackManager = uiCommon().virtualBox().GetExtensionPackManager();
        if (extPackManager.isNull() || extPackManager.IsExtPackUsable(GUI_ExtPackName))
        {
            m_machine.AddUSBController("XHCI", KUSBControllerType_XHCI);
            /* xHCI includes OHCI */
            fOhciEnabled = true;
        }
    }
    if (   !fOhciEnabled
        && !usbDeviceFilters.isNull() && comGuestType.GetRecommendedUSB() && m_machine.GetUSBProxyAvailable())
    {
        m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
        fOhciEnabled = true;
        /* USB 2.0 is only available if the proper ExtPack is installed.
         * Note. Configuring EHCI here and providing messages about
         * the missing extpack isn't exactly clean, but it is a
         * necessary evil to patch over legacy compatability issues
         * introduced by the new distribution model. */
        CExtPackManager extPackManager = uiCommon().virtualBox().GetExtensionPackManager();
        if (extPackManager.isNull() || extPackManager.IsExtPackUsable(GUI_ExtPackName))
            m_machine.AddUSBController("EHCI", KUSBControllerType_EHCI);
    }

    /* Create a floppy controller if recommended: */
    QString strFloppyName = getNextControllerName(KStorageBus_Floppy);
    if (comGuestType.GetRecommendedFloppy())
    {
        m_machine.AddStorageController(strFloppyName, KStorageBus_Floppy);
        CStorageController flpCtr = m_machine.GetStorageControllerByName(strFloppyName);
        flpCtr.SetControllerType(KStorageControllerType_I82078);
    }

    /* Create recommended DVD storage controller: */
    KStorageBus strDVDBus = comGuestType.GetRecommendedDVDStorageBus();
    QString strDVDName = getNextControllerName(strDVDBus);
    m_machine.AddStorageController(strDVDName, strDVDBus);

    /* Set recommended DVD storage controller type: */
    CStorageController dvdCtr = m_machine.GetStorageControllerByName(strDVDName);
    KStorageControllerType dvdStorageControllerType = comGuestType.GetRecommendedDVDStorageController();
    dvdCtr.SetControllerType(dvdStorageControllerType);

    /* Create recommended HD storage controller if it's not the same as the DVD controller: */
    KStorageBus ctrHDBus = comGuestType.GetRecommendedHDStorageBus();
    KStorageControllerType hdStorageControllerType = comGuestType.GetRecommendedHDStorageController();
    CStorageController hdCtr;
    QString strHDName;
    if (ctrHDBus != strDVDBus || hdStorageControllerType != dvdStorageControllerType)
    {
        strHDName = getNextControllerName(ctrHDBus);
        m_machine.AddStorageController(strHDName, ctrHDBus);
        hdCtr = m_machine.GetStorageControllerByName(strHDName);
        hdCtr.SetControllerType(hdStorageControllerType);
    }
    else
    {
        /* The HD controller is the same as DVD: */
        hdCtr = dvdCtr;
        strHDName = strDVDName;
    }

    /* Limit the AHCI port count if it's used because windows has trouble with
       too many ports and other guest (OS X in particular) may take extra long
       to boot: */
    if (hdStorageControllerType == KStorageControllerType_IntelAhci)
        hdCtr.SetPortCount(1 + (dvdStorageControllerType == KStorageControllerType_IntelAhci));
    else if (dvdStorageControllerType == KStorageControllerType_IntelAhci)
        dvdCtr.SetPortCount(1);

    /* Turn on PAE, if recommended: */
    m_machine.SetCPUProperty(KCPUPropertyType_PAE, comGuestType.GetRecommendedPAE());

    /* Set the recommended triple fault behavior: */
    m_machine.SetCPUProperty(KCPUPropertyType_TripleFaultReset, comGuestType.GetRecommendedTFReset());

    /* Set recommended firmware type: */
    m_machine.SetFirmwareType(getBoolFieldValue("EFIEnabled") ? KFirmwareType_EFI : KFirmwareType_BIOS);

    /* Set recommended human interface device types: */
    if (comGuestType.GetRecommendedUSBHID())
    {
        m_machine.SetKeyboardHIDType(KKeyboardHIDType_USBKeyboard);
        m_machine.SetPointingHIDType(KPointingHIDType_USBMouse);
        if (!fOhciEnabled && !usbDeviceFilters.isNull())
            m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
    }

    if (comGuestType.GetRecommendedUSBTablet())
    {
        m_machine.SetPointingHIDType(KPointingHIDType_USBTablet);
        if (!fOhciEnabled && !usbDeviceFilters.isNull())
            m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
    }

    /* Set HPET flag: */
    m_machine.SetHPETEnabled(comGuestType.GetRecommendedHPET());

    /* Set UTC flags: */
    m_machine.SetRTCUseUTC(comGuestType.GetRecommendedRTCUseUTC());

    /* Set graphic bits: */
    if (comGuestType.GetRecommended2DVideoAcceleration())
        comGraphics.SetAccelerate2DVideoEnabled(comGuestType.GetRecommended2DVideoAcceleration());

    if (comGuestType.GetRecommended3DAcceleration())
        comGraphics.SetAccelerate3DEnabled(comGuestType.GetRecommended3DAcceleration());
}

bool UIWizardNewVM::attachDefaultDevices(const CGuestOSType &comGuestType)
{
    bool success = false;
    QUuid uMachineId = m_machine.GetId();
    CSession session = uiCommon().openSession(uMachineId);
    if (!session.isNull())
    {
        CMachine machine = session.GetMachine();
        if (!m_virtualDisk.isNull())
        {
            KStorageBus enmHDDBus = comGuestType.GetRecommendedHDStorageBus();
            CStorageController comHDDController = m_machine.GetStorageControllerByInstance(enmHDDBus, 0);
            if (!comHDDController.isNull())
            {
                machine.AttachDevice(comHDDController.GetName(), 0, 0, KDeviceType_HardDisk, m_virtualDisk);
                if (!machine.isOk())
                    msgCenter().cannotAttachDevice(machine, UIMediumDeviceType_HardDisk, field("mediumPath").toString(),
                                                   StorageSlot(enmHDDBus, 0, 0), this);
            }
        }

        /* Attach optical drive: */
        KStorageBus enmDVDBus = comGuestType.GetRecommendedDVDStorageBus();
        CStorageController comDVDController = m_machine.GetStorageControllerByInstance(enmDVDBus, 0);
        if (!comDVDController.isNull())
        {
            CMedium opticalDisk;
            QString strISOFilePath = ISOFilePath();
            if (!strISOFilePath.isEmpty() && !getBoolFieldValue("isUnattendedEnabled"))
            {
                CVirtualBox vbox = uiCommon().virtualBox();
                opticalDisk =
                    vbox.OpenMedium(strISOFilePath, KDeviceType_DVD,  KAccessMode_ReadWrite, false);
                if (!vbox.isOk())
                    msgCenter().cannotOpenMedium(vbox, strISOFilePath, this);
            }
            machine.AttachDevice(comDVDController.GetName(), 1, 0, KDeviceType_DVD, opticalDisk);
            if (!machine.isOk())
                msgCenter().cannotAttachDevice(machine, UIMediumDeviceType_DVD, QString(),
                                               StorageSlot(enmDVDBus, 1, 0), this);
        }

        /* Attach an empty floppy drive if recommended */
        if (comGuestType.GetRecommendedFloppy()) {
            CStorageController comFloppyController = m_machine.GetStorageControllerByInstance(KStorageBus_Floppy, 0);
            if (!comFloppyController.isNull())
            {
                machine.AttachDevice(comFloppyController.GetName(), 0, 0, KDeviceType_Floppy, CMedium());
                if (!machine.isOk())
                    msgCenter().cannotAttachDevice(machine, UIMediumDeviceType_Floppy, QString(),
                                                   StorageSlot(KStorageBus_Floppy, 0, 0), this);
            }
        }

        if (machine.isOk())
        {
            machine.SaveSettings();
            if (machine.isOk())
                success = true;
            else
                msgCenter().cannotSaveMachineSettings(machine, this);
        }

        session.UnlockMachine();
    }
    if (!success)
    {
        CVirtualBox vbox = uiCommon().virtualBox();
        /* Unregister on failure */
        QVector<CMedium> aMedia = m_machine.Unregister(KCleanupMode_UnregisterOnly);   /// @todo replace with DetachAllReturnHardDisksOnly once a progress dialog is in place below
        if (vbox.isOk())
        {
            CProgress progress = m_machine.DeleteConfig(aMedia);
            progress.WaitForCompletion(-1);         /// @todo do this nicely with a progress dialog, this can delete lots of files
        }
        return false;
    }

    /* Ensure we don't try to delete a newly created virtual hard drive on success: */
    if (!m_virtualDisk.isNull())
        m_virtualDisk.detach();

    return true;
}

QString UIWizardNewVM::getStringFieldValue(const QString &strFieldName) const
{
    QVariant fieldValue = field(strFieldName);
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::QString))
        return fieldValue.toString();
    return QString();
}

bool UIWizardNewVM::getBoolFieldValue(const QString &strFieldName) const
{
    QVariant fieldValue = field(strFieldName);
    if (!fieldValue.isNull() && fieldValue.isValid() && fieldValue.canConvert(QMetaType::Bool))
        return fieldValue.toBool();
    return false;
}

void UIWizardNewVM::sltHandleWizardCancel()
{
    switch (mode())
    {
        case WizardMode_Basic:
        {
            UIWizardNewVMPageBasic1 *pPage = qobject_cast<UIWizardNewVMPageBasic1*> (page(Page1));
            /* Make sure that we were able to find the page that created the folder. */
            Assert(pPage);
            if (pPage)
                pPage->cleanupMachineFolder(true);
            break;
        }
        case WizardMode_Expert:
        {
            UIWizardNewVMPageExpert *pPage = qobject_cast<UIWizardNewVMPageExpert*> (page(PageExpert));
            if (pPage)
                pPage->cleanupMachineFolder(true);
            break;
        }
        default:
            break;
    }
}

void UIWizardNewVM::sltHandleDetectedOSTypeChange()
{
    UIWizardNewVMPageBasic1 *pPage = qobject_cast<UIWizardNewVMPageBasic1*>(page(Page1));
    if (!pPage)
        return;
    pPage->setTypeByISODetectedOSType(getStringFieldValue("detectedOSTypeId"));
}

void UIWizardNewVM::sltCustomButtonClicked(int iId)
{
    UIWizard::sltCustomButtonClicked(iId);
    setFieldsFromDefaultUnttendedInstallData();
}

void UIWizardNewVM::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Create Virtual Machine"));
    setButtonText(QWizard::FinishButton, tr("Create"));
}

QString UIWizardNewVM::getNextControllerName(KStorageBus type)
{
    QString strControllerName;
    switch (type)
    {
        case KStorageBus_IDE:
        {
            strControllerName = "IDE";
            ++m_iIDECount;
            if (m_iIDECount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iIDECount);
            break;
        }
        case KStorageBus_SATA:
        {
            strControllerName = "SATA";
            ++m_iSATACount;
            if (m_iSATACount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSATACount);
            break;
        }
        case KStorageBus_SCSI:
        {
            strControllerName = "SCSI";
            ++m_iSCSICount;
            if (m_iSCSICount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSCSICount);
            break;
        }
        case KStorageBus_Floppy:
        {
            strControllerName = "Floppy";
            ++m_iFloppyCount;
            if (m_iFloppyCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iFloppyCount);
            break;
        }
        case KStorageBus_SAS:
        {
            strControllerName = "SAS";
            ++m_iSASCount;
            if (m_iSASCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iSASCount);
            break;
        }
        case KStorageBus_USB:
        {
            strControllerName = "USB";
            ++m_iUSBCount;
            if (m_iUSBCount > 1)
                strControllerName = QString("%1 %2").arg(strControllerName).arg(m_iUSBCount);
            break;
        }
        default:
            break;
    }
    return strControllerName;
}

QUuid UIWizardNewVM::createdMachineId() const
{
    if (m_machine.isOk())
        return m_machine.GetId();
    return QUuid();
}

void UIWizardNewVM::setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData)
{
    m_unattendedInstallData = unattendedInstallData;
    setFieldsFromDefaultUnttendedInstallData();
}

void UIWizardNewVM::setFieldsFromDefaultUnttendedInstallData()
{
    setField("userName", m_unattendedInstallData.m_strUserName);
    setField("password", m_unattendedInstallData.m_strPassword);
    setField("hostname", m_unattendedInstallData.m_strHostname);
    setField("installGuestAdditions", m_unattendedInstallData.m_fInstallGuestAdditions);
    setField("guestAdditionsISOPath", m_unattendedInstallData.m_strGuestAdditionsISOPath);
}

QString UIWizardNewVM::ISOFilePath() const
{
    QString strPath = getStringFieldValue("ISOFilePath");
    if (strPath.isNull() || strPath.isEmpty())
        return QString();
    QFileInfo isoFileInfo(strPath);
    if (isoFileInfo.exists() && isoFileInfo.isReadable())
        return strPath;
    return QString();
}

CMedium &UIWizardNewVM::virtualDisk()
{
    return m_virtualDisk;
}

void UIWizardNewVM::setVirtualDisk(const CMedium &medium)
{
    m_virtualDisk = medium;
}

void UIWizardNewVM::setVirtualDisk(const QUuid &mediumId)
{
    if (m_virtualDisk.GetId() == mediumId)
        return;
    CMedium medium = uiCommon().medium(mediumId).medium();
    if (!medium.isNull())
        setVirtualDisk(medium);
}

const UIUnattendedInstallData &UIWizardNewVM::unattendedInstallData() const
{
    m_unattendedInstallData.m_strISOPath = getStringFieldValue("ISOFilePath");
    m_unattendedInstallData.m_strUserName = getStringFieldValue("userName");
    m_unattendedInstallData.m_strHostname = getStringFieldValue("hostname");
    m_unattendedInstallData.m_strPassword = getStringFieldValue("password");
    m_unattendedInstallData.m_strDetectedOSTypeId = getStringFieldValue("detectedOSTypeId");
    // m_unattendedInstallData.m_strDetectedOSVersion = getStringFieldValue("detectedOSVersion");
    // m_unattendedInstallData.m_strDetectedOSFlavor = getStringFieldValue("detectedOSFlavor");
    // m_unattendedInstallData.m_strDetectedOSLanguages = getStringFieldValue("detectedOSLanguages");
    // m_unattendedInstallData.m_strDetectedOSHints = getStringFieldValue("detectedOSHints");
    m_unattendedInstallData.m_strProductKey = getStringFieldValue("productKey");
    m_unattendedInstallData.m_strGuestAdditionsISOPath = getStringFieldValue("guestAdditionsISOPath");

    m_unattendedInstallData.m_fUnattendedEnabled = getBoolFieldValue("isUnattendedEnabled");
    m_unattendedInstallData.m_fStartHeadless = getBoolFieldValue("startHeadless");
    m_unattendedInstallData.m_fInstallGuestAdditions = getBoolFieldValue("installGuestAdditions");

    m_unattendedInstallData.m_uMachineUid = createdMachineId();

    return m_unattendedInstallData;
}

bool UIWizardNewVM::isUnattendedEnabled() const
{
    QVariant fieldValue = field("isUnattendedEnabled");
    if (fieldValue.isNull() || !fieldValue.isValid() || !fieldValue.canConvert(QMetaType::Bool))
        return false;
    return fieldValue.toBool();
}

bool UIWizardNewVM::isGuestOSTypeWindows() const
{
    return getStringFieldValue("guestOSFamiyId").contains("windows", Qt::CaseInsensitive);
}
