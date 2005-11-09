/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS Serial mouse driver
 * FILE:            drivers/input/sermouse/detect.c
 * PURPOSE:         Detect serial mouse type
 *
 * PROGRAMMERS:     Jason Filby (jasonfilby@yahoo.com)
 *                  Filip Navara (xnavara@volny.cz)
 *                  Herv� Poussineau (hpoussin@reactos.org)
 */

#define NDEBUG
#include <debug.h>

#include "sermouse.h"

/* Most of this file is ripped from reactos/drivers/bus/serenum/detect.c */

static NTSTATUS
SermouseDeviceIoControl(
	IN PDEVICE_OBJECT DeviceObject,
	IN ULONG CtlCode,
	IN PVOID InputBuffer OPTIONAL,
	IN ULONG InputBufferSize,
	IN OUT PVOID OutputBuffer OPTIONAL,
	IN OUT PULONG OutputBufferSize)
{
	KEVENT Event;
	PIRP Irp;
	IO_STATUS_BLOCK IoStatus;
	NTSTATUS Status;

	KeInitializeEvent (&Event, NotificationEvent, FALSE);

	Irp = IoBuildDeviceIoControlRequest(CtlCode,
		DeviceObject,
		InputBuffer,
		InputBufferSize,
		OutputBuffer,
		(OutputBufferSize) ? *OutputBufferSize : 0,
		FALSE,
		&Event,
		&IoStatus);
	if (Irp == NULL)
	{
		DPRINT("IoBuildDeviceIoControlRequest() failed\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = IoCallDriver(DeviceObject, Irp);

	if (Status == STATUS_PENDING)
	{
		DPRINT("Operation pending\n");
		KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
		Status = IoStatus.Status;
	}

	if (OutputBufferSize)
	{
		*OutputBufferSize = IoStatus.Information;
	}

	return Status;
}

static NTSTATUS
SermouseSendIrp(
	IN PDEVICE_OBJECT DeviceObject,
	IN ULONG MajorFunction)
{
	KEVENT Event;
	PIRP Irp;
	IO_STATUS_BLOCK IoStatus;
	NTSTATUS Status;

	KeInitializeEvent(&Event, NotificationEvent, FALSE);

	Irp = IoBuildSynchronousFsdRequest(
		MajorFunction,
		DeviceObject,
		NULL,
		0,
		NULL,
		&Event,
		&IoStatus);
	if (Irp == NULL)
	{
		DPRINT("IoBuildSynchronousFsdRequest() failed\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = IoCallDriver(DeviceObject, Irp);

	if (Status == STATUS_PENDING)
	{
		DPRINT("Operation pending\n");
		KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
		Status = IoStatus.Status;
	}

	return Status;
}

static NTSTATUS
ReadBytes(
	IN PDEVICE_OBJECT LowerDevice,
	OUT PUCHAR Buffer,
	IN ULONG BufferSize,
	OUT PULONG FilledBytes)
{
	PIRP Irp;
	IO_STATUS_BLOCK ioStatus;
	KEVENT event;
	LARGE_INTEGER zero;
	NTSTATUS Status;

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	zero.QuadPart = 0;
	Irp = IoBuildSynchronousFsdRequest(
		IRP_MJ_READ,
		LowerDevice,
		Buffer, BufferSize,
		&zero,
		&event,
		&ioStatus);
	if (!Irp)
		return FALSE;

	Status = IoCallDriver(LowerDevice, Irp);
	if (Status == STATUS_PENDING)
	{
		KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
		Status = ioStatus.Status;
	}
	DPRINT("Bytes received: %lu/%lu\n",
		ioStatus.Information, BufferSize);
	*FilledBytes = ioStatus.Information;
	return Status;
}

static NTSTATUS
SermouseWait(ULONG milliseconds)
{
	KTIMER Timer;
	LARGE_INTEGER DueTime;

	DueTime.QuadPart = milliseconds * -10;
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, DueTime, NULL);
	return KeWaitForSingleObject(&Timer, Executive, KernelMode, FALSE, NULL);
}

SERMOUSE_MOUSE_TYPE
SermouseDetectLegacyDevice(
	IN PDEVICE_OBJECT LowerDevice)
{
	ULONG Fcr, Mcr;
	ULONG BaudRate;
	ULONG Command;
	SERIAL_TIMEOUTS Timeouts;
	SERIAL_LINE_CONTROL LCR;
	ULONG i, Count = 0;
	UCHAR Buffer[16];
	SERMOUSE_MOUSE_TYPE MouseType = mtNone;
	NTSTATUS Status;

	RtlZeroMemory(Buffer, sizeof(Buffer));

	/* Open port */
	Status = SermouseSendIrp(LowerDevice, IRP_MJ_CREATE);
	if (!NT_SUCCESS(Status)) return mtNone;

	/* Reset UART */
	CHECKPOINT;
	Mcr = 0; /* MCR: DTR/RTS/OUT2 off */
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_MODEM_CONTROL,
		&Mcr, sizeof(Mcr), NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;

	/* Set communications parameters */
	CHECKPOINT;
	/* DLAB off */
	Fcr = 0;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_FIFO_CONTROL,
		&Fcr, sizeof(Fcr), NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;
	/* Set serial port speed */
	BaudRate = SERIAL_BAUD_1200;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_BAUD_RATE,
		&BaudRate, sizeof(BaudRate), NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;
	/* Set LCR */
	LCR.WordLength = 7;
	LCR.Parity = NO_PARITY;
	LCR.StopBits = STOP_BITS_2;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_LINE_CONTROL,
		&LCR, sizeof(LCR), NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;

	/* Disable DTR/RTS */
	CHECKPOINT;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_CLR_DTR,
		NULL, 0, NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_CLR_RTS,
		NULL, 0, NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;

	/* Flush receive buffer */
	CHECKPOINT;
	Command = SERIAL_PURGE_RXCLEAR;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_MODEM_CONTROL,
		&Command, sizeof(Command), NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;
	/* Wait 100 ms */
	SermouseWait(100);

	/* Enable DTR/RTS */
	CHECKPOINT;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_DTR,
		NULL, 0, NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;
	SermouseWait(200);
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_RTS,
		NULL, 0, NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;

	/* Set timeout to 500 microseconds */
	CHECKPOINT;
	Timeouts.ReadIntervalTimeout = 0;
	Timeouts.ReadTotalTimeoutMultiplier = 0;
	Timeouts.ReadTotalTimeoutConstant = 500;
	Timeouts.WriteTotalTimeoutMultiplier = Timeouts.WriteTotalTimeoutConstant = 0;
	Status = SermouseDeviceIoControl(LowerDevice, IOCTL_SERIAL_SET_TIMEOUTS,
		&Timeouts, sizeof(Timeouts), NULL, NULL);
	if (!NT_SUCCESS(Status)) goto ByeBye;

	/* Fill the read buffer */
	CHECKPOINT;
	Status = ReadBytes(LowerDevice, Buffer, sizeof(Buffer)/sizeof(Buffer[0]), &Count);
	if (!NT_SUCCESS(Status)) goto ByeBye;

	for (i = 0; i < Count; i++)
	{
		if (Buffer[i] == 'B')
		{
			/* Sign for Microsoft Ballpoint */
			DPRINT1("Microsoft Ballpoint device detected. THIS DEVICE IS NOT YET SUPPORTED");
			MouseType = mtNone;
			goto ByeBye;
		}
		else if (Buffer[i] == 'M')
		{
			/* Sign for Microsoft Mouse protocol followed by button specifier */
			if (i == sizeof(Buffer) - 1)
			{
				/* Overflow Error */
				goto ByeBye;
			}
			switch (Buffer[i + 1])
			{
				case '3':
					DPRINT("Microsoft Mouse with 3-buttons detected\n");
					MouseType = mtLogitech;
				case 'Z':
					DPRINT("Microsoft Wheel Mouse detected\n");
					MouseType = mtWheelZ;
				default:
					DPRINT("Microsoft Mouse with 2-buttons detected\n");
					MouseType = mtMicrosoft;
			}
			goto ByeBye;
		}
	}

ByeBye:
	/* Close port */
	SermouseSendIrp(LowerDevice, IRP_MJ_CLOSE);
	SermouseSendIrp(LowerDevice, IRP_MJ_CLEANUP);
	return MouseType;
}
