/* $Id: class2.c,v 1.8 2002/03/03 23:45:30 ekohl Exp $
 *
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            services/storage/class2/class2.c
 * PURPOSE:         SCSI class driver
 * PROGRAMMER:      Eric Kohl (ekohl@rz-online.de)
 */

/*
 * TODO:
 *	- a lot ;-)
 */

/* INCLUDES *****************************************************************/

#include <ddk/ntddk.h>
#include "../include/scsi.h"
#include "../include/class2.h"

#define NDEBUG
#include <debug.h>


#define VERSION "0.0.1"

#define INQUIRY_DATA_SIZE 2048


static NTSTATUS STDCALL
ScsiClassCreateClose(IN PDEVICE_OBJECT DeviceObject,
		     IN PIRP Irp);

static NTSTATUS STDCALL
ScsiClassReadWrite(IN PDEVICE_OBJECT DeviceObject,
		   IN PIRP Irp);

static NTSTATUS STDCALL
ScsiClassScsiDispatch(IN PDEVICE_OBJECT DeviceObject,
		     IN PIRP Irp);

static NTSTATUS STDCALL
ScsiClassDeviceDispatch(IN PDEVICE_OBJECT DeviceObject,
		       IN PIRP Irp);

static NTSTATUS STDCALL
ScsiClassShutdownFlush(IN PDEVICE_OBJECT DeviceObject,
		       IN PIRP Irp);


/* FUNCTIONS ****************************************************************/

//    DriverEntry
//
//  DESCRIPTION:
//    This function initializes the driver.
//
//  RUN LEVEL:
//    PASSIVE_LEVEL
//
//  ARGUMENTS:
//    IN  PDRIVER_OBJECT   DriverObject  System allocated Driver Object
//                                       for this driver
//    IN  PUNICODE_STRING  RegistryPath  Name of registry driver service 
//                                       key
//
//  RETURNS:
//    NTSTATUS

NTSTATUS STDCALL
DriverEntry(IN PDRIVER_OBJECT DriverObject,
	    IN PUNICODE_STRING RegistryPath)
{
  DbgPrint("Class Driver %s\n", VERSION);
  return(STATUS_SUCCESS);
}


VOID
ScsiClassDebugPrint(IN ULONG DebugPrintLevel,
		    IN PCHAR DebugMessage,
		    ...)
{
  char Buffer[256];
  va_list ap;

#if 0
  if (DebugPrintLevel > InternalDebugLevel)
    return;
#endif

  va_start(ap, DebugMessage);
  vsprintf(Buffer, DebugMessage, ap);
  va_end(ap);

  DbgPrint(Buffer);
}


NTSTATUS STDCALL
ScsiClassAsynchronousCompletion(IN PDEVICE_OBJECT DeviceObject,
				IN PIRP Irp,
				IN PVOID Context)
{
  UNIMPLEMENTED;
}


VOID STDCALL
ScsiClassBuildRequest(PDEVICE_OBJECT DeviceObject,
		      PIRP Irp)
{
  PDEVICE_EXTENSION DeviceExtension;
  PIO_STACK_LOCATION CurrentIrpStack;
  PIO_STACK_LOCATION NextIrpStack;
  LARGE_INTEGER StartingOffset;
  LARGE_INTEGER StartingBlock;
  PSCSI_REQUEST_BLOCK Srb;
  PCDB Cdb;
  ULONG LogicalBlockAddress;
  USHORT TransferBlocks;

  DeviceExtension = DeviceObject->DeviceExtension;
  CurrentIrpStack = IoGetCurrentIrpStackLocation(Irp);
  NextIrpStack = IoGetNextIrpStackLocation(Irp);
  StartingOffset = CurrentIrpStack->Parameters.Read.ByteOffset;

  /* calculate logical block address */
  StartingBlock.QuadPart = StartingOffset.QuadPart / 512; // >> deviceExtension->SectorShift;
  LogicalBlockAddress = (ULONG)StartingBlock.u.LowPart;

  DPRINT1("Logical block address: %lu\n", LogicalBlockAddress);

  /* allocate and initialize an SRB */
  /* FIXME: use lookaside list instead */
  Srb = ExAllocatePool(NonPagedPool,
		       sizeof(SCSI_REQUEST_BLOCK));

  Srb->SrbFlags = 0;
  Srb->Length = sizeof(SCSI_REQUEST_BLOCK); //SCSI_REQUEST_BLOCK_SIZE;
  Srb->OriginalRequest = Irp;
  Srb->PathId = DeviceExtension->PathId;
  Srb->TargetId = DeviceExtension->TargetId;
  Srb->Lun = DeviceExtension->Lun;
  Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
  Srb->DataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
  Srb->DataTransferLength = CurrentIrpStack->Parameters.Read.Length;
  Srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
  Srb->QueueSortKey = LogicalBlockAddress;

  Srb->SenseInfoBuffer = DeviceExtension->SenseData;
  Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

  Srb->TimeOutValue =
    ((Srb->DataTransferLength + 0xFFFF) >> 16) * DeviceExtension->TimeOutValue;

  Srb->SrbStatus = SRB_STATUS_SUCCESS;
  Srb->ScsiStatus = 0;
  Srb->NextSrb = 0;

  Srb->CdbLength = 10;
  Cdb = (PCDB)Srb->Cdb;

  /* Initialize ATAPI packet (12 bytes) */
  RtlZeroMemory(Cdb,
		MAXIMUM_CDB_SIZE);

  Cdb->CDB10.LogicalUnitNumber = DeviceExtension->Lun;
  TransferBlocks = (USHORT)(CurrentIrpStack->Parameters.Read.Length >> DeviceExtension->SectorShift);

  /* Copy little endian values into CDB in big endian format */
  Cdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte3;
  Cdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte2;
  Cdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte1;
  Cdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte0;

  Cdb->CDB10.TransferBlocksMsb = ((PFOUR_BYTE)&TransferBlocks)->Byte1;
  Cdb->CDB10.TransferBlocksLsb = ((PFOUR_BYTE)&TransferBlocks)->Byte0;


  if (CurrentIrpStack->MajorFunction == IRP_MJ_READ)
    {
      DPRINT1("ScsiClassBuildRequest: Read Command\n");

      Srb->SrbFlags |= SRB_FLAGS_DATA_IN;
      Cdb->CDB10.OperationCode = SCSIOP_READ;
    }
  else
    {
      DPRINT1("ScsiClassBuildRequest: Write Command\n");

      Srb->SrbFlags |= SRB_FLAGS_DATA_OUT;
      Cdb->CDB10.OperationCode = SCSIOP_WRITE;
    }


#if 0
  /* if this is not a write-through request, then allow caching */
  if (!(CurrentIrpStack->Flags & SL_WRITE_THROUGH))
    {
      Srb->SrbFlags |= SRB_FLAGS_ADAPTER_CACHE_ENABLE;
    }
  else
    {
      /* if write caching is enable then force media access in the cdb */
      if (DeviceExtension->DeviceFlags & DEV_WRITE_CACHE)
	{
	  Cdb->CDB10.ForceUnitAccess = TRUE;
	}
    }
#endif

  /* or in the default flags from the device object. */
  Srb->SrbFlags |= DeviceExtension->SrbFlags;


  NextIrpStack->MajorFunction = IRP_MJ_SCSI;
  NextIrpStack->Parameters.Scsi.Srb = Srb;
  NextIrpStack->DeviceObject = DeviceObject;

#if 0
  /* save retry count in current IRP stack */
  CurrentIrpStack->Parameters.Others.Argument4 = (PVOID)MAXIMUM_RETRIES;
#endif

  DPRINT1("IoSetCompletionRoutine (Irp %p  Srb %p)\n", Irp, Srb);
  IoSetCompletionRoutine(Irp,
			 ScsiClassIoComplete,
			 Srb,
			 TRUE,
			 TRUE,
			 TRUE);
}


NTSTATUS STDCALL
ScsiClassClaimDevice(PDEVICE_OBJECT PortDeviceObject,
		     PSCSI_INQUIRY_DATA LunInfo,
		     BOOLEAN Release,
		     PDEVICE_OBJECT *NewPortDeviceObject OPTIONAL)
{
  PIO_STACK_LOCATION IoStack;
  IO_STATUS_BLOCK IoStatusBlock;
  SCSI_REQUEST_BLOCK Srb;
  KEVENT Event;
  PIRP Irp;
  NTSTATUS Status;

  DPRINT("ScsiClassClaimDevice() called\n");

  if (NewPortDeviceObject != NULL)
    *NewPortDeviceObject = NULL;

  /* initialize an SRB */
  RtlZeroMemory(&Srb,
		sizeof(SCSI_REQUEST_BLOCK));
  Srb.Length = SCSI_REQUEST_BLOCK_SIZE;
  Srb.PathId = LunInfo->PathId;
  Srb.TargetId = LunInfo->TargetId;
  Srb.Lun = LunInfo->Lun;
  Srb.Function =
    (Release == TRUE) ? SRB_FUNCTION_RELEASE_DEVICE : SRB_FUNCTION_CLAIM_DEVICE;

  KeInitializeEvent(&Event,
		    NotificationEvent,
		    FALSE);

  Irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_EXECUTE_NONE,
				      PortDeviceObject,
				      NULL,
				      0,
				      NULL,
				      0,
				      TRUE,
				      &Event,
				      &IoStatusBlock);
  if (Irp == NULL)
    {
      DPRINT1("Failed to allocate Irp!\n");
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  /* Link Srb and Irp */
  IoStack = IoGetNextIrpStackLocation(Irp);
  IoStack->Parameters.Scsi.Srb = &Srb;
  Srb.OriginalRequest = Irp;

  /* Call SCSI port driver */
  Status = IoCallDriver(PortDeviceObject,
			Irp);
  if (Status == STATUS_PENDING)
    {
      KeWaitForSingleObject(&Event,
			    Suspended,
			    KernelMode,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  if (Release == TRUE)
    {
      ObDereferenceObject(PortDeviceObject);
      return(STATUS_SUCCESS);
    }

//  Status = ObReferenceObjectByPointer(Srb.DataBuffer,
  Status = ObReferenceObjectByPointer(PortDeviceObject,
				      0,
				      NULL,
				      KernelMode);

  if (NewPortDeviceObject != NULL)
    {
//      *NewPortDeviceObject = Srb.DataBuffer;
      *NewPortDeviceObject = PortDeviceObject;
    }

  return(STATUS_SUCCESS);
}


NTSTATUS STDCALL
ScsiClassCreateDeviceObject(IN PDRIVER_OBJECT DriverObject,
			    IN PCCHAR ObjectNameBuffer,
			    IN PDEVICE_OBJECT PhysicalDeviceObject OPTIONAL,
			    IN OUT PDEVICE_OBJECT *DeviceObject,
			    IN PCLASS_INIT_DATA InitializationData)
{
  PDEVICE_OBJECT InternalDeviceObject;
  PDEVICE_EXTENSION DeviceExtension;
  ANSI_STRING AnsiName;
  UNICODE_STRING DeviceName;
  NTSTATUS Status;

  DPRINT("ScsiClassCreateDeviceObject() called\n");

  *DeviceObject = NULL;

  RtlInitAnsiString(&AnsiName,
		    ObjectNameBuffer);

  Status = RtlAnsiStringToUnicodeString(&DeviceName,
					&AnsiName,
					TRUE);
  if (!NT_SUCCESS(Status))
    {
      return(Status);
    }

  DPRINT("Device name: '%wZ'\n", &DeviceName);

  Status = IoCreateDevice(DriverObject,
			  InitializationData->DeviceExtensionSize,
			  &DeviceName,
			  InitializationData->DeviceType,
			  InitializationData->DeviceCharacteristics,
			  FALSE,
			  &InternalDeviceObject);
  if (NT_SUCCESS(Status))
    {
      DeviceExtension = InternalDeviceObject->DeviceExtension;

      DeviceExtension->ClassError = InitializationData->ClassError;
      DeviceExtension->ClassReadWriteVerification = InitializationData->ClassReadWriteVerification;
      DeviceExtension->ClassFindDevices = InitializationData->ClassFindDevices;
      DeviceExtension->ClassDeviceControl = InitializationData->ClassDeviceControl;
      DeviceExtension->ClassShutdownFlush = InitializationData->ClassShutdownFlush;
      DeviceExtension->ClassCreateClose = InitializationData->ClassCreateClose;
      DeviceExtension->ClassStartIo = InitializationData->ClassStartIo;

      DeviceExtension->MediaChangeCount = 0;

      if (PhysicalDeviceObject != NULL)
	{
	  DeviceExtension->PhysicalDevice = PhysicalDeviceObject;
	}
      else
	{
	  DeviceExtension->PhysicalDevice = InternalDeviceObject;
        }

      *DeviceObject = InternalDeviceObject;
    }

  RtlFreeUnicodeString(&DeviceName);

  return(Status);
}


NTSTATUS STDCALL
ScsiClassDeviceControl(PDEVICE_OBJECT DeviceObject,
		       PIRP Irp)
{
  UNIMPLEMENTED;
}


PVOID STDCALL
ScsiClassFindModePage(PCHAR ModeSenseBuffer,
		      ULONG Length,
		      UCHAR PageMode,
		      BOOLEAN Use6Byte)
{
  UNIMPLEMENTED;
}


ULONG STDCALL
ScsiClassFindUnclaimedDevices(PCLASS_INIT_DATA InitializationData,
			      PSCSI_ADAPTER_BUS_INFO AdapterInformation)
{
  PSCSI_INQUIRY_DATA UnitInfo;
  PINQUIRYDATA InquiryData;
  PUCHAR Buffer;
  ULONG Bus;
  ULONG UnclaimedDevices = 0;
  NTSTATUS Status;

  DPRINT("ScsiClassFindUnclaimedDevices() called!\n");

  DPRINT("NumberOfBuses: %lu\n",AdapterInformation->NumberOfBuses);
  Buffer = (PUCHAR)AdapterInformation;
  for (Bus = 0; Bus < (ULONG)AdapterInformation->NumberOfBuses; Bus++)
    {
      DPRINT("Searching bus %lu\n", Bus);

      UnitInfo = (PSCSI_INQUIRY_DATA)(Buffer + AdapterInformation->BusData[Bus].InquiryDataOffset);

      while (AdapterInformation->BusData[Bus].InquiryDataOffset)
	{
	  InquiryData = (PINQUIRYDATA)UnitInfo->InquiryData;

	  DPRINT("Device: '%.8s'\n", InquiryData->VendorId);

	  if ((InitializationData->ClassFindDeviceCallBack(InquiryData) == TRUE) &&
	      (UnitInfo->DeviceClaimed == FALSE))
	    {
	      UnclaimedDevices++;
	    }

	  if (UnitInfo->NextInquiryDataOffset == 0)
	    break;

	  UnitInfo = (PSCSI_INQUIRY_DATA) (Buffer + UnitInfo->NextInquiryDataOffset);
	}
    }

  return(UnclaimedDevices);
}


NTSTATUS STDCALL
ScsiClassGetCapabilities(PDEVICE_OBJECT PortDeviceObject,
			 PIO_SCSI_CAPABILITIES *PortCapabilities)
{
  PIO_SCSI_CAPABILITIES Buffer;
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status;
  KEVENT Event;
  PIRP Irp;

  *PortCapabilities = NULL;
  Buffer = ExAllocatePool(NonPagedPool, /* FIXME: use paged pool */
			  sizeof(IO_SCSI_CAPABILITIES));
  if (Buffer == NULL)
    {
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  KeInitializeEvent(&Event,
		    NotificationEvent,
		    FALSE);

  Irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_GET_CAPABILITIES,
				      PortDeviceObject,
				      NULL,
				      0,
				      Buffer,
				      sizeof(IO_SCSI_CAPABILITIES),
				      FALSE,
				      &Event,
				      &IoStatusBlock);
  if (Irp == NULL)
    {
      ExFreePool(Buffer);
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  Status = IoCallDriver(PortDeviceObject,
			Irp);
  if (Status == STATUS_PENDING)
    {
      KeWaitForSingleObject(&Event,
			    Suspended,
			    KernelMode,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  if (!NT_SUCCESS(Status))
    {
      ExFreePool(Buffer);
    }
  else
    {
      *PortCapabilities = Buffer;
    }

  return(Status);
}


NTSTATUS STDCALL
ScsiClassGetInquiryData(PDEVICE_OBJECT PortDeviceObject,
			PSCSI_ADAPTER_BUS_INFO *ConfigInfo)
{
  PSCSI_ADAPTER_BUS_INFO Buffer;
  IO_STATUS_BLOCK IoStatusBlock;
  NTSTATUS Status;
  KEVENT Event;
  PIRP Irp;

  *ConfigInfo = NULL;
  Buffer = ExAllocatePool(NonPagedPool,
			  INQUIRY_DATA_SIZE);
  if (Buffer == NULL)
    {
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  KeInitializeEvent(&Event,
		    NotificationEvent,
		    FALSE);

  Irp = IoBuildDeviceIoControlRequest(IOCTL_SCSI_GET_INQUIRY_DATA,
				      PortDeviceObject,
				      NULL,
				      0,
				      Buffer,
				      INQUIRY_DATA_SIZE,
				      FALSE,
				      &Event,
				      &IoStatusBlock);
  if (Irp == NULL)
    {
      ExFreePool(Buffer);
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  Status = IoCallDriver(PortDeviceObject,
			Irp);
  if (Status == STATUS_PENDING)
    {
      KeWaitForSingleObject(&Event,
			    Suspended,
			    KernelMode,
			    FALSE,
			    NULL);
      Status = IoStatusBlock.Status;
    }

  if (!NT_SUCCESS(Status))
    {
      ExFreePool(Buffer);
    }
  else
    {
      *ConfigInfo = Buffer;
    }

  return(Status);
}


ULONG STDCALL
ScsiClassInitialize(PVOID Argument1,
		    PVOID Argument2,
		    PCLASS_INIT_DATA InitializationData)
{
  PCONFIGURATION_INFORMATION ConfigInfo;
  PDRIVER_OBJECT DriverObject = Argument1;
  WCHAR NameBuffer[80];
  UNICODE_STRING PortName;
  ULONG PortNumber;
  PDEVICE_OBJECT PortDeviceObject;
  PFILE_OBJECT FileObject;
  BOOLEAN DiskFound = FALSE;
  NTSTATUS Status;

  DPRINT("ScsiClassInitialize() called!\n");

  DriverObject->MajorFunction[IRP_MJ_CREATE] = ScsiClassCreateClose;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = ScsiClassCreateClose;
  DriverObject->MajorFunction[IRP_MJ_READ] = ScsiClassReadWrite;
  DriverObject->MajorFunction[IRP_MJ_WRITE] = ScsiClassReadWrite;
  DriverObject->MajorFunction[IRP_MJ_SCSI] = ScsiClassScsiDispatch;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ScsiClassDeviceDispatch;
  DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = ScsiClassShutdownFlush;
  DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = ScsiClassShutdownFlush;
  if (InitializationData->ClassStartIo)
    {
      DriverObject->DriverStartIo = InitializationData->ClassStartIo;
    }

  ConfigInfo = IoGetConfigurationInformation();

  /* look for ScsiPortX scsi port devices */
  for (PortNumber = 0; PortNumber < ConfigInfo->ScsiPortCount; PortNumber++)
    {
      swprintf(NameBuffer,
	       L"\\Device\\ScsiPort%lu",
	        PortNumber);
      RtlInitUnicodeString(&PortName,
			   NameBuffer);
      DPRINT("Checking scsi port %ld\n", PortNumber);
      Status = IoGetDeviceObjectPointer(&PortName,
					FILE_READ_ATTRIBUTES,
					&FileObject,
					&PortDeviceObject);
      DPRINT("Status 0x%08lX\n", Status);
      if (NT_SUCCESS(Status))
	{
	  DPRINT("ScsiPort%lu found.\n", PortNumber);

	  /* check scsi port for attached disk drives */
	  if (InitializationData->ClassFindDevices(DriverObject,
						   Argument2,
						   InitializationData,
						   PortDeviceObject,
						   PortNumber))
	    {
	      DiskFound = TRUE;
	    }
	}
    }

  DPRINT1("ScsiClassInitialize() done!\n");

  return((DiskFound == TRUE) ? STATUS_SUCCESS : STATUS_NO_SUCH_DEVICE);
}


VOID STDCALL
ScsiClassInitializeSrbLookasideList(PDEVICE_EXTENSION DeviceExtension,
				    ULONG NumberElements)
{
  UNIMPLEMENTED;
}


NTSTATUS STDCALL
ScsiClassInternalIoControl(PDEVICE_OBJECT DeviceObject,
			   PIRP Irp)
{
  UNIMPLEMENTED;
}


BOOLEAN STDCALL
ScsiClassInterpretSenseInfo(PDEVICE_OBJECT DeviceObject,
			    PSCSI_REQUEST_BLOCK Srb,
			    UCHAR MajorFunctionCode,
			    ULONG IoDeviceCode,
			    ULONG RetryCount,
			    NTSTATUS *Status)
{
  UNIMPLEMENTED;
}


NTSTATUS STDCALL
ScsiClassIoComplete(PDEVICE_OBJECT DeviceObject,
		    PIRP Irp,
		    PVOID Context)
{
  PDEVICE_EXTENSION DeviceExtension;
  PIO_STACK_LOCATION IrpStack;
  PSCSI_REQUEST_BLOCK Srb;
  NTSTATUS Status;

  DPRINT1("ScsiClassIoComplete(DeviceObject %p  Irp %p  Context %p) called\n",
	  DeviceObject, Irp, Context);

  DeviceExtension = DeviceObject->DeviceExtension;
  Srb = (PSCSI_REQUEST_BLOCK)Context;
  DPRINT1("Srb %p\n", Srb);

  IrpStack = IoGetCurrentIrpStackLocation(Irp);

#if 0
  if (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_SUCCESS)
    {
      Status = STATUS_SUCCESS;
    }
  else
    {
      /* FIXME: improve error handling */
      DPRINT1("Srb->SrbStatus %lx\n", Srb->SrbStatus);

      if (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_PENDING)
	{
	  Status = STATUS_SUCCESS;
	}
      else
      Status = STATUS_UNSUCCESSFUL;
    }
#endif

  /* FIXME: use lookaside list instead */
  DPRINT1("Freed SRB %p\n", IrpStack->Parameters.Scsi.Srb);
  ExFreePool(IrpStack->Parameters.Scsi.Srb);

//  Irp->IoStatus.Status = Status;
#if 0
  if (!NT_SUCCESS(Status) &&
      IoIsErrorUserInduced(Status))
    {
      IoSetHardErrorOrVerifyDevice(Irp,
				   DeviceObject);
      Irp->IoStatus.Information = 0;
    }

  if (Irp->PendingReturned)
    {
      IoMarkIrpPending(Irp);
    }

  if (DeviceExtension->ClassStartIo != NULL)
    {
      if (IrpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL)
	{
	  IoStartNextPacket(DeviceObject,
			    FALSE);
	}
    }
#endif

  DPRINT1("ScsiClassIoComplete() done (Status %lx)\n", Status);

//  return(Status);
  return(STATUS_SUCCESS);
}


NTSTATUS STDCALL
ScsiClassIoCompleteAssociated(PDEVICE_OBJECT DeviceObject,
			      PIRP Irp,
			      PVOID Context)
{
  UNIMPLEMENTED;
}


ULONG STDCALL
ScsiClassModeSense(PDEVICE_OBJECT DeviceObject,
		   CHAR ModeSenseBuffer,
		   ULONG Length,
		   UCHAR PageMode)
{
  UNIMPLEMENTED;
}


ULONG STDCALL
ScsiClassQueryTimeOutRegistryValue(IN PUNICODE_STRING RegistryPath)
{
  UNIMPLEMENTED;
}


NTSTATUS STDCALL
ScsiClassReadDriveCapacity(IN PDEVICE_OBJECT DeviceObject)
{
  PDEVICE_EXTENSION DeviceExtension;
  PREAD_CAPACITY_DATA CapacityBuffer;
  SCSI_REQUEST_BLOCK Srb;
  PCDB Cdb;
  NTSTATUS Status;
  ULONG LastSector;
  ULONG SectorSize;

  DPRINT("ScsiClassReadDriveCapacity() called\n");

  DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

  CapacityBuffer = ExAllocatePool(NonPagedPool, //NonPagedPoolCacheAligned,
				  sizeof(READ_CAPACITY_DATA));
  if (CapacityBuffer == NULL)
    {
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  RtlZeroMemory(&Srb, sizeof(SCSI_REQUEST_BLOCK));

  Srb.CdbLength = 10;
  Srb.TimeOutValue = DeviceExtension->TimeOutValue;

  Cdb = (PCDB)Srb.Cdb;
  Cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;


  Status = ScsiClassSendSrbSynchronous(DeviceObject,
				       &Srb,
				       CapacityBuffer,
				       sizeof(READ_CAPACITY_DATA),
				       FALSE);
  DPRINT("Status: %lx\n", Status);
  DPRINT("Srb: %p\n", &Srb);
  if (NT_SUCCESS(Status))
    {
	SectorSize = (((PUCHAR)&CapacityBuffer->BytesPerBlock)[0] << 24) |
		     (((PUCHAR)&CapacityBuffer->BytesPerBlock)[1] << 16) |
		     (((PUCHAR)&CapacityBuffer->BytesPerBlock)[2] << 8) |
		      ((PUCHAR)&CapacityBuffer->BytesPerBlock)[3];


	LastSector = (((PUCHAR)&CapacityBuffer->LogicalBlockAddress)[0] << 24) |
		     (((PUCHAR)&CapacityBuffer->LogicalBlockAddress)[1] << 16) |
		     (((PUCHAR)&CapacityBuffer->LogicalBlockAddress)[2] << 8) |
		      ((PUCHAR)&CapacityBuffer->LogicalBlockAddress)[3];

	DeviceExtension->DiskGeometry->BytesPerSector = SectorSize;

	DeviceExtension->PartitionLength.QuadPart = (LONGLONG)(LastSector + 1);
	WHICH_BIT(DeviceExtension->DiskGeometry->BytesPerSector, DeviceExtension->SectorShift);
	DeviceExtension->PartitionLength.QuadPart =
	  (DeviceExtension->PartitionLength.QuadPart << DeviceExtension->SectorShift);

	if (DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA)
	  {
	    DeviceExtension->DiskGeometry->MediaType = RemovableMedia;
	  }
	else
	  {
	    DeviceExtension->DiskGeometry->MediaType = FixedMedia;
	  }
	DeviceExtension->DiskGeometry->Cylinders.QuadPart = (LONGLONG)((LastSector + 1)/(32 * 64));
	DeviceExtension->DiskGeometry->SectorsPerTrack = 32;
	DeviceExtension->DiskGeometry->TracksPerCylinder = 64;

	DPRINT1("SectorSize: %lu  SectorCount: %lu\n", SectorSize, LastSector + 1);
    }

  ExFreePool(CapacityBuffer);

  DPRINT1("ScsiClassReadDriveCapacity() done\n");

  return(Status);
}


VOID STDCALL
ScsiClassReleaseQueue(IN PDEVICE_OBJECT DeviceObject)
{
  UNIMPLEMENTED;
}


NTSTATUS STDCALL
ScsiClassSendSrbAsynchronous(PDEVICE_OBJECT DeviceObject,
			     PSCSI_REQUEST_BLOCK Srb,
			     PIRP Irp,
			     PVOID BufferAddress,
			     ULONG BufferLength,
			     BOOLEAN WriteToDevice)
{
  UNIMPLEMENTED;
}


NTSTATUS STDCALL
ScsiClassSendSrbSynchronous(PDEVICE_OBJECT DeviceObject,
			    PSCSI_REQUEST_BLOCK Srb,
			    PVOID BufferAddress,
			    ULONG BufferLength,
			    BOOLEAN WriteToDevice)
{
  PDEVICE_EXTENSION DeviceExtension;
  IO_STATUS_BLOCK IoStatusBlock;
  PIO_STACK_LOCATION IoStack;
  ULONG RequestType;
  KEVENT Event;
  PIRP Irp;
  NTSTATUS Status;


  DPRINT("ScsiClassSendSrbSynchronous() called\n");

  DeviceExtension = DeviceObject->DeviceExtension;

  Srb->PathId = DeviceExtension->PathId;
  Srb->TargetId = DeviceExtension->TargetId;
  Srb->Lun = DeviceExtension->Lun;
  Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

  /* FIXME: more srb initialization required? */


  if (BufferAddress == NULL)
    {
        BufferLength = 0;
        RequestType = IOCTL_SCSI_EXECUTE_NONE;
        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }
  else
    {
      if (WriteToDevice == TRUE)
	{
	  RequestType = IOCTL_SCSI_EXECUTE_OUT;
	  Srb->SrbFlags = SRB_FLAGS_DATA_OUT;
	}
      else
	{
	  RequestType = IOCTL_SCSI_EXECUTE_IN;
	  Srb->SrbFlags = SRB_FLAGS_DATA_IN;
	}
    }

  Srb->DataTransferLength = BufferLength;
  Srb->DataBuffer = BufferAddress;


  KeInitializeEvent(&Event,
		    NotificationEvent,
		    FALSE);

  Irp = IoBuildDeviceIoControlRequest(RequestType,
				      DeviceExtension->PortDeviceObject,
				      NULL,
				      0,
				      BufferAddress,
				      BufferLength,
				      TRUE,
				      &Event,
				      &IoStatusBlock);
  if (Irp == NULL)
    {
      DPRINT("IoBuildDeviceIoControlRequest() failed\n");
      return(STATUS_INSUFFICIENT_RESOURCES);
    }

  /* FIXME: more irp initialization required? */


  /* Attach Srb to the Irp */
  IoStack = IoGetNextIrpStackLocation(Irp);
  IoStack->Parameters.Scsi.Srb = Srb;
  Srb->OriginalRequest = Irp;


  /* Call the SCSI port driver */
  Status = IoCallDriver(DeviceExtension->PortDeviceObject,
			Irp);
  if (Status == STATUS_PENDING)
    {
      KeWaitForSingleObject(&Event,
			    Suspended,
			    KernelMode,
			    FALSE,
			    NULL);
    }

  if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS)
    {
      /* FIXME!! */
      DPRINT1("Fix return value!\n");
      Status = STATUS_UNSUCCESSFUL;
    }
  else
    {
      Status = STATUS_SUCCESS;
    }

  DPRINT1("ScsiClassSendSrbSynchronous() done\n");

  return(Status);
}


VOID STDCALL
ScsiClassSplitRequest(PDEVICE_OBJECT DeviceObject,
		      PIRP Irp,
		      ULONG MaximumBytes)
{
  UNIMPLEMENTED;
}


/* INTERNAL FUNCTIONS *******************************************************/

static NTSTATUS STDCALL
ScsiClassCreateClose(IN PDEVICE_OBJECT DeviceObject,
		     IN PIRP Irp)
{
  PDEVICE_EXTENSION DeviceExtension;

  DPRINT("ScsiClassCreateClose() called\n");

  DeviceExtension = DeviceObject->DeviceExtension;

  if (DeviceExtension->ClassCreateClose)
    return(DeviceExtension->ClassCreateClose(DeviceObject,
					     Irp));

  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return(STATUS_SUCCESS);
}


static NTSTATUS STDCALL
ScsiClassReadWrite(IN PDEVICE_OBJECT DeviceObject,
		   IN PIRP Irp)
{
  PDEVICE_EXTENSION DeviceExtension;
  PIO_STACK_LOCATION IrpStack;
  ULONG TransferLength;
  ULONG TransferPages;
  NTSTATUS Status;

  DPRINT1("ScsiClassReadWrite() called\n");

  DeviceExtension = DeviceObject->DeviceExtension;
  IrpStack  = IoGetCurrentIrpStackLocation(Irp);

  DPRINT1("Relative Offset: %I64u  Length: %lu\n",
	 IrpStack->Parameters.Read.ByteOffset.QuadPart,
	 IrpStack->Parameters.Read.Length);

  TransferLength = IrpStack->Parameters.Read.Length;



  if ((DeviceObject->Flags & DO_VERIFY_VOLUME) &&
      !(IrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME))
    {
      IoSetHardErrorOrVerifyDevice(Irp,
				   DeviceObject);

      Irp->IoStatus.Status = STATUS_VERIFY_REQUIRED;
      Irp->IoStatus.Information = 0;

      IoCompleteRequest(Irp,
			IO_NO_INCREMENT);
      return(STATUS_VERIFY_REQUIRED);
    }

#if 0
  /* let the class driver perform its verification */
  Status = DeviceExtension->ClassReadWriteVerification(DeviceObject,
						       Irp);
  if (!NT_SUCCESS(Status))
    {
      IoCompleteRequest(Irp,
			IO_NO_INCREMENT);
      return(Status);
    }
  else if (Status == STATUS_PENDING)
    {
      IoMarkIrpPending(Irp);
      return(STATUS_PENDING);
    }
#endif

  /* Finish a zero-byte transfer. */
  if (TransferLength == 0)
    {
      Irp->IoStatus.Status = STATUS_SUCCESS;
      Irp->IoStatus.Information = 0;
      IoCompleteRequest(Irp,
			IO_NO_INCREMENT);
      return(STATUS_SUCCESS);
    }

  if (DeviceExtension->ClassStartIo != NULL)
    {
      DPRINT1("ScsiClassReadWrite() starting packet\n");

      IoMarkIrpPending(Irp);
      IoStartPacket(DeviceObject,
		    Irp,
		    NULL,
		    NULL);

      return(STATUS_PENDING);
    }

  IoMarkIrpPending(Irp);

  /* Adjust partition-relative starting offset to absolute offset */
  IrpStack->Parameters.Read.ByteOffset.QuadPart += DeviceExtension->StartingOffset.QuadPart;

  /* Calculate number of pages in this transfer. */
  TransferPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(Irp->MdlAddress),
						 IrpStack->Parameters.Read.Length);

#if 0
  if (IrpStack->Parameters.Read.Length > maximumTransferLength ||
      TransferPages > DeviceExtension->PortCapabilities->MaximumPhysicalPages)
    {

    }
#endif

  ScsiClassBuildRequest(DeviceObject,
			Irp);

  DPRINT1("ScsiClassReadWrite() done\n");

  /* Call the port driver */
  return(IoCallDriver(DeviceExtension->PortDeviceObject,
		      Irp));
}


static NTSTATUS STDCALL
ScsiClassScsiDispatch(IN PDEVICE_OBJECT DeviceObject,
		      IN PIRP Irp)
{
  DPRINT1("ScsiClassScsiDispatch() called\n");

  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return(STATUS_SUCCESS);
}


static NTSTATUS STDCALL
ScsiClassDeviceDispatch(IN PDEVICE_OBJECT DeviceObject,
			IN PIRP Irp)
{
  PDEVICE_EXTENSION DeviceExtension;

  DPRINT("ScsiClassDeviceDispatch() called\n");

  DeviceExtension = DeviceObject->DeviceExtension;
  if (DeviceExtension->ClassDeviceControl)
    {
      return(DeviceExtension->ClassDeviceControl(DeviceObject, Irp));
    }

  Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return(STATUS_INVALID_DEVICE_REQUEST);
}


static NTSTATUS STDCALL
ScsiClassShutdownFlush(IN PDEVICE_OBJECT DeviceObject,
		       IN PIRP Irp)
{
  PDEVICE_EXTENSION DeviceExtension;

  DPRINT("ScsiClassShutdownFlush() called\n");

  DeviceExtension = DeviceObject->DeviceExtension;
  if (DeviceExtension->ClassShutdownFlush)
    {
      return(DeviceExtension->ClassShutdownFlush(DeviceObject, Irp));
    }

  Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return(STATUS_INVALID_DEVICE_REQUEST);
}

/* EOF */
