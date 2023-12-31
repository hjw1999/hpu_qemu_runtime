/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Copyright (c) 2016 HP Development Company, L.P.
  Copyright (c) 2016 - 2018, ARM Limited. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Pi/PiMmCis.h>
#include <Library/AArch64/StandaloneMmCoreEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>

#include <Protocol/DebugSupport.h> // for EFI_SYSTEM_CONTEXT

#include <Guid/ZeroGuid.h>
#include <Guid/MmramMemoryReserve.h>


#include "StandaloneMmCpu.h"

// GUID to identify HOB with whereabouts of communication buffer with Normal
// World
extern EFI_GUID gEfiStandaloneMmNonSecureBufferGuid;

// GUID to identify HOB where the entry point of this CPU driver will be
// populated to allow the entry point driver to invoke it upon receipt of an
// event
extern EFI_GUID gEfiArmTfCpuDriverEpDescriptorGuid;

//
// Private copy of the MM system table for future use
//
EFI_MM_SYSTEM_TABLE *mMmst = NULL;

//
// Globals used to initialize the protocol
//
STATIC EFI_HANDLE            mMmCpuHandle = NULL;

EFI_STATUS
GetGuidedHobData (
  IN  VOID *HobList,
  IN  CONST EFI_GUID *HobGuid,
  OUT VOID **HobData
  )
{
  EFI_HOB_GUID_TYPE *Hob;

  if (!HobList || !HobGuid || !HobData)
    return EFI_INVALID_PARAMETER;

  Hob = GetNextGuidHob (HobGuid, HobList);
  if (!Hob)
    return EFI_NOT_FOUND;

  *HobData = GET_GUID_HOB_DATA (Hob);
  if (!HobData)
    return EFI_NOT_FOUND;

  return EFI_SUCCESS;
}

EFI_STATUS
StandloneMmCpuInitialize (
  IN EFI_HANDLE         ImageHandle,  // not actual imagehandle
  IN EFI_MM_SYSTEM_TABLE   *SystemTable  // not actual systemtable
  )
{
  ARM_TF_CPU_DRIVER_EP_DESCRIPTOR *CpuDriverEntryPointDesc;
  EFI_CONFIGURATION_TABLE         *ConfigurationTable;
  MP_INFORMATION_HOB_DATA         *MpInformationHobData;
  EFI_MMRAM_DESCRIPTOR            *NsCommBufMmramRange;
  EFI_STATUS                       Status;
  EFI_HANDLE                       DispatchHandle;
  UINT32                           MpInfoSize;
  UINTN                            Index;
  UINTN                            ArraySize;
  VOID                            *HobStart;

  ASSERT (SystemTable != NULL);
  mMmst = SystemTable;

  // publish the MM config protocol so the MM core can register its entry point
  Status = mMmst->MmInstallProtocolInterface (
                    &mMmCpuHandle,
                    &gEfiMmConfigurationProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &mMmConfig
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // register the root MMI handler
  Status = mMmst->MmiHandlerRegister (
                    PiMmCpuTpFwRootMmiHandler,
                    NULL,
                    &DispatchHandle
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Retrieve the Hoblist from the MMST to extract the details of the NS
  // communication buffer that has been reserved by S-EL1/EL3
  ConfigurationTable = mMmst->MmConfigurationTable;
  for (Index = 0; Index < mMmst->NumberOfTableEntries; Index++) {
    if (CompareGuid (&gEfiHobListGuid, &(ConfigurationTable[Index].VendorGuid))) {
      break;
    }
  }

  // Bail out if the Hoblist could not be found
  if (Index >= mMmst->NumberOfTableEntries) {
    DEBUG ((DEBUG_INFO, "Hoblist not found - 0x%x\n", Index));
    return EFI_OUT_OF_RESOURCES;
  }

  HobStart = ConfigurationTable[Index].VendorTable;

  //
  // Locate the HOB with the buffer to populate the entry point of this driver
  //
  Status = GetGuidedHobData (
             HobStart,
             &gEfiArmTfCpuDriverEpDescriptorGuid,
             (VOID **) &CpuDriverEntryPointDesc
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "ArmTfCpuDriverEpDesc HOB data extraction failed - 0x%x\n", Status));
    return Status;
  }

  // Share the entry point of the CPU driver
  DEBUG ((DEBUG_INFO, "Sharing Cpu Driver EP *0x%lx = 0x%lx\n",
          (UINT64) CpuDriverEntryPointDesc->ArmTfCpuDriverEpPtr,
          (UINT64) PiMmStandloneArmTfCpuDriverEntry));
  *(CpuDriverEntryPointDesc->ArmTfCpuDriverEpPtr) = PiMmStandloneArmTfCpuDriverEntry;

  // Find the descriptor that contains the whereabouts of the buffer for
  // communication with the Normal world.
  Status = GetGuidedHobData (
             HobStart,
             &gEfiStandaloneMmNonSecureBufferGuid,
             (VOID **) &NsCommBufMmramRange
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "NsCommBufMmramRange HOB data extraction failed - 0x%x\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "mNsCommBuffer.PhysicalStart - 0x%lx\n", (UINT64) NsCommBufMmramRange->PhysicalStart));
  DEBUG ((DEBUG_INFO, "mNsCommBuffer.PhysicalSize - 0x%lx\n", (UINT64) NsCommBufMmramRange->PhysicalSize));

  CopyMem (&mNsCommBuffer, NsCommBufMmramRange, sizeof(EFI_MMRAM_DESCRIPTOR));
  DEBUG ((DEBUG_INFO, "mNsCommBuffer: 0x%016lx - 0x%lx\n", mNsCommBuffer.CpuStart, mNsCommBuffer.PhysicalSize));

  //
  // Extract the MP information from the Hoblist
  //
  Status = GetGuidedHobData (
             HobStart,
             &gMpInformationHobGuid,
             (VOID **) &MpInformationHobData
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "MpInformationHob extraction failed - 0x%x\n", Status));
    return Status;
  }

  //
  // Allocate memory for the MP information and copy over the MP information
  // passed by Trusted Firmware. Use the number of processors passed in the HOB
  // to copy the processor information
  //
  MpInfoSize = sizeof (MP_INFORMATION_HOB_DATA) +
               (sizeof (EFI_PROCESSOR_INFORMATION) *
               MpInformationHobData->NumberOfProcessors);
  Status = mMmst->MmAllocatePool (
                    EfiRuntimeServicesData,
                    MpInfoSize,
                    (VOID **) &mMpInformationHobData
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "mMpInformationHobData mem alloc failed - 0x%x\n", Status));
    return Status;
  }

  CopyMem (mMpInformationHobData, MpInformationHobData, MpInfoSize);

  // Print MP information
  DEBUG ((DEBUG_INFO, "mMpInformationHobData: 0x%016lx - 0x%lx\n",
          mMpInformationHobData->NumberOfProcessors,
          mMpInformationHobData->NumberOfEnabledProcessors));
  for (Index = 0; Index < mMpInformationHobData->NumberOfProcessors; Index++) {
    DEBUG ((DEBUG_INFO, "mMpInformationHobData[0x%lx]: %d, %d, %d\n",
            mMpInformationHobData->ProcessorInfoBuffer[Index].ProcessorId,
            mMpInformationHobData->ProcessorInfoBuffer[Index].Location.Package,
            mMpInformationHobData->ProcessorInfoBuffer[Index].Location.Core,
            mMpInformationHobData->ProcessorInfoBuffer[Index].Location.Thread));
  }

  //
  // Allocate memory for a table to hold pointers to a
  // EFI_MM_COMMUNICATE_HEADER for each CPU
  //
  ArraySize = sizeof (EFI_MM_COMMUNICATE_HEADER *) *
              mMpInformationHobData->NumberOfEnabledProcessors;
  Status = mMmst->MmAllocatePool (
                    EfiRuntimeServicesData,
                    ArraySize,
                    (VOID **) &PerCpuGuidedEventContext
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "PerCpuGuidedEventContext mem alloc failed - 0x%x\n", Status));
    return Status;
  }
  return Status;
}
