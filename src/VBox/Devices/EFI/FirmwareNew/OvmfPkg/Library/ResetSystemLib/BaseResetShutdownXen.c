/** @file
  Base Reset System Library Shutdown API implementation for OVMF.

  Copyright (C) 2020, Red Hat, Inc.
  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2022, Citrix Systems, Inc.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>                   // BIT13

#include <Library/BaseLib.h>        // CpuDeadLoop()
#include <Library/DebugLib.h>       // ASSERT()
#include <Library/IoLib.h>          // IoOr16()
#include <Library/PciLib.h>         // PciRead16()
#include <Library/ResetSystemLib.h> // ResetShutdown()
#include <Library/XenHypercallLib.h>
#include <OvmfPlatforms.h>          // OVMF_HOSTBRIDGE_DID

/**
  Calling this function causes the system to enter a power state equivalent
  to the ACPI G2/S5 or G3 states.

  System shutdown should not return, if it returns, it means the system does
  not support shut down reset.
**/
VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  UINT16  AcpiPmBaseAddress;
  UINT16  HostBridgeDevId;

  AcpiPmBaseAddress = 0;
  HostBridgeDevId   = PciRead16 (OVMF_HOSTBRIDGE_DID);
  switch (HostBridgeDevId) {
    case INTEL_82441_DEVICE_ID:
      AcpiPmBaseAddress = PIIX4_PMBA_VALUE;
      break;
    case INTEL_Q35_MCH_DEVICE_ID:
      AcpiPmBaseAddress = ICH9_PMBASE_VALUE;
      break;
    default:
    {
      //
      // Fallback to using hypercall.
      // Necessary for PVH guest, but should work for HVM guest.
      //
      INTN                ReturnCode;
      XEN_SCHED_SHUTDOWN  ShutdownOp = {
        .Reason = XEN_SHED_SHUTDOWN_POWEROFF,
      };
      ReturnCode = XenHypercallSchedOp (XEN_SCHEDOP_SHUTDOWN, ShutdownOp);
      ASSERT (ReturnCode == 0);
      CpuDeadLoop ();
    }
  }

  IoBitFieldWrite16 (AcpiPmBaseAddress + 4, 10, 13, 0);
  IoOr16 (AcpiPmBaseAddress + 4, BIT13);
  CpuDeadLoop ();
}
