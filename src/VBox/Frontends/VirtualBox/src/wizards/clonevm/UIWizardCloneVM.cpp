/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVM class implementation.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UIWizardCloneVM.h"
#include "UIWizardCloneVMPageBasic1.h"
#include "UIWizardCloneVMPageBasic2.h"
#include "UIWizardCloneVMPageBasic3.h"
#include "UIWizardCloneVMPageExpert.h"

/* COM includes: */
#include "CConsole.h"
#include "CSystemProperties.h"


UIWizardCloneVM::UIWizardCloneVM(QWidget *pParent, const CMachine &machine,
                                 const QString &strGroup, CSnapshot snapshot /* = CSnapshot() */)
    : UINativeWizard(pParent, WizardType_CloneVM)
    , m_machine(machine)
    , m_snapshot(snapshot)
    , m_strGroup(strGroup)
    , m_iCloneModePageIndex(-1)
    , m_strCloneName(!machine.isNull() ? machine.GetName() : QString())
    , m_enmCloneMode(KCloneMode_MachineState)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_clone.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    setPixmapName(":/wizard_clone_bg.png");
#endif /* VBOX_WS_MAC */
}

void UIWizardCloneVM::setCloneModePageVisible(bool fIsFullClone)
{
    if (m_iCloneModePageIndex == -1)
        return;
    setPageVisible(m_iCloneModePageIndex, fIsFullClone);
}

bool UIWizardCloneVM::isCloneModePageVisible() const
{
    /* If we did not create the clone mode page return false: */
    if (m_iCloneModePageIndex == -1)
        return false;
    return isPageVisible(m_iCloneModePageIndex);
}

void UIWizardCloneVM::setCloneName(const QString &strCloneName)
{
    m_strCloneName = strCloneName;
}

const QString &UIWizardCloneVM::cloneName() const
{
    return m_strCloneName;
}

void UIWizardCloneVM::setCloneFilePath(const QString &strCloneFilePath)
{
    m_strCloneFilePath = strCloneFilePath;
}

const QString &UIWizardCloneVM::cloneFilePath() const
{
    return m_strCloneFilePath;
}

MACAddressClonePolicy UIWizardCloneVM::macAddressClonePolicy() const
{
    return m_enmMACAddressClonePolicy;
}

void UIWizardCloneVM::setMacAddressPolicy(MACAddressClonePolicy enmMACAddressClonePolicy)
{
    m_enmMACAddressClonePolicy = enmMACAddressClonePolicy;
}

bool UIWizardCloneVM::keepDiskNames() const
{
    return m_fKeepDiskNames;
}

void UIWizardCloneVM::setKeepDiskNames(bool fKeepDiskNames)
{
    m_fKeepDiskNames = fKeepDiskNames;
}

bool UIWizardCloneVM::keepHardwareUUIDs() const
{
    return m_fKeepHardwareUUIDs;
}

void UIWizardCloneVM::setKeepHardwareUUIDs(bool fKeepHardwareUUIDs)
{
    m_fKeepHardwareUUIDs = fKeepHardwareUUIDs;
}

bool UIWizardCloneVM::linkedClone() const
{
    return m_fLinkedClone;
}

void UIWizardCloneVM::setLinkedClone(bool fLinkedClone)
{
    m_fLinkedClone = fLinkedClone;
}

KCloneMode UIWizardCloneVM::cloneMode() const
{
    return m_enmCloneMode;
}

void UIWizardCloneVM::setCloneMode(KCloneMode enmCloneMode)
{
    m_enmCloneMode = enmCloneMode;
}

bool UIWizardCloneVM::cloneVM()
{
    /* Get VBox object: */
    CVirtualBox vbox = uiCommon().virtualBox();

    /* Prepare machine for cloning: */
    CMachine srcMachine = m_machine;

    /* If the user like to create a linked clone from the current machine, we
     * have to take a little bit more action. First we create an snapshot, so
     * that new differencing images on the source VM are created. Based on that
     * we could use the new snapshot machine for cloning. */
    if (m_fLinkedClone && m_snapshot.isNull())
    {

        /* Open session: */
        CSession session = uiCommon().openSession(m_machine.GetId());
        if (session.isNull())
            return false;

        /* Prepare machine: */
        CMachine machine = session.GetMachine();

        /* Take the snapshot: */
        QString strSnapshotName = tr("Linked Base for %1 and %2").arg(m_machine.GetName()).arg(m_strCloneName);
        QUuid uSnapshotId;
        CProgress progress = machine.TakeSnapshot(strSnapshotName, "", true, uSnapshotId);

        if (machine.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog: */
            msgCenter().showModalProgressDialog(progress, m_machine.GetName(), ":/progress_snapshot_create_90px.png", this);

            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotTakeSnapshot(progress, m_machine.GetName(), this);
                return false;
            }
        }
        else
        {
            msgCenter().cannotTakeSnapshot(machine, m_machine.GetName(), this);
            return false;
        }

        /* Unlock machine finally: */
        session.UnlockMachine();

        /* Get the new snapshot and the snapshot machine. */
        const CSnapshot &newSnapshot = m_machine.FindSnapshot(uSnapshotId.toString());
        if (newSnapshot.isNull())
        {
            msgCenter().cannotFindSnapshotByName(m_machine, strSnapshotName, this);
            return false;
        }
        srcMachine = newSnapshot.GetMachine();
    }

    /* Create a new machine object. */
    CMachine cloneMachine = vbox.CreateMachine(m_strCloneFilePath, m_strCloneName, QVector<QString>(), QString(), QString());
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateMachine(vbox, this);
        return false;
    }

    /* Clone options vector to pass to cloning: */
    QVector<KCloneOptions> options;
    /* Set the selected MAC address policy: */
    switch (m_enmMACAddressClonePolicy)
    {
        case MACAddressClonePolicy_KeepAllMACs:
            options.append(KCloneOptions_KeepAllMACs);
            break;
        case MACAddressClonePolicy_KeepNATMACs:
            options.append(KCloneOptions_KeepNATMACs);
            break;
        default:
            break;
    }

    if (m_fKeepDiskNames)
        options.append(KCloneOptions_KeepDiskNames);
    if (m_fKeepHardwareUUIDs)
        options.append(KCloneOptions_KeepHwUUIDs);

    /* Linked clones requested? */
    if (m_fLinkedClone)
        options.append(KCloneOptions_Link);

    /* Clone VM: */
    UINotificationProgressMachineCopy *pNotification = new UINotificationProgressMachineCopy(srcMachine,
                                                                                             cloneMachine,
                                                                                             m_enmCloneMode,
                                                                                             options);
    connect(pNotification, &UINotificationProgressMachineCopy::sigMachineCopied,
            &uiCommon(), &UICommon::sltHandleMachineCreated);
    gpNotificationCenter->append(pNotification);

    return true;
}

void UIWizardCloneVM::retranslateUi()
{
    /* Call to base-class: */
    UINativeWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Clone Virtual Machine"));
}

void UIWizardCloneVM::populatePages()
{
    QString strDefaultMachineFolder = uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            addPage(new UIWizardCloneVMPageBasic1(m_strCloneName, strDefaultMachineFolder, m_strGroup));
            addPage(new UIWizardCloneVMPageBasic2(m_snapshot.isNull()));
            if (m_machine.GetSnapshotCount() > 0)
                m_iCloneModePageIndex = addPage(new UIWizardCloneVMPageBasic3(m_snapshot.isNull() ? false : m_snapshot.GetChildrenCount() > 0));
            break;
        }
        case WizardMode_Expert:
        {
            addPage(new UIWizardCloneVMPageExpert(m_machine.GetName(),
                                                  strDefaultMachineFolder,
                                                  m_snapshot.isNull(),
                                                  m_snapshot.isNull() ? false : m_snapshot.GetChildrenCount() > 0,
                                                  m_strGroup));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    //UINativeWizard::prepare();
}
