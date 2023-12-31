/** @file
  MM Driver Dispatcher.

  Step #1 - When a FV protocol is added to the system every driver in the FV
            is added to the mDiscoveredList. The Before, and After Depex are
            pre-processed as drivers are added to the mDiscoveredList. If an Apriori
            file exists in the FV those drivers are addeded to the
            mScheduledQueue. The mFvHandleList is used to make sure a
            FV is only processed once.

  Step #2 - Dispatch. Remove driver from the mScheduledQueue and load and
            start it. After mScheduledQueue is drained check the
            mDiscoveredList to see if any item has a Depex that is ready to
            be placed on the mScheduledQueue.

  Step #3 - Adding to the mScheduledQueue requires that you process Before
            and After dependencies. This is done recursively as the call to add
            to the mScheduledQueue checks for Before and recursively adds
            all Befores. It then addes the item that was passed in and then
            processess the After dependecies by recursively calling the routine.

  Dispatcher Rules:
  The rules for the dispatcher are similar to the DXE dispatcher.

  The rules for DXE dispatcher are in chapter 10 of the DXE CIS. Figure 10-3
  is the state diagram for the DXE dispatcher

  Depex - Dependency Expresion.

  Copyright (c) 2014, Hewlett-Packard Development Company, L.P.
  Copyright (c) 2009 - 2014, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2016 - 2018, ARM Limited. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "StandaloneMmCore.h"

//
// MM Dispatcher Data structures
//
#define KNOWN_HANDLE_SIGNATURE  SIGNATURE_32('k','n','o','w')

typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;         // mFvHandleList
  EFI_HANDLE      Handle;
} KNOWN_HANDLE;

//
// Function Prototypes
//

EFI_STATUS
MmCoreFfsFindMmDriver (
  IN  EFI_FIRMWARE_VOLUME_HEADER  *FwVolHeader
  );

/**
  Insert InsertedDriverEntry onto the mScheduledQueue. To do this you
  must add any driver with a before dependency on InsertedDriverEntry first.
  You do this by recursively calling this routine. After all the Befores are
  processed you can add InsertedDriverEntry to the mScheduledQueue.
  Then you can add any driver with an After dependency on InsertedDriverEntry
  by recursively calling this routine.

  @param  InsertedDriverEntry   The driver to insert on the ScheduledLink Queue

**/
VOID
MmInsertOnScheduledQueueWhileProcessingBeforeAndAfter (
  IN  EFI_MM_DRIVER_ENTRY   *InsertedDriverEntry
  );

//
// The Driver List contains one copy of every driver that has been discovered.
// Items are never removed from the driver list. List of EFI_MM_DRIVER_ENTRY
//
LIST_ENTRY  mDiscoveredList = INITIALIZE_LIST_HEAD_VARIABLE (mDiscoveredList);

//
// Queue of drivers that are ready to dispatch. This queue is a subset of the
// mDiscoveredList.list of EFI_MM_DRIVER_ENTRY.
//
LIST_ENTRY  mScheduledQueue = INITIALIZE_LIST_HEAD_VARIABLE (mScheduledQueue);

//
// List of handles who's Fv's have been parsed and added to the mFwDriverList.
//
LIST_ENTRY  mFvHandleList = INITIALIZE_LIST_HEAD_VARIABLE (mFvHandleList);

//
// Flag for the MM Dispacher.  TRUE if dispatcher is execuing.
//
BOOLEAN  gDispatcherRunning = FALSE;

//
// Flag for the MM Dispacher.  TRUE if there is one or more MM drivers ready to be dispatched
//
BOOLEAN  gRequestDispatch = FALSE;

//
// The global variable is defined for Loading modules at fixed address feature to track the MM code
// memory range usage. It is a bit mapped array in which every bit indicates the correspoding
// memory page available or not.
//
GLOBAL_REMOVE_IF_UNREFERENCED    UINT64                *mMmCodeMemoryRangeUsageBitMap=NULL;

/**
  To check memory usage bit map array to figure out if the memory range in which the image will be loaded
  is available or not. If memory range is avaliable, the function will mark the correponding bits to 1
  which indicates the memory range is used. The function is only invoked when load modules at fixed address
  feature is enabled.

  @param  ImageBase                The base addres the image will be loaded at.
  @param  ImageSize                The size of the image

  @retval EFI_SUCCESS              The memory range the image will be loaded in is available
  @retval EFI_NOT_FOUND            The memory range the image will be loaded in is not available
**/
EFI_STATUS
CheckAndMarkFixLoadingMemoryUsageBitMap (
  IN  EFI_PHYSICAL_ADDRESS          ImageBase,
  IN  UINTN                         ImageSize
  )
{
  UINT32                             MmCodePageNumber;
  UINT64                             MmCodeSize;
  EFI_PHYSICAL_ADDRESS               MmCodeBase;
  UINTN                              BaseOffsetPageNumber;
  UINTN                              TopOffsetPageNumber;
  UINTN                              Index;

  //
  // Build tool will calculate the smm code size and then patch the PcdLoadFixAddressMmCodePageNumber
  //
  MmCodePageNumber = 0;
  MmCodeSize = EFI_PAGES_TO_SIZE (MmCodePageNumber);
  MmCodeBase = gLoadModuleAtFixAddressMmramBase;

  //
  // If the memory usage bit map is not initialized,  do it. Every bit in the array
  // indicate the status of the corresponding memory page, available or not
  //
  if (mMmCodeMemoryRangeUsageBitMap == NULL) {
    mMmCodeMemoryRangeUsageBitMap = AllocateZeroPool (((MmCodePageNumber / 64) + 1) * sizeof (UINT64));
  }

  //
  // If the Dxe code memory range is not allocated or the bit map array allocation failed, return EFI_NOT_FOUND
  //
  if (mMmCodeMemoryRangeUsageBitMap == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // see if the memory range for loading the image is in the MM code range.
  //
  if (MmCodeBase + MmCodeSize <  ImageBase + ImageSize || MmCodeBase >  ImageBase) {
    return EFI_NOT_FOUND;
  }

  //
  // Test if the memory is avalaible or not.
  //
  BaseOffsetPageNumber = (UINTN)EFI_SIZE_TO_PAGES ((UINT32)(ImageBase - MmCodeBase));
  TopOffsetPageNumber  = (UINTN)EFI_SIZE_TO_PAGES ((UINT32)(ImageBase + ImageSize - MmCodeBase));
  for (Index = BaseOffsetPageNumber; Index < TopOffsetPageNumber; Index ++) {
    if ((mMmCodeMemoryRangeUsageBitMap[Index / 64] & LShiftU64 (1, (Index % 64))) != 0) {
      //
      // This page is already used.
      //
      return EFI_NOT_FOUND;
    }
  }

  //
  // Being here means the memory range is available.  So mark the bits for the memory range
  //
  for (Index = BaseOffsetPageNumber; Index < TopOffsetPageNumber; Index ++) {
    mMmCodeMemoryRangeUsageBitMap[Index / 64] |= LShiftU64 (1, (Index % 64));
  }
  return  EFI_SUCCESS;
}

/**
  Get the fixed loading address from image header assigned by build tool. This function only be called
  when Loading module at Fixed address feature enabled.

  @param  ImageContext              Pointer to the image context structure that describes the PE/COFF
                                    image that needs to be examined by this function.
  @retval EFI_SUCCESS               An fixed loading address is assigned to this image by build tools .
  @retval EFI_NOT_FOUND             The image has no assigned fixed loadding address.

**/
EFI_STATUS
GetPeCoffImageFixLoadingAssignedAddress(
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  UINTN                              SectionHeaderOffset;
  EFI_STATUS                         Status;
  EFI_IMAGE_SECTION_HEADER           SectionHeader;
  EFI_IMAGE_OPTIONAL_HEADER_UNION    *ImgHdr;
  EFI_PHYSICAL_ADDRESS               FixLoadingAddress;
  UINT16                             Index;
  UINTN                              Size;
  UINT16                             NumberOfSections;
  UINT64                             ValueInSectionHeader;

  FixLoadingAddress = 0;
  Status = EFI_NOT_FOUND;

  //
  // Get PeHeader pointer
  //
  ImgHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)((CHAR8* )ImageContext->Handle + ImageContext->PeCoffHeaderOffset);
  SectionHeaderOffset = ImageContext->PeCoffHeaderOffset + sizeof (UINT32) + sizeof (EFI_IMAGE_FILE_HEADER) +
    ImgHdr->Pe32.FileHeader.SizeOfOptionalHeader;
  NumberOfSections = ImgHdr->Pe32.FileHeader.NumberOfSections;

  //
  // Get base address from the first section header that doesn't point to code section.
  //
  for (Index = 0; Index < NumberOfSections; Index++) {
    //
    // Read section header from file
    //
    Size = sizeof (EFI_IMAGE_SECTION_HEADER);
    Status = ImageContext->ImageRead (
                             ImageContext->Handle,
                             SectionHeaderOffset,
                             &Size,
                             &SectionHeader
                             );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = EFI_NOT_FOUND;

    if ((SectionHeader.Characteristics & EFI_IMAGE_SCN_CNT_CODE) == 0) {
      //
      // Build tool will save the address in PointerToRelocations & PointerToLineNumbers fields
      // in the first section header that doesn't point to code section in image header. So there
      // is an assumption that when the feature is enabled, if a module with a loading address
      // assigned by tools, the PointerToRelocations & PointerToLineNumbers fields should not be
      // Zero, or else, these 2 fields should be set to Zero
      //
      ValueInSectionHeader = ReadUnaligned64 ((UINT64*)&SectionHeader.PointerToRelocations);
      if (ValueInSectionHeader != 0) {
        //
        // Found first section header that doesn't point to code section in which build tool saves the
        // offset to SMRAM base as image base in PointerToRelocations & PointerToLineNumbers fields
        //
        FixLoadingAddress = (EFI_PHYSICAL_ADDRESS)(gLoadModuleAtFixAddressMmramBase + (INT64)ValueInSectionHeader);
        //
        // Check if the memory range is available.
        //
        Status = CheckAndMarkFixLoadingMemoryUsageBitMap (FixLoadingAddress, (UINTN)(ImageContext->ImageSize + ImageContext->SectionAlignment));
        if (!EFI_ERROR(Status)) {
          //
          // The assigned address is valid. Return the specified loading address
          //
          ImageContext->ImageAddress = FixLoadingAddress;
        }
      }
      break;
    }
    SectionHeaderOffset += sizeof (EFI_IMAGE_SECTION_HEADER);
  }
  DEBUG ((DEBUG_INFO|DEBUG_LOAD, "LOADING MODULE FIXED INFO: Loading module at fixed address %x, Status = %r\n",
          FixLoadingAddress, Status));
  return Status;
}
/**
  Loads an EFI image into SMRAM.

  @param  DriverEntry             EFI_MM_DRIVER_ENTRY instance

  @return EFI_STATUS

**/
EFI_STATUS
EFIAPI
MmLoadImage (
  IN OUT EFI_MM_DRIVER_ENTRY  *DriverEntry
  )
{
  VOID                           *Buffer;
  UINTN                          PageCount;
  EFI_STATUS                     Status;
  EFI_PHYSICAL_ADDRESS           DstBuffer;
  PE_COFF_LOADER_IMAGE_CONTEXT   ImageContext;

  DEBUG ((DEBUG_INFO, "MmLoadImage - %g\n", &DriverEntry->FileName));

  Buffer = AllocateCopyPool (DriverEntry->Pe32DataSize, DriverEntry->Pe32Data);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status               = EFI_SUCCESS;

  //
  // Initialize ImageContext
  //
  ImageContext.Handle = Buffer;
  ImageContext.ImageRead = PeCoffLoaderImageReadFromMemory;

  //
  // Get information about the image being loaded
  //
  Status = PeCoffLoaderGetImageInfo (&ImageContext);
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      MmFreePool (Buffer);
    }
    return Status;
  }

  PageCount = (UINTN)EFI_SIZE_TO_PAGES ((UINTN)ImageContext.ImageSize + ImageContext.SectionAlignment);
  DstBuffer = (UINTN)(-1);

  Status = MmAllocatePages (
             AllocateMaxAddress,
             EfiRuntimeServicesCode,
             PageCount,
             &DstBuffer
             );
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      MmFreePool (Buffer);
    }
    return Status;
  }

  ImageContext.ImageAddress = (EFI_PHYSICAL_ADDRESS)DstBuffer;

  //
  // Align buffer on section boundry
  //
  ImageContext.ImageAddress += ImageContext.SectionAlignment - 1;
  ImageContext.ImageAddress &= ~((EFI_PHYSICAL_ADDRESS)(ImageContext.SectionAlignment - 1));

  //
  // Load the image to our new buffer
  //
  Status = PeCoffLoaderLoadImage (&ImageContext);
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      MmFreePool (Buffer);
    }
    MmFreePages (DstBuffer, PageCount);
    return Status;
  }

  //
  // Relocate the image in our new buffer
  //
  Status = PeCoffLoaderRelocateImage (&ImageContext);
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      MmFreePool (Buffer);
    }
    MmFreePages (DstBuffer, PageCount);
    return Status;
  }

  //
  // Flush the instruction cache so the image data are written before we execute it
  //
  InvalidateInstructionCacheRange ((VOID *)(UINTN) ImageContext.ImageAddress, (UINTN) ImageContext.ImageSize);

  //
  // Save Image EntryPoint in DriverEntry
  //
  DriverEntry->ImageEntryPoint  = ImageContext.EntryPoint;
  DriverEntry->ImageBuffer      = DstBuffer;
  DriverEntry->NumberOfPage     = PageCount;

  if (mEfiSystemTable != NULL) {
    Status = mEfiSystemTable->BootServices->AllocatePool (
                                              EfiBootServicesData,
                                              sizeof (EFI_LOADED_IMAGE_PROTOCOL),
                                              (VOID **)&DriverEntry->LoadedImage
                                              );
    if (EFI_ERROR (Status)) {
      if (Buffer != NULL) {
        MmFreePool (Buffer);
      }
      MmFreePages (DstBuffer, PageCount);
      return Status;
    }

    ZeroMem (DriverEntry->LoadedImage, sizeof (EFI_LOADED_IMAGE_PROTOCOL));
    //
    // Fill in the remaining fields of the Loaded Image Protocol instance.
    // Note: ImageBase is an SMRAM address that can not be accessed outside of SMRAM if SMRAM window is closed.
    //
    DriverEntry->LoadedImage->Revision      = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
    DriverEntry->LoadedImage->ParentHandle  = NULL;
    DriverEntry->LoadedImage->SystemTable   = mEfiSystemTable;
    DriverEntry->LoadedImage->DeviceHandle  = NULL;
    DriverEntry->LoadedImage->FilePath      = NULL;

    DriverEntry->LoadedImage->ImageBase     = (VOID *)(UINTN)DriverEntry->ImageBuffer;
    DriverEntry->LoadedImage->ImageSize     = ImageContext.ImageSize;
    DriverEntry->LoadedImage->ImageCodeType = EfiRuntimeServicesCode;
    DriverEntry->LoadedImage->ImageDataType = EfiRuntimeServicesData;

    //
    // Create a new image handle in the UEFI handle database for the MM Driver
    //
    DriverEntry->ImageHandle = NULL;
    Status = mEfiSystemTable->BootServices->InstallMultipleProtocolInterfaces (
                                              &DriverEntry->ImageHandle,
                                              &gEfiLoadedImageProtocolGuid,
                                              DriverEntry->LoadedImage,
                                              NULL
                                              );
  }

  //
  // Print the load address and the PDB file name if it is available
  //
  DEBUG_CODE_BEGIN ();

  UINTN Index;
  UINTN StartIndex;
  CHAR8 EfiFileName[256];

  DEBUG ((DEBUG_INFO | DEBUG_LOAD,
          "Loading MM driver at 0x%11p EntryPoint=0x%11p ",
          (VOID *)(UINTN) ImageContext.ImageAddress,
          FUNCTION_ENTRY_POINT (ImageContext.EntryPoint)));

  //
  // Print Module Name by Pdb file path.
  // Windows and Unix style file path are all trimmed correctly.
  //
  if (ImageContext.PdbPointer != NULL) {
    StartIndex = 0;
    for (Index = 0; ImageContext.PdbPointer[Index] != 0; Index++) {
      if ((ImageContext.PdbPointer[Index] == '\\') || (ImageContext.PdbPointer[Index] == '/')) {
        StartIndex = Index + 1;
      }
    }

    //
    // Copy the PDB file name to our temporary string, and replace .pdb with .efi
    // The PDB file name is limited in the range of 0~255.
    // If the length is bigger than 255, trim the redudant characters to avoid overflow in array boundary.
    //
    for (Index = 0; Index < sizeof (EfiFileName) - 4; Index++) {
      EfiFileName[Index] = ImageContext.PdbPointer[Index + StartIndex];
      if (EfiFileName[Index] == 0) {
        EfiFileName[Index] = '.';
      }
      if (EfiFileName[Index] == '.') {
        EfiFileName[Index + 1] = 'e';
        EfiFileName[Index + 2] = 'f';
        EfiFileName[Index + 3] = 'i';
        EfiFileName[Index + 4] = 0;
        break;
      }
    }

    if (Index == sizeof (EfiFileName) - 4) {
      EfiFileName[Index] = 0;
    }
    DEBUG ((DEBUG_INFO | DEBUG_LOAD, "%a", EfiFileName));
  }
  DEBUG ((DEBUG_INFO | DEBUG_LOAD, "\n"));

  DEBUG_CODE_END ();

  //
  // Free buffer allocated by Fv->ReadSection.
  //
  // The UEFI Boot Services FreePool() function must be used because Fv->ReadSection
  // used the UEFI Boot Services AllocatePool() function
  //
  MmFreePool (Buffer);
  return Status;
}

/**
  Preprocess dependency expression and update DriverEntry to reflect the
  state of  Before and After dependencies. If DriverEntry->Before
  or DriverEntry->After is set it will never be cleared.

  @param  DriverEntry           DriverEntry element to update .

  @retval EFI_SUCCESS           It always works.

**/
EFI_STATUS
MmPreProcessDepex (
  IN EFI_MM_DRIVER_ENTRY  *DriverEntry
  )
{
  UINT8  *Iterator;

  Iterator = DriverEntry->Depex;
  DriverEntry->Dependent = TRUE;

  if (*Iterator == EFI_DEP_BEFORE) {
    DriverEntry->Before = TRUE;
  } else if (*Iterator == EFI_DEP_AFTER) {
    DriverEntry->After = TRUE;
  }

  if (DriverEntry->Before || DriverEntry->After) {
    CopyMem (&DriverEntry->BeforeAfterGuid, Iterator + 1, sizeof (EFI_GUID));
  }

  return EFI_SUCCESS;
}

/**
  Read Depex and pre-process the Depex for Before and After. If Section Extraction
  protocol returns an error via ReadSection defer the reading of the Depex.

  @param  DriverEntry           Driver to work on.

  @retval EFI_SUCCESS           Depex read and preprossesed
  @retval EFI_PROTOCOL_ERROR    The section extraction protocol returned an error
                                and  Depex reading needs to be retried.
  @retval Error                 DEPEX not found.

**/
EFI_STATUS
MmGetDepexSectionAndPreProccess (
  IN EFI_MM_DRIVER_ENTRY  *DriverEntry
  )
{
  EFI_STATUS                     Status;

  //
  // Data already read
  //
  if (DriverEntry->Depex == NULL) {
    Status = EFI_NOT_FOUND;
  } else {
    Status = EFI_SUCCESS;
  }
  if (EFI_ERROR (Status)) {
    if (Status == EFI_PROTOCOL_ERROR) {
      //
      // The section extraction protocol failed so set protocol error flag
      //
      DriverEntry->DepexProtocolError = TRUE;
    } else {
      //
      // If no Depex assume depend on all architectural protocols
      //
      DriverEntry->Depex = NULL;
      DriverEntry->Dependent = TRUE;
      DriverEntry->DepexProtocolError = FALSE;
    }
  } else {
    //
    // Set Before and After state information based on Depex
    // Driver will be put in Dependent state
    //
    MmPreProcessDepex (DriverEntry);
    DriverEntry->DepexProtocolError = FALSE;
  }

  return Status;
}

/**
  This is the main Dispatcher for MM and it exits when there are no more
  drivers to run. Drain the mScheduledQueue and load and start a PE
  image for each driver. Search the mDiscoveredList to see if any driver can
  be placed on the mScheduledQueue. If no drivers are placed on the
  mScheduledQueue exit the function.

  @retval EFI_SUCCESS           All of the MM Drivers that could be dispatched
                                have been run and the MM Entry Point has been
                                registered.
  @retval EFI_NOT_READY         The MM Driver that registered the MM Entry Point
                                was just dispatched.
  @retval EFI_NOT_FOUND         There are no MM Drivers available to be dispatched.
  @retval EFI_ALREADY_STARTED   The MM Dispatcher is already running

**/
EFI_STATUS
MmDispatcher (
  VOID
  )
{
  EFI_STATUS            Status;
  LIST_ENTRY            *Link;
  EFI_MM_DRIVER_ENTRY  *DriverEntry;
  BOOLEAN               ReadyToRun;
  BOOLEAN               PreviousMmEntryPointRegistered;

  DEBUG ((DEBUG_INFO, "MmDispatcher\n"));

  if (!gRequestDispatch) {
    DEBUG ((DEBUG_INFO, "  !gRequestDispatch\n"));
    return EFI_NOT_FOUND;
  }

  if (gDispatcherRunning) {
    DEBUG ((DEBUG_INFO, "  gDispatcherRunning\n"));
    //
    // If the dispatcher is running don't let it be restarted.
    //
    return EFI_ALREADY_STARTED;
  }

  gDispatcherRunning = TRUE;

  do {
    //
    // Drain the Scheduled Queue
    //
    DEBUG ((DEBUG_INFO, "  Drain the Scheduled Queue\n"));
    while (!IsListEmpty (&mScheduledQueue)) {
      DriverEntry = CR (
                      mScheduledQueue.ForwardLink,
                      EFI_MM_DRIVER_ENTRY,
                      ScheduledLink,
                      EFI_MM_DRIVER_ENTRY_SIGNATURE
                      );
      DEBUG ((DEBUG_INFO, "  DriverEntry (Scheduled) - %g\n", &DriverEntry->FileName));

      //
      // Load the MM Driver image into memory. If the Driver was transitioned from
      // Untrused to Scheduled it would have already been loaded so we may need to
      // skip the LoadImage
      //
      if (DriverEntry->ImageHandle == NULL) {
        Status = MmLoadImage (DriverEntry);

        //
        // Update the driver state to reflect that it's been loaded
        //
        if (EFI_ERROR (Status)) {
          //
          // The MM Driver could not be loaded, and do not attempt to load or start it again.
          // Take driver from Scheduled to Initialized.
          //
          DriverEntry->Initialized  = TRUE;
          DriverEntry->Scheduled = FALSE;
          RemoveEntryList (&DriverEntry->ScheduledLink);

          //
          // If it's an error don't try the StartImage
          //
          continue;
        }
      }

      DriverEntry->Scheduled    = FALSE;
      DriverEntry->Initialized  = TRUE;
      RemoveEntryList (&DriverEntry->ScheduledLink);

      //
      // Cache state of MmEntryPointRegistered before calling entry point
      //
      PreviousMmEntryPointRegistered = gMmCorePrivate->MmEntryPointRegistered;

      //
      // For each MM driver, pass NULL as ImageHandle
      //
      if (mEfiSystemTable == NULL) {
        DEBUG ((DEBUG_INFO, "StartImage - 0x%x (Standalone Mode)\n", DriverEntry->ImageEntryPoint));
        Status = ((MM_IMAGE_ENTRY_POINT)(UINTN)DriverEntry->ImageEntryPoint) (DriverEntry->ImageHandle, &gMmCoreMmst);
      } else {
        DEBUG ((DEBUG_INFO, "StartImage - 0x%x (Tradition Mode)\n", DriverEntry->ImageEntryPoint));
        Status = ((EFI_IMAGE_ENTRY_POINT)(UINTN)DriverEntry->ImageEntryPoint) (
                                                               DriverEntry->ImageHandle,
                                                               mEfiSystemTable
                                                               );
      }
      if (EFI_ERROR(Status)) {
        DEBUG ((DEBUG_INFO, "StartImage Status - %r\n", Status));
        MmFreePages(DriverEntry->ImageBuffer, DriverEntry->NumberOfPage);
      }

      if (!PreviousMmEntryPointRegistered && gMmCorePrivate->MmEntryPointRegistered) {
        //
        // Return immediately if the MM Entry Point was registered by the MM
        // Driver that was just dispatched.  The MM IPL will reinvoke the MM
        // Core Dispatcher.  This is required so MM Mode may be enabled as soon
        // as all the dependent MM Drivers for MM Mode have been dispatched.
        // Once the MM Entry Point has been registered, then MM Mode will be
        // used.
        //
        gRequestDispatch = TRUE;
        gDispatcherRunning = FALSE;
        return EFI_NOT_READY;
      }
    }

    //
    // Search DriverList for items to place on Scheduled Queue
    //
    DEBUG ((DEBUG_INFO, "  Search DriverList for items to place on Scheduled Queue\n"));
    ReadyToRun = FALSE;
    for (Link = mDiscoveredList.ForwardLink; Link != &mDiscoveredList; Link = Link->ForwardLink) {
      DriverEntry = CR (Link, EFI_MM_DRIVER_ENTRY, Link, EFI_MM_DRIVER_ENTRY_SIGNATURE);
      DEBUG ((DEBUG_INFO, "  DriverEntry (Discovered) - %g\n", &DriverEntry->FileName));

      if (DriverEntry->DepexProtocolError) {
        //
        // If Section Extraction Protocol did not let the Depex be read before retry the read
        //
        Status = MmGetDepexSectionAndPreProccess (DriverEntry);
      }

      if (DriverEntry->Dependent) {
        if (MmIsSchedulable (DriverEntry)) {
          MmInsertOnScheduledQueueWhileProcessingBeforeAndAfter (DriverEntry);
          ReadyToRun = TRUE;
        }
      }
    }
  } while (ReadyToRun);

  //
  // If there is no more MM driver to dispatch, stop the dispatch request
  //
  DEBUG ((DEBUG_INFO, "  no more MM driver to dispatch, stop the dispatch request\n"));
  gRequestDispatch = FALSE;
  for (Link = mDiscoveredList.ForwardLink; Link != &mDiscoveredList; Link = Link->ForwardLink) {
    DriverEntry = CR (Link, EFI_MM_DRIVER_ENTRY, Link, EFI_MM_DRIVER_ENTRY_SIGNATURE);
    DEBUG ((DEBUG_INFO, "  DriverEntry (Discovered) - %g\n", &DriverEntry->FileName));

    if (!DriverEntry->Initialized) {
      //
      // We have MM driver pending to dispatch
      //
      gRequestDispatch = TRUE;
      break;
    }
  }

  gDispatcherRunning = FALSE;

  return EFI_SUCCESS;
}

/**
  Insert InsertedDriverEntry onto the mScheduledQueue. To do this you
  must add any driver with a before dependency on InsertedDriverEntry first.
  You do this by recursively calling this routine. After all the Befores are
  processed you can add InsertedDriverEntry to the mScheduledQueue.
  Then you can add any driver with an After dependency on InsertedDriverEntry
  by recursively calling this routine.

  @param  InsertedDriverEntry   The driver to insert on the ScheduledLink Queue

**/
VOID
MmInsertOnScheduledQueueWhileProcessingBeforeAndAfter (
  IN  EFI_MM_DRIVER_ENTRY   *InsertedDriverEntry
  )
{
  LIST_ENTRY            *Link;
  EFI_MM_DRIVER_ENTRY *DriverEntry;

  //
  // Process Before Dependency
  //
  for (Link = mDiscoveredList.ForwardLink; Link != &mDiscoveredList; Link = Link->ForwardLink) {
    DriverEntry = CR(Link, EFI_MM_DRIVER_ENTRY, Link, EFI_MM_DRIVER_ENTRY_SIGNATURE);
    if (DriverEntry->Before && DriverEntry->Dependent && DriverEntry != InsertedDriverEntry) {
      DEBUG ((DEBUG_DISPATCH, "Evaluate MM DEPEX for FFS(%g)\n", &DriverEntry->FileName));
      DEBUG ((DEBUG_DISPATCH, "  BEFORE FFS(%g) = ", &DriverEntry->BeforeAfterGuid));
      if (CompareGuid (&InsertedDriverEntry->FileName, &DriverEntry->BeforeAfterGuid)) {
        //
        // Recursively process BEFORE
        //
        DEBUG ((DEBUG_DISPATCH, "TRUE\n  END\n  RESULT = TRUE\n"));
        MmInsertOnScheduledQueueWhileProcessingBeforeAndAfter (DriverEntry);
      } else {
        DEBUG ((DEBUG_DISPATCH, "FALSE\n  END\n  RESULT = FALSE\n"));
      }
    }
  }

  //
  // Convert driver from Dependent to Scheduled state
  //

  InsertedDriverEntry->Dependent = FALSE;
  InsertedDriverEntry->Scheduled = TRUE;
  InsertTailList (&mScheduledQueue, &InsertedDriverEntry->ScheduledLink);


  //
  // Process After Dependency
  //
  for (Link = mDiscoveredList.ForwardLink; Link != &mDiscoveredList; Link = Link->ForwardLink) {
    DriverEntry = CR(Link, EFI_MM_DRIVER_ENTRY, Link, EFI_MM_DRIVER_ENTRY_SIGNATURE);
    if (DriverEntry->After && DriverEntry->Dependent && DriverEntry != InsertedDriverEntry) {
      DEBUG ((DEBUG_DISPATCH, "Evaluate MM DEPEX for FFS(%g)\n", &DriverEntry->FileName));
      DEBUG ((DEBUG_DISPATCH, "  AFTER FFS(%g) = ", &DriverEntry->BeforeAfterGuid));
      if (CompareGuid (&InsertedDriverEntry->FileName, &DriverEntry->BeforeAfterGuid)) {
        //
        // Recursively process AFTER
        //
        DEBUG ((DEBUG_DISPATCH, "TRUE\n  END\n  RESULT = TRUE\n"));
        MmInsertOnScheduledQueueWhileProcessingBeforeAndAfter (DriverEntry);
      } else {
        DEBUG ((DEBUG_DISPATCH, "FALSE\n  END\n  RESULT = FALSE\n"));
      }
    }
  }
}

/**
  Return TRUE if the Fv has been processed, FALSE if not.

  @param  FvHandle              The handle of a FV that's being tested

  @retval TRUE                  Fv protocol on FvHandle has been processed
  @retval FALSE                 Fv protocol on FvHandle has not yet been
                                processed

**/
BOOLEAN
FvHasBeenProcessed (
  IN EFI_HANDLE  FvHandle
  )
{
  LIST_ENTRY    *Link;
  KNOWN_HANDLE  *KnownHandle;

  for (Link = mFvHandleList.ForwardLink; Link != &mFvHandleList; Link = Link->ForwardLink) {
    KnownHandle = CR (Link, KNOWN_HANDLE, Link, KNOWN_HANDLE_SIGNATURE);
    if (KnownHandle->Handle == FvHandle) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
  Remember that Fv protocol on FvHandle has had it's drivers placed on the
  mDiscoveredList. This fucntion adds entries on the mFvHandleList. Items are
  never removed/freed from the mFvHandleList.

  @param  FvHandle              The handle of a FV that has been processed

**/
VOID
FvIsBeingProcesssed (
  IN EFI_HANDLE  FvHandle
  )
{
  KNOWN_HANDLE  *KnownHandle;

  DEBUG ((DEBUG_INFO, "FvIsBeingProcesssed - 0x%08x\n", FvHandle));

  KnownHandle = AllocatePool (sizeof (KNOWN_HANDLE));
  ASSERT (KnownHandle != NULL);

  KnownHandle->Signature = KNOWN_HANDLE_SIGNATURE;
  KnownHandle->Handle = FvHandle;
  InsertTailList (&mFvHandleList, &KnownHandle->Link);
}

/**
  Add an entry to the mDiscoveredList. Allocate memory to store the DriverEntry,
  and initilize any state variables. Read the Depex from the FV and store it
  in DriverEntry. Pre-process the Depex to set the Before and After state.
  The Discovered list is never free'ed and contains booleans that represent the
  other possible MM driver states.

  @param  Fv                    Fv protocol, needed to read Depex info out of
                                FLASH.
  @param  FvHandle              Handle for Fv, needed in the
                                EFI_MM_DRIVER_ENTRY so that the PE image can be
                                read out of the FV at a later time.
  @param  DriverName            Name of driver to add to mDiscoveredList.

  @retval EFI_SUCCESS           If driver was added to the mDiscoveredList.
  @retval EFI_ALREADY_STARTED   The driver has already been started. Only one
                                DriverName may be active in the system at any one
                                time.

**/
EFI_STATUS
MmAddToDriverList (
  IN EFI_HANDLE   FvHandle,
  IN VOID         *Pe32Data,
  IN UINTN        Pe32DataSize,
  IN VOID         *Depex,
  IN UINTN        DepexSize,
  IN EFI_GUID     *DriverName
  )
{
  EFI_MM_DRIVER_ENTRY  *DriverEntry;

  DEBUG ((DEBUG_INFO, "MmAddToDriverList - %g (0x%08x)\n", DriverName, Pe32Data));

  //
  // Create the Driver Entry for the list. ZeroPool initializes lots of variables to
  // NULL or FALSE.
  //
  DriverEntry = AllocateZeroPool (sizeof (EFI_MM_DRIVER_ENTRY));
  ASSERT (DriverEntry != NULL);

  DriverEntry->Signature        = EFI_MM_DRIVER_ENTRY_SIGNATURE;
  CopyGuid (&DriverEntry->FileName, DriverName);
  DriverEntry->FvHandle         = FvHandle;
  DriverEntry->Pe32Data         = Pe32Data;
  DriverEntry->Pe32DataSize     = Pe32DataSize;
  DriverEntry->Depex            = Depex;
  DriverEntry->DepexSize        = DepexSize;

  MmGetDepexSectionAndPreProccess (DriverEntry);

  InsertTailList (&mDiscoveredList, &DriverEntry->Link);
  gRequestDispatch = TRUE;

  return EFI_SUCCESS;
}

/**
  This function is the main entry point for an MM handler dispatch
  or communicate-based callback.

  Event notification that is fired every time a FV dispatch protocol is added.
  More than one protocol may have been added when this event is fired, so you
  must loop on MmLocateHandle () to see how many protocols were added and
  do the following to each FV:
  If the Fv has already been processed, skip it. If the Fv has not been
  processed then mark it as being processed, as we are about to process it.
  Read the Fv and add any driver in the Fv to the mDiscoveredList.The
  mDiscoveredList is never free'ed and contains variables that define
  the other states the MM driver transitions to..
  While you are at it read the A Priori file into memory.
  Place drivers in the A Priori list onto the mScheduledQueue.

  @param  DispatchHandle  The unique handle assigned to this handler by SmiHandlerRegister().
  @param  Context         Points to an optional handler context which was specified when the handler was registered.
  @param  CommBuffer      A pointer to a collection of data in memory that will
                          be conveyed from a non-MM environment into an MM environment.
  @param  CommBufferSize  The size of the CommBuffer.

  @return Status Code

**/
EFI_STATUS
EFIAPI
MmDriverDispatchHandler (
  IN     EFI_HANDLE  DispatchHandle,
  IN     CONST VOID  *Context,        OPTIONAL
  IN OUT VOID        *CommBuffer,     OPTIONAL
  IN OUT UINTN       *CommBufferSize  OPTIONAL
  )
{
  EFI_STATUS                            Status;

  DEBUG ((DEBUG_INFO, "MmDriverDispatchHandler\n"));

  //
  // Execute the MM Dispatcher on any newly discovered FVs and previously
  // discovered MM drivers that have been discovered but not dispatched.
  //
  Status = MmDispatcher ();

  //
  // Check to see if CommBuffer and CommBufferSize are valid
  //
  if (CommBuffer != NULL && CommBufferSize != NULL) {
    if (*CommBufferSize > 0) {
      if (Status == EFI_NOT_READY) {
        //
        // If a the MM Core Entry Point was just registered, then set flag to
        // request the MM Dispatcher to be restarted.
        //
        *(UINT8 *)CommBuffer = COMM_BUFFER_MM_DISPATCH_RESTART;
      } else if (!EFI_ERROR (Status)) {
        //
        // Set the flag to show that the MM Dispatcher executed without errors
        //
        *(UINT8 *)CommBuffer = COMM_BUFFER_MM_DISPATCH_SUCCESS;
      } else {
        //
        // Set the flag to show that the MM Dispatcher encountered an error
        //
        *(UINT8 *)CommBuffer = COMM_BUFFER_MM_DISPATCH_ERROR;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  This function is the main entry point for an MM handler dispatch
  or communicate-based callback.

  @param  DispatchHandle  The unique handle assigned to this handler by SmiHandlerRegister().
  @param  Context         Points to an optional handler context which was specified when the handler was registered.
  @param  CommBuffer      A pointer to a collection of data in memory that will
                          be conveyed from a non-MM environment into an MM environment.
  @param  CommBufferSize  The size of the CommBuffer.

  @return Status Code

**/
EFI_STATUS
EFIAPI
MmFvDispatchHandler (
  IN     EFI_HANDLE               DispatchHandle,
  IN     CONST VOID               *Context,        OPTIONAL
  IN OUT VOID                     *CommBuffer,     OPTIONAL
  IN OUT UINTN                    *CommBufferSize  OPTIONAL
  )
{
  EFI_STATUS                            Status;
  EFI_MM_COMMUNICATE_FV_DISPATCH_DATA  *CommunicationFvDispatchData;
  EFI_FIRMWARE_VOLUME_HEADER            *FwVolHeader;

  DEBUG ((DEBUG_INFO, "MmFvDispatchHandler\n"));

  CommunicationFvDispatchData = CommBuffer;

  DEBUG ((DEBUG_INFO, "  Dispatch - 0x%016lx - 0x%016lx\n", CommunicationFvDispatchData->Address,
          CommunicationFvDispatchData->Size));

  FwVolHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)CommunicationFvDispatchData->Address;

  MmCoreFfsFindMmDriver (FwVolHeader);

  //
  // Execute the MM Dispatcher on any newly discovered FVs and previously
  // discovered MM drivers that have been discovered but not dispatched.
  //
  Status = MmDispatcher ();

  return Status;
}

/**
  Traverse the discovered list for any drivers that were discovered but not loaded
  because the dependency experessions evaluated to false.

**/
VOID
MmDisplayDiscoveredNotDispatched (
  VOID
  )
{
  LIST_ENTRY                   *Link;
  EFI_MM_DRIVER_ENTRY         *DriverEntry;

  for (Link = mDiscoveredList.ForwardLink;Link !=&mDiscoveredList; Link = Link->ForwardLink) {
    DriverEntry = CR (Link, EFI_MM_DRIVER_ENTRY, Link, EFI_MM_DRIVER_ENTRY_SIGNATURE);
    if (DriverEntry->Dependent) {
      DEBUG ((DEBUG_LOAD, "MM Driver %g was discovered but not loaded!!\n", &DriverEntry->FileName));
    }
  }
}
