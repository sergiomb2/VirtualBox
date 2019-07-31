/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineAttributeSetter namespace implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <QVariant>

/* GUI includes: */
#include "UIBootOrderEditor.h"
#include "UICommon.h"
#include "UIMachineAttributeSetter.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAudioAdapter.h"


void UIMachineAttributeSetter::setMachineAttribute(const CMachine &comConstMachine,
                                                   const MachineAttribute &enmType,
                                                   const QVariant &guiAttribute)
{
    /* Get editable machine & session: */
    CMachine comMachine = comConstMachine;
    CSession comSession = uiCommon().tryToOpenSessionFor(comMachine);

    /* Main API block: */
    do
    {
        /* Save machine settings? */
        bool fSaveSettings = true;
        /* Error happened? */
        bool fErrorHappened = false;

        /* Assign attribute depending on passed type: */
        switch (enmType)
        {
            case MachineAttribute_Name:
            {
                /* Change machine name: */
                comMachine.SetName(guiAttribute.toString());
                if (!comMachine.isOk())
                {
                    msgCenter().cannotChangeMachineAttribute(comMachine);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_Location:
            {
                /* Do not save machine settings: */
                fSaveSettings = false;
                /* Prepare machine move progress: */
                CProgress comProgress = comMachine.MoveTo(guiAttribute.toString(), "basic");
                if (!comMachine.isOk())
                {
                    msgCenter().cannotMoveMachine(comMachine);
                    fErrorHappened = true;
                    break;
                }
                /* Show machine move progress: */
                msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(), ":/progress_clone_90px.png");
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                {
                    msgCenter().cannotMoveMachine(comProgress, comMachine.GetName());
                    fErrorHappened = true;
                    break;
                }
                break;
            }
            case MachineAttribute_OSType:
            {
                /* Change machine OS type: */
                comMachine.SetOSTypeId(guiAttribute.toString());
                if (!comMachine.isOk())
                {
                    msgCenter().cannotChangeMachineAttribute(comMachine);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_BaseMemory:
            {
                /* Change machine base memory (RAM): */
                comMachine.SetMemorySize(guiAttribute.toInt());
                if (!comMachine.isOk())
                {
                    msgCenter().cannotChangeMachineAttribute(comMachine);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_BootOrder:
            {
                /* Change machine boot order: */
                saveBootItems(guiAttribute.value<UIBootItemDataList>(), comMachine);
                if (!comMachine.isOk())
                {
                    msgCenter().cannotChangeMachineAttribute(comMachine);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_VideoMemory:
            {
                /* Change machine video memory (VRAM): */
                comMachine.SetVRAMSize(guiAttribute.toInt());
                if (!comMachine.isOk())
                {
                    msgCenter().cannotChangeMachineAttribute(comMachine);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_GraphicsControllerType:
            {
                /* Change machine graphics controller type: */
                comMachine.SetGraphicsControllerType(guiAttribute.value<KGraphicsControllerType>());
                if (!comMachine.isOk())
                {
                    msgCenter().cannotChangeMachineAttribute(comMachine);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_AudioHostDriverType:
            {
                /* Acquire audio adapter: */
                CAudioAdapter comAdapter = comMachine.GetAudioAdapter();
                if (!comMachine.isOk())
                {
                    msgCenter().cannotAcquireMachineParameter(comMachine);
                    fErrorHappened = true;
                    break;
                }
                /* Change audio host driver type: */
                comAdapter.SetAudioDriver(guiAttribute.value<KAudioDriverType>());
                if (!comAdapter.isOk())
                {
                    msgCenter().cannotChangeAudioAdapterAttribute(comAdapter);
                    fErrorHappened = true;
                }
                break;
            }
            case MachineAttribute_AudioControllerType:
            {
                /* Acquire audio adapter: */
                CAudioAdapter comAdapter = comMachine.GetAudioAdapter();
                if (!comMachine.isOk())
                {
                    msgCenter().cannotAcquireMachineParameter(comMachine);
                    fErrorHappened = true;
                    break;
                }
                /* Change audio controller type: */
                comAdapter.SetAudioController(guiAttribute.value<KAudioControllerType>());
                if (!comAdapter.isOk())
                {
                    msgCenter().cannotChangeAudioAdapterAttribute(comAdapter);
                    fErrorHappened = true;
                }
                break;
            }
            default:
                break;
        }

        /* Error happened? */
        if (fErrorHappened)
            break;
        /* Save machine settings? */
        if (!fSaveSettings)
            break;

        /* Save machine settings: */
        comMachine.SaveSettings();
        if (!comMachine.isOk())
        {
            msgCenter().cannotSaveMachineSettings(comMachine);
            break;
        }
    }
    while (0);

    /* Close session to editable comMachine if necessary: */
    if (!comSession.isNull())
        comSession.UnlockMachine();
}
