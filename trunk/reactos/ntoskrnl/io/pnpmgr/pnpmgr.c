/*
 * PROJECT:         ReactOS Kernel
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * FILE:            ntoskrnl/io/pnpmgr/pnpmgr.c
 * PURPOSE:         Initializes the PnP manager
 * PROGRAMMERS:     Casper S. Hornstrup (chorns@users.sourceforge.net)
 *                  Copyright 2007 Herv� Poussineau (hpoussin@reactos.org)
 */

/* INCLUDES ******************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <debug.h>

//#define ENABLE_ACPI

/* GLOBALS *******************************************************************/

PDEVICE_NODE IopRootDeviceNode;
KSPIN_LOCK IopDeviceTreeLock;
ERESOURCE PpRegistryDeviceResource;
KGUARDED_MUTEX PpDeviceReferenceTableLock;
RTL_AVL_TABLE PpDeviceReferenceTable;

extern ULONG ExpInitializationPhase;
extern BOOLEAN PnpSystemInit;

/* DATA **********************************************************************/

PDRIVER_OBJECT IopRootDriverObject;
FAST_MUTEX IopBusTypeGuidListLock;
PIO_BUS_TYPE_GUID_LIST IopBusTypeGuidList = NULL;

#if defined (ALLOC_PRAGMA)
#pragma alloc_text(INIT, PnpInit)
#pragma alloc_text(INIT, PnpInit2)
#endif

typedef struct _INVALIDATE_DEVICE_RELATION_DATA
{
    PDEVICE_OBJECT DeviceObject;
    DEVICE_RELATION_TYPE Type;
    PIO_WORKITEM WorkItem;
} INVALIDATE_DEVICE_RELATION_DATA, *PINVALIDATE_DEVICE_RELATION_DATA;

/* FUNCTIONS *****************************************************************/

NTSTATUS
IopAssignDeviceResources(
   IN PDEVICE_NODE DeviceNode,
   OUT ULONG *pRequiredSize);

NTSTATUS
IopTranslateDeviceResources(
   IN PDEVICE_NODE DeviceNode,
   IN ULONG RequiredSize);

NTSTATUS
IopUpdateResourceMapForPnPDevice(
   IN PDEVICE_NODE DeviceNode);

NTSTATUS
NTAPI
IopCreateDeviceKeyPath(IN PCUNICODE_STRING RegistryPath,
                       IN ULONG CreateOptions,
                       OUT PHANDLE Handle);

PDEVICE_NODE
FASTCALL
IopGetDeviceNode(PDEVICE_OBJECT DeviceObject)
{
   return ((PEXTENDED_DEVOBJ_EXTENSION)DeviceObject->DeviceObjectExtension)->DeviceNode;
}

NTSTATUS
FASTCALL
IopInitializeDevice(PDEVICE_NODE DeviceNode,
                    PDRIVER_OBJECT DriverObject)
{
   PDEVICE_OBJECT Fdo;
   NTSTATUS Status;

   if (!DriverObject->DriverExtension->AddDevice)
      return STATUS_SUCCESS;

   /* This is a Plug and Play driver */
   DPRINT("Plug and Play driver found\n");
   ASSERT(DeviceNode->PhysicalDeviceObject);

   /* Check if this plug-and-play driver is used as a legacy one for this device node */
   if (IopDeviceNodeHasFlag(DeviceNode, DNF_LEGACY_DRIVER))
   {
      IopDeviceNodeSetFlag(DeviceNode, DNF_ADDED);
      return STATUS_SUCCESS;
   }

   DPRINT("Calling %wZ->AddDevice(%wZ)\n",
      &DriverObject->DriverName,
      &DeviceNode->InstancePath);
   Status = DriverObject->DriverExtension->AddDevice(
      DriverObject, DeviceNode->PhysicalDeviceObject);
   if (!NT_SUCCESS(Status))
   {
      IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
      return Status;
   }

   /* Check if driver added a FDO above the PDO */
   Fdo = IoGetAttachedDeviceReference(DeviceNode->PhysicalDeviceObject);
   if (Fdo == DeviceNode->PhysicalDeviceObject)
   {
      /* FIXME: What do we do? Unload the driver or just disable the device? */
      DPRINT1("An FDO was not attached\n");
      ObDereferenceObject(Fdo);
      IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
      return STATUS_UNSUCCESSFUL;
   }

   /* Check if we have a ACPI device (needed for power management) */
   if (Fdo->DeviceType == FILE_DEVICE_ACPI)
   {
      static BOOLEAN SystemPowerDeviceNodeCreated = FALSE;

      /* There can be only one system power device */
      if (!SystemPowerDeviceNodeCreated)
      {
         PopSystemPowerDeviceNode = DeviceNode;
         ObReferenceObject(PopSystemPowerDeviceNode);
         SystemPowerDeviceNodeCreated = TRUE;
      }
   }

   ObDereferenceObject(Fdo);

   IopDeviceNodeSetFlag(DeviceNode, DNF_ADDED);
   IopDeviceNodeSetFlag(DeviceNode, DNF_NEED_ENUMERATION_ONLY);

   return STATUS_SUCCESS;
}

NTSTATUS
IopStartDevice(
   PDEVICE_NODE DeviceNode)
{
   IO_STATUS_BLOCK IoStatusBlock;
   IO_STACK_LOCATION Stack;
   ULONG RequiredLength;
   NTSTATUS Status;
   HANDLE InstanceHandle = INVALID_HANDLE_VALUE, ControlHandle = INVALID_HANDLE_VALUE;
   UNICODE_STRING KeyName;
   OBJECT_ATTRIBUTES ObjectAttributes;

   IopDeviceNodeSetFlag(DeviceNode, DNF_ASSIGNING_RESOURCES);
   DPRINT("Sending IRP_MN_FILTER_RESOURCE_REQUIREMENTS to device stack\n");
   Stack.Parameters.FilterResourceRequirements.IoResourceRequirementList = DeviceNode->ResourceRequirements;
   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_FILTER_RESOURCE_REQUIREMENTS,
      &Stack);
   if (!NT_SUCCESS(Status) && Status != STATUS_NOT_SUPPORTED)
   {
      DPRINT("IopInitiatePnpIrp(IRP_MN_FILTER_RESOURCE_REQUIREMENTS) failed\n");
      IopDeviceNodeClearFlag(DeviceNode, DNF_ASSIGNING_RESOURCES);
      return Status;
   }
   else if (NT_SUCCESS(Status))
   {
      DeviceNode->ResourceRequirements = (PIO_RESOURCE_REQUIREMENTS_LIST)IoStatusBlock.Information;
   }

   Status = IopAssignDeviceResources(DeviceNode, &RequiredLength);
   if (NT_SUCCESS(Status))
   {
      Status = IopTranslateDeviceResources(DeviceNode, RequiredLength);
      if (NT_SUCCESS(Status))
      {
         Status = IopUpdateResourceMapForPnPDevice(DeviceNode);
         if (!NT_SUCCESS(Status))
         {
             DPRINT("IopUpdateResourceMap() failed (Status 0x%08lx)\n", Status);
         }
      }
      else
      {
         DPRINT("IopTranslateDeviceResources() failed (Status 0x%08lx)\n", Status);
      }
   }
   else
   {
      DPRINT("IopAssignDeviceResources() failed (Status 0x%08lx)\n", Status);
   }
   IopDeviceNodeClearFlag(DeviceNode, DNF_ASSIGNING_RESOURCES);

   if (!NT_SUCCESS(Status))
       goto ByeBye;

   DPRINT("Sending IRP_MN_START_DEVICE to driver\n");
   Stack.Parameters.StartDevice.AllocatedResources = DeviceNode->ResourceList;
   Stack.Parameters.StartDevice.AllocatedResourcesTranslated = DeviceNode->ResourceListTranslated;

   /*
    * Windows NT Drivers receive IRP_MN_START_DEVICE in a critical region and
    * actually _depend_ on this!. This is because NT will lock the Device Node
    * with an ERESOURCE, which of course requires APCs to be disabled.
    */
   KeEnterCriticalRegion();

   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_START_DEVICE,
      &Stack);

   KeLeaveCriticalRegion();

   if (!NT_SUCCESS(Status))
   {
      DPRINT1("IRP_MN_START_DEVICE failed for %wZ\n", &DeviceNode->InstancePath);
      IopDeviceNodeClearFlag(DeviceNode, DNF_NEED_ENUMERATION_ONLY);
      goto ByeBye;
   }
   else
   {
      if (IopDeviceNodeHasFlag(DeviceNode, DNF_NEED_ENUMERATION_ONLY))
      {
         DPRINT("Device needs enumeration, invalidating bus relations\n");
         /* Invalidate device relations synchronously
            (otherwise there will be dirty read of DeviceNode) */
         IopEnumerateDevice(DeviceNode->PhysicalDeviceObject);
         IopDeviceNodeClearFlag(DeviceNode, DNF_NEED_ENUMERATION_ONLY);
      }
   }

   Status = IopCreateDeviceKeyPath(&DeviceNode->InstancePath, 0, &InstanceHandle);
   if (!NT_SUCCESS(Status))
       goto ByeBye;

   RtlInitUnicodeString(&KeyName, L"Control");
   InitializeObjectAttributes(&ObjectAttributes,
                              &KeyName,
                              OBJ_CASE_INSENSITIVE,
                              InstanceHandle,
                              NULL);
   Status = ZwCreateKey(&ControlHandle, KEY_SET_VALUE, &ObjectAttributes, 0, NULL, REG_OPTION_VOLATILE, NULL);
   if (!NT_SUCCESS(Status))
       goto ByeBye;

   RtlInitUnicodeString(&KeyName, L"ActiveService");
   Status = ZwSetValueKey(ControlHandle, &KeyName, 0, REG_SZ, DeviceNode->ServiceName.Buffer, DeviceNode->ServiceName.Length);

   if (NT_SUCCESS(Status) && DeviceNode->ResourceList)
   {
       RtlInitUnicodeString(&KeyName, L"AllocConfig");
       Status = ZwSetValueKey(ControlHandle, &KeyName, 0, REG_RESOURCE_LIST,
                              DeviceNode->ResourceList, CM_RESOURCE_LIST_SIZE(DeviceNode->ResourceList));
   }

ByeBye:
   if (NT_SUCCESS(Status))
       IopDeviceNodeSetFlag(DeviceNode, DNF_STARTED);
   else
       IopDeviceNodeSetFlag(DeviceNode, DNF_START_FAILED);

   if (ControlHandle != INVALID_HANDLE_VALUE)
       ZwClose(ControlHandle);

   if (InstanceHandle != INVALID_HANDLE_VALUE)
       ZwClose(InstanceHandle);

   return Status;
}

NTSTATUS
NTAPI
IopQueryDeviceCapabilities(PDEVICE_NODE DeviceNode,
                           PDEVICE_CAPABILITIES DeviceCaps)
{
   IO_STATUS_BLOCK StatusBlock;
   IO_STACK_LOCATION Stack;

   /* Set up the Header */
   RtlZeroMemory(DeviceCaps, sizeof(DEVICE_CAPABILITIES));
   DeviceCaps->Size = sizeof(DEVICE_CAPABILITIES);
   DeviceCaps->Version = 1;
   DeviceCaps->Address = -1;
   DeviceCaps->UINumber = -1;

   /* Set up the Stack */
   RtlZeroMemory(&Stack, sizeof(IO_STACK_LOCATION));
   Stack.Parameters.DeviceCapabilities.Capabilities = DeviceCaps;

   /* Send the IRP */
   return IopInitiatePnpIrp(DeviceNode->PhysicalDeviceObject,
                            &StatusBlock,
                            IRP_MN_QUERY_CAPABILITIES,
                            &Stack);
}

static VOID NTAPI
IopAsynchronousInvalidateDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID InvalidateContext)
{
    PINVALIDATE_DEVICE_RELATION_DATA Data = InvalidateContext;

    IoSynchronousInvalidateDeviceRelations(
        Data->DeviceObject,
        Data->Type);

    ObDereferenceObject(Data->DeviceObject);
    IoFreeWorkItem(Data->WorkItem);
    ExFreePool(Data);
}

NTSTATUS
IopGetSystemPowerDeviceObject(PDEVICE_OBJECT *DeviceObject)
{
   KIRQL OldIrql;

   if (PopSystemPowerDeviceNode)
   {
      KeAcquireSpinLock(&IopDeviceTreeLock, &OldIrql);
      *DeviceObject = PopSystemPowerDeviceNode->PhysicalDeviceObject;
      KeReleaseSpinLock(&IopDeviceTreeLock, OldIrql);

      return STATUS_SUCCESS;
   }

   return STATUS_UNSUCCESSFUL;
}

USHORT
NTAPI
IopGetBusTypeGuidIndex(LPGUID BusTypeGuid)
{
   USHORT i = 0, FoundIndex = 0xFFFF;
   ULONG NewSize;
   PVOID NewList;

   /* Acquire the lock */
   ExAcquireFastMutex(&IopBusTypeGuidListLock);

   /* Loop all entries */
   while (i < IopBusTypeGuidList->GuidCount)
   {
       /* Try to find a match */
       if (RtlCompareMemory(BusTypeGuid,
                            &IopBusTypeGuidList->Guids[i],
                            sizeof(GUID)) == sizeof(GUID))
       {
           /* Found it */
           FoundIndex = i;
           goto Quickie;
       }
       i++;
   }

   /* Check if we have to grow the list */
   if (IopBusTypeGuidList->GuidCount)
   {
       /* Calculate the new size */
       NewSize = sizeof(IO_BUS_TYPE_GUID_LIST) +
                (sizeof(GUID) * IopBusTypeGuidList->GuidCount);

       /* Allocate the new copy */
       NewList = ExAllocatePool(PagedPool, NewSize);

       if (!NewList) {
	   /* Fail */
	   ExFreePool(IopBusTypeGuidList);
	   goto Quickie;
       }

       /* Now copy them, decrease the size too */
       NewSize -= sizeof(GUID);
       RtlCopyMemory(NewList, IopBusTypeGuidList, NewSize);

       /* Free the old list */
       ExFreePool(IopBusTypeGuidList);

       /* Use the new buffer */
       IopBusTypeGuidList = NewList;
   }

   /* Copy the new GUID */
   RtlCopyMemory(&IopBusTypeGuidList->Guids[IopBusTypeGuidList->GuidCount],
                 BusTypeGuid,
                 sizeof(GUID));

   /* The new entry is the index */
   FoundIndex = (USHORT)IopBusTypeGuidList->GuidCount;
   IopBusTypeGuidList->GuidCount++;

Quickie:
   ExReleaseFastMutex(&IopBusTypeGuidListLock);
   return FoundIndex;
}

/*
 * DESCRIPTION
 * 	Creates a device node
 *
 * ARGUMENTS
 *   ParentNode           = Pointer to parent device node
 *   PhysicalDeviceObject = Pointer to PDO for device object. Pass NULL
 *                          to have the root device node create one
 *                          (eg. for legacy drivers)
 *   DeviceNode           = Pointer to storage for created device node
 *
 * RETURN VALUE
 * 	Status
 */
NTSTATUS
IopCreateDeviceNode(PDEVICE_NODE ParentNode,
                    PDEVICE_OBJECT PhysicalDeviceObject,
                    PUNICODE_STRING ServiceName,
                    PDEVICE_NODE *DeviceNode)
{
   PDEVICE_NODE Node;
   NTSTATUS Status;
   KIRQL OldIrql;
   UNICODE_STRING FullServiceName;
   UNICODE_STRING LegacyPrefix = RTL_CONSTANT_STRING(L"LEGACY_");
   UNICODE_STRING UnknownDeviceName = RTL_CONSTANT_STRING(L"UNKNOWN");
   UNICODE_STRING KeyName, ClassName, ClassGUID;
   PUNICODE_STRING ServiceName1;
   ULONG LegacyValue;
   HANDLE InstanceHandle;

   DPRINT("ParentNode 0x%p PhysicalDeviceObject 0x%p ServiceName %wZ\n",
      ParentNode, PhysicalDeviceObject, ServiceName);

   Node = (PDEVICE_NODE)ExAllocatePool(NonPagedPool, sizeof(DEVICE_NODE));
   if (!Node)
   {
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   RtlZeroMemory(Node, sizeof(DEVICE_NODE));

   if (!ServiceName)
       ServiceName1 = &UnknownDeviceName;
   else
       ServiceName1 = ServiceName;

   if (!PhysicalDeviceObject)
   {
      FullServiceName.MaximumLength = LegacyPrefix.Length + ServiceName1->Length;
      FullServiceName.Length = 0;
      FullServiceName.Buffer = ExAllocatePool(PagedPool, FullServiceName.MaximumLength);
      if (!FullServiceName.Buffer)
      {
          ExFreePool(Node);
          return STATUS_INSUFFICIENT_RESOURCES;
      }

      RtlAppendUnicodeStringToString(&FullServiceName, &LegacyPrefix);
      RtlAppendUnicodeStringToString(&FullServiceName, ServiceName1);

      Status = PnpRootCreateDevice(&FullServiceName, &PhysicalDeviceObject, &Node->InstancePath);
      if (!NT_SUCCESS(Status))
      {
         DPRINT1("PnpRootCreateDevice() failed with status 0x%08X\n", Status);
         ExFreePool(Node);
         return Status;
      }

      /* Create the device key for legacy drivers */
      Status = IopCreateDeviceKeyPath(&Node->InstancePath, REG_OPTION_VOLATILE, &InstanceHandle);
      if (!NT_SUCCESS(Status))
      {
          ZwClose(InstanceHandle);
          ExFreePool(Node);
          ExFreePool(FullServiceName.Buffer);
          return Status;
      }

      Node->ServiceName.Buffer = ExAllocatePool(PagedPool, ServiceName1->Length);
      if (!Node->ServiceName.Buffer)
      {
          ZwClose(InstanceHandle);
          ExFreePool(Node);
          ExFreePool(FullServiceName.Buffer);
          return Status;
      }

      Node->ServiceName.MaximumLength = ServiceName1->Length;
      Node->ServiceName.Length = 0;

      RtlAppendUnicodeStringToString(&Node->ServiceName, ServiceName1);

      if (ServiceName)
      {
          RtlInitUnicodeString(&KeyName, L"Service");
          Status = ZwSetValueKey(InstanceHandle, &KeyName, 0, REG_SZ, ServiceName->Buffer, ServiceName->Length);
      }

      if (NT_SUCCESS(Status))
      {
          RtlInitUnicodeString(&KeyName, L"Legacy");

          LegacyValue = 1;
          Status = ZwSetValueKey(InstanceHandle, &KeyName, 0, REG_DWORD, &LegacyValue, sizeof(LegacyValue));
          if (NT_SUCCESS(Status))
          {
              RtlInitUnicodeString(&KeyName, L"Class");

              RtlInitUnicodeString(&ClassName, L"LegacyDriver");
              Status = ZwSetValueKey(InstanceHandle, &KeyName, 0, REG_SZ, ClassName.Buffer, ClassName.Length);
              if (NT_SUCCESS(Status))
              {
                  RtlInitUnicodeString(&KeyName, L"ClassGUID");

                  RtlInitUnicodeString(&ClassGUID, L"{8ECC055D-047F-11D1-A537-0000F8753ED1}");
                  Status = ZwSetValueKey(InstanceHandle, &KeyName, 0, REG_SZ, ClassGUID.Buffer, ClassGUID.Length);
              }
          }
      }

      ZwClose(InstanceHandle);
      ExFreePool(FullServiceName.Buffer);

      if (!NT_SUCCESS(Status))
      {
          ExFreePool(Node);
          return Status;
      }

      /* This is for drivers passed on the command line to ntoskrnl.exe */
      IopDeviceNodeSetFlag(Node, DNF_LEGACY_DRIVER);
   }

   Node->PhysicalDeviceObject = PhysicalDeviceObject;

   ((PEXTENDED_DEVOBJ_EXTENSION)PhysicalDeviceObject->DeviceObjectExtension)->DeviceNode = Node;

    if (ParentNode)
    {
        KeAcquireSpinLock(&IopDeviceTreeLock, &OldIrql);
        Node->Parent = ParentNode;
        Node->Sibling = ParentNode->Child;
        ParentNode->Child = Node;
        if (ParentNode->LastChild == NULL)
            ParentNode->LastChild = Node;
        KeReleaseSpinLock(&IopDeviceTreeLock, OldIrql);
        Node->Level = ParentNode->Level + 1;
    }

    PhysicalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

   *DeviceNode = Node;

   return STATUS_SUCCESS;
}

NTSTATUS
IopFreeDeviceNode(PDEVICE_NODE DeviceNode)
{
   KIRQL OldIrql;
   PDEVICE_NODE PrevSibling = NULL;

   /* All children must be deleted before a parent is deleted */
   ASSERT(!DeviceNode->Child);

   KeAcquireSpinLock(&IopDeviceTreeLock, &OldIrql);

   ASSERT(DeviceNode->PhysicalDeviceObject);

   ObDereferenceObject(DeviceNode->PhysicalDeviceObject);

    /* Get previous sibling */
    if (DeviceNode->Parent && DeviceNode->Parent->Child != DeviceNode)
    {
        PrevSibling = DeviceNode->Parent->Child;
        while (PrevSibling->Sibling != DeviceNode)
            PrevSibling = PrevSibling->Sibling;
    }

    /* Unlink from parent if it exists */
    if (DeviceNode->Parent)
    {
        if (DeviceNode->Parent->LastChild == DeviceNode)
        {
            DeviceNode->Parent->LastChild = PrevSibling;
            if (PrevSibling)
                PrevSibling->Sibling = NULL;
        }
        if (DeviceNode->Parent->Child == DeviceNode)
            DeviceNode->Parent->Child = DeviceNode->Sibling;
    }

    /* Unlink from sibling list */
    if (PrevSibling)
        PrevSibling->Sibling = DeviceNode->Sibling;

   KeReleaseSpinLock(&IopDeviceTreeLock, OldIrql);

   RtlFreeUnicodeString(&DeviceNode->InstancePath);

   RtlFreeUnicodeString(&DeviceNode->ServiceName);

   if (DeviceNode->ResourceList)
   {
      ExFreePool(DeviceNode->ResourceList);
   }

   if (DeviceNode->ResourceListTranslated)
   {
      ExFreePool(DeviceNode->ResourceListTranslated);
   }

   if (DeviceNode->ResourceRequirements)
   {
      ExFreePool(DeviceNode->ResourceRequirements);
   }

   if (DeviceNode->BootResources)
   {
      ExFreePool(DeviceNode->BootResources);
   }

   ExFreePool(DeviceNode);

   return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
IopSynchronousCall(IN PDEVICE_OBJECT DeviceObject,
                   IN PIO_STACK_LOCATION IoStackLocation,
                   OUT PVOID *Information)
{
    PIRP Irp;
    PIO_STACK_LOCATION IrpStack;
    IO_STATUS_BLOCK IoStatusBlock;
    KEVENT Event;
    NTSTATUS Status;
    PDEVICE_OBJECT TopDeviceObject;
    PAGED_CODE();
    
    /* Call the top of the device stack */
    TopDeviceObject = IoGetAttachedDeviceReference(DeviceObject);
    
    /* Allocate an IRP */
    Irp = IoAllocateIrp(TopDeviceObject->StackSize, FALSE);
    if (!Irp) return STATUS_INSUFFICIENT_RESOURCES;
    
    /* Initialize to failure */
    Irp->IoStatus.Status = IoStatusBlock.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = IoStatusBlock.Information = 0;
    
    /* Initialize the event */
    KeInitializeEvent(&Event, SynchronizationEvent, FALSE);
    
    /* Set them up */
    Irp->UserIosb = &IoStatusBlock;
    Irp->UserEvent = &Event;
    
    /* Queue the IRP */
    Irp->Tail.Overlay.Thread = PsGetCurrentThread();
    IoQueueThreadIrp(Irp);
    
    /* Copy-in the stack */
    IrpStack = IoGetNextIrpStackLocation(Irp);
    *IrpStack = *IoStackLocation;
    
    /* Call the driver */
    Status = IoCallDriver(TopDeviceObject, Irp);
    if (Status == STATUS_PENDING)
    {
        /* Wait for it */
        KeWaitForSingleObject(&Event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        Status = IoStatusBlock.Status;
    }
    
    /* Return the information */
    *Information = (PVOID)IoStatusBlock.Information;
    return Status;
}

NTSTATUS
NTAPI
IopInitiatePnpIrp(IN PDEVICE_OBJECT DeviceObject,
                  IN OUT PIO_STATUS_BLOCK IoStatusBlock,
                  IN ULONG MinorFunction,
                  IN PIO_STACK_LOCATION Stack OPTIONAL)
{
    IO_STACK_LOCATION IoStackLocation;
    
    /* Fill out the stack information */
    RtlZeroMemory(&IoStackLocation, sizeof(IO_STACK_LOCATION));
    IoStackLocation.MajorFunction = IRP_MJ_PNP;
    IoStackLocation.MinorFunction = MinorFunction;
    if (Stack)
    {
        /* Copy the rest */
        RtlCopyMemory(&IoStackLocation.Parameters,
                      &Stack->Parameters,
                      sizeof(Stack->Parameters));
    }
    
    /* Do the PnP call */
    IoStatusBlock->Status = IopSynchronousCall(DeviceObject,
                                               &IoStackLocation,
                                               (PVOID)&IoStatusBlock->Information);
    return IoStatusBlock->Status;
}

NTSTATUS
IopTraverseDeviceTreeNode(PDEVICETREE_TRAVERSE_CONTEXT Context)
{
   PDEVICE_NODE ParentDeviceNode;
   PDEVICE_NODE ChildDeviceNode;
   NTSTATUS Status;

   /* Copy context data so we don't overwrite it in subsequent calls to this function */
   ParentDeviceNode = Context->DeviceNode;

   /* Call the action routine */
   Status = (Context->Action)(ParentDeviceNode, Context->Context);
   if (!NT_SUCCESS(Status))
   {
      return Status;
   }

   /* Traversal of all children nodes */
   for (ChildDeviceNode = ParentDeviceNode->Child;
        ChildDeviceNode != NULL;
        ChildDeviceNode = ChildDeviceNode->Sibling)
   {
      /* Pass the current device node to the action routine */
      Context->DeviceNode = ChildDeviceNode;

      Status = IopTraverseDeviceTreeNode(Context);
      if (!NT_SUCCESS(Status))
      {
         return Status;
      }
   }

   return Status;
}


NTSTATUS
IopTraverseDeviceTree(PDEVICETREE_TRAVERSE_CONTEXT Context)
{
   NTSTATUS Status;

   DPRINT("Context 0x%p\n", Context);

   DPRINT("IopTraverseDeviceTree(DeviceNode 0x%p  FirstDeviceNode 0x%p  Action %x  Context 0x%p)\n",
      Context->DeviceNode, Context->FirstDeviceNode, Context->Action, Context->Context);

   /* Start from the specified device node */
   Context->DeviceNode = Context->FirstDeviceNode;

   /* Recursively traverse the device tree */
   Status = IopTraverseDeviceTreeNode(Context);
   if (Status == STATUS_UNSUCCESSFUL)
   {
      /* The action routine just wanted to terminate the traversal with status
      code STATUS_SUCCESS */
      Status = STATUS_SUCCESS;
   }

   return Status;
}


/*
 * IopCreateDeviceKeyPath
 *
 * Creates a registry key
 *
 * Parameters
 *    RegistryPath
 *        Name of the key to be created.
 *    Handle
 *        Handle to the newly created key
 *
 * Remarks
 *     This method can create nested trees, so parent of RegistryPath can
 *     be not existant, and will be created if needed.
 */
NTSTATUS
NTAPI
IopCreateDeviceKeyPath(IN PCUNICODE_STRING RegistryPath,
                       IN ULONG CreateOptions,
                       OUT PHANDLE Handle)
{
    UNICODE_STRING EnumU = RTL_CONSTANT_STRING(ENUM_ROOT);
    HANDLE hParent = NULL, hKey;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING KeyName;
    LPCWSTR Current, Last;
    ULONG dwLength;
    NTSTATUS Status;

    /* Assume failure */
    *Handle = NULL;

    /* Open root key for device instances */
    Status = IopOpenRegistryKeyEx(&hParent, NULL, &EnumU, KEY_CREATE_SUB_KEY);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("ZwOpenKey('%wZ') failed with status 0x%08lx\n", &EnumU, Status);
        return Status;
    }

    Current = KeyName.Buffer = RegistryPath->Buffer;
    Last = &RegistryPath->Buffer[RegistryPath->Length / sizeof(WCHAR)];

    /* Go up to the end of the string */
    while (Current <= Last)
    {
        if (Current != Last && *Current != '\\')
        {
            /* Not the end of the string and not a separator */
            Current++;
            continue;
        }

        /* Prepare relative key name */
        dwLength = (ULONG_PTR)Current - (ULONG_PTR)KeyName.Buffer;
        KeyName.MaximumLength = KeyName.Length = dwLength;
        DPRINT("Create '%wZ'\n", &KeyName);

        /* Open key */
        InitializeObjectAttributes(&ObjectAttributes,
                                   &KeyName,
                                   OBJ_CASE_INSENSITIVE,
                                   hParent,
                                   NULL);
        Status = ZwCreateKey(&hKey,
                             Current == Last ? KEY_ALL_ACCESS : KEY_CREATE_SUB_KEY,
                             &ObjectAttributes,
                             0,
                             NULL,
                             CreateOptions,
                             NULL);

        /* Close parent key handle, we don't need it anymore */
        if (hParent)
            ZwClose(hParent);

        /* Key opening/creating failed? */
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("ZwCreateKey('%wZ') failed with status 0x%08lx\n", &KeyName, Status);
            return Status;
        }

        /* Check if it is the end of the string */
        if (Current == Last)
        {
            /* Yes, return success */
            *Handle = hKey;
            return STATUS_SUCCESS;
        }

        /* Start with this new parent key */
        hParent = hKey;
        Current++;
        KeyName.Buffer = (LPWSTR)Current;
    }

    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
IopUpdateResourceMap(IN PDEVICE_NODE DeviceNode, PWCHAR Level1Key, PWCHAR Level2Key)
{
  NTSTATUS Status;
  ULONG Disposition;
  HANDLE PnpMgrLevel1, PnpMgrLevel2, ResourceMapKey;
  UNICODE_STRING KeyName;
  OBJECT_ATTRIBUTES ObjectAttributes;

  RtlInitUnicodeString(&KeyName,
		       L"\\Registry\\Machine\\HARDWARE\\RESOURCEMAP");
  InitializeObjectAttributes(&ObjectAttributes,
			     &KeyName,
			     OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
			     0,
			     NULL);
  Status = ZwCreateKey(&ResourceMapKey,
		       KEY_ALL_ACCESS,
		       &ObjectAttributes,
		       0,
		       NULL,
		       REG_OPTION_VOLATILE,
		       &Disposition);
  if (!NT_SUCCESS(Status))
      return Status;

  RtlInitUnicodeString(&KeyName, Level1Key);
  InitializeObjectAttributes(&ObjectAttributes,
			     &KeyName,
			     OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
			     ResourceMapKey,
			     NULL);
  Status = ZwCreateKey(&PnpMgrLevel1,
                       KEY_ALL_ACCESS,
                       &ObjectAttributes,
                       0,
                       NULL,
                       REG_OPTION_VOLATILE,
                       &Disposition);
  ZwClose(ResourceMapKey);
  if (!NT_SUCCESS(Status))
      return Status;

  RtlInitUnicodeString(&KeyName, Level2Key);
  InitializeObjectAttributes(&ObjectAttributes,
			     &KeyName,
			     OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
			     PnpMgrLevel1,
			     NULL);
  Status = ZwCreateKey(&PnpMgrLevel2,
                       KEY_ALL_ACCESS,
                       &ObjectAttributes,
                       0,
                       NULL,
                       REG_OPTION_VOLATILE,
                       &Disposition);
  ZwClose(PnpMgrLevel1);
  if (!NT_SUCCESS(Status))
      return Status;

  if (DeviceNode->ResourceList)
  {
      WCHAR NameBuff[256];
      UNICODE_STRING NameU;
      UNICODE_STRING Suffix;
      ULONG OldLength;

      ASSERT(DeviceNode->ResourceListTranslated);

      NameU.Buffer = NameBuff;
      NameU.Length = 0;
      NameU.MaximumLength = 256 * sizeof(WCHAR);

      Status = IoGetDeviceProperty(DeviceNode->PhysicalDeviceObject,
                                   DevicePropertyPhysicalDeviceObjectName,
                                   NameU.MaximumLength,
                                   NameU.Buffer,
                                   &OldLength);
      ASSERT(Status == STATUS_SUCCESS);

      NameU.Length = (USHORT)OldLength;

      RtlInitUnicodeString(&Suffix, L".Raw");
      RtlAppendUnicodeStringToString(&NameU, &Suffix);

      Status = ZwSetValueKey(PnpMgrLevel2,
                             &NameU,
                             0,
                             REG_RESOURCE_LIST,
                             DeviceNode->ResourceList,
                             CM_RESOURCE_LIST_SIZE(DeviceNode->ResourceList));
      if (!NT_SUCCESS(Status))
      {
          ZwClose(PnpMgrLevel2);
          return Status;
      }

      /* "Remove" the suffix by setting the length back to what it used to be */
      NameU.Length = (USHORT)OldLength;

      RtlInitUnicodeString(&Suffix, L".Translated");
      RtlAppendUnicodeStringToString(&NameU, &Suffix);

      Status = ZwSetValueKey(PnpMgrLevel2,
                             &NameU,
                             0,
                             REG_RESOURCE_LIST,
                             DeviceNode->ResourceListTranslated,
                             CM_RESOURCE_LIST_SIZE(DeviceNode->ResourceListTranslated));
      ZwClose(PnpMgrLevel2);
      if (!NT_SUCCESS(Status))
          return Status;
  }
  else
  {
      ZwClose(PnpMgrLevel2);
  }

  IopDeviceNodeSetFlag(DeviceNode, DNF_RESOURCE_ASSIGNED);

  return STATUS_SUCCESS;
}

NTSTATUS
IopUpdateResourceMapForPnPDevice(IN PDEVICE_NODE DeviceNode)
{
  return IopUpdateResourceMap(DeviceNode, L"PnP Manager", L"PnpManager");
}

NTSTATUS
IopSetDeviceInstanceData(HANDLE InstanceKey,
                         PDEVICE_NODE DeviceNode)
{
   OBJECT_ATTRIBUTES ObjectAttributes;
   UNICODE_STRING KeyName;
   HANDLE LogConfKey;
   ULONG ResCount;
   ULONG ListSize, ResultLength;
   NTSTATUS Status;
   HANDLE ControlHandle;

   DPRINT("IopSetDeviceInstanceData() called\n");

   /* Create the 'LogConf' key */
   RtlInitUnicodeString(&KeyName, L"LogConf");
   InitializeObjectAttributes(&ObjectAttributes,
                              &KeyName,
                              OBJ_CASE_INSENSITIVE,
                              InstanceKey,
                              NULL);
   Status = ZwCreateKey(&LogConfKey,
                        KEY_ALL_ACCESS,
                        &ObjectAttributes,
                        0,
                        NULL,
                        0,
                        NULL);
   if (NT_SUCCESS(Status))
   {
      /* Set 'BootConfig' value */
      if (DeviceNode->BootResources != NULL)
      {
         ResCount = DeviceNode->BootResources->Count;
         if (ResCount != 0)
         {
            ListSize = CM_RESOURCE_LIST_SIZE(DeviceNode->BootResources);

            RtlInitUnicodeString(&KeyName, L"BootConfig");
            Status = ZwSetValueKey(LogConfKey,
                                   &KeyName,
                                   0,
                                   REG_RESOURCE_LIST,
                                   DeviceNode->BootResources,
                                   ListSize);
         }
      }

      /* Set 'BasicConfigVector' value */
      if (DeviceNode->ResourceRequirements != NULL &&
         DeviceNode->ResourceRequirements->ListSize != 0)
      {
         RtlInitUnicodeString(&KeyName, L"BasicConfigVector");
         Status = ZwSetValueKey(LogConfKey,
                                &KeyName,
                                0,
                                REG_RESOURCE_REQUIREMENTS_LIST,
                                DeviceNode->ResourceRequirements,
                                DeviceNode->ResourceRequirements->ListSize);
      }

      ZwClose(LogConfKey);
   }

   /* Set the 'ConfigFlags' value */
   RtlInitUnicodeString(&KeyName, L"ConfigFlags");
   Status = ZwQueryValueKey(InstanceKey,
                            &KeyName,
                            KeyValueBasicInformation,
                            NULL,
                            0,
                            &ResultLength);
  if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
  {
    /* Write the default value */
    ULONG DefaultConfigFlags = 0;
    Status = ZwSetValueKey(InstanceKey,
                           &KeyName,
                           0,
                           REG_DWORD,
                           &DefaultConfigFlags,
                           sizeof(DefaultConfigFlags));
  }

   /* Create the 'Control' key */
   RtlInitUnicodeString(&KeyName, L"Control");
   InitializeObjectAttributes(&ObjectAttributes,
                              &KeyName,
                              OBJ_CASE_INSENSITIVE,
                              InstanceKey,
                              NULL);
   Status = ZwCreateKey(&ControlHandle, 0, &ObjectAttributes, 0, NULL, REG_OPTION_VOLATILE, NULL);

   if (NT_SUCCESS(Status))
       ZwClose(ControlHandle);

  DPRINT("IopSetDeviceInstanceData() done\n");

  return Status;
}

BOOLEAN
IopCheckResourceDescriptor(
   IN PCM_PARTIAL_RESOURCE_DESCRIPTOR ResDesc,
   IN PCM_RESOURCE_LIST ResourceList,
   IN BOOLEAN Silent,
   OUT OPTIONAL PCM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDescriptor)
{
   ULONG i, ii;
   BOOLEAN Result = FALSE;

   if (ResDesc->ShareDisposition == CmResourceShareShared)
       return FALSE;

   for (i = 0; i < ResourceList->Count; i++)
   {
      PCM_PARTIAL_RESOURCE_LIST ResList = &ResourceList->List[i].PartialResourceList;
      for (ii = 0; ii < ResList->Count; ii++)
      {
         PCM_PARTIAL_RESOURCE_DESCRIPTOR ResDesc2 = &ResList->PartialDescriptors[ii];

         /* We don't care about shared resources */
         if (ResDesc->ShareDisposition == CmResourceShareShared &&
             ResDesc2->ShareDisposition == CmResourceShareShared)
             continue;

         /* Make sure we're comparing the same types */
         if (ResDesc->Type != ResDesc2->Type)
             continue;

         switch (ResDesc->Type)
         {
             case CmResourceTypeMemory:
                 if ((ResDesc->u.Memory.Start.QuadPart < ResDesc2->u.Memory.Start.QuadPart &&
                      ResDesc->u.Memory.Start.QuadPart + ResDesc->u.Memory.Length >
                      ResDesc2->u.Memory.Start.QuadPart) || (ResDesc2->u.Memory.Start.QuadPart <
                      ResDesc->u.Memory.Start.QuadPart && ResDesc2->u.Memory.Start.QuadPart +
                      ResDesc2->u.Memory.Length > ResDesc->u.Memory.Start.QuadPart))
                 {
                      if (!Silent)
                      {
                          DPRINT1("Resource conflict: Memory (0x%x to 0x%x vs. 0x%x to 0x%x)\n",
                                  ResDesc->u.Memory.Start.QuadPart, ResDesc->u.Memory.Start.QuadPart +
                                  ResDesc->u.Memory.Length, ResDesc2->u.Memory.Start.QuadPart,
                                  ResDesc2->u.Memory.Start.QuadPart + ResDesc2->u.Memory.Length);
                      }

                      Result = TRUE;

                      goto ByeBye;
                 }
                 break;

             case CmResourceTypePort:
                 if ((ResDesc->u.Port.Start.QuadPart < ResDesc2->u.Port.Start.QuadPart &&
                      ResDesc->u.Port.Start.QuadPart + ResDesc->u.Port.Length >
                      ResDesc2->u.Port.Start.QuadPart) || (ResDesc2->u.Port.Start.QuadPart <
                      ResDesc->u.Port.Start.QuadPart && ResDesc2->u.Port.Start.QuadPart +
                      ResDesc2->u.Port.Length > ResDesc->u.Port.Start.QuadPart))
                 {
                      if (!Silent)
                      {
                          DPRINT1("Resource conflict: Port (0x%x to 0x%x vs. 0x%x to 0x%x)\n",
                                  ResDesc->u.Port.Start.QuadPart, ResDesc->u.Port.Start.QuadPart +
                                  ResDesc->u.Port.Length, ResDesc2->u.Port.Start.QuadPart,
                                  ResDesc2->u.Port.Start.QuadPart + ResDesc2->u.Port.Length);
                      }

                      Result = TRUE;

                      goto ByeBye;
                 }
                 break;

             case CmResourceTypeInterrupt:
                 if (ResDesc->u.Interrupt.Vector == ResDesc2->u.Interrupt.Vector)
                 {
                      if (!Silent)
                      {
                          DPRINT1("Resource conflict: IRQ (0x%x 0x%x vs. 0x%x 0x%x)\n",
                                  ResDesc->u.Interrupt.Vector, ResDesc->u.Interrupt.Level,
                                  ResDesc2->u.Interrupt.Vector, ResDesc2->u.Interrupt.Level);
                      }

                      Result = TRUE;

                      goto ByeBye;
                 }
                 break;

             case CmResourceTypeBusNumber:
                 if ((ResDesc->u.BusNumber.Start < ResDesc2->u.BusNumber.Start &&
                      ResDesc->u.BusNumber.Start + ResDesc->u.BusNumber.Length >
                      ResDesc2->u.BusNumber.Start) || (ResDesc2->u.BusNumber.Start <
                      ResDesc->u.BusNumber.Start && ResDesc2->u.BusNumber.Start +
                      ResDesc2->u.BusNumber.Length > ResDesc->u.BusNumber.Start))
                 {
                      if (!Silent)
                      {
                          DPRINT1("Resource conflict: Bus number (0x%x to 0x%x vs. 0x%x to 0x%x)\n",
                                  ResDesc->u.BusNumber.Start, ResDesc->u.BusNumber.Start +
                                  ResDesc->u.BusNumber.Length, ResDesc2->u.BusNumber.Start,
                                  ResDesc2->u.BusNumber.Start + ResDesc2->u.BusNumber.Length);
                      }

                      Result = TRUE;

                      goto ByeBye;
                 }
                 break;

             case CmResourceTypeDma:
                 if (ResDesc->u.Dma.Channel == ResDesc2->u.Dma.Channel)
                 {
                     if (!Silent)
                     {
                         DPRINT1("Resource conflict: Dma (0x%x 0x%x vs. 0x%x 0x%x)\n",
                                 ResDesc->u.Dma.Channel, ResDesc->u.Dma.Port,
                                 ResDesc2->u.Dma.Channel, ResDesc2->u.Dma.Port);
                     }

                     Result = TRUE;

                     goto ByeBye;
                 }
                 break;
         }
      }
   }

ByeBye:

   if (Result && ConflictingDescriptor)
   {
       RtlCopyMemory(ConflictingDescriptor,
                     ResDesc,
                     sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
   }

   return Result;
}


BOOLEAN
IopCheckForResourceConflict(
   IN PCM_RESOURCE_LIST ResourceList1,
   IN PCM_RESOURCE_LIST ResourceList2,
   IN BOOLEAN Silent,
   OUT OPTIONAL PCM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDescriptor)
{
   ULONG i, ii;
   BOOLEAN Result = FALSE;

   for (i = 0; i < ResourceList1->Count; i++)
   {
      PCM_PARTIAL_RESOURCE_LIST ResList = &ResourceList1->List[i].PartialResourceList;
      for (ii = 0; ii < ResList->Count; ii++)
      {
         PCM_PARTIAL_RESOURCE_DESCRIPTOR ResDesc = &ResList->PartialDescriptors[ii];

         Result = IopCheckResourceDescriptor(ResDesc,
                                             ResourceList2,
                                             Silent,
                                             ConflictingDescriptor);
         if (Result) goto ByeBye;
      }
   }

        
ByeBye:

   return Result;
}

NTSTATUS
IopDetectResourceConflict(
   IN PCM_RESOURCE_LIST ResourceList,
   IN BOOLEAN Silent,
   OUT OPTIONAL PCM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDescriptor)
{
   OBJECT_ATTRIBUTES ObjectAttributes;
   UNICODE_STRING KeyName;
   HANDLE ResourceMapKey = INVALID_HANDLE_VALUE, ChildKey2 = INVALID_HANDLE_VALUE, ChildKey3 = INVALID_HANDLE_VALUE;
   ULONG KeyInformationLength, RequiredLength, KeyValueInformationLength, KeyNameInformationLength;
   PKEY_BASIC_INFORMATION KeyInformation;
   PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation;
   PKEY_VALUE_BASIC_INFORMATION KeyNameInformation;
   ULONG ChildKeyIndex1 = 0, ChildKeyIndex2 = 0, ChildKeyIndex3 = 0;
   NTSTATUS Status;

   RtlInitUnicodeString(&KeyName, L"\\Registry\\Machine\\HARDWARE\\RESOURCEMAP");
   InitializeObjectAttributes(&ObjectAttributes, &KeyName, OBJ_CASE_INSENSITIVE, 0, NULL);
   Status = ZwOpenKey(&ResourceMapKey, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &ObjectAttributes);
   if (!NT_SUCCESS(Status))
   {
      /* The key is missing which means we are the first device */
      return STATUS_SUCCESS;
   }

   while (TRUE)
   {
      Status = ZwEnumerateKey(ResourceMapKey,
                              ChildKeyIndex1,
                              KeyBasicInformation,
                              NULL,
                              0,
                              &RequiredLength);
      if (Status == STATUS_NO_MORE_ENTRIES)
          break;
      else if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
      {
          KeyInformationLength = RequiredLength;
          KeyInformation = ExAllocatePool(PagedPool, KeyInformationLength);
          if (!KeyInformation)
          {
              Status = STATUS_INSUFFICIENT_RESOURCES;
              goto cleanup;
          }

          Status = ZwEnumerateKey(ResourceMapKey, 
                                  ChildKeyIndex1,
                                  KeyBasicInformation,
                                  KeyInformation,
                                  KeyInformationLength,
                                  &RequiredLength);
      }
      else
         goto cleanup;
      ChildKeyIndex1++;
      if (!NT_SUCCESS(Status))
          goto cleanup;

      KeyName.Buffer = KeyInformation->Name;
      KeyName.MaximumLength = KeyName.Length = KeyInformation->NameLength;
      InitializeObjectAttributes(&ObjectAttributes,
                                 &KeyName,
                                 OBJ_CASE_INSENSITIVE,
                                 ResourceMapKey,
                                 NULL);
      Status = ZwOpenKey(&ChildKey2, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &ObjectAttributes);
      ExFreePool(KeyInformation);
      if (!NT_SUCCESS(Status))
          goto cleanup;

      while (TRUE)
      {
          Status = ZwEnumerateKey(ChildKey2, 
                                  ChildKeyIndex2,
                                  KeyBasicInformation,
                                  NULL,
                                  0,
                                  &RequiredLength);
          if (Status == STATUS_NO_MORE_ENTRIES)
              break;
          else if (Status == STATUS_BUFFER_TOO_SMALL)
          {
              KeyInformationLength = RequiredLength;
              KeyInformation = ExAllocatePool(PagedPool, KeyInformationLength);
              if (!KeyInformation)
              {
                  Status = STATUS_INSUFFICIENT_RESOURCES;
                  goto cleanup;
              }

              Status = ZwEnumerateKey(ChildKey2,
                                      ChildKeyIndex2,
                                      KeyBasicInformation,
                                      KeyInformation,
                                      KeyInformationLength,
                                      &RequiredLength);
          }
          else
              goto cleanup;
          ChildKeyIndex2++;
          if (!NT_SUCCESS(Status))
              goto cleanup;

          KeyName.Buffer = KeyInformation->Name;
          KeyName.MaximumLength = KeyName.Length = KeyInformation->NameLength;
          InitializeObjectAttributes(&ObjectAttributes,
                                     &KeyName,
                                     OBJ_CASE_INSENSITIVE,
                                     ChildKey2,
                                     NULL);
          Status = ZwOpenKey(&ChildKey3, KEY_QUERY_VALUE, &ObjectAttributes);
          ExFreePool(KeyInformation);
          if (!NT_SUCCESS(Status))
              goto cleanup;

          while (TRUE)
          {
              Status = ZwEnumerateValueKey(ChildKey3,
                                           ChildKeyIndex3,
                                           KeyValuePartialInformation,
                                           NULL,
                                           0,
                                           &RequiredLength);
              if (Status == STATUS_NO_MORE_ENTRIES)
                  break;
              else if (Status == STATUS_BUFFER_TOO_SMALL)
              {
                  KeyValueInformationLength = RequiredLength;
                  KeyValueInformation = ExAllocatePool(PagedPool, KeyValueInformationLength);
                  if (!KeyValueInformation)
                  {
                      Status = STATUS_INSUFFICIENT_RESOURCES;
                      goto cleanup;
                  }

                  Status = ZwEnumerateValueKey(ChildKey3,
                                               ChildKeyIndex3,
                                               KeyValuePartialInformation,
                                               KeyValueInformation,
                                               KeyValueInformationLength,
                                               &RequiredLength);
              }
              else
                  goto cleanup;
              if (!NT_SUCCESS(Status))
                  goto cleanup;

              Status = ZwEnumerateValueKey(ChildKey3,
                                           ChildKeyIndex3,
                                           KeyValueBasicInformation,
                                           NULL,
                                           0,
                                           &RequiredLength);
              if (Status == STATUS_BUFFER_TOO_SMALL)
              {
                  KeyNameInformationLength = RequiredLength;
                  KeyNameInformation = ExAllocatePool(PagedPool, KeyNameInformationLength + sizeof(WCHAR));
                  if (!KeyNameInformation)
                  {
                      Status = STATUS_INSUFFICIENT_RESOURCES;
                      goto cleanup;
                  }

                  Status = ZwEnumerateValueKey(ChildKey3,
                                               ChildKeyIndex3,
                                               KeyValueBasicInformation,
                                               KeyNameInformation,
                                               KeyNameInformationLength,
                                               &RequiredLength);
              }
              else
                  goto cleanup;

              ChildKeyIndex3++;

              if (!NT_SUCCESS(Status))
                  goto cleanup;

              KeyNameInformation->Name[KeyNameInformation->NameLength / sizeof(WCHAR)] = UNICODE_NULL;

              /* Skip translated entries */
              if (wcsstr(KeyNameInformation->Name, L".Translated"))
              {
                  ExFreePool(KeyNameInformation);
                  continue;
              }

              ExFreePool(KeyNameInformation);

              if (IopCheckForResourceConflict(ResourceList,
                                              (PCM_RESOURCE_LIST)KeyValueInformation->Data,
                                              Silent,
                                              ConflictingDescriptor))
              {
                  ExFreePool(KeyValueInformation);
                  Status = STATUS_CONFLICTING_ADDRESSES;
                  goto cleanup;
              }

              ExFreePool(KeyValueInformation);
          }
      }
   }

cleanup:
   if (ResourceMapKey != INVALID_HANDLE_VALUE)
       ZwClose(ResourceMapKey);
   if (ChildKey2 != INVALID_HANDLE_VALUE)
       ZwClose(ChildKey2);
   if (ChildKey3 != INVALID_HANDLE_VALUE)
       ZwClose(ChildKey3);

   if (Status == STATUS_NO_MORE_ENTRIES)
       Status = STATUS_SUCCESS;

   return Status;
}

BOOLEAN
IopCheckDescriptorForConflict(PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc, OPTIONAL PCM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDescriptor)
{
   CM_RESOURCE_LIST CmList;
   NTSTATUS Status;

   CmList.Count = 1;
   CmList.List[0].InterfaceType = InterfaceTypeUndefined;
   CmList.List[0].BusNumber = 0;
   CmList.List[0].PartialResourceList.Version = 1;
   CmList.List[0].PartialResourceList.Revision = 1;
   CmList.List[0].PartialResourceList.Count = 1;
   CmList.List[0].PartialResourceList.PartialDescriptors[0] = *CmDesc;

   Status = IopDetectResourceConflict(&CmList, TRUE, ConflictingDescriptor);
   if (Status == STATUS_CONFLICTING_ADDRESSES)
       return TRUE;

   return FALSE;
}

BOOLEAN
IopFindBusNumberResource(
   IN PIO_RESOURCE_DESCRIPTOR IoDesc,
   OUT PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc)
{
   ULONG Start;
   CM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDesc;

   ASSERT(IoDesc->Type == CmDesc->Type);
   ASSERT(IoDesc->Type == CmResourceTypeBusNumber);

   for (Start = IoDesc->u.BusNumber.MinBusNumber;
        Start < IoDesc->u.BusNumber.MaxBusNumber;
        Start++)
   {
        CmDesc->u.BusNumber.Length = IoDesc->u.BusNumber.Length;
        CmDesc->u.BusNumber.Start = Start;

        if (IopCheckDescriptorForConflict(CmDesc, &ConflictingDesc))
        {
            Start += ConflictingDesc.u.BusNumber.Start + ConflictingDesc.u.BusNumber.Length;
        }
        else
        {
            return TRUE;
        }
   }

   return FALSE;
}

BOOLEAN
IopFindMemoryResource(
   IN PIO_RESOURCE_DESCRIPTOR IoDesc,
   OUT PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc)
{
   ULONGLONG Start;
   CM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDesc;

   ASSERT(IoDesc->Type == CmDesc->Type);
   ASSERT(IoDesc->Type == CmResourceTypeMemory);

   for (Start = IoDesc->u.Memory.MinimumAddress.QuadPart;
        Start < IoDesc->u.Memory.MaximumAddress.QuadPart;
        Start++)
   {
        CmDesc->u.Memory.Length = IoDesc->u.Memory.Length;
        CmDesc->u.Memory.Start.QuadPart = Start;

        if (IopCheckDescriptorForConflict(CmDesc, &ConflictingDesc))
        {
            Start += ConflictingDesc.u.Memory.Start.QuadPart + ConflictingDesc.u.Memory.Length;
        }
        else
        {
            return TRUE;
        }
   }

   return FALSE;
}

BOOLEAN
IopFindPortResource(
   IN PIO_RESOURCE_DESCRIPTOR IoDesc,
   OUT PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc)
{
   ULONGLONG Start;
   CM_PARTIAL_RESOURCE_DESCRIPTOR ConflictingDesc;

   ASSERT(IoDesc->Type == CmDesc->Type);
   ASSERT(IoDesc->Type == CmResourceTypePort);

   for (Start = IoDesc->u.Port.MinimumAddress.QuadPart;
        Start < IoDesc->u.Port.MaximumAddress.QuadPart;
        Start++)
   {
        CmDesc->u.Port.Length = IoDesc->u.Port.Length;
        CmDesc->u.Port.Start.QuadPart = Start;

        if (IopCheckDescriptorForConflict(CmDesc, &ConflictingDesc))
        {
            Start += ConflictingDesc.u.Port.Start.QuadPart + ConflictingDesc.u.Port.Length;
        }
        else
        {
            return TRUE;
        }
   }

   return FALSE;
}

BOOLEAN
IopFindDmaResource(
   IN PIO_RESOURCE_DESCRIPTOR IoDesc,
   OUT PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc)
{
   ULONG Channel;

   ASSERT(IoDesc->Type == CmDesc->Type);
   ASSERT(IoDesc->Type == CmResourceTypeDma);

   for (Channel = IoDesc->u.Dma.MinimumChannel;
        Channel < IoDesc->u.Dma.MaximumChannel;
        Channel++)
   {
        CmDesc->u.Dma.Channel = Channel;
        CmDesc->u.Dma.Port = 0;

        if (!IopCheckDescriptorForConflict(CmDesc, NULL))
            return TRUE;
   }

   return FALSE;
}

BOOLEAN
IopFindInterruptResource(
   IN PIO_RESOURCE_DESCRIPTOR IoDesc,
   OUT PCM_PARTIAL_RESOURCE_DESCRIPTOR CmDesc)
{
   ULONG Vector;

   ASSERT(IoDesc->Type == CmDesc->Type);
   ASSERT(IoDesc->Type == CmResourceTypeInterrupt);

   for (Vector = IoDesc->u.Interrupt.MinimumVector;
        Vector < IoDesc->u.Interrupt.MaximumVector;
        Vector++)
   {
        CmDesc->u.Interrupt.Vector = Vector;
        CmDesc->u.Interrupt.Level = Vector;
        CmDesc->u.Interrupt.Affinity = (KAFFINITY)-1;

        if (!IopCheckDescriptorForConflict(CmDesc, NULL))
            return TRUE;
   }

   return FALSE;
}

NTSTATUS
IopCreateResourceListFromRequirements(
   IN PIO_RESOURCE_REQUIREMENTS_LIST RequirementsList,
   OUT PCM_RESOURCE_LIST *ResourceList)
{
   ULONG i, ii, Size;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR ResDesc;

   Size = FIELD_OFFSET(CM_RESOURCE_LIST, List);
   for (i = 0; i < RequirementsList->AlternativeLists; i++)
   {
      PIO_RESOURCE_LIST ResList = &RequirementsList->List[i];
      Size += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.PartialDescriptors)
            + ResList->Count * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
   }

   *ResourceList = ExAllocatePool(PagedPool, Size);
   if (!*ResourceList)
       return STATUS_INSUFFICIENT_RESOURCES;

   (*ResourceList)->Count = 1;
   (*ResourceList)->List[0].BusNumber = RequirementsList->BusNumber;
   (*ResourceList)->List[0].InterfaceType = RequirementsList->InterfaceType;
   (*ResourceList)->List[0].PartialResourceList.Version = 1;
   (*ResourceList)->List[0].PartialResourceList.Revision = 1;
   (*ResourceList)->List[0].PartialResourceList.Count = 0;

   ResDesc = &(*ResourceList)->List[0].PartialResourceList.PartialDescriptors[0];

   for (i = 0; i < RequirementsList->AlternativeLists; i++)
   {
      PIO_RESOURCE_LIST ResList = &RequirementsList->List[i];
      for (ii = 0; ii < ResList->Count; ii++)
      {
         PIO_RESOURCE_DESCRIPTOR ReqDesc = &ResList->Descriptors[ii];

         /* FIXME: Handle alternate ranges */
         if (ReqDesc->Option == IO_RESOURCE_ALTERNATIVE)
             continue;

         ResDesc->Type = ReqDesc->Type;
         ResDesc->Flags = ReqDesc->Flags;
         ResDesc->ShareDisposition = ReqDesc->ShareDisposition;

         switch (ReqDesc->Type)
         {
            case CmResourceTypeInterrupt:
              if (!IopFindInterruptResource(ReqDesc, ResDesc))
              {
                  DPRINT1("Failed to find an available interrupt resource (0x%x to 0x%x)\n",
                           ReqDesc->u.Interrupt.MinimumVector, ReqDesc->u.Interrupt.MaximumVector);

                  if (ReqDesc->Option == 0)
                  {
                      ExFreePool(*ResourceList);
                      return STATUS_CONFLICTING_ADDRESSES;
                  }
              }
              break;

            case CmResourceTypePort:
              if (!IopFindPortResource(ReqDesc, ResDesc))
              {
                  DPRINT1("Failed to find an available port resource (0x%x to 0x%x length: 0x%x)\n",
                          ReqDesc->u.Port.MinimumAddress.QuadPart, ReqDesc->u.Port.MaximumAddress.QuadPart,
                          ReqDesc->u.Port.Length);

                  if (ReqDesc->Option == 0)
                  {
                      ExFreePool(*ResourceList);
                      return STATUS_CONFLICTING_ADDRESSES;
                  }
              }
              break;

            case CmResourceTypeMemory:
              if (!IopFindMemoryResource(ReqDesc, ResDesc))
              {
                  DPRINT1("Failed to find an available memory resource (0x%x to 0x%x length: 0x%x)\n",
                          ReqDesc->u.Memory.MinimumAddress.QuadPart, ReqDesc->u.Memory.MaximumAddress.QuadPart,
                          ReqDesc->u.Memory.Length);

                  if (ReqDesc->Option == 0)
                  {
                      ExFreePool(*ResourceList);
                      return STATUS_CONFLICTING_ADDRESSES;
                  }
              }
              break;

            case CmResourceTypeBusNumber:
              if (!IopFindBusNumberResource(ReqDesc, ResDesc))
              {
                  DPRINT1("Failed to find an available bus number resource (0x%x to 0x%x length: 0x%x)\n",
                          ReqDesc->u.BusNumber.MinBusNumber, ReqDesc->u.BusNumber.MaxBusNumber,
                          ReqDesc->u.BusNumber.Length);

                  if (ReqDesc->Option == 0)
                  {
                      ExFreePool(*ResourceList);
                      return STATUS_CONFLICTING_ADDRESSES;
                  }
              }
              break;

            case CmResourceTypeDma:
              if (!IopFindDmaResource(ReqDesc, ResDesc))
              {
                  DPRINT1("Failed to find an available dma resource (0x%x to 0x%x)\n",
                          ReqDesc->u.Dma.MinimumChannel, ReqDesc->u.Dma.MaximumChannel);

                  if (ReqDesc->Option == 0)
                  {
                      ExFreePool(*ResourceList);
                      return STATUS_CONFLICTING_ADDRESSES;
                  }
              }
              break;

            default:
              DPRINT1("Unsupported resource type: %x\n", ReqDesc->Type);
              break;
         }

         (*ResourceList)->List[0].PartialResourceList.Count++;
         ResDesc++;
      }
   }

   return STATUS_SUCCESS;
}

NTSTATUS
IopAssignDeviceResources(
   IN PDEVICE_NODE DeviceNode,
   OUT ULONG *pRequiredSize)
{
   PCM_PARTIAL_RESOURCE_LIST pPartialResourceList;
   ULONG Size;
   ULONG i;
   ULONG j;
   NTSTATUS Status;

   if (!DeviceNode->BootResources && !DeviceNode->ResourceRequirements)
   {
      /* No resource needed for this device */
      DeviceNode->ResourceList = NULL;
      *pRequiredSize = 0;
      return STATUS_SUCCESS;
   }

   /* Fill DeviceNode->ResourceList
    * FIXME: the PnP arbiter should go there!
    * Actually, use the BootResources if provided, else the resource requirements
    */

   if (DeviceNode->BootResources)
   {
      /* Browse the boot resources to know if we have some custom structures */
      Size = FIELD_OFFSET(CM_RESOURCE_LIST, List);
      for (i = 0; i < DeviceNode->BootResources->Count; i++)
      {
         pPartialResourceList = &DeviceNode->BootResources->List[i].PartialResourceList;
         Size += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.PartialDescriptors)
            + pPartialResourceList->Count * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
         for (j = 0; j < pPartialResourceList->Count; j++)
         {
            if (pPartialResourceList->PartialDescriptors[j].Type == CmResourceTypeDeviceSpecific)
               Size += pPartialResourceList->PartialDescriptors[j].u.DeviceSpecificData.DataSize;
         }
      }

      DeviceNode->ResourceList = ExAllocatePool(PagedPool, Size);
      if (!DeviceNode->ResourceList)
      {
         Status = STATUS_NO_MEMORY;
         goto ByeBye;
      }
      RtlCopyMemory(DeviceNode->ResourceList, DeviceNode->BootResources, Size);

      Status = IopDetectResourceConflict(DeviceNode->ResourceList, FALSE, NULL);
      if (NT_SUCCESS(Status) || !DeviceNode->ResourceRequirements)
      {
          if (!NT_SUCCESS(Status) && !DeviceNode->ResourceRequirements)
          {
              DPRINT1("Using conflicting boot resources because no requirements were supplied!\n");
          }

          *pRequiredSize = Size;
          return STATUS_SUCCESS;
      }
      else
      {
          DPRINT1("Boot resources for %wZ cause a resource conflict!\n", &DeviceNode->InstancePath);
          ExFreePool(DeviceNode->ResourceList);
      }
   }

   Status = IopCreateResourceListFromRequirements(DeviceNode->ResourceRequirements,
                                                  &DeviceNode->ResourceList);
   if (!NT_SUCCESS(Status))
       goto ByeBye;

   Size = FIELD_OFFSET(CM_RESOURCE_LIST, List);
   for (i = 0; i < DeviceNode->ResourceList->Count; i++)
   {
      pPartialResourceList = &DeviceNode->ResourceList->List[i].PartialResourceList;
      Size += FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.PartialDescriptors)
          + pPartialResourceList->Count * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
   }

   Status = IopDetectResourceConflict(DeviceNode->ResourceList, FALSE, NULL);
   if (!NT_SUCCESS(Status))
       goto ByeBye;

   *pRequiredSize = Size;
   return STATUS_SUCCESS;

ByeBye:
   if (DeviceNode->ResourceList)
   {
      ExFreePool(DeviceNode->ResourceList);
      DeviceNode->ResourceList = NULL;
   }
   *pRequiredSize = 0;
   return Status;
}


NTSTATUS
IopTranslateDeviceResources(
   IN PDEVICE_NODE DeviceNode,
   IN ULONG RequiredSize)
{
   PCM_PARTIAL_RESOURCE_LIST pPartialResourceList;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR DescriptorRaw, DescriptorTranslated;
   ULONG i, j;
   NTSTATUS Status;

   if (!DeviceNode->ResourceList)
   {
      DeviceNode->ResourceListTranslated = NULL;
      return STATUS_SUCCESS;
   }

   /* That's easy to translate a resource list. Just copy the
    * untranslated one and change few fields in the copy
    */
   DeviceNode->ResourceListTranslated = ExAllocatePool(PagedPool, RequiredSize);
   if (!DeviceNode->ResourceListTranslated)
   {
      Status =STATUS_NO_MEMORY;
      goto cleanup;
   }
   RtlCopyMemory(DeviceNode->ResourceListTranslated, DeviceNode->ResourceList, RequiredSize);

   for (i = 0; i < DeviceNode->ResourceList->Count; i++)
   {
      pPartialResourceList = &DeviceNode->ResourceList->List[i].PartialResourceList;
      for (j = 0; j < pPartialResourceList->Count; j++)
      {
         DescriptorRaw = &pPartialResourceList->PartialDescriptors[j];
         DescriptorTranslated = &DeviceNode->ResourceListTranslated->List[i].PartialResourceList.PartialDescriptors[j];
         switch (DescriptorRaw->Type)
         {
            case CmResourceTypePort:
            {
               ULONG AddressSpace = 1; /* IO space */
               if (!HalTranslateBusAddress(
                  DeviceNode->ResourceList->List[i].InterfaceType,
                  DeviceNode->ResourceList->List[i].BusNumber,
                  DescriptorRaw->u.Port.Start,
                  &AddressSpace,
                  &DescriptorTranslated->u.Port.Start))
               {
                  Status = STATUS_UNSUCCESSFUL;
                  goto cleanup;
               }
               break;
            }
            case CmResourceTypeInterrupt:
            {
               DescriptorTranslated->u.Interrupt.Vector = HalGetInterruptVector(
                  DeviceNode->ResourceList->List[i].InterfaceType,
                  DeviceNode->ResourceList->List[i].BusNumber,
                  DescriptorRaw->u.Interrupt.Level,
                  DescriptorRaw->u.Interrupt.Vector,
                  (PKIRQL)&DescriptorTranslated->u.Interrupt.Level,
                  &DescriptorRaw->u.Interrupt.Affinity);
               break;
            }
            case CmResourceTypeMemory:
            {
               ULONG AddressSpace = 0; /* Memory space */
               if (!HalTranslateBusAddress(
                  DeviceNode->ResourceList->List[i].InterfaceType,
                  DeviceNode->ResourceList->List[i].BusNumber,
                  DescriptorRaw->u.Memory.Start,
                  &AddressSpace,
                  &DescriptorTranslated->u.Memory.Start))
               {
                  Status = STATUS_UNSUCCESSFUL;
                  goto cleanup;
               }
            }

            case CmResourceTypeDma:
            case CmResourceTypeBusNumber:
            case CmResourceTypeDeviceSpecific:
               /* Nothing to do */
               break;
            default:
               DPRINT1("Unknown resource descriptor type 0x%x\n", DescriptorRaw->Type);
               Status = STATUS_NOT_IMPLEMENTED;
               goto cleanup;
         }
      }
   }
   return STATUS_SUCCESS;

cleanup:
   /* Yes! Also delete ResourceList because ResourceList and
    * ResourceListTranslated should be a pair! */
   ExFreePool(DeviceNode->ResourceList);
   DeviceNode->ResourceList = NULL;
   if (DeviceNode->ResourceListTranslated)
   {
      ExFreePool(DeviceNode->ResourceListTranslated);
      DeviceNode->ResourceList = NULL;
   }
   return Status;
}


/*
 * IopGetParentIdPrefix
 *
 * Retrieve (or create) a string which identifies a device.
 *
 * Parameters
 *    DeviceNode
 *        Pointer to device node.
 *    ParentIdPrefix
 *        Pointer to the string where is returned the parent node identifier
 *
 * Remarks
 *     If the return code is STATUS_SUCCESS, the ParentIdPrefix string is
 *     valid and its Buffer field is NULL-terminated. The caller needs to
 *     to free the string with RtlFreeUnicodeString when it is no longer
 *     needed.
 */

NTSTATUS
IopGetParentIdPrefix(PDEVICE_NODE DeviceNode,
                     PUNICODE_STRING ParentIdPrefix)
{
   ULONG KeyNameBufferLength;
   PKEY_VALUE_PARTIAL_INFORMATION ParentIdPrefixInformation = NULL;
   UNICODE_STRING KeyName;
   UNICODE_STRING KeyValue;
   UNICODE_STRING ValueName;
   HANDLE hKey = NULL;
   ULONG crc32;
   NTSTATUS Status;

   /* HACK: As long as some devices have a NULL device
    * instance path, the following test is required :(
    */
   if (DeviceNode->Parent->InstancePath.Length == 0)
   {
      DPRINT1("Parent of %wZ has NULL Instance path, please report!\n",
          &DeviceNode->InstancePath);
      return STATUS_UNSUCCESSFUL;
   }

   /* 1. Try to retrieve ParentIdPrefix from registry */
   KeyNameBufferLength = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data[0]) + MAX_PATH * sizeof(WCHAR);
   ParentIdPrefixInformation = ExAllocatePool(PagedPool, KeyNameBufferLength + sizeof(WCHAR));
   if (!ParentIdPrefixInformation)
   {
       Status = STATUS_INSUFFICIENT_RESOURCES;
       goto cleanup;
   }


   KeyName.Buffer = ExAllocatePool(PagedPool, (49 * sizeof(WCHAR)) + DeviceNode->Parent->InstancePath.Length);
   if (!KeyName.Buffer)
   {
       Status = STATUS_INSUFFICIENT_RESOURCES;
       goto cleanup;
   }
   KeyName.Length = 0;
   KeyName.MaximumLength = (49 * sizeof(WCHAR)) + DeviceNode->Parent->InstancePath.Length;

   RtlAppendUnicodeToString(&KeyName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\");
   RtlAppendUnicodeStringToString(&KeyName, &DeviceNode->Parent->InstancePath);

   Status = IopOpenRegistryKeyEx(&hKey, NULL, &KeyName, KEY_QUERY_VALUE | KEY_SET_VALUE);
   if (!NT_SUCCESS(Status))
      goto cleanup;
   RtlInitUnicodeString(&ValueName, L"ParentIdPrefix");
   Status = ZwQueryValueKey(
      hKey, &ValueName,
      KeyValuePartialInformation, ParentIdPrefixInformation,
      KeyNameBufferLength, &KeyNameBufferLength);
   if (NT_SUCCESS(Status))
   {
      if (ParentIdPrefixInformation->Type != REG_SZ)
         Status = STATUS_UNSUCCESSFUL;
      else
      {
         KeyValue.Length = KeyValue.MaximumLength = (USHORT)ParentIdPrefixInformation->DataLength;
         KeyValue.Buffer = (PWSTR)ParentIdPrefixInformation->Data;
      }
      goto cleanup;
   }
   if (Status != STATUS_OBJECT_NAME_NOT_FOUND)
   {
      KeyValue.Length = KeyValue.MaximumLength = (USHORT)ParentIdPrefixInformation->DataLength;
      KeyValue.Buffer = (PWSTR)ParentIdPrefixInformation->Data;
      goto cleanup;
   }

   /* 2. Create the ParentIdPrefix value */
   crc32 = RtlComputeCrc32(0,
                           (PUCHAR)DeviceNode->Parent->InstancePath.Buffer,
                           DeviceNode->Parent->InstancePath.Length);

   swprintf((PWSTR)ParentIdPrefixInformation->Data, L"%lx&%lx", DeviceNode->Parent->Level, crc32);
   RtlInitUnicodeString(&KeyValue, (PWSTR)ParentIdPrefixInformation->Data);

   /* 3. Try to write the ParentIdPrefix to registry */
   Status = ZwSetValueKey(hKey,
                          &ValueName,
                          0,
                          REG_SZ,
                          (PVOID)KeyValue.Buffer,
                          (wcslen(KeyValue.Buffer) + 1) * sizeof(WCHAR));

cleanup:
   if (NT_SUCCESS(Status))
   {
      /* Duplicate the string to return it */
      Status = RtlDuplicateUnicodeString(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, &KeyValue, ParentIdPrefix);
   }
   ExFreePool(ParentIdPrefixInformation);
   RtlFreeUnicodeString(&KeyName);
   if (hKey != NULL)
      ZwClose(hKey);
   return Status;
}


/*
 * IopActionInterrogateDeviceStack
 *
 * Retrieve information for all (direct) child nodes of a parent node.
 *
 * Parameters
 *    DeviceNode
 *       Pointer to device node.
 *    Context
 *       Pointer to parent node to retrieve child node information for.
 *
 * Remarks
 *    We only return a status code indicating an error (STATUS_UNSUCCESSFUL)
 *    when we reach a device node which is not a direct child of the device
 *    node for which we retrieve information of child nodes for. Any errors
 *    that occur is logged instead so that all child services have a chance
 *    of being interrogated.
 */

NTSTATUS
IopActionInterrogateDeviceStack(PDEVICE_NODE DeviceNode,
                                PVOID Context)
{
   IO_STATUS_BLOCK IoStatusBlock;
   PDEVICE_NODE ParentDeviceNode;
   WCHAR InstancePath[MAX_PATH];
   IO_STACK_LOCATION Stack;
   NTSTATUS Status;
   PWSTR Ptr;
   USHORT Length;
   USHORT TotalLength;
   ULONG RequiredLength;
   LCID LocaleId;
   HANDLE InstanceKey = NULL;
   UNICODE_STRING ValueName;
   UNICODE_STRING ParentIdPrefix = { 0, 0, NULL };
   DEVICE_CAPABILITIES DeviceCapabilities;

   DPRINT("IopActionInterrogateDeviceStack(%p, %p)\n", DeviceNode, Context);
   DPRINT("PDO 0x%p\n", DeviceNode->PhysicalDeviceObject);

   ParentDeviceNode = (PDEVICE_NODE)Context;

   /*
    * We are called for the parent too, but we don't need to do special
    * handling for this node
    */

   if (DeviceNode == ParentDeviceNode)
   {
      DPRINT("Success\n");
      return STATUS_SUCCESS;
   }

   /*
    * Make sure this device node is a direct child of the parent device node
    * that is given as an argument
    */

   if (DeviceNode->Parent != ParentDeviceNode)
   {
      /* Stop the traversal immediately and indicate successful operation */
      DPRINT("Stop\n");
      return STATUS_UNSUCCESSFUL;
   }

   /* Get Locale ID */
   Status = ZwQueryDefaultLocale(FALSE, &LocaleId);
   if (!NT_SUCCESS(Status))
   {
      DPRINT("ZwQueryDefaultLocale() failed with status 0x%lx\n", Status);
      return Status;
   }

   /*
    * FIXME: For critical errors, cleanup and disable device, but always
    * return STATUS_SUCCESS.
    */

   DPRINT("Sending IRP_MN_QUERY_ID.BusQueryDeviceID to device stack\n");

   Stack.Parameters.QueryId.IdType = BusQueryDeviceID;
   Status = IopInitiatePnpIrp(DeviceNode->PhysicalDeviceObject,
                              &IoStatusBlock,
                              IRP_MN_QUERY_ID,
                              &Stack);
   if (NT_SUCCESS(Status))
   {
      /* Copy the device id string */
      wcscpy(InstancePath, (PWSTR)IoStatusBlock.Information);

      /*
       * FIXME: Check for valid characters, if there is invalid characters
       * then bugcheck.
       */
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
   }

   DPRINT("Sending IRP_MN_QUERY_CAPABILITIES to device stack\n");

   Status = IopQueryDeviceCapabilities(DeviceNode, &DeviceCapabilities);
   if (!NT_SUCCESS(Status))
   {
      DPRINT("IopInitiatePnpIrp() failed (Status 0x%08lx)\n", Status);
   }

   DeviceNode->CapabilityFlags = *(PULONG)((ULONG_PTR)&DeviceCapabilities + 4);

   if (!DeviceCapabilities.UniqueID)
   {
      /* Device has not a unique ID. We need to prepend parent bus unique identifier */
      DPRINT("Instance ID is not unique\n");
      Status = IopGetParentIdPrefix(DeviceNode, &ParentIdPrefix);
      if (!NT_SUCCESS(Status))
      {
         DPRINT("IopGetParentIdPrefix() failed (Status 0x%08lx)\n", Status);
      }
   }

   DPRINT("Sending IRP_MN_QUERY_ID.BusQueryInstanceID to device stack\n");

   Stack.Parameters.QueryId.IdType = BusQueryInstanceID;
   Status = IopInitiatePnpIrp(DeviceNode->PhysicalDeviceObject,
                              &IoStatusBlock,
                              IRP_MN_QUERY_ID,
                              &Stack);
   if (NT_SUCCESS(Status))
   {
      /* Append the instance id string */
      wcscat(InstancePath, L"\\");
      if (ParentIdPrefix.Length > 0)
      {
         /* Add information from parent bus device to InstancePath */
         wcscat(InstancePath, ParentIdPrefix.Buffer);
         if (IoStatusBlock.Information && *(PWSTR)IoStatusBlock.Information)
            wcscat(InstancePath, L"&");
      }
      if (IoStatusBlock.Information)
         wcscat(InstancePath, (PWSTR)IoStatusBlock.Information);

      /*
       * FIXME: Check for valid characters, if there is invalid characters
       * then bugcheck
       */
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
   }
   RtlFreeUnicodeString(&ParentIdPrefix);

   if (!RtlCreateUnicodeString(&DeviceNode->InstancePath, InstancePath))
   {
      DPRINT("No resources\n");
      /* FIXME: Cleanup and disable device */
   }

   DPRINT("InstancePath is %S\n", DeviceNode->InstancePath.Buffer);

   /*
    * Create registry key for the instance id, if it doesn't exist yet
    */
   Status = IopCreateDeviceKeyPath(&DeviceNode->InstancePath, 0, &InstanceKey);
   if (!NT_SUCCESS(Status))
   {
      DPRINT1("Failed to create the instance key! (Status %lx)\n", Status);
   }

   {
      /* Set 'Capabilities' value */
      RtlInitUnicodeString(&ValueName, L"Capabilities");
      Status = ZwSetValueKey(InstanceKey,
                             &ValueName,
                             0,
                             REG_DWORD,
                             (PVOID)&DeviceNode->CapabilityFlags,
                             sizeof(ULONG));

      /* Set 'UINumber' value */
      if (DeviceCapabilities.UINumber != MAXULONG)
      {
         RtlInitUnicodeString(&ValueName, L"UINumber");
         Status = ZwSetValueKey(InstanceKey,
                                &ValueName,
                                0,
                                REG_DWORD,
                                &DeviceCapabilities.UINumber,
                                sizeof(ULONG));
      }
   }

   DPRINT("Sending IRP_MN_QUERY_ID.BusQueryHardwareIDs to device stack\n");

   Stack.Parameters.QueryId.IdType = BusQueryHardwareIDs;
   Status = IopInitiatePnpIrp(DeviceNode->PhysicalDeviceObject,
                              &IoStatusBlock,
                              IRP_MN_QUERY_ID,
                              &Stack);
   if (NT_SUCCESS(Status))
   {
      /*
       * FIXME: Check for valid characters, if there is invalid characters
       * then bugcheck.
       */
      TotalLength = 0;
      Ptr = (PWSTR)IoStatusBlock.Information;
      DPRINT("Hardware IDs:\n");
      while (*Ptr)
      {
         DPRINT("  %S\n", Ptr);
         Length = wcslen(Ptr) + 1;

         Ptr += Length;
         TotalLength += Length;
      }
      DPRINT("TotalLength: %hu\n", TotalLength);
      DPRINT("\n");

      RtlInitUnicodeString(&ValueName, L"HardwareID");
      Status = ZwSetValueKey(InstanceKey,
			     &ValueName,
			     0,
			     REG_MULTI_SZ,
			     (PVOID)IoStatusBlock.Information,
			     (TotalLength + 1) * sizeof(WCHAR));
      if (!NT_SUCCESS(Status))
      {
         DPRINT1("ZwSetValueKey() failed (Status %lx)\n", Status);
      }
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
   }

   DPRINT("Sending IRP_MN_QUERY_ID.BusQueryCompatibleIDs to device stack\n");

   Stack.Parameters.QueryId.IdType = BusQueryCompatibleIDs;
   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_QUERY_ID,
      &Stack);
   if (NT_SUCCESS(Status) && IoStatusBlock.Information)
   {
      /*
      * FIXME: Check for valid characters, if there is invalid characters
      * then bugcheck.
      */
      TotalLength = 0;
      Ptr = (PWSTR)IoStatusBlock.Information;
      DPRINT("Compatible IDs:\n");
      while (*Ptr)
      {
         DPRINT("  %S\n", Ptr);
         Length = wcslen(Ptr) + 1;

         Ptr += Length;
         TotalLength += Length;
      }
      DPRINT("TotalLength: %hu\n", TotalLength);
      DPRINT("\n");

      RtlInitUnicodeString(&ValueName, L"CompatibleIDs");
      Status = ZwSetValueKey(InstanceKey,
         &ValueName,
         0,
         REG_MULTI_SZ,
         (PVOID)IoStatusBlock.Information,
         (TotalLength + 1) * sizeof(WCHAR));
      if (!NT_SUCCESS(Status))
      {
         DPRINT1("ZwSetValueKey() failed (Status %lx) or no Compatible ID returned\n", Status);
      }
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
   }

   DPRINT("Sending IRP_MN_QUERY_DEVICE_TEXT.DeviceTextDescription to device stack\n");

   Stack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextDescription;
   Stack.Parameters.QueryDeviceText.LocaleId = LocaleId;
   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_QUERY_DEVICE_TEXT,
      &Stack);
   /* This key is mandatory, so even if the Irp fails, we still write it */
   RtlInitUnicodeString(&ValueName, L"DeviceDesc");
   if (ZwQueryValueKey(InstanceKey, &ValueName, KeyValueBasicInformation, NULL, 0, &RequiredLength) == STATUS_OBJECT_NAME_NOT_FOUND)
   {
      if (NT_SUCCESS(Status) &&
         IoStatusBlock.Information &&
         (*(PWSTR)IoStatusBlock.Information != 0))
      {
         /* This key is overriden when a driver is installed. Don't write the
          * new description if another one already exists */
         Status = ZwSetValueKey(InstanceKey,
                                &ValueName,
                                0,
                                REG_SZ,
                                (PVOID)IoStatusBlock.Information,
                                (wcslen((PWSTR)IoStatusBlock.Information) + 1) * sizeof(WCHAR));
      }
      else
      {
         UNICODE_STRING DeviceDesc = RTL_CONSTANT_STRING(L"Unknown device");
         DPRINT("Driver didn't return DeviceDesc (Status 0x%08lx), so place unknown device there\n", Status);

         Status = ZwSetValueKey(InstanceKey,
            &ValueName,
            0,
            REG_SZ,
            DeviceDesc.Buffer,
            DeviceDesc.MaximumLength);

         if (!NT_SUCCESS(Status))
         {
            DPRINT1("ZwSetValueKey() failed (Status 0x%lx)\n", Status);
         }

      }
   }

   DPRINT("Sending IRP_MN_QUERY_DEVICE_TEXT.DeviceTextLocation to device stack\n");

   Stack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextLocationInformation;
   Stack.Parameters.QueryDeviceText.LocaleId = LocaleId;
   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_QUERY_DEVICE_TEXT,
      &Stack);
   if (NT_SUCCESS(Status) && IoStatusBlock.Information)
   {
      DPRINT("LocationInformation: %S\n", (PWSTR)IoStatusBlock.Information);
      RtlInitUnicodeString(&ValueName, L"LocationInformation");
      Status = ZwSetValueKey(InstanceKey,
         &ValueName,
         0,
         REG_SZ,
         (PVOID)IoStatusBlock.Information,
         (wcslen((PWSTR)IoStatusBlock.Information) + 1) * sizeof(WCHAR));
      if (!NT_SUCCESS(Status))
      {
         DPRINT1("ZwSetValueKey() failed (Status %lx)\n", Status);
      }
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x) or IoStatusBlock.Information=NULL\n", Status);
   }

   DPRINT("Sending IRP_MN_QUERY_BUS_INFORMATION to device stack\n");

   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_QUERY_BUS_INFORMATION,
      NULL);
   if (NT_SUCCESS(Status) && IoStatusBlock.Information)
   {
      PPNP_BUS_INFORMATION BusInformation =
         (PPNP_BUS_INFORMATION)IoStatusBlock.Information;

      DeviceNode->ChildBusNumber = BusInformation->BusNumber;
      DeviceNode->ChildInterfaceType = BusInformation->LegacyBusType;
      DeviceNode->ChildBusTypeIndex = IopGetBusTypeGuidIndex(&BusInformation->BusTypeGuid);
      ExFreePool(BusInformation);
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x) or IoStatusBlock.Information=NULL\n", Status);

      DeviceNode->ChildBusNumber = 0xFFFFFFF0;
      DeviceNode->ChildInterfaceType = InterfaceTypeUndefined;
      DeviceNode->ChildBusTypeIndex = -1;
   }

   DPRINT("Sending IRP_MN_QUERY_RESOURCES to device stack\n");

   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_QUERY_RESOURCES,
      NULL);
   if (NT_SUCCESS(Status) && IoStatusBlock.Information)
   {
      DeviceNode->BootResources =
         (PCM_RESOURCE_LIST)IoStatusBlock.Information;
      IopDeviceNodeSetFlag(DeviceNode, DNF_HAS_BOOT_CONFIG);
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %x) or IoStatusBlock.Information=NULL\n", Status);
      DeviceNode->BootResources = NULL;
   }

   DPRINT("Sending IRP_MN_QUERY_RESOURCE_REQUIREMENTS to device stack\n");

   Status = IopInitiatePnpIrp(
      DeviceNode->PhysicalDeviceObject,
      &IoStatusBlock,
      IRP_MN_QUERY_RESOURCE_REQUIREMENTS,
      NULL);
   if (NT_SUCCESS(Status))
   {
      DeviceNode->ResourceRequirements =
         (PIO_RESOURCE_REQUIREMENTS_LIST)IoStatusBlock.Information;
      if (IoStatusBlock.Information)
         IopDeviceNodeSetFlag(DeviceNode, DNF_RESOURCE_REPORTED);
      else
         IopDeviceNodeSetFlag(DeviceNode, DNF_NO_RESOURCE_REQUIRED);
   }
   else
   {
      DPRINT("IopInitiatePnpIrp() failed (Status %08lx)\n", Status);
      DeviceNode->ResourceRequirements = NULL;
   }


   if (InstanceKey != NULL)
   {
      IopSetDeviceInstanceData(InstanceKey, DeviceNode);
   }

   ZwClose(InstanceKey);

   IopDeviceNodeSetFlag(DeviceNode, DNF_PROCESSED);

   if (!IopDeviceNodeHasFlag(DeviceNode, DNF_LEGACY_DRIVER))
   {
      /* Report the device to the user-mode pnp manager */
      IopQueueTargetDeviceEvent(&GUID_DEVICE_ENUMERATED,
                                &DeviceNode->InstancePath);
   }

   return STATUS_SUCCESS;
}


NTSTATUS
IopEnumerateDevice(
    IN PDEVICE_OBJECT DeviceObject)
{
    PDEVICE_NODE DeviceNode = IopGetDeviceNode(DeviceObject);
    DEVICETREE_TRAVERSE_CONTEXT Context;
    PDEVICE_RELATIONS DeviceRelations;
    PDEVICE_OBJECT ChildDeviceObject;
    IO_STATUS_BLOCK IoStatusBlock;
    PDEVICE_NODE ChildDeviceNode;
    IO_STACK_LOCATION Stack;
    NTSTATUS Status;
    ULONG i;

    DPRINT("DeviceObject 0x%p\n", DeviceObject);

    DPRINT("Sending GUID_DEVICE_ARRIVAL\n");

    /* Report the device to the user-mode pnp manager */
    IopQueueTargetDeviceEvent(&GUID_DEVICE_ARRIVAL,
                              &DeviceNode->InstancePath);

    DPRINT("Sending IRP_MN_QUERY_DEVICE_RELATIONS to device stack\n");

    Stack.Parameters.QueryDeviceRelations.Type = BusRelations;

    Status = IopInitiatePnpIrp(
        DeviceObject,
        &IoStatusBlock,
        IRP_MN_QUERY_DEVICE_RELATIONS,
        &Stack);
    if (!NT_SUCCESS(Status) || Status == STATUS_PENDING)
    {
        DPRINT("IopInitiatePnpIrp() failed with status 0x%08lx\n", Status);
        return Status;
    }

    DeviceRelations = (PDEVICE_RELATIONS)IoStatusBlock.Information;

    if (!DeviceRelations)
    {
        DPRINT("No PDOs\n");
        return STATUS_UNSUCCESSFUL;
    }

    DPRINT("Got %u PDOs\n", DeviceRelations->Count);

    /*
     * Create device nodes for all discovered devices
     */
    for (i = 0; i < DeviceRelations->Count; i++)
    {
        ChildDeviceObject = DeviceRelations->Objects[i];
        ASSERT((ChildDeviceObject->Flags & DO_DEVICE_INITIALIZING) == 0);

        ChildDeviceNode = IopGetDeviceNode(ChildDeviceObject);
        if (!ChildDeviceNode)
        {
            /* One doesn't exist, create it */
            Status = IopCreateDeviceNode(
                DeviceNode,
                ChildDeviceObject,
                NULL,
                &ChildDeviceNode);
            if (NT_SUCCESS(Status))
            {
                /* Mark the node as enumerated */
                ChildDeviceNode->Flags |= DNF_ENUMERATED;

                /* Mark the DO as bus enumerated */
                ChildDeviceObject->Flags |= DO_BUS_ENUMERATED_DEVICE;
            }
            else
            {
                /* Ignore this DO */
                DPRINT1("IopCreateDeviceNode() failed with status 0x%08x. Skipping PDO %u\n", Status, i);
                ObDereferenceObject(ChildDeviceNode);
            }
        }
        else
        {
            /* Mark it as enumerated */
            ChildDeviceNode->Flags |= DNF_ENUMERATED;
            ObDereferenceObject(ChildDeviceObject);
        }
    }
    ExFreePool(DeviceRelations);

    /*
     * Retrieve information about all discovered children from the bus driver
     */
    IopInitDeviceTreeTraverseContext(
        &Context,
        DeviceNode,
        IopActionInterrogateDeviceStack,
        DeviceNode);

    Status = IopTraverseDeviceTree(&Context);
    if (!NT_SUCCESS(Status))
    {
        DPRINT("IopTraverseDeviceTree() failed with status 0x%08lx\n", Status);
        return Status;
    }

    /*
     * Retrieve configuration from the registry for discovered children
     */
    IopInitDeviceTreeTraverseContext(
        &Context,
        DeviceNode,
        IopActionConfigureChildServices,
        DeviceNode);

    Status = IopTraverseDeviceTree(&Context);
    if (!NT_SUCCESS(Status))
    {
        DPRINT("IopTraverseDeviceTree() failed with status 0x%08lx\n", Status);
        return Status;
    }

    /*
     * Initialize services for discovered children.
     */
    Status = IopInitializePnpServices(DeviceNode);
    if (!NT_SUCCESS(Status))
    {
        DPRINT("IopInitializePnpServices() failed with status 0x%08lx\n", Status);
        return Status;
    }

    DPRINT("IopEnumerateDevice() finished\n");
    return STATUS_SUCCESS;
}


/*
 * IopActionConfigureChildServices
 *
 * Retrieve configuration for all (direct) child nodes of a parent node.
 *
 * Parameters
 *    DeviceNode
 *       Pointer to device node.
 *    Context
 *       Pointer to parent node to retrieve child node configuration for.
 *
 * Remarks
 *    We only return a status code indicating an error (STATUS_UNSUCCESSFUL)
 *    when we reach a device node which is not a direct child of the device
 *    node for which we configure child services for. Any errors that occur is
 *    logged instead so that all child services have a chance of beeing
 *    configured.
 */

NTSTATUS
IopActionConfigureChildServices(PDEVICE_NODE DeviceNode,
                                PVOID Context)
{
   RTL_QUERY_REGISTRY_TABLE QueryTable[3];
   PDEVICE_NODE ParentDeviceNode;
   PUNICODE_STRING Service;
   UNICODE_STRING ClassGUID;
   NTSTATUS Status;
   DEVICE_CAPABILITIES DeviceCaps;

   DPRINT("IopActionConfigureChildServices(%p, %p)\n", DeviceNode, Context);

   ParentDeviceNode = (PDEVICE_NODE)Context;

   /*
    * We are called for the parent too, but we don't need to do special
    * handling for this node
    */
   if (DeviceNode == ParentDeviceNode)
   {
      DPRINT("Success\n");
      return STATUS_SUCCESS;
   }

   /*
    * Make sure this device node is a direct child of the parent device node
    * that is given as an argument
    */
   if (DeviceNode->Parent != ParentDeviceNode)
   {
      /* Stop the traversal immediately and indicate successful operation */
      DPRINT("Stop\n");
      return STATUS_UNSUCCESSFUL;
   }

   if (!IopDeviceNodeHasFlag(DeviceNode, DNF_DISABLED))
   {
      WCHAR RegKeyBuffer[MAX_PATH];
      UNICODE_STRING RegKey;

      RegKey.Length = 0;
      RegKey.MaximumLength = sizeof(RegKeyBuffer);
      RegKey.Buffer = RegKeyBuffer;

      /*
       * Retrieve configuration from Enum key
       */

      Service = &DeviceNode->ServiceName;

      RtlZeroMemory(QueryTable, sizeof(QueryTable));
      RtlInitUnicodeString(Service, NULL);
      RtlInitUnicodeString(&ClassGUID, NULL);

      QueryTable[0].Name = L"Service";
      QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
      QueryTable[0].EntryContext = Service;

      QueryTable[1].Name = L"ClassGUID";
      QueryTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
      QueryTable[1].EntryContext = &ClassGUID;
      QueryTable[1].DefaultType = REG_SZ;
      QueryTable[1].DefaultData = L"";
      QueryTable[1].DefaultLength = 0;

      RtlAppendUnicodeToString(&RegKey, L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\");
      RtlAppendUnicodeStringToString(&RegKey, &DeviceNode->InstancePath);

      Status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
         RegKey.Buffer, QueryTable, NULL, NULL);

      if (!NT_SUCCESS(Status))
      {
         /* FIXME: Log the error */
         DPRINT("Could not retrieve configuration for device %wZ (Status 0x%08x)\n",
            &DeviceNode->InstancePath, Status);
         IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
         return STATUS_SUCCESS;
      }

      if (Service->Buffer == NULL)
      {
         if (NT_SUCCESS(IopQueryDeviceCapabilities(DeviceNode, &DeviceCaps)) &&
             DeviceCaps.RawDeviceOK)
         {
            DPRINT1("%wZ is using parent bus driver (%wZ)\n", &DeviceNode->InstancePath, &ParentDeviceNode->ServiceName);

            DeviceNode->ServiceName.Length = 0;
            DeviceNode->ServiceName.MaximumLength = 0;
            DeviceNode->ServiceName.Buffer = NULL;
         }
         else if (ClassGUID.Length != 0)
         {
            /* Device has a ClassGUID value, but no Service value.
             * Suppose it is using the NULL driver, so state the
             * device is started */
            DPRINT1("%wZ is using NULL driver\n", &DeviceNode->InstancePath);
            IopDeviceNodeSetFlag(DeviceNode, DNF_STARTED);
         }
         else
         {
            IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
         }
         return STATUS_SUCCESS;
      }

      DPRINT("Got Service %S\n", Service->Buffer);
   }

   return STATUS_SUCCESS;
}

/*
 * IopActionInitChildServices
 *
 * Initialize the service for all (direct) child nodes of a parent node
 *
 * Parameters
 *    DeviceNode
 *       Pointer to device node.
 *    Context
 *       Pointer to parent node to initialize child node services for.
 *
 * Remarks
 *    If the driver image for a service is not loaded and initialized
 *    it is done here too. We only return a status code indicating an
 *    error (STATUS_UNSUCCESSFUL) when we reach a device node which is
 *    not a direct child of the device node for which we initialize
 *    child services for. Any errors that occur is logged instead so
 *    that all child services have a chance of being initialized.
 */

NTSTATUS
IopActionInitChildServices(PDEVICE_NODE DeviceNode,
                           PVOID Context)
{
   PDEVICE_NODE ParentDeviceNode;
   NTSTATUS Status;
   BOOLEAN BootDrivers = !PnpSystemInit;

   DPRINT("IopActionInitChildServices(%p, %p)\n", DeviceNode, Context);

   ParentDeviceNode = (PDEVICE_NODE)Context;

   /*
    * We are called for the parent too, but we don't need to do special
    * handling for this node
    */
   if (DeviceNode == ParentDeviceNode)
   {
      DPRINT("Success\n");
      return STATUS_SUCCESS;
   }

   /*
    * Make sure this device node is a direct child of the parent device node
    * that is given as an argument
    */
#if 0
   if (DeviceNode->Parent != ParentDeviceNode)
   {
      /*
       * Stop the traversal immediately and indicate unsuccessful operation
       */
      DPRINT("Stop\n");
      return STATUS_UNSUCCESSFUL;
   }
#endif
   if (IopDeviceNodeHasFlag(DeviceNode, DNF_STARTED) ||
       IopDeviceNodeHasFlag(DeviceNode, DNF_ADDED) ||
       IopDeviceNodeHasFlag(DeviceNode, DNF_DISABLED))
       return STATUS_SUCCESS;

   if (DeviceNode->ServiceName.Buffer == NULL)
   {
      /* We don't need to worry about loading the driver because we're
       * being driven in raw mode so our parent must be loaded to get here */
      Status = IopStartDevice(DeviceNode);
      if (!NT_SUCCESS(Status))
      {
          DPRINT1("IopStartDevice(%wZ) failed with status 0x%08x\n",
                  &DeviceNode->InstancePath, Status);
      }
   }
   else
   {
      PLDR_DATA_TABLE_ENTRY ModuleObject;
      PDRIVER_OBJECT DriverObject;

      /* Get existing DriverObject pointer (in case the driver has
         already been loaded and initialized) */
      Status = IopGetDriverObject(
          &DriverObject,
          &DeviceNode->ServiceName,
          FALSE);

      if (!NT_SUCCESS(Status))
      {
         /* Driver is not initialized, try to load it */
         Status = IopLoadServiceModule(&DeviceNode->ServiceName, &ModuleObject);

         if (NT_SUCCESS(Status) || Status == STATUS_IMAGE_ALREADY_LOADED)
         {
            /* STATUS_IMAGE_ALREADY_LOADED means this driver
               was loaded by the bootloader */
            if ((Status != STATUS_IMAGE_ALREADY_LOADED) ||
                (Status == STATUS_IMAGE_ALREADY_LOADED && !DriverObject))
            {
               /* Initialize the driver */
               Status = IopInitializeDriverModule(DeviceNode, ModuleObject,
                  &DeviceNode->ServiceName, FALSE, &DriverObject);
            }
            else
            {
               Status = STATUS_SUCCESS;
            }
         }
         else
         {
            DPRINT1("IopLoadServiceModule(%wZ) failed with status 0x%08x\n",
                    &DeviceNode->ServiceName, Status);
         }
      }

      /* Driver is loaded and initialized at this point */
      if (NT_SUCCESS(Status))
      {
          /* Initialize the device, including all filters */
          Status = PipCallDriverAddDevice(DeviceNode, FALSE, DriverObject);
      }
      else
      {
         /*
          * Don't disable when trying to load only boot drivers
          */
         if (!BootDrivers)
         {
            IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
            IopDeviceNodeSetFlag(DeviceNode, DNF_START_FAILED);
            /* FIXME: Log the error (possibly in IopInitializeDeviceNodeService) */
            DPRINT1("Initialization of service %S failed (Status %x)\n",
              DeviceNode->ServiceName.Buffer, Status);
         }
      }
   }

   return STATUS_SUCCESS;
}

/*
 * IopInitializePnpServices
 *
 * Initialize services for discovered children
 *
 * Parameters
 *    DeviceNode
 *       Top device node to start initializing services.
 *
 * Return Value
 *    Status
 */
NTSTATUS
IopInitializePnpServices(IN PDEVICE_NODE DeviceNode)
{
   DEVICETREE_TRAVERSE_CONTEXT Context;

   DPRINT("IopInitializePnpServices(%p)\n", DeviceNode);

   IopInitDeviceTreeTraverseContext(
      &Context,
      DeviceNode,
      IopActionInitChildServices,
      DeviceNode);

   return IopTraverseDeviceTree(&Context);
}

static NTSTATUS INIT_FUNCTION
IopEnumerateDetectedDevices(
   IN HANDLE hBaseKey,
   IN PUNICODE_STRING RelativePath OPTIONAL,
   IN HANDLE hRootKey,
   IN BOOLEAN EnumerateSubKeys,
   IN PCM_FULL_RESOURCE_DESCRIPTOR ParentBootResources,
   IN ULONG ParentBootResourcesLength)
{
   UNICODE_STRING IdentifierU = RTL_CONSTANT_STRING(L"Identifier");
   UNICODE_STRING HardwareIDU = RTL_CONSTANT_STRING(L"HardwareID");
   UNICODE_STRING ConfigurationDataU = RTL_CONSTANT_STRING(L"Configuration Data");
   UNICODE_STRING BootConfigU = RTL_CONSTANT_STRING(L"BootConfig");
   UNICODE_STRING LogConfU = RTL_CONSTANT_STRING(L"LogConf");
   OBJECT_ATTRIBUTES ObjectAttributes;
   HANDLE hDevicesKey = NULL;
   HANDLE hDeviceKey = NULL;
   HANDLE hLevel1Key, hLevel2Key = NULL, hLogConf;
   UNICODE_STRING Level2NameU;
   WCHAR Level2Name[5];
   ULONG IndexDevice = 0;
   ULONG IndexSubKey;
   PKEY_BASIC_INFORMATION pDeviceInformation = NULL;
   ULONG DeviceInfoLength = sizeof(KEY_BASIC_INFORMATION) + 50 * sizeof(WCHAR);
   PKEY_VALUE_PARTIAL_INFORMATION pValueInformation = NULL;
   ULONG ValueInfoLength = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + 50 * sizeof(WCHAR);
   UNICODE_STRING DeviceName, ValueName;
   ULONG RequiredSize;
   PCM_FULL_RESOURCE_DESCRIPTOR BootResources = NULL;
   ULONG BootResourcesLength;
   NTSTATUS Status;

   const UNICODE_STRING IdentifierPci = RTL_CONSTANT_STRING(L"PCI");
   UNICODE_STRING HardwareIdPci = RTL_CONSTANT_STRING(L"*PNP0A03\0");
   static ULONG DeviceIndexPci = 0;
   const UNICODE_STRING IdentifierSerial = RTL_CONSTANT_STRING(L"SerialController");
   UNICODE_STRING HardwareIdSerial = RTL_CONSTANT_STRING(L"*PNP0501\0");
   static ULONG DeviceIndexSerial = 0;
   const UNICODE_STRING IdentifierKeyboard = RTL_CONSTANT_STRING(L"KeyboardController");
   UNICODE_STRING HardwareIdKeyboard = RTL_CONSTANT_STRING(L"*PNP0303\0");
   static ULONG DeviceIndexKeyboard = 0;
   const UNICODE_STRING IdentifierMouse = RTL_CONSTANT_STRING(L"PointerController");
   UNICODE_STRING HardwareIdMouse = RTL_CONSTANT_STRING(L"*PNP0F13\0");
   static ULONG DeviceIndexMouse = 0;
   const UNICODE_STRING IdentifierParallel = RTL_CONSTANT_STRING(L"ParallelController");
   UNICODE_STRING HardwareIdParallel = RTL_CONSTANT_STRING(L"*PNP0400\0");
   static ULONG DeviceIndexParallel = 0;
   const UNICODE_STRING IdentifierFloppy = RTL_CONSTANT_STRING(L"FloppyDiskPeripheral");
   UNICODE_STRING HardwareIdFloppy = RTL_CONSTANT_STRING(L"*PNP0700\0");
   static ULONG DeviceIndexFloppy = 0;
   const UNICODE_STRING IdentifierIsa = RTL_CONSTANT_STRING(L"ISA");
   UNICODE_STRING HardwareIdIsa = RTL_CONSTANT_STRING(L"*PNP0A00\0");
   static ULONG DeviceIndexIsa = 0;
   UNICODE_STRING HardwareIdKey;
   PUNICODE_STRING pHardwareId;
   ULONG DeviceIndex = 0;
   PUCHAR CmResourceList;
   ULONG ListCount;

    if (RelativePath)
    {
        Status = IopOpenRegistryKeyEx(&hDevicesKey, hBaseKey, RelativePath, KEY_ENUMERATE_SUB_KEYS);
        if (!NT_SUCCESS(Status))
        {
            DPRINT("ZwOpenKey() failed with status 0x%08lx\n", Status);
            goto cleanup;
        }
    }
    else
        hDevicesKey = hBaseKey;

   pDeviceInformation = ExAllocatePool(PagedPool, DeviceInfoLength);
   if (!pDeviceInformation)
   {
      DPRINT("ExAllocatePool() failed\n");
      Status = STATUS_NO_MEMORY;
      goto cleanup;
   }

   pValueInformation = ExAllocatePool(PagedPool, ValueInfoLength);
   if (!pValueInformation)
   {
      DPRINT("ExAllocatePool() failed\n");
      Status = STATUS_NO_MEMORY;
      goto cleanup;
   }

   while (TRUE)
   {
      Status = ZwEnumerateKey(hDevicesKey, IndexDevice, KeyBasicInformation, pDeviceInformation, DeviceInfoLength, &RequiredSize);
      if (Status == STATUS_NO_MORE_ENTRIES)
         break;
      else if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
      {
         ExFreePool(pDeviceInformation);
         DeviceInfoLength = RequiredSize;
         pDeviceInformation = ExAllocatePool(PagedPool, DeviceInfoLength);
         if (!pDeviceInformation)
         {
            DPRINT("ExAllocatePool() failed\n");
            Status = STATUS_NO_MEMORY;
            goto cleanup;
         }
         Status = ZwEnumerateKey(hDevicesKey, IndexDevice, KeyBasicInformation, pDeviceInformation, DeviceInfoLength, &RequiredSize);
      }
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwEnumerateKey() failed with status 0x%08lx\n", Status);
         goto cleanup;
      }
      IndexDevice++;

      /* Open device key */
      DeviceName.Length = DeviceName.MaximumLength = (USHORT)pDeviceInformation->NameLength;
      DeviceName.Buffer = pDeviceInformation->Name;

      Status = IopOpenRegistryKeyEx(&hDeviceKey, hDevicesKey, &DeviceName,
          KEY_QUERY_VALUE + (EnumerateSubKeys ? KEY_ENUMERATE_SUB_KEYS : 0));
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwOpenKey() failed with status 0x%08lx\n", Status);
         goto cleanup;
      }

      /* Read boot resources, and add then to parent ones */
      Status = ZwQueryValueKey(hDeviceKey, &ConfigurationDataU, KeyValuePartialInformation, pValueInformation, ValueInfoLength, &RequiredSize);
      if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
      {
         ExFreePool(pValueInformation);
         ValueInfoLength = RequiredSize;
         pValueInformation = ExAllocatePool(PagedPool, ValueInfoLength);
         if (!pValueInformation)
         {
            DPRINT("ExAllocatePool() failed\n");
            ZwDeleteKey(hLevel2Key);
            Status = STATUS_NO_MEMORY;
            goto cleanup;
         }
         Status = ZwQueryValueKey(hDeviceKey, &ConfigurationDataU, KeyValuePartialInformation, pValueInformation, ValueInfoLength, &RequiredSize);
      }
      if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
      {
         BootResources = ParentBootResources;
         BootResourcesLength = ParentBootResourcesLength;
      }
      else if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwQueryValueKey() failed with status 0x%08lx\n", Status);
         goto nextdevice;
      }
      else if (pValueInformation->Type != REG_FULL_RESOURCE_DESCRIPTOR)
      {
         DPRINT("Wrong registry type: got 0x%lx, expected 0x%lx\n", pValueInformation->Type, REG_FULL_RESOURCE_DESCRIPTOR);
         goto nextdevice;
      }
      else
      {
         static const ULONG Header = FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR, PartialResourceList.PartialDescriptors);

         /* Concatenate current resources and parent ones */
         if (ParentBootResourcesLength == 0)
            BootResourcesLength = pValueInformation->DataLength;
         else
            BootResourcesLength = ParentBootResourcesLength
               + pValueInformation->DataLength
               - Header;
         BootResources = ExAllocatePool(PagedPool, BootResourcesLength);
         if (!BootResources)
         {
            DPRINT("ExAllocatePool() failed\n");
            goto nextdevice;
         }
         if (ParentBootResourcesLength < sizeof(CM_FULL_RESOURCE_DESCRIPTOR))
         {
            RtlCopyMemory(BootResources, pValueInformation->Data, pValueInformation->DataLength);
         }
         else if (ParentBootResources->PartialResourceList.PartialDescriptors[ParentBootResources->PartialResourceList.Count - 1].Type == CmResourceTypeDeviceSpecific)
         {
            RtlCopyMemory(BootResources, pValueInformation->Data, pValueInformation->DataLength);
            RtlCopyMemory(
               (PVOID)((ULONG_PTR)BootResources + pValueInformation->DataLength),
               (PVOID)((ULONG_PTR)ParentBootResources + Header),
               ParentBootResourcesLength - Header);
            BootResources->PartialResourceList.Count += ParentBootResources->PartialResourceList.Count;
         }
         else
         {
            RtlCopyMemory(BootResources, pValueInformation->Data, Header);
            RtlCopyMemory(
               (PVOID)((ULONG_PTR)BootResources + Header),
               (PVOID)((ULONG_PTR)ParentBootResources + Header),
               ParentBootResourcesLength - Header);
            RtlCopyMemory(
               (PVOID)((ULONG_PTR)BootResources + ParentBootResourcesLength),
               pValueInformation->Data + Header,
               pValueInformation->DataLength - Header);
            BootResources->PartialResourceList.Count += ParentBootResources->PartialResourceList.Count;
         }
      }

      if (EnumerateSubKeys)
      {
         IndexSubKey = 0;
         while (TRUE)
         {
            Status = ZwEnumerateKey(hDeviceKey, IndexSubKey, KeyBasicInformation, pDeviceInformation, DeviceInfoLength, &RequiredSize);
            if (Status == STATUS_NO_MORE_ENTRIES)
               break;
            else if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
            {
               ExFreePool(pDeviceInformation);
               DeviceInfoLength = RequiredSize;
               pDeviceInformation = ExAllocatePool(PagedPool, DeviceInfoLength);
               if (!pDeviceInformation)
               {
                  DPRINT("ExAllocatePool() failed\n");
                  Status = STATUS_NO_MEMORY;
                  goto cleanup;
               }
               Status = ZwEnumerateKey(hDeviceKey, IndexSubKey, KeyBasicInformation, pDeviceInformation, DeviceInfoLength, &RequiredSize);
            }
            if (!NT_SUCCESS(Status))
            {
               DPRINT("ZwEnumerateKey() failed with status 0x%08lx\n", Status);
               goto cleanup;
            }
            IndexSubKey++;
            DeviceName.Length = DeviceName.MaximumLength = (USHORT)pDeviceInformation->NameLength;
            DeviceName.Buffer = pDeviceInformation->Name;

            Status = IopEnumerateDetectedDevices(
               hDeviceKey,
               &DeviceName,
               hRootKey,
               TRUE,
               BootResources,
               BootResourcesLength);
            if (!NT_SUCCESS(Status))
               goto cleanup;
         }
      }

      /* Read identifier */
      Status = ZwQueryValueKey(hDeviceKey, &IdentifierU, KeyValuePartialInformation, pValueInformation, ValueInfoLength, &RequiredSize);
      if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
      {
         ExFreePool(pValueInformation);
         ValueInfoLength = RequiredSize;
         pValueInformation = ExAllocatePool(PagedPool, ValueInfoLength);
         if (!pValueInformation)
         {
            DPRINT("ExAllocatePool() failed\n");
            Status = STATUS_NO_MEMORY;
            goto cleanup;
         }
         Status = ZwQueryValueKey(hDeviceKey, &IdentifierU, KeyValuePartialInformation, pValueInformation, ValueInfoLength, &RequiredSize);
      }
      if (!NT_SUCCESS(Status))
      {
         if (Status != STATUS_OBJECT_NAME_NOT_FOUND)
         {
            DPRINT("ZwQueryValueKey() failed with status 0x%08lx\n", Status);
            goto nextdevice;
         }
         ValueName.Length = ValueName.MaximumLength = 0;
      }
      else if (pValueInformation->Type != REG_SZ)
      {
         DPRINT("Wrong registry type: got 0x%lx, expected 0x%lx\n", pValueInformation->Type, REG_SZ);
         goto nextdevice;
      }
      else
      {
         /* Assign hardware id to this device */
         ValueName.Length = ValueName.MaximumLength = (USHORT)pValueInformation->DataLength;
         ValueName.Buffer = (PWCHAR)pValueInformation->Data;
         if (ValueName.Length >= sizeof(WCHAR) && ValueName.Buffer[ValueName.Length / sizeof(WCHAR) - 1] == UNICODE_NULL)
            ValueName.Length -= sizeof(WCHAR);
      }

      if (RelativePath && RtlCompareUnicodeString(RelativePath, &IdentifierSerial, FALSE) == 0)
      {
         pHardwareId = &HardwareIdSerial;
         DeviceIndex = DeviceIndexSerial++;
      }
      else if (RelativePath && RtlCompareUnicodeString(RelativePath, &IdentifierKeyboard, FALSE) == 0)
      {
         pHardwareId = &HardwareIdKeyboard;
         DeviceIndex = DeviceIndexKeyboard++;
      }
      else if (RelativePath && RtlCompareUnicodeString(RelativePath, &IdentifierMouse, FALSE) == 0)
      {
         pHardwareId = &HardwareIdMouse;
         DeviceIndex = DeviceIndexMouse++;
      }
      else if (RelativePath && RtlCompareUnicodeString(RelativePath, &IdentifierParallel, FALSE) == 0)
      {
         pHardwareId = &HardwareIdParallel;
         DeviceIndex = DeviceIndexParallel++;
      }
      else if (RelativePath && RtlCompareUnicodeString(RelativePath, &IdentifierFloppy, FALSE) == 0)
      {
         pHardwareId = &HardwareIdFloppy;
         DeviceIndex = DeviceIndexFloppy++;
      }
      else if (NT_SUCCESS(Status))
      {
         /* Try to also match the device identifier */
         if (RtlCompareUnicodeString(&ValueName, &IdentifierPci, FALSE) == 0)
         {
            pHardwareId = &HardwareIdPci;
            DeviceIndex = DeviceIndexPci++;
         }
         else if (RtlCompareUnicodeString(&ValueName, &IdentifierIsa, FALSE) == 0)
         {
            pHardwareId = &HardwareIdIsa;
            DeviceIndex = DeviceIndexIsa++;
         }
         else
         {
            DPRINT("Unknown device '%wZ'\n", &ValueName);
            goto nextdevice;
         }
      }
      else
      {
         /* Unknown key path */
         DPRINT("Unknown key path '%wZ'\n", RelativePath);
         goto nextdevice;
      }

      /* Prepare hardware id key (hardware id value without final \0) */
      HardwareIdKey = *pHardwareId;
      HardwareIdKey.Length -= sizeof(UNICODE_NULL);

      /* Add the detected device to Root key */
      InitializeObjectAttributes(&ObjectAttributes, &HardwareIdKey, OBJ_KERNEL_HANDLE, hRootKey, NULL);
      Status = ZwCreateKey(
         &hLevel1Key,
         KEY_CREATE_SUB_KEY,
         &ObjectAttributes,
         0,
         NULL,
         REG_OPTION_NON_VOLATILE,
         NULL);
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwCreateKey() failed with status 0x%08lx\n", Status);
         goto nextdevice;
      }
      swprintf(Level2Name, L"%04lu", DeviceIndex);
      RtlInitUnicodeString(&Level2NameU, Level2Name);
      InitializeObjectAttributes(&ObjectAttributes, &Level2NameU, OBJ_KERNEL_HANDLE, hLevel1Key, NULL);
      Status = ZwCreateKey(
         &hLevel2Key,
         KEY_SET_VALUE | KEY_CREATE_SUB_KEY,
         &ObjectAttributes,
         0,
         NULL,
         REG_OPTION_NON_VOLATILE,
         NULL);
      ZwClose(hLevel1Key);
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwCreateKey() failed with status 0x%08lx\n", Status);
         goto nextdevice;
      }
      DPRINT("Found %wZ #%lu (%wZ)\n", &ValueName, DeviceIndex, &HardwareIdKey);
      Status = ZwSetValueKey(hLevel2Key, &HardwareIDU, 0, REG_MULTI_SZ, pHardwareId->Buffer, pHardwareId->MaximumLength);
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwSetValueKey() failed with status 0x%08lx\n", Status);
         ZwDeleteKey(hLevel2Key);
         goto nextdevice;
      }
      /* Create 'LogConf' subkey */
      InitializeObjectAttributes(&ObjectAttributes, &LogConfU, OBJ_KERNEL_HANDLE, hLevel2Key, NULL);
      Status = ZwCreateKey(
         &hLogConf,
         KEY_SET_VALUE,
         &ObjectAttributes,
         0,
         NULL,
         REG_OPTION_VOLATILE,
         NULL);
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwCreateKey() failed with status 0x%08lx\n", Status);
         ZwDeleteKey(hLevel2Key);
         goto nextdevice;
      }
      if (BootResourcesLength >= sizeof(CM_FULL_RESOURCE_DESCRIPTOR))
      {
         CmResourceList = ExAllocatePool(PagedPool, BootResourcesLength + sizeof(ULONG));
         if (!CmResourceList)
         {
            ZwClose(hLogConf);
            ZwDeleteKey(hLevel2Key);
            goto nextdevice;
         }

         /* Add the list count (1st member of CM_RESOURCE_LIST) */
         ListCount = 1;
         RtlCopyMemory(CmResourceList,
                       &ListCount,
                       sizeof(ULONG));

         /* Now add the actual list (2nd member of CM_RESOURCE_LIST) */
         RtlCopyMemory(CmResourceList + sizeof(ULONG),
                       BootResources,
                       BootResourcesLength);

         /* Save boot resources to 'LogConf\BootConfig' */
         Status = ZwSetValueKey(hLogConf, &BootConfigU, 0, REG_RESOURCE_LIST, CmResourceList, BootResourcesLength + sizeof(ULONG));
         if (!NT_SUCCESS(Status))
         {
            DPRINT("ZwSetValueKey() failed with status 0x%08lx\n", Status);
            ZwClose(hLogConf);
            ZwDeleteKey(hLevel2Key);
            goto nextdevice;
         }
      }
      ZwClose(hLogConf);

nextdevice:
      if (BootResources && BootResources != ParentBootResources)
      {
         ExFreePool(BootResources);
         BootResources = NULL;
      }
      if (hLevel2Key)
      {
         ZwClose(hLevel2Key);
         hLevel2Key = NULL;
      }
      if (hDeviceKey)
      {
         ZwClose(hDeviceKey);
         hDeviceKey = NULL;
      }
   }

   Status = STATUS_SUCCESS;

cleanup:
   if (hDevicesKey && hDevicesKey != hBaseKey)
      ZwClose(hDevicesKey);
   if (hDeviceKey)
      ZwClose(hDeviceKey);
   if (pDeviceInformation)
      ExFreePool(pDeviceInformation);
   if (pValueInformation)
      ExFreePool(pValueInformation);
   return Status;
}

static BOOLEAN INIT_FUNCTION
IopIsAcpiComputer(VOID)
{
#ifndef ENABLE_ACPI
   return FALSE;
#else
   UNICODE_STRING MultiKeyPathU = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\MultifunctionAdapter");
   UNICODE_STRING IdentifierU = RTL_CONSTANT_STRING(L"Identifier");
   UNICODE_STRING AcpiBiosIdentifier = RTL_CONSTANT_STRING(L"ACPI BIOS");
   OBJECT_ATTRIBUTES ObjectAttributes;
   PKEY_BASIC_INFORMATION pDeviceInformation = NULL;
   ULONG DeviceInfoLength = sizeof(KEY_BASIC_INFORMATION) + 50 * sizeof(WCHAR);
   PKEY_VALUE_PARTIAL_INFORMATION pValueInformation = NULL;
   ULONG ValueInfoLength = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + 50 * sizeof(WCHAR);
   ULONG RequiredSize;
   ULONG IndexDevice = 0;
   UNICODE_STRING DeviceName, ValueName;
   HANDLE hDevicesKey = NULL;
   HANDLE hDeviceKey = NULL;
   NTSTATUS Status;
   BOOLEAN ret = FALSE;

   InitializeObjectAttributes(&ObjectAttributes, &MultiKeyPathU, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
   Status = ZwOpenKey(&hDevicesKey, KEY_ENUMERATE_SUB_KEYS, &ObjectAttributes);
   if (!NT_SUCCESS(Status))
   {
      DPRINT("ZwOpenKey() failed with status 0x%08lx\n", Status);
      goto cleanup;
   }

   pDeviceInformation = ExAllocatePool(PagedPool, DeviceInfoLength);
   if (!pDeviceInformation)
   {
      DPRINT("ExAllocatePool() failed\n");
      Status = STATUS_NO_MEMORY;
      goto cleanup;
   }

   pValueInformation = ExAllocatePool(PagedPool, ValueInfoLength);
   if (!pDeviceInformation)
   {
      DPRINT("ExAllocatePool() failed\n");
      Status = STATUS_NO_MEMORY;
      goto cleanup;
   }

   while (TRUE)
   {
      Status = ZwEnumerateKey(hDevicesKey, IndexDevice, KeyBasicInformation, pDeviceInformation, DeviceInfoLength, &RequiredSize);
      if (Status == STATUS_NO_MORE_ENTRIES)
         break;
      else if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
      {
         ExFreePool(pDeviceInformation);
         DeviceInfoLength = RequiredSize;
         pDeviceInformation = ExAllocatePool(PagedPool, DeviceInfoLength);
         if (!pDeviceInformation)
         {
            DPRINT("ExAllocatePool() failed\n");
            Status = STATUS_NO_MEMORY;
            goto cleanup;
         }
         Status = ZwEnumerateKey(hDevicesKey, IndexDevice, KeyBasicInformation, pDeviceInformation, DeviceInfoLength, &RequiredSize);
      }
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwEnumerateKey() failed with status 0x%08lx\n", Status);
         goto cleanup;
      }
      IndexDevice++;

      /* Open device key */
      DeviceName.Length = DeviceName.MaximumLength = pDeviceInformation->NameLength;
      DeviceName.Buffer = pDeviceInformation->Name;
      InitializeObjectAttributes(&ObjectAttributes, &DeviceName, OBJ_KERNEL_HANDLE, hDevicesKey, NULL);
      Status = ZwOpenKey(
         &hDeviceKey,
         KEY_QUERY_VALUE,
         &ObjectAttributes);
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwOpenKey() failed with status 0x%08lx\n", Status);
         goto cleanup;
      }

      /* Read identifier */
      Status = ZwQueryValueKey(hDeviceKey, &IdentifierU, KeyValuePartialInformation, pValueInformation, ValueInfoLength, &RequiredSize);
      if (Status == STATUS_BUFFER_OVERFLOW || Status == STATUS_BUFFER_TOO_SMALL)
      {
         ExFreePool(pValueInformation);
         ValueInfoLength = RequiredSize;
         pValueInformation = ExAllocatePool(PagedPool, ValueInfoLength);
         if (!pValueInformation)
         {
            DPRINT("ExAllocatePool() failed\n");
            Status = STATUS_NO_MEMORY;
            goto cleanup;
         }
         Status = ZwQueryValueKey(hDeviceKey, &IdentifierU, KeyValuePartialInformation, pValueInformation, ValueInfoLength, &RequiredSize);
      }
      if (!NT_SUCCESS(Status))
      {
         DPRINT("ZwQueryValueKey() failed with status 0x%08lx\n", Status);
         goto nextdevice;
      }
      else if (pValueInformation->Type != REG_SZ)
      {
         DPRINT("Wrong registry type: got 0x%lx, expected 0x%lx\n", pValueInformation->Type, REG_SZ);
         goto nextdevice;
      }

      ValueName.Length = ValueName.MaximumLength = pValueInformation->DataLength;
      ValueName.Buffer = (PWCHAR)pValueInformation->Data;
      if (ValueName.Length >= sizeof(WCHAR) && ValueName.Buffer[ValueName.Length / sizeof(WCHAR) - 1] == UNICODE_NULL)
         ValueName.Length -= sizeof(WCHAR);
      if (RtlCompareUnicodeString(&ValueName, &AcpiBiosIdentifier, FALSE) == 0)
      {
         DPRINT("Found ACPI BIOS\n");
         ret = TRUE;
         goto cleanup;
      }

nextdevice:
      ZwClose(hDeviceKey);
      hDeviceKey = NULL;
   }

cleanup:
   if (pDeviceInformation)
      ExFreePool(pDeviceInformation);
   if (pValueInformation)
      ExFreePool(pValueInformation);
   if (hDevicesKey)
      ZwClose(hDevicesKey);
   if (hDeviceKey)
      ZwClose(hDeviceKey);
   return ret;
#endif
}

static NTSTATUS INIT_FUNCTION
IopUpdateRootKey(VOID)
{
   UNICODE_STRING EnumU = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Enum");
   UNICODE_STRING RootPathU = RTL_CONSTANT_STRING(L"Root");
   UNICODE_STRING MultiKeyPathU = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System\\MultifunctionAdapter");
   UNICODE_STRING DeviceDescU = RTL_CONSTANT_STRING(L"DeviceDesc");
   UNICODE_STRING HardwareIDU = RTL_CONSTANT_STRING(L"HardwareID");
   UNICODE_STRING LogConfU = RTL_CONSTANT_STRING(L"LogConf");
   UNICODE_STRING HalAcpiDevice = RTL_CONSTANT_STRING(L"ACPI_HAL");
   UNICODE_STRING HalAcpiId = RTL_CONSTANT_STRING(L"0000");
   UNICODE_STRING HalAcpiDeviceDesc = RTL_CONSTANT_STRING(L"HAL ACPI");
   UNICODE_STRING HalAcpiHardwareID = RTL_CONSTANT_STRING(L"*PNP0C08\0");
   OBJECT_ATTRIBUTES ObjectAttributes;
   HANDLE hEnum, hRoot, hHalAcpiDevice, hHalAcpiId, hLogConf;
   NTSTATUS Status;

   InitializeObjectAttributes(&ObjectAttributes, &EnumU, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
   Status = ZwCreateKey(&hEnum, KEY_CREATE_SUB_KEY, &ObjectAttributes, 0, NULL, 0, NULL);
   if (!NT_SUCCESS(Status))
   {
      DPRINT1("ZwCreateKey() failed with status 0x%08lx\n", Status);
      return Status;
   }

   InitializeObjectAttributes(&ObjectAttributes, &RootPathU, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, hEnum, NULL);
   Status = ZwCreateKey(&hRoot, KEY_CREATE_SUB_KEY, &ObjectAttributes, 0, NULL, 0, NULL);
   ZwClose(hEnum);
   if (!NT_SUCCESS(Status))
   {
      DPRINT1("ZwOpenKey() failed with status 0x%08lx\n", Status);
      return Status;
   }

   if (IopIsAcpiComputer())
   {
      InitializeObjectAttributes(&ObjectAttributes, &HalAcpiDevice, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, hRoot, NULL);
      Status = ZwCreateKey(&hHalAcpiDevice, KEY_CREATE_SUB_KEY, &ObjectAttributes, 0, NULL, 0, NULL);
      ZwClose(hRoot);
      if (!NT_SUCCESS(Status))
         return Status;
      InitializeObjectAttributes(&ObjectAttributes, &HalAcpiId, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, hHalAcpiDevice, NULL);
      Status = ZwCreateKey(&hHalAcpiId, KEY_CREATE_SUB_KEY, &ObjectAttributes, 0, NULL, 0, NULL);
      ZwClose(hHalAcpiDevice);
      if (!NT_SUCCESS(Status))
         return Status;
      Status = ZwSetValueKey(hHalAcpiId, &DeviceDescU, 0, REG_SZ, HalAcpiDeviceDesc.Buffer, HalAcpiDeviceDesc.MaximumLength);
      if (NT_SUCCESS(Status))
         Status = ZwSetValueKey(hHalAcpiId, &HardwareIDU, 0, REG_MULTI_SZ, HalAcpiHardwareID.Buffer, HalAcpiHardwareID.MaximumLength);
      if (NT_SUCCESS(Status))
      {
          InitializeObjectAttributes(&ObjectAttributes, &LogConfU, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, hHalAcpiId, NULL);
          Status = ZwCreateKey(&hLogConf, 0, &ObjectAttributes, 0, NULL, REG_OPTION_VOLATILE, NULL);
          if (NT_SUCCESS(Status))
              ZwClose(hLogConf);
      }
      ZwClose(hHalAcpiId);
      return Status;
   }
   else
   {
        Status = IopOpenRegistryKeyEx(&hEnum, NULL, &MultiKeyPathU, KEY_ENUMERATE_SUB_KEYS);
        if (!NT_SUCCESS(Status))
        {
            /* Nothing to do, don't return with an error status */
            DPRINT("ZwOpenKey() failed with status 0x%08lx\n", Status);
            ZwClose(hRoot);
            return STATUS_SUCCESS;
        }
        Status = IopEnumerateDetectedDevices(
            hEnum,
            NULL,
            hRoot,
            TRUE,
            NULL,
            0);
        ZwClose(hEnum);
        ZwClose(hRoot);
        return Status;
   }
}

NTSTATUS
NTAPI
IopOpenRegistryKeyEx(PHANDLE KeyHandle,
                     HANDLE ParentKey,
                     PUNICODE_STRING Name,
                     ACCESS_MASK DesiredAccess)
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;

    PAGED_CODE();

    *KeyHandle = NULL;

    InitializeObjectAttributes(&ObjectAttributes,
        Name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        ParentKey,
        NULL);

    Status = ZwOpenKey(KeyHandle, DesiredAccess, &ObjectAttributes);

    return Status;
}

NTSTATUS
NTAPI
IopGetRegistryValue(IN HANDLE Handle,
                    IN PWSTR ValueName,
                    OUT PKEY_VALUE_FULL_INFORMATION *Information)
{
    UNICODE_STRING ValueString;
    NTSTATUS Status;
    PKEY_VALUE_FULL_INFORMATION FullInformation;
    ULONG Size;
    PAGED_CODE();

    RtlInitUnicodeString(&ValueString, ValueName);

    Status = ZwQueryValueKey(Handle,
                             &ValueString,
                             KeyValueFullInformation,
                             NULL,
                             0,
                             &Size);
    if ((Status != STATUS_BUFFER_OVERFLOW) &&
        (Status != STATUS_BUFFER_TOO_SMALL))
    {
        return Status;
    }

    FullInformation = ExAllocatePool(NonPagedPool, Size);
    if (!FullInformation) return STATUS_INSUFFICIENT_RESOURCES;

    Status = ZwQueryValueKey(Handle,
                             &ValueString,
                             KeyValueFullInformation,
                             FullInformation,
                             Size,
                             &Size);
    if (!NT_SUCCESS(Status))
    {
        ExFreePool(FullInformation);
        return Status;
    }

    *Information = FullInformation;
    return STATUS_SUCCESS;
}

static NTSTATUS INIT_FUNCTION
NTAPI
PnpDriverInitializeEmpty(IN struct _DRIVER_OBJECT *DriverObject, IN PUNICODE_STRING RegistryPath)
{
   return STATUS_SUCCESS;
}

VOID INIT_FUNCTION
PnpInit(VOID)
{
    PDEVICE_OBJECT Pdo;
    NTSTATUS Status;

    DPRINT("PnpInit()\n");

    KeInitializeSpinLock(&IopDeviceTreeLock);
	ExInitializeFastMutex(&IopBusTypeGuidListLock);
	
    /* Initialize the Bus Type GUID List */
    IopBusTypeGuidList = ExAllocatePool(NonPagedPool, sizeof(IO_BUS_TYPE_GUID_LIST));
    if (!IopBusTypeGuidList) {
	DPRINT1("ExAllocatePool() failed\n");
	KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, STATUS_NO_MEMORY, 0, 0, 0);
    }

    RtlZeroMemory(IopBusTypeGuidList, sizeof(IO_BUS_TYPE_GUID_LIST));
    ExInitializeFastMutex(&IopBusTypeGuidList->Lock);

    /* Initialize PnP-Event notification support */
    Status = IopInitPlugPlayEvents();
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("IopInitPlugPlayEvents() failed\n");
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    /*
    * Create root device node
    */

    Status = IopCreateDriver(NULL, PnpDriverInitializeEmpty, NULL, 0, 0, &IopRootDriverObject);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("IoCreateDriverObject() failed\n");
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    Status = IoCreateDevice(IopRootDriverObject, 0, NULL, FILE_DEVICE_CONTROLLER,
        0, FALSE, &Pdo);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("IoCreateDevice() failed\n");
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    Status = IopCreateDeviceNode(NULL, Pdo, NULL, &IopRootDeviceNode);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Insufficient resources\n");
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    if (!RtlCreateUnicodeString(&IopRootDeviceNode->InstancePath,
        L"HTREE\\ROOT\\0"))
    {
        DPRINT1("Failed to create the instance path!\n");
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, STATUS_NO_MEMORY, 0, 0, 0);
    }

    /* Report the device to the user-mode pnp manager */
    IopQueueTargetDeviceEvent(&GUID_DEVICE_ARRIVAL,
        &IopRootDeviceNode->InstancePath);

    IopRootDeviceNode->PhysicalDeviceObject->Flags |= DO_BUS_ENUMERATED_DEVICE;
    PnpRootDriverEntry(IopRootDriverObject, NULL);
    IopRootDeviceNode->PhysicalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    IopRootDriverObject->DriverExtension->AddDevice(
        IopRootDriverObject,
        IopRootDeviceNode->PhysicalDeviceObject);

    /* Move information about devices detected by Freeloader to SYSTEM\CurrentControlSet\Root\ */
    Status = IopUpdateRootKey();
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("IopUpdateRootKey() failed\n");
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }
}

RTL_GENERIC_COMPARE_RESULTS
NTAPI
PiCompareInstancePath(IN PRTL_AVL_TABLE Table,
                      IN PVOID FirstStruct,
                      IN PVOID SecondStruct)
{
    /* FIXME: TODO */
    ASSERT(FALSE);
    return 0;
}

//
//  The allocation function is called by the generic table package whenever
//  it needs to allocate memory for the table.
//

PVOID
NTAPI
PiAllocateGenericTableEntry(IN PRTL_AVL_TABLE Table,
                            IN CLONG ByteSize)
{
    /* FIXME: TODO */
    ASSERT(FALSE);
    return NULL;
}

VOID
NTAPI
PiFreeGenericTableEntry(IN PRTL_AVL_TABLE Table,
                        IN PVOID Buffer)
{
    /* FIXME: TODO */
    ASSERT(FALSE);
}

VOID
NTAPI
PpInitializeDeviceReferenceTable(VOID)
{
    /* Setup the guarded mutex and AVL table */
    KeInitializeGuardedMutex(&PpDeviceReferenceTableLock);
    RtlInitializeGenericTableAvl(
        &PpDeviceReferenceTable,
        (PRTL_AVL_COMPARE_ROUTINE)PiCompareInstancePath,
        (PRTL_AVL_ALLOCATE_ROUTINE)PiAllocateGenericTableEntry,
        (PRTL_AVL_FREE_ROUTINE)PiFreeGenericTableEntry,
        NULL);
}

BOOLEAN
NTAPI
PiInitPhase0(VOID)
{
    /* Initialize the resource when accessing device registry data */
    ExInitializeResourceLite(&PpRegistryDeviceResource);

    /* Setup the device reference AVL table */
    PpInitializeDeviceReferenceTable();
    return TRUE;
}

BOOLEAN
NTAPI
PpInitSystem(VOID)
{
    /* Check the initialization phase */
    switch (ExpInitializationPhase)
    {
    case 0:

        /* Do Phase 0 */
        return PiInitPhase0();

    case 1:

        /* Do Phase 1 */
        return TRUE;
        //return PiInitPhase1();

    default:

        /* Don't know any other phase! Bugcheck! */
        KeBugCheck(UNEXPECTED_INITIALIZATION_CALL);
        return FALSE;
    }
}

/* PUBLIC FUNCTIONS **********************************************************/

/*
 * @implemented
 */
NTSTATUS
NTAPI
IoGetDeviceProperty(IN PDEVICE_OBJECT DeviceObject,
                    IN DEVICE_REGISTRY_PROPERTY DeviceProperty,
                    IN ULONG BufferLength,
                    OUT PVOID PropertyBuffer,
                    OUT PULONG ResultLength)
{
    PDEVICE_NODE DeviceNode = IopGetDeviceNode(DeviceObject);
    DEVICE_CAPABILITIES DeviceCaps;
    ULONG Length;
    PVOID Data = NULL;
    PWSTR Ptr;
    NTSTATUS Status;
    POBJECT_NAME_INFORMATION ObjectNameInfo = NULL;
    ULONG RequiredLength, ObjectNameInfoLength;

    DPRINT("IoGetDeviceProperty(0x%p %d)\n", DeviceObject, DeviceProperty);

    *ResultLength = 0;

    if (DeviceNode == NULL)
        return STATUS_INVALID_DEVICE_REQUEST;

    switch (DeviceProperty)
    {
    case DevicePropertyBusNumber:
        Length = sizeof(ULONG);
        Data = &DeviceNode->ChildBusNumber;
        break;

        /* Complete, untested */
    case DevicePropertyBusTypeGuid:
        /* Sanity check */
        if ((DeviceNode->ChildBusTypeIndex != 0xFFFF) &&
            (DeviceNode->ChildBusTypeIndex < IopBusTypeGuidList->GuidCount))
        {
            /* Return the GUID */
            *ResultLength = sizeof(GUID);

            /* Check if the buffer given was large enough */
            if (BufferLength < *ResultLength)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            /* Copy the GUID */
            RtlCopyMemory(PropertyBuffer,
                &(IopBusTypeGuidList->Guids[DeviceNode->ChildBusTypeIndex]),
                sizeof(GUID));
            return STATUS_SUCCESS;
        }
        else
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        break;

    case DevicePropertyLegacyBusType:
        Length = sizeof(INTERFACE_TYPE);
        Data = &DeviceNode->ChildInterfaceType;
        break;

    case DevicePropertyAddress:
        /* Query the device caps */
        Status = IopQueryDeviceCapabilities(DeviceNode, &DeviceCaps);
        if (NT_SUCCESS(Status) && (DeviceCaps.Address != MAXULONG))
        {
            /* Return length */
            *ResultLength = sizeof(ULONG);

            /* Check if the buffer given was large enough */
            if (BufferLength < *ResultLength)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            /* Return address */
            *(PULONG)PropertyBuffer = DeviceCaps.Address;
            return STATUS_SUCCESS;
        }
        else
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        break;

//    case DevicePropertyUINumber:
//      if (DeviceNode->CapabilityFlags == NULL)
//         return STATUS_INVALID_DEVICE_REQUEST;
//      Length = sizeof(ULONG);
//      Data = &DeviceNode->CapabilityFlags->UINumber;
//      break;

    case DevicePropertyClassName:
    case DevicePropertyClassGuid:
    case DevicePropertyDriverKeyName:
    case DevicePropertyManufacturer:
    case DevicePropertyFriendlyName:
    case DevicePropertyHardwareID:
    case DevicePropertyCompatibleIDs:
    case DevicePropertyDeviceDescription:
    case DevicePropertyLocationInformation:
    case DevicePropertyUINumber:
        {
            LPCWSTR RegistryPropertyName;
            UNICODE_STRING EnumRoot = RTL_CONSTANT_STRING(ENUM_ROOT);
            UNICODE_STRING ValueName;
            KEY_VALUE_PARTIAL_INFORMATION *ValueInformation;
            ULONG ValueInformationLength;
            HANDLE KeyHandle, EnumRootHandle;
            NTSTATUS Status;

            switch (DeviceProperty)
            {
            case DevicePropertyClassName:
                RegistryPropertyName = L"Class"; break;
            case DevicePropertyClassGuid:
                RegistryPropertyName = L"ClassGuid"; break;
            case DevicePropertyDriverKeyName:
                RegistryPropertyName = L"Driver"; break;
            case DevicePropertyManufacturer:
                RegistryPropertyName = L"Mfg"; break;
            case DevicePropertyFriendlyName:
                RegistryPropertyName = L"FriendlyName"; break;
            case DevicePropertyHardwareID:
                RegistryPropertyName = L"HardwareID"; break;
            case DevicePropertyCompatibleIDs:
                RegistryPropertyName = L"CompatibleIDs"; break;
            case DevicePropertyDeviceDescription:
                RegistryPropertyName = L"DeviceDesc"; break;
            case DevicePropertyLocationInformation:
                RegistryPropertyName = L"LocationInformation"; break;
            case DevicePropertyUINumber:
                RegistryPropertyName = L"UINumber"; break;
            default:
                /* Should not happen */
                ASSERT(FALSE);
                return STATUS_UNSUCCESSFUL;
            }

            DPRINT("Registry property %S\n", RegistryPropertyName);

            /* Open Enum key */
            Status = IopOpenRegistryKeyEx(&EnumRootHandle, NULL,
                &EnumRoot, KEY_READ);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Error opening ENUM_ROOT, Status=0x%08x\n", Status);
                return Status;
            }

            /* Open instance key */
            Status = IopOpenRegistryKeyEx(&KeyHandle, EnumRootHandle,
                &DeviceNode->InstancePath, KEY_READ);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Error opening InstancePath, Status=0x%08x\n", Status);
                ZwClose(EnumRootHandle);
                return Status;
            }

            /* Allocate buffer to read as much data as required by the caller */
            ValueInformationLength = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION,
                Data[0]) + BufferLength;
            ValueInformation = ExAllocatePool(PagedPool, ValueInformationLength);
            if (!ValueInformation)
            {
                ZwClose(KeyHandle);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            /* Read the value */
            RtlInitUnicodeString(&ValueName, RegistryPropertyName);
            Status = ZwQueryValueKey(KeyHandle, &ValueName,
                KeyValuePartialInformation, ValueInformation,
                ValueInformationLength,
                &ValueInformationLength);
            ZwClose(KeyHandle);

            /* Return data */
            *ResultLength = ValueInformation->DataLength;

            if (!NT_SUCCESS(Status))
            {
                ExFreePool(ValueInformation);
                if (Status == STATUS_BUFFER_OVERFLOW)
                    return STATUS_BUFFER_TOO_SMALL;
                DPRINT1("Problem: Status=0x%08x, ResultLength = %d\n", Status, *ResultLength);
                return Status;
            }

            /* FIXME: Verify the value (NULL-terminated, correct format). */
            RtlCopyMemory(PropertyBuffer, ValueInformation->Data,
                ValueInformation->DataLength);
            ExFreePool(ValueInformation);

            return STATUS_SUCCESS;
        }

    case DevicePropertyBootConfiguration:
        Length = 0;
        if (DeviceNode->BootResources->Count != 0)
        {
            Length = CM_RESOURCE_LIST_SIZE(DeviceNode->BootResources);
        }
        Data = DeviceNode->BootResources;
        break;

        /* FIXME: use a translated boot configuration instead */
    case DevicePropertyBootConfigurationTranslated:
        Length = 0;
        if (DeviceNode->BootResources->Count != 0)
        {
            Length = CM_RESOURCE_LIST_SIZE(DeviceNode->BootResources);
        }
        Data = DeviceNode->BootResources;
        break;

    case DevicePropertyEnumeratorName:
        /* A buffer overflow can't happen here, since InstancePath
        * always contains the enumerator name followed by \\ */
        Ptr = wcschr(DeviceNode->InstancePath.Buffer, L'\\');
        ASSERT(Ptr);
        Length = (Ptr - DeviceNode->InstancePath.Buffer) * sizeof(WCHAR);
        Data = DeviceNode->InstancePath.Buffer;
        break;

    case DevicePropertyPhysicalDeviceObjectName:
        Status = ObQueryNameString(DeviceNode->PhysicalDeviceObject,
                                   NULL,
                                   0,
                                   &RequiredLength);
        if (Status == STATUS_SUCCESS)
        {
            Length = 0;
            Data = L"";
        }
        else if (Status == STATUS_INFO_LENGTH_MISMATCH)
        {
            ObjectNameInfoLength = RequiredLength;
            ObjectNameInfo = ExAllocatePool(PagedPool, ObjectNameInfoLength);
            if (!ObjectNameInfo)
                return STATUS_INSUFFICIENT_RESOURCES;

            Status = ObQueryNameString(DeviceNode->PhysicalDeviceObject,
                                       ObjectNameInfo,
                                       ObjectNameInfoLength,
                                       &RequiredLength);
            if (NT_SUCCESS(Status))
            {
                Length = ObjectNameInfo->Name.Length;
                Data = ObjectNameInfo->Name.Buffer;
            }
            else
                return Status;
        }
        else
            return Status;
        break;

    default:
        return STATUS_INVALID_PARAMETER_2;
    }

    /* Prepare returned values */
    *ResultLength = Length;
    if (BufferLength < Length)
    {
        if (ObjectNameInfo != NULL)
            ExFreePool(ObjectNameInfo);

        return STATUS_BUFFER_TOO_SMALL;
    }
    RtlCopyMemory(PropertyBuffer, Data, Length);

    /* NULL terminate the string (if required) */
    if (DeviceProperty == DevicePropertyEnumeratorName ||
        DeviceProperty == DevicePropertyPhysicalDeviceObjectName)
        ((LPWSTR)PropertyBuffer)[Length / sizeof(WCHAR)] = UNICODE_NULL;

    if (ObjectNameInfo != NULL)
        ExFreePool(ObjectNameInfo);

    return STATUS_SUCCESS;
}

/*
 * @unimplemented
 */
VOID
NTAPI
IoInvalidateDeviceState(IN PDEVICE_OBJECT PhysicalDeviceObject)
{
    UNIMPLEMENTED;
}

/**
 * @name IoOpenDeviceRegistryKey
 *
 * Open a registry key unique for a specified driver or device instance.
 *
 * @param DeviceObject   Device to get the registry key for.
 * @param DevInstKeyType Type of the key to return.
 * @param DesiredAccess  Access mask (eg. KEY_READ | KEY_WRITE).
 * @param DevInstRegKey  Handle to the opened registry key on
 *                       successful return.
 *
 * @return Status.
 *
 * @implemented
 */
NTSTATUS
NTAPI
IoOpenDeviceRegistryKey(IN PDEVICE_OBJECT DeviceObject,
                        IN ULONG DevInstKeyType,
                        IN ACCESS_MASK DesiredAccess,
                        OUT PHANDLE DevInstRegKey)
{
   static WCHAR RootKeyName[] =
      L"\\Registry\\Machine\\System\\CurrentControlSet\\";
   static WCHAR ProfileKeyName[] =
      L"Hardware Profiles\\Current\\System\\CurrentControlSet\\";
   static WCHAR ClassKeyName[] = L"Control\\Class\\";
   static WCHAR EnumKeyName[] = L"Enum\\";
   static WCHAR DeviceParametersKeyName[] = L"Device Parameters";
   ULONG KeyNameLength;
   LPWSTR KeyNameBuffer;
   UNICODE_STRING KeyName;
   ULONG DriverKeyLength;
   OBJECT_ATTRIBUTES ObjectAttributes;
   PDEVICE_NODE DeviceNode = NULL;
   NTSTATUS Status;

   DPRINT("IoOpenDeviceRegistryKey() called\n");

   if ((DevInstKeyType & (PLUGPLAY_REGKEY_DEVICE | PLUGPLAY_REGKEY_DRIVER)) == 0)
   {
       DPRINT1("IoOpenDeviceRegistryKey(): got wrong params, exiting... \n");
       return STATUS_INVALID_PARAMETER;
   }

   /*
    * Calculate the length of the base key name. This is the full
    * name for driver key or the name excluding "Device Parameters"
    * subkey for device key.
    */

   KeyNameLength = sizeof(RootKeyName);
   if (DevInstKeyType & PLUGPLAY_REGKEY_CURRENT_HWPROFILE)
      KeyNameLength += sizeof(ProfileKeyName) - sizeof(UNICODE_NULL);
   if (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER)
   {
      KeyNameLength += sizeof(ClassKeyName) - sizeof(UNICODE_NULL);
      Status = IoGetDeviceProperty(DeviceObject, DevicePropertyDriverKeyName,
                                   0, NULL, &DriverKeyLength);
      if (Status != STATUS_BUFFER_TOO_SMALL)
         return Status;
      KeyNameLength += DriverKeyLength;
   }
   else
   {
      DeviceNode = IopGetDeviceNode(DeviceObject);
      KeyNameLength += sizeof(EnumKeyName) - sizeof(UNICODE_NULL) +
                       DeviceNode->InstancePath.Length;
   }

   /*
    * Now allocate the buffer for the key name...
    */

   KeyNameBuffer = ExAllocatePool(PagedPool, KeyNameLength);
   if (KeyNameBuffer == NULL)
      return STATUS_INSUFFICIENT_RESOURCES;

   KeyName.Length = 0;
   KeyName.MaximumLength = (USHORT)KeyNameLength;
   KeyName.Buffer = KeyNameBuffer;

   /*
    * ...and build the key name.
    */

   KeyName.Length += sizeof(RootKeyName) - sizeof(UNICODE_NULL);
   RtlCopyMemory(KeyNameBuffer, RootKeyName, KeyName.Length);

   if (DevInstKeyType & PLUGPLAY_REGKEY_CURRENT_HWPROFILE)
      RtlAppendUnicodeToString(&KeyName, ProfileKeyName);

   if (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER)
   {
      RtlAppendUnicodeToString(&KeyName, ClassKeyName);
      Status = IoGetDeviceProperty(DeviceObject, DevicePropertyDriverKeyName,
                                   DriverKeyLength, KeyNameBuffer +
                                   (KeyName.Length / sizeof(WCHAR)),
                                   &DriverKeyLength);
      if (!NT_SUCCESS(Status))
      {
         DPRINT1("Call to IoGetDeviceProperty() failed with Status 0x%08lx\n", Status);
         ExFreePool(KeyNameBuffer);
         return Status;
      }
      KeyName.Length += (USHORT)DriverKeyLength - sizeof(UNICODE_NULL);
   }
   else
   {
      RtlAppendUnicodeToString(&KeyName, EnumKeyName);
      Status = RtlAppendUnicodeStringToString(&KeyName, &DeviceNode->InstancePath);
      if (DeviceNode->InstancePath.Length == 0)
      {
         ExFreePool(KeyNameBuffer);
         return Status;
      }
   }

   /*
    * Open the base key.
    */
   Status = IopOpenRegistryKeyEx(DevInstRegKey, NULL, &KeyName, DesiredAccess);
   if (!NT_SUCCESS(Status))
   {
      DPRINT1("IoOpenDeviceRegistryKey(%wZ): Base key doesn't exist, exiting... (Status 0x%08lx)\n", &KeyName, Status);
      ExFreePool(KeyNameBuffer);
      return Status;
   }
   ExFreePool(KeyNameBuffer);

   /*
    * For driver key we're done now.
    */

   if (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER)
      return Status;

   /*
    * Let's go further. For device key we must open "Device Parameters"
    * subkey and create it if it doesn't exist yet.
    */

   RtlInitUnicodeString(&KeyName, DeviceParametersKeyName);
   InitializeObjectAttributes(&ObjectAttributes, &KeyName,
                              OBJ_CASE_INSENSITIVE, *DevInstRegKey, NULL);
   Status = ZwCreateKey(DevInstRegKey, DesiredAccess, &ObjectAttributes,
                        0, NULL, REG_OPTION_NON_VOLATILE, NULL);
   ZwClose(ObjectAttributes.RootDirectory);

   return Status;
}

/*
 * @unimplemented
 */
VOID
NTAPI
IoRequestDeviceEject(IN PDEVICE_OBJECT PhysicalDeviceObject)
{
   UNIMPLEMENTED;
}

/*
 * @implemented
 */
VOID
NTAPI
IoInvalidateDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_RELATION_TYPE Type)
{
    PIO_WORKITEM WorkItem;
    PINVALIDATE_DEVICE_RELATION_DATA Data;

    Data = ExAllocatePool(PagedPool, sizeof(INVALIDATE_DEVICE_RELATION_DATA));
    if (!Data)
        return;
    WorkItem = IoAllocateWorkItem(DeviceObject);
    if (!WorkItem)
    {
        ExFreePool(Data);
        return;
    }

    ObReferenceObject(DeviceObject);
    Data->DeviceObject = DeviceObject;
    Data->Type = Type;
    Data->WorkItem = WorkItem;

    IoQueueWorkItem(
        WorkItem,
        IopAsynchronousInvalidateDeviceRelations,
        DelayedWorkQueue,
        Data);
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
IoSynchronousInvalidateDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_RELATION_TYPE Type)
{
    PAGED_CODE();
 
    switch (Type)
    {
        case BusRelations:
            /* Enumerate the device */
            return IopEnumerateDevice(DeviceObject);
        case PowerRelations:
             /* Not handled yet */
             return STATUS_NOT_IMPLEMENTED;
        case TargetDeviceRelation:
            /* Nothing to do */
            return STATUS_SUCCESS;
        default:
            /* Ejection relations are not supported */
            return STATUS_NOT_SUPPORTED;
    }
}

/*
 * @unimplemented
 */
BOOLEAN
NTAPI
IoTranslateBusAddress(IN INTERFACE_TYPE InterfaceType,
                      IN ULONG BusNumber,
                      IN PHYSICAL_ADDRESS BusAddress,
                      IN OUT PULONG AddressSpace,
                      OUT PPHYSICAL_ADDRESS TranslatedAddress)
{
    UNIMPLEMENTED;
    return FALSE;
}
