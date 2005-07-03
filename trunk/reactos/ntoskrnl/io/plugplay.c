/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            ntoskrnl/io/plugplay.c
 * PURPOSE:         Plug-and-play interface routines
 *
 * PROGRAMMERS:     Eric Kohl <eric.kohl@t-online.de>
 */

/* INCLUDES *****************************************************************/

#include <ntoskrnl.h>

#define NDEBUG
#include <internal/debug.h>


typedef struct _PNP_EVENT_ENTRY
{
  LIST_ENTRY ListEntry;
  PLUGPLAY_EVENT_BLOCK Event;
} PNP_EVENT_ENTRY, *PPNP_EVENT_ENTRY;


/* GLOBALS *******************************************************************/

static LIST_ENTRY IopPnpEventQueueHead;
static KEVENT IopPnpNotifyEvent;

/* FUNCTIONS *****************************************************************/

NTSTATUS INIT_FUNCTION
IopInitPlugPlayEvents(VOID)
{
    InitializeListHead(&IopPnpEventQueueHead);

    KeInitializeEvent(&IopPnpNotifyEvent,
                      SynchronizationEvent,
                      FALSE);

    return STATUS_SUCCESS;
}


NTSTATUS
IopQueueTargetDeviceEvent(const GUID *Guid,
                          PUNICODE_STRING DeviceIds)
{
    PPNP_EVENT_ENTRY EventEntry;
    DWORD TotalSize;

    TotalSize =
        FIELD_OFFSET(PLUGPLAY_EVENT_BLOCK, TargetDevice.DeviceIds) +
        DeviceIds->MaximumLength;

    EventEntry = ExAllocatePool(NonPagedPool,
                                TotalSize + FIELD_OFFSET(PNP_EVENT_ENTRY, Event));
    if (EventEntry == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

    memcpy(&EventEntry->Event.EventGuid,
           Guid,
           sizeof(GUID));
    EventEntry->Event.EventCategory = TargetDeviceChangeEvent;
    EventEntry->Event.TotalSize = TotalSize;

    memcpy(&EventEntry->Event.TargetDevice.DeviceIds,
           DeviceIds->Buffer,
           DeviceIds->MaximumLength);

    InsertHeadList(&IopPnpEventQueueHead,
                   &EventEntry->ListEntry);
    KeSetEvent(&IopPnpNotifyEvent,
               0,
               FALSE);

    return STATUS_SUCCESS;
}


/*
 * Remove the current PnP event from the tail of the event queue
 * and signal IopPnpNotifyEvent if there is yet another event in the queue.
 */
static NTSTATUS
IopRemovePlugPlayEvent(VOID)
{
  /* Remove a pnp event entry from the tail of the queue */
  if (!IsListEmpty(&IopPnpEventQueueHead))
  {
    ExFreePool(RemoveTailList(&IopPnpEventQueueHead));
  }

  /* Signal the next pnp event in the queue */
  if (!IsListEmpty(&IopPnpEventQueueHead))
  {
    KeSetEvent(&IopPnpNotifyEvent,
               0,
               FALSE);
  }

  return STATUS_SUCCESS;
}


/*
 * Plug and Play event structure used by NtGetPlugPlayEvent.
 *
 * EventGuid
 *    Can be one of the following values:
 *       GUID_HWPROFILE_QUERY_CHANGE
 *       GUID_HWPROFILE_CHANGE_CANCELLED
 *       GUID_HWPROFILE_CHANGE_COMPLETE
 *       GUID_TARGET_DEVICE_QUERY_REMOVE
 *       GUID_TARGET_DEVICE_REMOVE_CANCELLED
 *       GUID_TARGET_DEVICE_REMOVE_COMPLETE
 *       GUID_PNP_CUSTOM_NOTIFICATION
 *       GUID_PNP_POWER_NOTIFICATION
 *       GUID_DEVICE_* (see above)
 *
 * EventCategory
 *    Type of the event that happened.
 *
 * Result
 *    ?
 *
 * Flags
 *    ?
 *
 * TotalSize
 *    Size of the event block including the device IDs and other
 *    per category specific fields.
 */
/*
 * NtGetPlugPlayEvent
 *
 * Returns one Plug & Play event from a global queue.
 *
 * Parameters
 *    Reserved1
 *    Reserved2
 *       Always set to zero.
 *
 *    Buffer
 *       The buffer that will be filled with the event information on
 *       successful return from the function.
 *
 *    BufferSize
 *       Size of the buffer pointed by the Buffer parameter. If the
 *       buffer size is not large enough to hold the whole event
 *       information, error STATUS_BUFFER_TOO_SMALL is returned and
 *       the buffer remains untouched.
 *
 * Return Values
 *    STATUS_PRIVILEGE_NOT_HELD
 *    STATUS_BUFFER_TOO_SMALL
 *    STATUS_SUCCESS
 *
 * Remarks
 *    This function isn't multi-thread safe!
 *
 * @implemented
 */
NTSTATUS STDCALL
NtGetPlugPlayEvent(IN ULONG Reserved1,
                   IN ULONG Reserved2,
                   OUT PPLUGPLAY_EVENT_BLOCK Buffer,
                   IN ULONG BufferSize)
{
  PPNP_EVENT_ENTRY Entry;
  NTSTATUS Status;

  DPRINT("NtGetPlugPlayEvent() called\n");

  /* Function can only be called from user-mode */
  if (KeGetPreviousMode() != UserMode)
  {
    DPRINT1("NtGetPlugPlayEvent cannot be called from kernel mode!\n");
    return STATUS_ACCESS_DENIED;
  }

  /* Check for Tcb privilege */
  if (!SeSinglePrivilegeCheck(SeTcbPrivilege,
                              UserMode))
  {
    DPRINT1("NtGetPlugPlayEvent: Caller does not hold the SeTcbPrivilege privilege!\n");
    return STATUS_PRIVILEGE_NOT_HELD;
  }

  /* Wait for a PnP event */
  DPRINT("Waiting for pnp notification event\n");
  Status = KeWaitForSingleObject(&IopPnpNotifyEvent,
                                 UserRequest,
                                 KernelMode,
                                 FALSE,
                                 NULL);
  if (!NT_SUCCESS(Status))
  {
    DPRINT1("KeWaitForSingleObject() failed (Status %lx)\n", Status);
    return Status;
  }

  /* Get entry from the tail of the queue */
  Entry = CONTAINING_RECORD(IopPnpEventQueueHead.Blink,
                            PNP_EVENT_ENTRY,
                            ListEntry);

  /* Check the buffer size */
  if (BufferSize < Entry->Event.TotalSize)
  {
    DPRINT1("Buffer is too small for the pnp-event\n");
    return STATUS_BUFFER_TOO_SMALL;
  }

  /* Copy event data to the user buffer */
  memcpy(Buffer,
         &Entry->Event,
         Entry->Event.TotalSize);

  DPRINT("NtGetPlugPlayEvent() done\n");

  return STATUS_SUCCESS;
}


static PDEVICE_OBJECT
IopGetDeviceObjectFromDeviceInstance(PUNICODE_STRING DeviceInstance)
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING KeyName, ValueName;
    LPWSTR KeyNameBuffer;
    HANDLE InstanceKeyHandle;
    HANDLE ControlKeyHandle;
    NTSTATUS Status;
    PKEY_VALUE_PARTIAL_INFORMATION ValueInformation;
    ULONG ValueInformationLength;
    PDEVICE_OBJECT DeviceObject = NULL;

    DPRINT("IopGetDeviceObjectFromDeviceInstance(%wZ) called\n", DeviceInstance);

    KeyNameBuffer = ExAllocatePool(PagedPool,
                                   (49 * sizeof(WCHAR)) + DeviceInstance->Length);
    if (KeyNameBuffer == NULL)
    {
        DPRINT1("Failed to allocate key name buffer!\n");
        return NULL;
    }

    wcscpy(KeyNameBuffer, L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\");
    wcscat(KeyNameBuffer, DeviceInstance->Buffer);

    RtlInitUnicodeString(&KeyName,
                         KeyNameBuffer);
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&InstanceKeyHandle,
                       KEY_READ,
                       &ObjectAttributes);
    ExFreePool(KeyNameBuffer);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to open the instance key (Status %lx)\n", Status);
        return NULL;
    }

    /* Open the 'Control' subkey */
    RtlInitUnicodeString(&KeyName,
                         L"Control");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               InstanceKeyHandle,
                               NULL);

    Status = ZwOpenKey(&ControlKeyHandle,
                       KEY_READ,
                       &ObjectAttributes);
    ZwClose(InstanceKeyHandle);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to open the 'Control' key (Status %lx)\n", Status);
        return NULL;
    }

    /* Query the 'DeviceReference' value */
    RtlInitUnicodeString(&ValueName,
                         L"DeviceReference");
    ValueInformationLength = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION,
                             Data[0]) + sizeof(ULONG);
    ValueInformation = ExAllocatePool(PagedPool, ValueInformationLength);
    if (ValueInformation == NULL)
    {
        DPRINT1("Failed to allocate the name information buffer!\n");
        ZwClose(ControlKeyHandle);
        return NULL;
    }

    Status = ZwQueryValueKey(ControlKeyHandle,
                             &ValueName,
                             KeyValuePartialInformation,
                             ValueInformation,
                             ValueInformationLength,
                             &ValueInformationLength);
    ZwClose(ControlKeyHandle);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to open the 'Control' key (Status %lx)\n", Status);
        return NULL;
    }

    /* Check the device object */
    RtlCopyMemory(&DeviceObject,
                  ValueInformation->Data,
                  sizeof(PDEVICE_OBJECT));

    DPRINT("DeviceObject: %p\n", DeviceObject);

    if (DeviceObject->Type != IO_TYPE_DEVICE ||
        DeviceObject->DeviceObjectExtension == NULL ||
        DeviceObject->DeviceObjectExtension->DeviceNode == NULL ||
        !RtlEqualUnicodeString(&DeviceObject->DeviceObjectExtension->DeviceNode->InstancePath,
                               DeviceInstance, TRUE))
    {
        DPRINT1("Invalid object type!\n");
        return NULL;
    }

    DPRINT("Instance path: %wZ\n", &DeviceObject->DeviceObjectExtension->DeviceNode->InstancePath);

    ObReferenceObject(DeviceObject);

    DPRINT("IopGetDeviceObjectFromDeviceInstance() done\n");

    return DeviceObject;
}


static NTSTATUS
IopGetDeviceProperty(PPLUGPLAY_CONTROL_PROPERTY_DATA PropertyData)
{
    PDEVICE_OBJECT DeviceObject = NULL;
    NTSTATUS Status;

    DPRINT("IopGetDeviceProperty() called\n");
    DPRINT("Device name: %wZ\n", &PropertyData->DeviceInstance);

    /* Get the device object */
    DeviceObject = IopGetDeviceObjectFromDeviceInstance(&PropertyData->DeviceInstance);
    if (DeviceObject == NULL)
        return STATUS_NO_SUCH_DEVICE;

    Status = IoGetDeviceProperty(DeviceObject,
                                 PropertyData->Property,
                                 PropertyData->BufferSize,
                                 PropertyData->Buffer,
                                 &PropertyData->BufferSize);

    ObDereferenceObject(DeviceObject);

    return Status;
}


static NTSTATUS
IopGetRelatedDevice(PPLUGPLAY_CONTROL_RELATED_DEVICE_DATA RelatedDeviceData)
{
    UNICODE_STRING RootDeviceName;
    PDEVICE_OBJECT DeviceObject = NULL;
    PDEVICE_NODE DeviceNode = NULL;
    PDEVICE_NODE RelatedDeviceNode;

    DPRINT("IopGetRelatedDevice() called\n");
    DPRINT("Device name: %wZ\n", &RelatedDeviceData->TargetDeviceInstance);

    RtlInitUnicodeString(&RootDeviceName,
                         L"HTREE\\ROOT\\0");
    if (RtlEqualUnicodeString(&RelatedDeviceData->TargetDeviceInstance,
                              &RootDeviceName,
                              TRUE))
    {
        DeviceNode = IopRootDeviceNode;
    }
    else
    {
        /* Get the device object */
        DeviceObject = IopGetDeviceObjectFromDeviceInstance(&RelatedDeviceData->TargetDeviceInstance);
        if (DeviceObject == NULL)
            return STATUS_NO_SUCH_DEVICE;

        DeviceNode = DeviceObject->DeviceObjectExtension->DeviceNode;
    }

    switch (RelatedDeviceData->Relation)
    {
        case PNP_GET_PARENT_DEVICE:
            RelatedDeviceNode = DeviceNode->Parent;
            break;

        case PNP_GET_CHILD_DEVICE:
            RelatedDeviceNode = DeviceNode->Child;
            break;

        case PNP_GET_SIBLING_DEVICE:
            RelatedDeviceNode = DeviceNode->NextSibling;
            break;

        default:
            if (DeviceObject != NULL)
            {
                ObDereferenceObject(DeviceObject);
            }

            return STATUS_INVALID_PARAMETER;
    }

    if (RelatedDeviceNode == NULL)
    {
        if (DeviceObject)
        {
            ObDereferenceObject(DeviceObject);
        }

        return STATUS_NO_SUCH_DEVICE;
    }

    if (RelatedDeviceNode->InstancePath.Length >
        RelatedDeviceData->RelatedDeviceInstance.MaximumLength)
    {
        if (DeviceObject)
        {
            ObDereferenceObject(DeviceObject);
        }

        return STATUS_BUFFER_TOO_SMALL;
    }

    /* Copy related device instance name */
    RtlCopyMemory(RelatedDeviceData->RelatedDeviceInstance.Buffer,
                  RelatedDeviceNode->InstancePath.Buffer,
                  RelatedDeviceNode->InstancePath.Length);
    RelatedDeviceData->RelatedDeviceInstance.Length =
        RelatedDeviceNode->InstancePath.Length;

    if (DeviceObject != NULL)
    {
        ObDereferenceObject(DeviceObject);
    }

    DPRINT("IopGetRelatedDevice() done\n");

    return STATUS_SUCCESS;
}


static NTSTATUS
IopDeviceStatus(PPLUGPLAY_CONTROL_STATUS_DATA StatusData)
{
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_NODE DeviceNode;

    DPRINT("IopDeviceStatus() called\n");
    DPRINT("Device name: %wZ\n", &StatusData->DeviceInstance);

    /* Get the device object */
    DeviceObject = IopGetDeviceObjectFromDeviceInstance(&StatusData->DeviceInstance);
    if (DeviceObject == NULL)
        return STATUS_NO_SUCH_DEVICE;

    DeviceNode = DeviceObject->DeviceObjectExtension->DeviceNode;

    switch (StatusData->Operation)
    {
        case PNP_GET_DEVICE_STATUS:
            DPRINT("Get status data\n");
            StatusData->DeviceStatus = DeviceNode->Flags;
            StatusData->DeviceProblem = DeviceNode->Problem;
            break;

        case PNP_SET_DEVICE_STATUS:
            DPRINT("Set status data\n");
            DeviceNode->Flags = StatusData->DeviceStatus;
            DeviceNode->Problem = StatusData->DeviceProblem;
            break;

        case PNP_CLEAR_DEVICE_STATUS:
            DPRINT1("FIXME: Clear status data!\n");
            break;
    }

    ObDereferenceObject(DeviceObject);

    return STATUS_SUCCESS;
}


static NTSTATUS
IopGetDeviceDepth(PPLUGPLAY_CONTROL_DEPTH_DATA DepthData)
{
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_NODE DeviceNode;

    DPRINT("IopGetDeviceDepth() called\n");
    DPRINT("Device name: %wZ\n", &DepthData->DeviceInstance);

    /* Get the device object */
    DeviceObject = IopGetDeviceObjectFromDeviceInstance(&DepthData->DeviceInstance);
    if (DeviceObject == NULL)
        return STATUS_NO_SUCH_DEVICE;

    DeviceNode = DeviceObject->DeviceObjectExtension->DeviceNode;

    DepthData->Depth = DeviceNode->Level;

    ObDereferenceObject(DeviceObject);

    return STATUS_SUCCESS;
}


/*
 * NtPlugPlayControl
 *
 * A function for doing various Plug & Play operations from user mode.
 *
 * Parameters
 *    PlugPlayControlClass
 *       0x00   Reenumerate device tree
 *
 *              Buffer points to UNICODE_STRING decribing the instance
 *              path (like "HTREE\ROOT\0" or "Root\ACPI_HAL\0000"). For
 *              more information about instance paths see !devnode command
 *              in kernel debugger or look at "Inside Windows 2000" book,
 *              chapter "Driver Loading, Initialization, and Installation".
 *
 *       0x01   Register new device
 *       0x02   Deregister device
 *       0x03   Initialize device
 *       0x04   Start device
 *       0x06   Query and remove device
 *       0x07   User response
 *
 *              Called after processing the message from NtGetPlugPlayEvent.
 *
 *       0x08   Generate legacy device
 *       0x09   Get interface device list
 *       0x0A   Get property data
 *       0x0B   Device class association (Registration)
 *       0x0C   Get related device
 *       0x0D   Get device interface alias
 *       0x0E   Get/set/clear device status
 *       0x0F   Get device depth
 *       0x10   Query device relations
 *       0x11   Query target device relation
 *       0x12   Query conflict list
 *       0x13   Retrieve dock data
 *       0x14   Reset device
 *       0x15   Halt device
 *       0x16   Get blocked driver data
 *
 *    Buffer
 *       The buffer contains information that is specific to each control
 *       code. The buffer is read-only.
 *
 *    BufferSize
 *       Size of the buffer pointed by the Buffer parameter. If the
 *       buffer size specifies incorrect value for specified control
 *       code, error ??? is returned.
 *
 * Return Values
 *    STATUS_PRIVILEGE_NOT_HELD
 *    STATUS_SUCCESS
 *    ...
 *
 * @unimplemented
 */
NTSTATUS STDCALL
NtPlugPlayControl(IN PLUGPLAY_CONTROL_CLASS PlugPlayControlClass,
                  IN OUT PVOID Buffer,
                  IN ULONG BufferLength)
{
    NTSTATUS Status = STATUS_SUCCESS;

    DPRINT("NtPlugPlayControl(%lu %p %lu) called\n",
           PlugPlayControlClass, Buffer, BufferLength);

    /* Function can only be called from user-mode */
    if (KeGetPreviousMode() != UserMode)
    {
        DPRINT1("NtGetPlugPlayEvent cannot be called from kernel mode!\n");
        return STATUS_ACCESS_DENIED;
    }

    /* Check for Tcb privilege */
    if (!SeSinglePrivilegeCheck(SeTcbPrivilege,
                                UserMode))
    {
        DPRINT1("NtGetPlugPlayEvent: Caller does not hold the SeTcbPrivilege privilege!\n");
        return STATUS_PRIVILEGE_NOT_HELD;
    }

    /* Probe the buffer */
    _SEH_TRY
    {
        ProbeForWrite(Buffer,
                      BufferLength,
                      sizeof(ULONG));
    }
    _SEH_HANDLE
    {
        Status = _SEH_GetExceptionCode();
    }
    _SEH_END;

    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    switch (PlugPlayControlClass)
    {
        case PlugPlayControlUserResponse:
            if (Buffer || BufferLength != 0)
                return STATUS_INVALID_PARAMETER;
            return IopRemovePlugPlayEvent();

        case PlugPlayControlProperty:
            if (!Buffer || BufferLength < sizeof(PLUGPLAY_CONTROL_PROPERTY_DATA))
                return STATUS_INVALID_PARAMETER;
            return IopGetDeviceProperty((PPLUGPLAY_CONTROL_PROPERTY_DATA)Buffer);

        case PlugPlayControlGetRelatedDevice:
            if (!Buffer || BufferLength < sizeof(PLUGPLAY_CONTROL_RELATED_DEVICE_DATA))
                return STATUS_INVALID_PARAMETER;
            return IopGetRelatedDevice((PPLUGPLAY_CONTROL_RELATED_DEVICE_DATA)Buffer);

        case PlugPlayControlDeviceStatus:
            if (!Buffer || BufferLength < sizeof(PLUGPLAY_CONTROL_STATUS_DATA))
                return STATUS_INVALID_PARAMETER;
            return IopDeviceStatus((PPLUGPLAY_CONTROL_STATUS_DATA)Buffer);

        case PlugPlayControlGetDeviceDepth:
            if (!Buffer || BufferLength < sizeof(PLUGPLAY_CONTROL_DEPTH_DATA))
                return STATUS_INVALID_PARAMETER;
            return IopGetDeviceDepth((PPLUGPLAY_CONTROL_DEPTH_DATA)Buffer);

        default:
            return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_NOT_IMPLEMENTED;
}

/* EOF */
