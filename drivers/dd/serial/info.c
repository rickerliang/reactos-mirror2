/* $Id:
 * 
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            drivers/dd/serial/info.c
 * PURPOSE:         Serial IRP_MJ_QUERY_INFORMATION operations
 * 
 * PROGRAMMERS:     Herv� Poussineau (poussine@freesurf.fr)
 */

#define NDEBUG2
#include "serial.h"

NTSTATUS STDCALL
SerialQueryInformation(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp)
{
	PSERIAL_DEVICE_EXTENSION DeviceExtension;
	PIO_STACK_LOCATION Stack;
	PVOID SystemBuffer;
	ULONG BufferLength;
	ULONG_PTR Information = 0;
	NTSTATUS Status;
	
	DeviceExtension = (PSERIAL_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	Stack = IoGetCurrentIrpStackLocation(Irp);
	SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
	BufferLength = Stack->Parameters.QueryFile.Length;
	
	switch (Stack->Parameters.QueryFile.FileInformationClass)
	{
		case FileStandardInformation:
		{
			PFILE_STANDARD_INFORMATION StandardInfo = (PFILE_STANDARD_INFORMATION)SystemBuffer;
			
			DPRINT("Serial: IRP_MJ_QUERY_INFORMATION / FileStandardInformation\n");
			if (BufferLength < sizeof(FILE_STANDARD_INFORMATION))
				Status = STATUS_BUFFER_OVERFLOW;
			else
			{
				StandardInfo->AllocationSize.QuadPart = 0;
				StandardInfo->EndOfFile.QuadPart = 0;
      		StandardInfo->Directory = FALSE;
      		StandardInfo->NumberOfLinks = 0;
  				StandardInfo->DeletePending = FALSE; /* FIXME: should be TRUE sometimes */
  				Status = STATUS_SUCCESS;
			}
			break;
		}
		case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION PositionInfo = (PFILE_POSITION_INFORMATION)SystemBuffer;
			
			DPRINT("Serial: IRP_MJ_QUERY_INFORMATION / FilePositionInformation\n");
			if (BufferLength < sizeof(PFILE_POSITION_INFORMATION))
				Status = STATUS_BUFFER_OVERFLOW;
			else
			{
				PositionInfo->CurrentByteOffset.QuadPart = 0;
  				Status = STATUS_SUCCESS;
			}
			break;
		}
		default:
		{
			DPRINT("Serial: IRP_MJ_QUERY_INFORMATION: Unexpected file information class 0x%02x\n", Stack->Parameters.QueryFile.FileInformationClass);
			return ForwardIrpAndForget(DeviceObject, Irp);
		}
	}
	
	Irp->IoStatus.Information = Information;
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}
