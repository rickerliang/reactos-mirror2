#include <ntifs.h>
#include <kbdmou.h>
#include <ntddkbd.h>
#include <stdio.h>

#define MAX_PATH 260

typedef enum
{
	dsStopped,
	dsStarted,
	dsPaused,
	dsRemoved,
	dsSurpriseRemoved
} PORT_DEVICE_STATE;

typedef struct _CLASS_DRIVER_EXTENSION
{
	UNICODE_STRING RegistryPath;

	/* Registry settings */
	ULONG ConnectMultiplePorts;
	ULONG DataQueueSize;
	UNICODE_STRING DeviceBaseName;

	PDEVICE_OBJECT MainClassDeviceObject;
} CLASS_DRIVER_EXTENSION, *PCLASS_DRIVER_EXTENSION;

typedef struct _COMMON_DEVICE_EXTENSION
{
	BOOLEAN IsClassDO;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef struct _PORT_DEVICE_EXTENSION
{
	COMMON_DEVICE_EXTENSION Common;

	PORT_DEVICE_STATE PnpState;
	PDEVICE_OBJECT LowerDevice;
	UNICODE_STRING InterfaceName;
} PORT_DEVICE_EXTENSION, *PPORT_DEVICE_EXTENSION;

typedef struct _CLASS_DEVICE_EXTENSION
{
	COMMON_DEVICE_EXTENSION Common;

	PCLASS_DRIVER_EXTENSION DriverExtension;

	KSPIN_LOCK SpinLock;
	BOOLEAN ReadIsPending;
	ULONG InputCount;
	PKEYBOARD_INPUT_DATA PortData;
} CLASS_DEVICE_EXTENSION, *PCLASS_DEVICE_EXTENSION;

/* misc.c */

NTSTATUS NTAPI
ForwardIrpAndForget(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp);
