/** @file

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


#include <Library/ArmSvcLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>

#include <Protocol/DebugSupport.h> // for EFI_SYSTEM_CONTEXT

#include <Guid/ZeroGuid.h>
#include <Guid/MmramMemoryReserve.h>

#include <IndustryStandard/ArmStdSmc.h>

#include "StandaloneMmCpu.h"

EFI_STATUS
EFIAPI
MmFoundationEntryRegister (
  IN CONST EFI_MM_CONFIGURATION_PROTOCOL  *This,
  IN EFI_MM_ENTRY_POINT                    MmEntryPoint
  );

//
// On ARM platforms every event is expected to have a GUID associated with
// it. It will be used by the MM Entry point to find the handler for the
// event. It will either be populated in a EFI_MM_COMMUNICATE_HEADER by the
// caller of the event (e.g. MM_COMMUNICATE SMC) or by the CPU driver
// (e.g. during an asynchronous event). In either case, this context is
// maintained in an array which has an entry for each CPU. The pointer to this
// array is held in PerCpuGuidedEventContext. Memory is allocated once the
// number of CPUs in the system are made known through the
// MP_INFORMATION_HOB_DATA.
//
EFI_MM_COMMUNICATE_HEADER **PerCpuGuidedEventContext = NULL;

// Descriptor with whereabouts of memory used for communication with the normal world
EFI_MMRAM_DESCRIPTOR  mNsCommBuffer;

MP_INFORMATION_HOB_DATA *mMpInformationHobData;

EFI_MM_CONFIGURATION_PROTOCOL mMmConfig = {
  0,
  MmFoundationEntryRegister
};

STATIC EFI_MM_ENTRY_POINT     mMmEntryPoint = NULL;

EFI_STATUS
PiMmStandloneArmTfCpuDriverEntry (
  IN UINTN EventId,
  IN UINTN CpuNumber,
  IN UINTN NsCommBufferAddr
  )
{
  EFI_MM_COMMUNICATE_HEADER *GuidedEventContext = NULL;
  EFI_MM_ENTRY_CONTEXT        MmEntryPointContext = {0};
  EFI_STATUS                  Status;
  UINTN                       NsCommBufferSize;

  DEBUG ((DEBUG_INFO, "Received event - 0x%x on cpu %d\n", EventId, CpuNumber));

  Status = EFI_SUCCESS;
  //
  // ARM TF passes SMC FID of the MM_COMMUNICATE interface as the Event ID upon
  // receipt of a synchronous MM request. Use the Event ID to distinguish
  // between synchronous and asynchronous events.
  //
  if (ARM_SMC_ID_MM_COMMUNICATE_AARCH64 != EventId) {
    DEBUG ((DEBUG_INFO, "UnRecognized Event - 0x%x\n", EventId));
    return EFI_INVALID_PARAMETER;
  }

  // Perform parameter validation of NsCommBufferAddr
  if (NsCommBufferAddr && (NsCommBufferAddr < mNsCommBuffer.PhysicalStart))
    return EFI_ACCESS_DENIED;

  if ((NsCommBufferAddr + sizeof (EFI_MM_COMMUNICATE_HEADER)) >=
      (mNsCommBuffer.PhysicalStart + mNsCommBuffer.PhysicalSize))
    return EFI_INVALID_PARAMETER;

  // Find out the size of the buffer passed
  NsCommBufferSize = ((EFI_MM_COMMUNICATE_HEADER *) NsCommBufferAddr)->MessageLength +
    sizeof (EFI_MM_COMMUNICATE_HEADER);

  // perform bounds check.
  if (NsCommBufferAddr + NsCommBufferSize >=
      mNsCommBuffer.PhysicalStart + mNsCommBuffer.PhysicalSize)
    return EFI_ACCESS_DENIED;


  // Now that the secure world can see the normal world buffer, allocate
  // memory to copy the communication buffer to the secure world.
  Status = mMmst->MmAllocatePool (
                    EfiRuntimeServicesData,
                    NsCommBufferSize,
                    (VOID **) &GuidedEventContext
                    );

  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_INFO, "Mem alloc failed - 0x%x\n", EventId));
    return EFI_OUT_OF_RESOURCES;
  }

  // X1 contains the VA of the normal world memory accessible from
  // S-EL0
  CopyMem (GuidedEventContext, (CONST VOID *) NsCommBufferAddr, NsCommBufferSize);

  // Stash the pointer to the allocated Event Context for this CPU
  PerCpuGuidedEventContext[CpuNumber] = GuidedEventContext;

  MmEntryPointContext.CurrentlyExecutingCpu = CpuNumber;
  MmEntryPointContext.NumberOfCpus = mMpInformationHobData->NumberOfProcessors;

  // Populate the MM system table with MP and state information
  mMmst->CurrentlyExecutingCpu = CpuNumber;
  mMmst->NumberOfCpus = mMpInformationHobData->NumberOfProcessors;
  mMmst->CpuSaveStateSize = 0;
  mMmst->CpuSaveState = NULL;

  if (mMmEntryPoint == NULL) {
    DEBUG ((DEBUG_INFO, "Mm Entry point Not Found\n"));
    return EFI_UNSUPPORTED;
  }

  mMmEntryPoint (&MmEntryPointContext);

  // Free the memory allocation done earlier and reset the per-cpu context
  ASSERT (GuidedEventContext);
  CopyMem ((VOID *)NsCommBufferAddr, (CONST VOID *) GuidedEventContext, NsCommBufferSize);

  Status = mMmst->MmFreePool ((VOID *) GuidedEventContext);
  if (Status != EFI_SUCCESS) {
    return EFI_OUT_OF_RESOURCES;
  }
  PerCpuGuidedEventContext[CpuNumber] = NULL;

  return Status;
}

EFI_STATUS
EFIAPI
MmFoundationEntryRegister (
  IN CONST EFI_MM_CONFIGURATION_PROTOCOL  *This,
  IN EFI_MM_ENTRY_POINT                    MmEntryPoint
  )
{
  // store the entry point in a global
  mMmEntryPoint = MmEntryPoint;
  return EFI_SUCCESS;
}

/**
  This function is the main entry point for an MM handler dispatch
  or communicate-based callback.

  @param  DispatchHandle  The unique handle assigned to this handler by MmiHandlerRegister().
  @param  Context         Points to an optional handler context which was specified when the handler was registered.
  @param  CommBuffer      A pointer to a collection of data in memory that will
                          be conveyed from a non-MM environment into an MM environment.
  @param  CommBufferSize  The size of the CommBuffer.

  @return Status Code

**/
EFI_STATUS
EFIAPI
PiMmCpuTpFwRootMmiHandler (
  IN     EFI_HANDLE               DispatchHandle,
  IN     CONST VOID               *Context,        OPTIONAL
  IN OUT VOID                     *CommBuffer,     OPTIONAL
  IN OUT UINTN                    *CommBufferSize  OPTIONAL
  )
{
  EFI_STATUS Status;
  UINTN      CpuNumber;

  ASSERT (Context == NULL);
  ASSERT (CommBuffer == NULL);
  ASSERT (CommBufferSize == NULL);

  CpuNumber = mMmst->CurrentlyExecutingCpu;
  if (!PerCpuGuidedEventContext[CpuNumber])
    return EFI_NOT_FOUND;

  DEBUG ((DEBUG_INFO, "CommBuffer - 0x%x, CommBufferSize - 0x%x\n",
          PerCpuGuidedEventContext[CpuNumber],
          PerCpuGuidedEventContext[CpuNumber]->MessageLength));

  Status = mMmst->MmiManage (
                    &PerCpuGuidedEventContext[CpuNumber]->HeaderGuid,
                    NULL,
                    PerCpuGuidedEventContext[CpuNumber]->Data,
                    &PerCpuGuidedEventContext[CpuNumber]->MessageLength
                    );

  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_WARN, "Unable to manage Guided Event - %d\n", Status));
  }

  return Status;
}
