/* $Id$ */
/** @file
 * VBox Console COM Class implementation - VM Configuration Bits for ARMv8.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN_CONSOLE
#include "LoggingNew.h"

#include "ConsoleImpl.h"
#include "Global.h"

// generated header
#include "SchemaDefs.h"

#include "AutoCaller.h"

#include <iprt/base64.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>
#if 0 /* enable to play with lots of memory. */
# include <iprt/env.h>
#endif
#include <iprt/stream.h>

#include <VBox/vmm/vmmr3vtable.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/version.h>

#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/* Darwin compile kludge */
#undef PVM

#ifdef VBOX_WITH_VIRT_ARMV8
/**
 * Worker for configConstructor.
 *
 * @return  VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pVM         The cross context VM handle.
 * @param   pVMM        The VMM vtable.
 * @param   pAlock      The automatic lock instance.  This is for when we have
 *                      to leave it in order to avoid deadlocks (ext packs and
 *                      more).
 *
 * @todo This is a big hack at the moment and provides a static VM config to work with, will be adjusted later
 *       on to adhere to the VM config when sorting out the API bits.
 */
int Console::i_configConstructorArmV8(PUVM pUVM, PVM pVM, PCVMMR3VTABLE pVMM, AutoWriteLock *pAlock)
{
    RT_NOREF(pVM /* when everything is disabled */);
    VMMDev         *pVMMDev   = m_pVMMDev; Assert(pVMMDev);
    ComPtr<IMachine> pMachine = i_machine();

    int             vrc;
    HRESULT         hrc;
    Utf8Str         strTmp;
    Bstr            bstr;

#define H()         AssertLogRelMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR)

    /*
     * Get necessary objects and frequently used parameters.
     */
    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                             H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                                   H();

    ComPtr<ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());           H();

    ComPtr<IBIOSSettings> biosSettings;
    hrc = pMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());                     H();

    ComPtr<INvramStore> nvramStore;
    hrc = pMachine->COMGETTER(NonVolatileStore)(nvramStore.asOutParam());                   H();

    hrc = pMachine->COMGETTER(HardwareUUID)(bstr.asOutParam());                             H();
    RTUUID HardwareUuid;
    vrc = RTUuidFromUtf16(&HardwareUuid, bstr.raw());
    AssertRCReturn(vrc, vrc);

    ULONG cRamMBs;
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                        H();
    uint64_t const cbRam   = cRamMBs * (uint64_t)_1M;

    ULONG cCpus = 1;
    hrc = pMachine->COMGETTER(CPUCount)(&cCpus);                                            H();

    ULONG ulCpuExecutionCap = 100;
    hrc = pMachine->COMGETTER(CPUExecutionCap)(&ulCpuExecutionCap);                         H();

    Bstr osTypeId;
    hrc = pMachine->COMGETTER(OSTypeId)(osTypeId.asOutParam());                             H();
    LogRel(("Guest OS type: '%s'\n", Utf8Str(osTypeId).c_str()));

    /*
     * Get root node first.
     * This is the only node in the tree.
     */
    PCFGMNODE pRoot = pVMM->pfnCFGMR3GetRootU(pUVM);
    Assert(pRoot);

    // catching throws from InsertConfigString and friends.
    try
    {

        /*
         * Set the root (and VMM) level values.
         */
        hrc = pMachine->COMGETTER(Name)(bstr.asOutParam());                                 H();
        InsertConfigString(pRoot,  "Name",                 bstr);
        InsertConfigBytes(pRoot,   "UUID", &HardwareUuid, sizeof(HardwareUuid));
        InsertConfigInteger(pRoot, "NumCPUs",              cCpus);
        InsertConfigInteger(pRoot, "CpuExecutionCap",      ulCpuExecutionCap);
        InsertConfigInteger(pRoot, "TimerMillies",         10);

        /*
         * NEM
         */
        PCFGMNODE pNEM;
        InsertConfigNode(pRoot, "NEM", &pNEM);

        /*
         * MM values.
         */
        PCFGMNODE pMM;
        InsertConfigNode(pRoot, "MM", &pMM);

        /*
         * Memory setup.
         */
        PCFGMNODE pMem = NULL;
        InsertConfigNode(pMM, "MemRegions", &pMem);

        PCFGMNODE pMemRegion = NULL;
        InsertConfigNode(pMem, "Conventional", &pMemRegion);
        InsertConfigInteger(pMemRegion, "GCPhysStart", 0x40000000);
        InsertConfigInteger(pMemRegion, "Size", cbRam);

        /*
         * PDM config.
         *  Load drivers in VBoxC.[so|dll]
         */
        PCFGMNODE pPDM;
        PCFGMNODE pNode;
        PCFGMNODE pMod;
        InsertConfigNode(pRoot,    "PDM", &pPDM);
        InsertConfigNode(pPDM,     "Devices", &pNode);
        InsertConfigNode(pPDM,     "Drivers", &pNode);
        InsertConfigNode(pNode,    "VBoxC", &pMod);
#ifdef VBOX_WITH_XPCOM
        // VBoxC is located in the components subdirectory
        char szPathVBoxC[RTPATH_MAX];
        vrc = RTPathAppPrivateArch(szPathVBoxC, RTPATH_MAX - sizeof("/components/VBoxC"));  AssertRC(vrc);
        strcat(szPathVBoxC, "/components/VBoxC");
        InsertConfigString(pMod,   "Path",  szPathVBoxC);
#else
        InsertConfigString(pMod,   "Path",  "VBoxC");
#endif


        /*
         * Block cache settings.
         */
        PCFGMNODE pPDMBlkCache;
        InsertConfigNode(pPDM, "BlkCache", &pPDMBlkCache);

        /* I/O cache size */
        ULONG ioCacheSize = 5;
        hrc = pMachine->COMGETTER(IOCacheSize)(&ioCacheSize);                               H();
        InsertConfigInteger(pPDMBlkCache, "CacheSize", ioCacheSize * _1M);

        /*
         * Bandwidth groups.
         */
        ComPtr<IBandwidthControl> bwCtrl;

        hrc = pMachine->COMGETTER(BandwidthControl)(bwCtrl.asOutParam());                   H();

        com::SafeIfaceArray<IBandwidthGroup> bwGroups;
        hrc = bwCtrl->GetAllBandwidthGroups(ComSafeArrayAsOutParam(bwGroups));              H();

        PCFGMNODE pAc;
        InsertConfigNode(pPDM, "AsyncCompletion", &pAc);
        PCFGMNODE pAcFile;
        InsertConfigNode(pAc,  "File", &pAcFile);
        PCFGMNODE pAcFileBwGroups;
        InsertConfigNode(pAcFile,  "BwGroups", &pAcFileBwGroups);
#ifdef VBOX_WITH_NETSHAPER
        PCFGMNODE pNetworkShaper;
        InsertConfigNode(pPDM, "NetworkShaper",  &pNetworkShaper);
        PCFGMNODE pNetworkBwGroups;
        InsertConfigNode(pNetworkShaper, "BwGroups", &pNetworkBwGroups);
#endif /* VBOX_WITH_NETSHAPER */

        for (size_t i = 0; i < bwGroups.size(); i++)
        {
            Bstr strName;
            hrc = bwGroups[i]->COMGETTER(Name)(strName.asOutParam());                       H();
            if (strName.isEmpty())
                return pVMM->pfnVMR3SetError(pUVM, VERR_CFGM_NO_NODE, RT_SRC_POS, N_("No bandwidth group name specified"));

            BandwidthGroupType_T enmType = BandwidthGroupType_Null;
            hrc = bwGroups[i]->COMGETTER(Type)(&enmType);                                   H();
            LONG64 cMaxBytesPerSec = 0;
            hrc = bwGroups[i]->COMGETTER(MaxBytesPerSec)(&cMaxBytesPerSec);                 H();

            if (enmType == BandwidthGroupType_Disk)
            {
                PCFGMNODE pBwGroup;
                InsertConfigNode(pAcFileBwGroups, Utf8Str(strName).c_str(), &pBwGroup);
                InsertConfigInteger(pBwGroup, "Max", cMaxBytesPerSec);
                InsertConfigInteger(pBwGroup, "Start", cMaxBytesPerSec);
                InsertConfigInteger(pBwGroup, "Step", 0);
            }
#ifdef VBOX_WITH_NETSHAPER
            else if (enmType == BandwidthGroupType_Network)
            {
                /* Network bandwidth groups. */
                PCFGMNODE pBwGroup;
                InsertConfigNode(pNetworkBwGroups, Utf8Str(strName).c_str(), &pBwGroup);
                InsertConfigInteger(pBwGroup, "Max", cMaxBytesPerSec);
            }
#endif /* VBOX_WITH_NETSHAPER */
        }

        /*
         * Devices
         */
        PCFGMNODE pDevices = NULL;      /* /Devices */
        PCFGMNODE pDev = NULL;          /* /Devices/Dev/ */
        PCFGMNODE pInst = NULL;         /* /Devices/Dev/0/ */
        PCFGMNODE pCfg = NULL;          /* /Devices/Dev/.../Config/ */
        PCFGMNODE pLunL0 = NULL;        /* /Devices/Dev/0/LUN#0/ */
        PCFGMNODE pLunL1 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/ */
        PCFGMNODE pLunL1Cfg = NULL;     /* /Devices/Dev/0/LUN#0/AttachedDriver/Config */

        InsertConfigNode(pRoot, "Devices", &pDevices);

        InsertConfigNode(pDevices, "efi-armv8",             &pDev);
        InsertConfigNode(pDev,     "0",                     &pInst);
        InsertConfigNode(pInst,    "Config",                &pCfg);
        InsertConfigInteger(pCfg,  "GCPhysLoadAddress",     0);
        InsertConfigString(pCfg,   "EfiRom",                "VBoxEFIAArch64.fd");

        InsertConfigNode(pDevices, "gic",                   &pDev);
        InsertConfigNode(pDev,     "0",                     &pInst);
        InsertConfigInteger(pInst, "Trusted",               1);
        InsertConfigNode(pInst,    "Config",                &pCfg);
        InsertConfigInteger(pCfg,  "DistributorMmioBase",   0x08000000);
        InsertConfigInteger(pCfg,  "RedistributorMmioBase", 0x080a0000);


        InsertConfigNode(pDevices, "qemu-fw-cfg",   &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "MmioSize",       4096);
        InsertConfigInteger(pCfg,  "MmioBase", 0x09020000);
        InsertConfigInteger(pCfg,  "DmaEnabled",        1);
        InsertConfigInteger(pCfg,  "QemuRamfbSupport",  1);
        InsertConfigNode(pInst,    "LUN#0",           &pLunL0);
        InsertConfigString(pLunL0, "Driver",          "MainDisplay");

        InsertConfigNode(pDevices, "flash-cfi",         &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "BaseAddress",  64 * _1M);
        InsertConfigInteger(pCfg,  "Size",        768 * _1K);
        InsertConfigString(pCfg,   "FlashFile",   "nvram");
        /* Attach the NVRAM storage driver. */
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver",      "NvramStore");

        InsertConfigNode(pDevices, "arm-pl011",     &pDev);
        for (ULONG ulInstance = 0; ulInstance < 1 /** @todo SchemaDefs::SerialPortCount*/; ++ulInstance)
        {
            ComPtr<ISerialPort> serialPort;
            hrc = pMachine->GetSerialPort(ulInstance, serialPort.asOutParam());             H();
            BOOL fEnabledSerPort = FALSE;
            if (serialPort)
            {
                hrc = serialPort->COMGETTER(Enabled)(&fEnabledSerPort);                     H();
            }
            if (!fEnabledSerPort)
            {
                m_aeSerialPortMode[ulInstance] = PortMode_Disconnected;
                continue;
            }

            InsertConfigNode(pDev, Utf8StrFmt("%u", ulInstance).c_str(), &pInst);
            InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
            InsertConfigNode(pInst, "Config", &pCfg);

            InsertConfigInteger(pCfg,  "Irq",               1);
            InsertConfigInteger(pCfg,  "MmioBase", 0x09000000);

            BOOL  fServer;
            hrc = serialPort->COMGETTER(Server)(&fServer);                                  H();
            hrc = serialPort->COMGETTER(Path)(bstr.asOutParam());                           H();

            PortMode_T eHostMode;
            hrc = serialPort->COMGETTER(HostMode)(&eHostMode);                              H();

            m_aeSerialPortMode[ulInstance] = eHostMode;
            if (eHostMode != PortMode_Disconnected)
            {
                vrc = i_configSerialPort(pInst, eHostMode, Utf8Str(bstr).c_str(), RT_BOOL(fServer));
                if (RT_FAILURE(vrc))
                    return vrc;
            }
        }

        InsertConfigNode(pDevices, "arm-pl031-rtc", &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "Irq",               2);
        InsertConfigInteger(pCfg,  "MmioBase", 0x09010000);

        InsertConfigNode(pDevices, "arm-pl061-gpio",&pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "Irq",               7);
        InsertConfigInteger(pCfg,  "MmioBase", 0x09030000);

        InsertConfigNode(pDevices, "pci-generic-ecam",  &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "MmioEcamBase",   0x3f000000);
        InsertConfigInteger(pCfg,  "MmioEcamLength", 0x01000000);
        InsertConfigInteger(pCfg,  "MmioPioBase",    0x3eff0000);
        InsertConfigInteger(pCfg,  "MmioPioSize",    0x0000ffff);
        InsertConfigInteger(pCfg,  "IntPinA",        3);
        InsertConfigInteger(pCfg,  "IntPinB",        4);
        InsertConfigInteger(pCfg,  "IntPinC",        5);
        InsertConfigInteger(pCfg,  "IntPinD",        6);

        InsertConfigNode(pDevices, "usb-xhci",      &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigInteger(pInst, "Trusted",           1);
        InsertConfigInteger(pInst, "PCIBusNo",          0);
        InsertConfigInteger(pInst, "PCIDeviceNo",       2);
        InsertConfigInteger(pInst, "PCIFunctionNo",     0);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver","VUSBRootHub");
        InsertConfigNode(pInst,    "LUN#1",       &pLunL0);
        InsertConfigString(pLunL0, "Driver","VUSBRootHub");

        InsertConfigNode(pDevices, "e1000",         &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigInteger(pInst, "Trusted",           1);
        InsertConfigInteger(pInst, "PCIBusNo",          0);
        InsertConfigInteger(pInst, "PCIDeviceNo",       1);
        InsertConfigInteger(pInst, "PCIFunctionNo",     0);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "CableConnected",    1);
        InsertConfigInteger(pCfg,  "LineSpeed",         0);
        InsertConfigInteger(pCfg,  "AdapterType",       0);

        const char *pszMac = "080027ede92c";
        Assert(strlen(pszMac) == 12);
        RTMAC Mac;
        RT_ZERO(Mac);
        char *pMac = (char*)&Mac;
        for (uint32_t i = 0; i < 6; ++i)
        {
            int c1 = *pszMac++ - '0';
            if (c1 > 9)
                c1 -= 7;
            int c2 = *pszMac++ - '0';
            if (c2 > 9)
                c2 -= 7;
            *pMac++ = (char)(((c1 & 0x0f) << 4) | (c2 & 0x0f));
        }
        InsertConfigBytes(pCfg,    "MAC",   &Mac, sizeof(Mac));
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver",        "NAT");
        InsertConfigNode(pLunL0,    "Config",  &pLunL1Cfg);
        InsertConfigString(pLunL1Cfg, "Network", "10.0.2.0/24");
        InsertConfigString(pLunL1Cfg, "TFTPPrefix", "/Users/vbox/Library/VirtualBox/TFTP");
        InsertConfigString(pLunL1Cfg, "BootFile", "default.pxe");
        InsertConfigInteger(pLunL1Cfg,  "AliasMode",         0);
        InsertConfigInteger(pLunL1Cfg,  "DNSProxy",          0);
        InsertConfigInteger(pLunL1Cfg,  "LocalhostReachable", 1);
        InsertConfigInteger(pLunL1Cfg,  "PassDomain",         1);
        InsertConfigInteger(pLunL1Cfg,  "UseHostResolver",    0);

        PCFGMNODE pUsb = NULL;
        InsertConfigNode(pRoot,    "USB",           &pUsb);

        /*
         * Storage controllers.
         */
        com::SafeIfaceArray<IStorageController> ctrls;
        PCFGMNODE aCtrlNodes[StorageControllerType_VirtioSCSI + 1] = {};
        hrc = pMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));       H();

        for (size_t i = 0; i < ctrls.size(); ++i)
        {
            DeviceType_T *paLedDevType = NULL;

            StorageControllerType_T enmCtrlType;
            hrc = ctrls[i]->COMGETTER(ControllerType)(&enmCtrlType);                        H();
            AssertRelease((unsigned)enmCtrlType < RT_ELEMENTS(aCtrlNodes)
                          || enmCtrlType == StorageControllerType_USB);

            StorageBus_T enmBus;
            hrc = ctrls[i]->COMGETTER(Bus)(&enmBus);                                        H();

            Bstr controllerName;
            hrc = ctrls[i]->COMGETTER(Name)(controllerName.asOutParam());                   H();

            ULONG ulInstance = 999;
            hrc = ctrls[i]->COMGETTER(Instance)(&ulInstance);                               H();

            BOOL fUseHostIOCache;
            hrc = ctrls[i]->COMGETTER(UseHostIOCache)(&fUseHostIOCache);                    H();

            BOOL fBootable;
            hrc = ctrls[i]->COMGETTER(Bootable)(&fBootable);                                H();

            PCFGMNODE pCtlInst = NULL;
            const char *pszCtrlDev = i_storageControllerTypeToStr(enmCtrlType);
            if (enmCtrlType != StorageControllerType_USB)
            {
                /* /Devices/<ctrldev>/ */
                pDev = aCtrlNodes[enmCtrlType];
                if (!pDev)
                {
                    InsertConfigNode(pDevices, pszCtrlDev, &pDev);
                    aCtrlNodes[enmCtrlType] = pDev; /* IDE variants are handled in the switch */
                }

                /* /Devices/<ctrldev>/<instance>/ */
                InsertConfigNode(pDev, Utf8StrFmt("%u", ulInstance).c_str(), &pCtlInst);

                /* Device config: /Devices/<ctrldev>/<instance>/<values> & /ditto/Config/<values> */
                InsertConfigInteger(pCtlInst, "Trusted",   1);
                InsertConfigNode(pCtlInst,    "Config",    &pCfg);
            }

            switch (enmCtrlType)
            {
                case StorageControllerType_USB:
                {
                    if (pUsb)
                    {
                        /*
                         * USB MSDs are handled a bit different as the device instance
                         * doesn't match the storage controller instance but the port.
                         */
                        InsertConfigNode(pUsb, "Msd", &pDev);
                        pCtlInst = pDev;
                    }
                    else
                        return pVMM->pfnVMR3SetError(pUVM, VERR_NOT_FOUND, RT_SRC_POS,
                                N_("There is no USB controller enabled but there\n"
                                   "is at least one USB storage device configured for this VM.\n"
                                   "To fix this problem either enable the USB controller or remove\n"
                                   "the storage device from the VM"));
                    break;
                }

                case StorageControllerType_IntelAhci:
                {
                    InsertConfigInteger(pCtlInst, "PCIBusNo",          0);
                    InsertConfigInteger(pCtlInst, "PCIDeviceNo",       3);
                    InsertConfigInteger(pCtlInst, "PCIFunctionNo",     0);

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "PortCount", cPorts);
                    InsertConfigInteger(pCfg, "Bootable",  fBootable);

                    com::SafeIfaceArray<IMediumAttachment> atts;
                    hrc = pMachine->GetMediumAttachmentsOfController(controllerName.raw(),
                                                                     ComSafeArrayAsOutParam(atts));  H();

                    /* Configure the hotpluggable flag for the port. */
                    for (unsigned idxAtt = 0; idxAtt < atts.size(); ++idxAtt)
                    {
                        IMediumAttachment *pMediumAtt = atts[idxAtt];

                        LONG lPortNum = 0;
                        hrc = pMediumAtt->COMGETTER(Port)(&lPortNum);                       H();

                        BOOL fHotPluggable = FALSE;
                        hrc = pMediumAtt->COMGETTER(HotPluggable)(&fHotPluggable);          H();
                        if (SUCCEEDED(hrc))
                        {
                            PCFGMNODE pPortCfg;
                            char szName[24];
                            RTStrPrintf(szName, sizeof(szName), "Port%d", lPortNum);

                            InsertConfigNode(pCfg, szName, &pPortCfg);
                            InsertConfigInteger(pPortCfg, "Hotpluggable", fHotPluggable ? 1 : 0);
                        }
                    }
                    break;
                }
                case StorageControllerType_VirtioSCSI:
                {
                    InsertConfigInteger(pCtlInst, "PCIBusNo",          0);
                    InsertConfigInteger(pCtlInst, "PCIDeviceNo",       3);
                    InsertConfigInteger(pCtlInst, "PCIFunctionNo",     0);

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "NumTargets", cPorts);
                    InsertConfigInteger(pCfg, "Bootable",   fBootable);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD) /*?*/,
                                         cPorts, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_LsiLogic:
                case StorageControllerType_BusLogic:
                case StorageControllerType_PIIX3:
                case StorageControllerType_PIIX4:
                case StorageControllerType_ICH6:
                case StorageControllerType_I82078:
                case StorageControllerType_LsiLogicSas:
                case StorageControllerType_NVMe:

                default:
                    AssertLogRelMsgFailedReturn(("invalid storage controller type: %d\n", enmCtrlType), VERR_MAIN_CONFIG_CONSTRUCTOR_IPE);
            }

            /* Attach the media to the storage controllers. */
            com::SafeIfaceArray<IMediumAttachment> atts;
            hrc = pMachine->GetMediumAttachmentsOfController(controllerName.raw(),
                                                            ComSafeArrayAsOutParam(atts));  H();

            /* Builtin I/O cache - per device setting. */
            BOOL fBuiltinIOCache = true;
            hrc = pMachine->COMGETTER(IOCacheEnabled)(&fBuiltinIOCache);                    H();

            bool fInsertDiskIntegrityDrv = false;
            Bstr strDiskIntegrityFlag;
            hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EnableDiskIntegrityDriver").raw(),
                                         strDiskIntegrityFlag.asOutParam());
            if (   hrc   == S_OK
                && strDiskIntegrityFlag == "1")
                fInsertDiskIntegrityDrv = true;

            for (size_t j = 0; j < atts.size(); ++j)
            {
                IMediumAttachment *pMediumAtt = atts[j];
                vrc = i_configMediumAttachment(pszCtrlDev,
                                               ulInstance,
                                               enmBus,
                                               !!fUseHostIOCache,
                                               enmCtrlType == StorageControllerType_NVMe ? false : !!fBuiltinIOCache,
                                               fInsertDiskIntegrityDrv,
                                               false /* fSetupMerge */,
                                               0 /* uMergeSource */,
                                               0 /* uMergeTarget */,
                                               pMediumAtt,
                                               mMachineState,
                                               NULL /* phrc */,
                                               false /* fAttachDetach */,
                                               false /* fForceUnmount */,
                                               false /* fHotplug */,
                                               pUVM,
                                               pVMM,
                                               paLedDevType,
                                               NULL /* ppLunL0 */);
                if (RT_FAILURE(vrc))
                    return vrc;
            }
            H();
        }
        H();

        InsertConfigNode(pUsb,     "HidKeyboard",   &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigInteger(pInst, "Trusted",           1);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver",       "KeyboardQueue");
        InsertConfigNode(pLunL0,   "AttachedDriver",  &pLunL1);
        InsertConfigString(pLunL1, "Driver",          "MainKeyboard");

        InsertConfigNode(pUsb,     "HidMouse", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigNode(pInst,    "Config", &pCfg);
        InsertConfigString(pCfg,   "Mode", "absolute");
        InsertConfigNode(pInst,    "LUN#0", &pLunL0);
        InsertConfigString(pLunL0, "Driver",        "MouseQueue");
        InsertConfigNode(pLunL0,   "Config", &pCfg);
        InsertConfigInteger(pCfg,  "QueueSize",            128);

        InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
        InsertConfigString(pLunL1, "Driver",        "MainMouse");
    }
    catch (ConfigError &x)
    {
        // InsertConfig threw something:
        pVMM->pfnVMR3SetError(pUVM, x.m_vrc, RT_SRC_POS, "Caught ConfigError: %Rrc - %s", x.m_vrc, x.what());
        return x.m_vrc;
    }
    catch (HRESULT hrcXcpt)
    {
        AssertLogRelMsgFailedReturn(("hrc=%Rhrc\n", hrcXcpt), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR);
    }

#ifdef VBOX_WITH_EXTPACK
    /*
     * Call the extension pack hooks if everything went well thus far.
     */
    if (RT_SUCCESS(vrc))
    {
        pAlock->release();
        vrc = mptrExtPackManager->i_callAllVmConfigureVmmHooks(this, pVM, pVMM);
        pAlock->acquire();
    }
#endif

    /*
     * Apply the CFGM overlay.
     */
    if (RT_SUCCESS(vrc))
        vrc = i_configCfgmOverlay(pRoot, virtualBox, pMachine);

    /*
     * Dump all extradata API settings tweaks, both global and per VM.
     */
    if (RT_SUCCESS(vrc))
        vrc = i_configDumpAPISettingsTweaks(virtualBox, pMachine);

#undef H

    pAlock->release(); /* Avoid triggering the lock order inversion check. */

    /*
     * Register VM state change handler.
     */
    int vrc2 = pVMM->pfnVMR3AtStateRegister(pUVM, Console::i_vmstateChangeCallback, this);
    AssertRC(vrc2);
    if (RT_SUCCESS(vrc))
        vrc = vrc2;

    /*
     * Register VM runtime error handler.
     */
    vrc2 = pVMM->pfnVMR3AtRuntimeErrorRegister(pUVM, Console::i_atVMRuntimeErrorCallback, this);
    AssertRC(vrc2);
    if (RT_SUCCESS(vrc))
        vrc = vrc2;

    pAlock->acquire();

    LogFlowFunc(("vrc = %Rrc\n", vrc));
    LogFlowFuncLeave();

    return vrc;
}
#endif /* !VBOX_WITH_VIRT_ARMV8 */

