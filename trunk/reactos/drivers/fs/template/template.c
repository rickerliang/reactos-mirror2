/*
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * FILE:             services/fs/template/template.c
 * PURPOSE:          Bare filesystem template
 * PROGRAMMER:       David Welch (welch@mcmail.com)
 * UPDATE HISTORY: 
 */

/* INCLUDES *****************************************************************/

#include <ddk/ntddk.h>

#define NDEBUG
#include <debug.h>

typedef struct
{
   PDEVICE_OBJECT StorageDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

/* GLOBALS *****************************************************************/

static PDRIVER_OBJECT DriverObject;

/* FUNCTIONS ****************************************************************/

NTSTATUS
FsdCloseFile(PDEVICE_EXTENSION DeviceExt,
	     PFILE_OBJECT FileObject)
/*
 * FUNCTION: Closes a file
 */
{
   return(STATUS_SUCCESS);
}

NTSTATUS
FsdOpenFile(PDEVICE_EXTENSION DeviceExt,
	    PFILE_OBJECT FileObject,
	    PWSTR FileName)
/*
 * FUNCTION: Opens a file
 */
{
   return(STATUS_SUCCESS);
}

BOOLEAN
FsdHasFileSystem(PDEVICE_OBJECT DeviceToMount)
/*
 * FUNCTION: Tests if the device contains a filesystem that can be mounted 
 * by this fsd
 */
{
   return(TRUE);
}

NTSTATUS
FsdMountDevice(PDEVICE_EXTENSION DeviceExt,
	       PDEVICE_OBJECT DeviceToMount)
/*
 * FUNCTION: Mounts the device
 */
{
   return(STATUS_SUCCESS);
}


NTSTATUS
FsdReadFile(PDEVICE_EXTENSION DeviceExt,
	    PFILE_OBJECT FileObject,
	    PVOID Buffer,
	    ULONG Length,
	    ULONG Offset)
/*
 * FUNCTION: Reads data from a file
 */
{
   return(STATUS_SUCCESS);
}


NTSTATUS STDCALL
FsdClose(PDEVICE_OBJECT DeviceObject,
	 PIRP Irp)
{
   PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
   PFILE_OBJECT FileObject = Stack->FileObject;
   PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
   NTSTATUS Status;
   
   Status = FsdCloseFile(DeviceExtension,FileObject);

   Irp->IoStatus.Status = Status;
   Irp->IoStatus.Information = 0;
   
   IoCompleteRequest(Irp, IO_NO_INCREMENT);
   return(Status);
}

NTSTATUS STDCALL
FsdCreate(PDEVICE_OBJECT DeviceObject,
	  PIRP Irp)
{
   PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
   PFILE_OBJECT FileObject = Stack->FileObject;
   NTSTATUS Status;
   PDEVICE_EXTENSION DeviceExt;
   
   DeviceExt = DeviceObject->DeviceExtension;
   Status = FsdOpenFile(DeviceExt,FileObject,FileObject->FileName.Buffer);
   
   Irp->IoStatus.Status = Status;
   Irp->IoStatus.Information = 0;
   
   IoCompleteRequest(Irp, IO_NO_INCREMENT);
   return(Status);
}


NTSTATUS STDCALL
FsdWrite(PDEVICE_OBJECT DeviceObject,
	 PIRP Irp)
{
   DPRINT("FsdWrite(DeviceObject %x Irp %x)\n",DeviceObject,Irp);
   
   Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
   Irp->IoStatus.Information = 0;
   return(STATUS_UNSUCCESSFUL);
}

NTSTATUS STDCALL
FsdRead(PDEVICE_OBJECT DeviceObject,
	PIRP Irp)
{
   ULONG Length;
   PVOID Buffer;
   ULONG Offset;
   PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
   PFILE_OBJECT FileObject = Stack->FileObject;
   PDEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
   NTSTATUS Status;
   
   DPRINT("FsdRead(DeviceObject %x, Irp %x)\n",DeviceObject,Irp);
   
   Length = Stack->Parameters.Read.Length;
   Buffer = MmGetSystemAddressForMdl(Irp->MdlAddress);
   Offset = Stack->Parameters.Read.ByteOffset.LowPart;
   
   Status = FsdReadFile(DeviceExt,FileObject,Buffer,Length,Offset);
   
   Irp->IoStatus.Status = Status;
   Irp->IoStatus.Information = Length;
   IoCompleteRequest(Irp,IO_NO_INCREMENT);
   return(Status);
}


NTSTATUS
FsdMount(PDEVICE_OBJECT DeviceToMount)
{
   PDEVICE_OBJECT DeviceObject;
   PDEVICE_EXTENSION DeviceExt;
   
   IoCreateDevice(DriverObject,
		  sizeof(DEVICE_EXTENSION),
		  NULL,
		  FILE_DEVICE_FILE_SYSTEM,
		  0,
		  FALSE,
		  &DeviceObject);
   DeviceObject->Flags = DeviceObject->Flags | DO_DIRECT_IO;
   DeviceExt = (PVOID)DeviceObject->DeviceExtension;
   
   FsdMountDevice(DeviceExt,
		  DeviceToMount);
   
   DeviceExt->StorageDevice = IoAttachDeviceToDeviceStack(DeviceObject,
							  DeviceToMount);
   return(STATUS_SUCCESS);
}

NTSTATUS STDCALL
FsdFileSystemControl(PDEVICE_OBJECT DeviceObject,
		     PIRP Irp)
{
   PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
   PVPB	vpb = Stack->Parameters.Mount.Vpb;
   PDEVICE_OBJECT DeviceToMount = Stack->Parameters.Mount.DeviceObject;
   NTSTATUS Status;
   
   if (FsdHasFileSystem(DeviceToMount))
     {
	Status = FsdMount(DeviceToMount);
     }
   else
     {
	Status = STATUS_UNRECOGNIZED_VOLUME;
     }
   
   Irp->IoStatus.Status = Status;
   Irp->IoStatus.Information = 0;
   
   IoCompleteRequest(Irp, IO_NO_INCREMENT);
   return(Status);
}

NTSTATUS STDCALL
DriverEntry(PDRIVER_OBJECT _DriverObject,
	    PUNICODE_STRING RegistryPath)
/*
 * FUNCTION: Called by the system to initalize the driver
 * ARGUMENTS:
 *           DriverObject = object describing this driver
 *           RegistryPath = path to our configuration entries
 * RETURNS: Success or failure
 */
{
   PDEVICE_OBJECT DeviceObject;
   NTSTATUS Status;
   UNICODE_STRING DeviceName;
   
   DbgPrint("Bare FSD Template 0.0.1\n");
   
   DriverObject = _DriverObject;
   
   RtlInitUnicodeString(&DeviceName,
			L"\\Device\\BareFsd");
   Status = IoCreateDevice(DriverObject,
			   0,
			   &DeviceName,
			   FILE_DEVICE_FILE_SYSTEM,
			   0,
			   FALSE,
			   &DeviceObject);
   if (!NT_SUCCESS(Status))
     {
	return(ret);
     }
   
   DeviceObject->Flags=0;
   DriverObject->MajorFunction[IRP_MJ_CLOSE] = FsdClose;
   DriverObject->MajorFunction[IRP_MJ_CREATE] = FsdCreate;
   DriverObject->MajorFunction[IRP_MJ_READ] = FsdRead;
   DriverObject->MajorFunction[IRP_MJ_WRITE] = FsdWrite;
   DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] =
                      FsdFileSystemControl;
   DriverObject->DriverUnload = NULL;
   
   IoRegisterFileSystem(DeviceObject);
   
   return(STATUS_SUCCESS);
}

