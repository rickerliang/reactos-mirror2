/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS NT User-Mode Library
 * FILE:            dll/ntdll/ldr/ldrinit.c
 * PURPOSE:         User-Mode Process/Thread Startup
 * PROGRAMMERS:     Alex Ionescu (alex@relsoft.net)
 *                  Aleksey Bragin (aleksey@reactos.org)
 */

/* INCLUDES *****************************************************************/

#include <ntdll.h>
#define NDEBUG
#include <debug.h>

/* GLOBALS *******************************************************************/

HKEY ImageExecOptionsKey;
HKEY Wow64ExecOptionsKey;
UNICODE_STRING ImageExecOptionsString = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options");
UNICODE_STRING Wow64OptionsString = RTL_CONSTANT_STRING(L"");
UNICODE_STRING NtDllString = RTL_CONSTANT_STRING(L"ntdll.dll");

BOOLEAN LdrpInLdrInit;
LONG LdrpProcessInitialized;
BOOLEAN LdrpLoaderLockInit;
BOOLEAN LdrpLdrDatabaseIsSetup;

BOOLEAN LdrpDllValidation;

PLDR_DATA_TABLE_ENTRY LdrpImageEntry;
PUNICODE_STRING LdrpTopLevelDllBeingLoaded;
extern PTEB LdrpTopLevelDllBeingLoadedTeb; // defined in rtlsupp.c!
PLDR_DATA_TABLE_ENTRY LdrpCurrentDllInitializer;
PLDR_DATA_TABLE_ENTRY LdrpNtDllDataTableEntry;

RTL_BITMAP TlsBitMap;
RTL_BITMAP TlsExpansionBitMap;
RTL_BITMAP FlsBitMap;
BOOLEAN LdrpImageHasTls;
LIST_ENTRY LdrpTlsList;
ULONG LdrpNumberOfTlsEntries;
ULONG LdrpNumberOfProcessors;
PVOID NtDllBase;
LARGE_INTEGER RtlpTimeout;
BOOLEAN RtlpTimeoutDisable;
LIST_ENTRY LdrpHashTable[LDR_HASH_TABLE_ENTRIES];
LIST_ENTRY LdrpDllNotificationList;
HANDLE LdrpKnownDllObjectDirectory;
UNICODE_STRING LdrpKnownDllPath;
WCHAR LdrpKnownDllPathBuffer[128];
UNICODE_STRING LdrpDefaultPath;

PEB_LDR_DATA PebLdr;

RTL_CRITICAL_SECTION_DEBUG LdrpLoaderLockDebug;
RTL_CRITICAL_SECTION LdrpLoaderLock =
{
    &LdrpLoaderLockDebug,
    -1,
    0,
    0,
    0,
    0
};
RTL_CRITICAL_SECTION FastPebLock;

BOOLEAN ShowSnaps;

ULONG LdrpFatalHardErrorCount;

//extern LIST_ENTRY RtlCriticalSectionList;

VOID RtlpInitializeVectoredExceptionHandling(VOID);
VOID NTAPI RtlpInitDeferedCriticalSection(VOID);
VOID RtlInitializeHeapManager(VOID);
extern BOOLEAN RtlpPageHeapEnabled;
extern ULONG RtlpDphGlobalFlags;

NTSTATUS LdrPerformRelocations(PIMAGE_NT_HEADERS NTHeaders, PVOID ImageBase);
NTSTATUS NTAPI
LdrpInitializeProcess_(PCONTEXT Context,
                      PVOID SystemArgument1);


/* FUNCTIONS *****************************************************************/

/*
 * @implemented
 */
NTSTATUS
NTAPI
LdrOpenImageFileOptionsKey(IN PUNICODE_STRING SubKey,
                           IN BOOLEAN Wow64,
                           OUT PHKEY NewKeyHandle)
{
    PHKEY RootKeyLocation;
    HANDLE RootKey;
    UNICODE_STRING SubKeyString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    PWCHAR p1;

    /* Check which root key to open */
    if (Wow64)
        RootKeyLocation = &Wow64ExecOptionsKey;
    else
        RootKeyLocation = &ImageExecOptionsKey;

    /* Get the current key */
    RootKey = *RootKeyLocation;

    /* Setup the object attributes */
    InitializeObjectAttributes(&ObjectAttributes,
                               Wow64 ? 
                               &Wow64OptionsString : &ImageExecOptionsString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    /* Open the root key */
    Status = ZwOpenKey(&RootKey, KEY_ENUMERATE_SUB_KEYS, &ObjectAttributes);
    if (NT_SUCCESS(Status))
    {
        /* Write the key handle */
        if (_InterlockedCompareExchange((LONG*)RootKeyLocation, (LONG)RootKey, 0) != 0)
        {
            /* Someone already opened it, use it instead */
            NtClose(RootKey);
            RootKey = *RootKeyLocation;
        }

        /* Extract the name */
        SubKeyString = *SubKey;
        p1 = (PWCHAR)((ULONG_PTR)SubKeyString.Buffer + SubKeyString.Length);
        while (SubKey->Length)
        {
            if (p1[-1] == L'\\') break;
            p1--;
            SubKeyString.Length -= sizeof(*p1);
        }
        SubKeyString.Buffer = p1;
        SubKeyString.Length = SubKeyString.MaximumLength - SubKeyString.Length - sizeof(WCHAR);

        /* Setup the object attributes */
        InitializeObjectAttributes(&ObjectAttributes,
                                   &SubKeyString,
                                   OBJ_CASE_INSENSITIVE,
                                   RootKey,
                                   NULL);

        /* Open the setting key */
        Status = ZwOpenKey((PHANDLE)NewKeyHandle, GENERIC_READ, &ObjectAttributes);
    }

    /* Return to caller */
    return Status;
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
LdrQueryImageFileKeyOption(IN HKEY KeyHandle,
                           IN PCWSTR ValueName,
                           IN ULONG Type,
                           OUT PVOID Buffer,
                           IN ULONG BufferSize,
                           OUT PULONG ReturnedLength OPTIONAL)
{
    ULONG KeyInfo[256];
    UNICODE_STRING ValueNameString, IntegerString;
    ULONG KeyInfoSize, ResultSize;
    PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION)&KeyInfo;
    BOOLEAN FreeHeap = FALSE;
    NTSTATUS Status;

    /* Build a string for the value name */
    Status = RtlInitUnicodeStringEx(&ValueNameString, ValueName);
    if (!NT_SUCCESS(Status)) return Status;

    /* Query the value */
    Status = NtQueryValueKey(KeyHandle,
                             &ValueNameString,
                             KeyValuePartialInformation,
                             KeyValueInformation,
                             sizeof(KeyInfo),
                             &ResultSize);
    if (Status == STATUS_BUFFER_OVERFLOW)
    {
        /* Our local buffer wasn't enough, allocate one */
        KeyInfoSize = sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                      KeyValueInformation->DataLength;
        KeyValueInformation = RtlAllocateHeap(RtlGetProcessHeap(),
                                              0,
                                              KeyInfoSize);
        if (KeyInfo == NULL)
        {
            /* Give up this time */
            Status = STATUS_NO_MEMORY;
        }

        /* Try again */
        Status = NtQueryValueKey(KeyHandle,
                                 &ValueNameString,
                                 KeyValuePartialInformation,
                                 KeyValueInformation,
                                 KeyInfoSize,
                                 &ResultSize);
        FreeHeap = TRUE;
    }

    /* Check for success */
    if (NT_SUCCESS(Status))
    {
        /* Handle binary data */
        if (KeyValueInformation->Type == REG_BINARY)
        {
            /* Check validity */
            if ((Buffer) && (KeyValueInformation->DataLength <= BufferSize))
            {
                /* Copy into buffer */
                RtlMoveMemory(Buffer,
                              &KeyValueInformation->Data,
                              KeyValueInformation->DataLength);
            }
            else
            {
                Status = STATUS_BUFFER_OVERFLOW;
            }

            /* Copy the result length */
            if (ReturnedLength) *ReturnedLength = KeyValueInformation->DataLength;
        }
        else if (KeyValueInformation->Type == REG_DWORD)
        {
            /* Check for valid type */
            if (KeyValueInformation->Type != Type)
            {
                /* Error */
                Status = STATUS_OBJECT_TYPE_MISMATCH;
            }
            else
            {
                /* Check validity */
                if ((Buffer) &&
                    (BufferSize == sizeof(ULONG)) &&
                    (KeyValueInformation->DataLength <= BufferSize))
                {
                    /* Copy into buffer */
                    RtlMoveMemory(Buffer,
                                  &KeyValueInformation->Data,
                                  KeyValueInformation->DataLength);
                }
                else
                {
                    Status = STATUS_BUFFER_OVERFLOW;
                }

                /* Copy the result length */
                if (ReturnedLength) *ReturnedLength = KeyValueInformation->DataLength;
            }
        }
        else if (KeyValueInformation->Type != REG_SZ)
        {
            /* We got something weird */
            Status = STATUS_OBJECT_TYPE_MISMATCH;
        }
        else
        {
            /*  String, check what you requested */
            if (Type == REG_DWORD)
            {
                /* Validate */
                if (BufferSize != sizeof(ULONG))
                {
                    /* Invalid size */
                    BufferSize = 0;
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                }
                else
                {
                    /* OK, we know what you want... */
                    IntegerString.Buffer = (PWSTR)KeyValueInformation->Data;
                    IntegerString.Length = KeyValueInformation->DataLength -
                                           sizeof(WCHAR);
                    IntegerString.MaximumLength = KeyValueInformation->DataLength;
                    Status = RtlUnicodeStringToInteger(&IntegerString, 0, (PULONG)Buffer);
                }
            }
            else
            {
                /* Validate */
                if (KeyValueInformation->DataLength > BufferSize)
                {
                    /* Invalid */
                    Status = STATUS_BUFFER_OVERFLOW;
                }
                else
                {
                    /* Set the size */
                    BufferSize = KeyValueInformation->DataLength;
                }

                /* Copy the string */
                RtlMoveMemory(Buffer, &KeyValueInformation->Data, BufferSize);
            }

            /* Copy the result length */
            if (ReturnedLength) *ReturnedLength = KeyValueInformation->DataLength;
        }
    }

    /* Check if buffer was in heap */
    if (FreeHeap) RtlFreeHeap(RtlGetProcessHeap(), 0, KeyValueInformation);

    /* Close key and return */
    NtClose(KeyHandle);
    return Status;
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
LdrQueryImageFileExecutionOptionsEx(IN PUNICODE_STRING SubKey,
                                    IN PCWSTR ValueName,
                                    IN ULONG Type,
                                    OUT PVOID Buffer,
                                    IN ULONG BufferSize,
                                    OUT PULONG ReturnedLength OPTIONAL,
                                    IN BOOLEAN Wow64)
{
    NTSTATUS Status;
    HKEY KeyHandle;

    /* Open a handle to the key */
    Status = LdrOpenImageFileOptionsKey(SubKey, Wow64, &KeyHandle);

    /* Check for success */
    if (NT_SUCCESS(Status))
    {
        /* Query the data */
        Status = LdrQueryImageFileKeyOption(KeyHandle,
                                            ValueName,
                                            Type,
                                            Buffer,
                                            BufferSize,
                                            ReturnedLength);

        /* Close the key */
        NtClose(KeyHandle);
    }

    /* Return to caller */
    return Status;
}

/*
 * @implemented
 */
NTSTATUS
NTAPI
LdrQueryImageFileExecutionOptions(IN PUNICODE_STRING SubKey,
                                  IN PCWSTR ValueName,
                                  IN ULONG Type,
                                  OUT PVOID Buffer,
                                  IN ULONG BufferSize,
                                  OUT PULONG ReturnedLength OPTIONAL)
{
    /* Call the newer function */
    return LdrQueryImageFileExecutionOptionsEx(SubKey,
                                               ValueName,
                                               Type,
                                               Buffer,
                                               BufferSize,
                                               ReturnedLength,
                                               FALSE);
}

VOID
NTAPI
LdrpEnsureLoaderLockIsHeld()
{
    // Ignored atm
}

PVOID
NTAPI
LdrpFetchAddressOfSecurityCookie(PVOID BaseAddress, ULONG SizeOfImage)
{
    PIMAGE_LOAD_CONFIG_DIRECTORY ConfigDir;
    ULONG DirSize;
    PVOID Cookie = NULL;

    /* Check NT header first */
    if (!RtlImageNtHeader(BaseAddress)) return NULL;

    /* Get the pointer to the config directory */
    ConfigDir = RtlImageDirectoryEntryToData(BaseAddress,
                                             TRUE,
                                             IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG,
                                             &DirSize);

    /* Check for sanity */
    if (!ConfigDir ||
        (DirSize != 64 && ConfigDir->Size != DirSize) ||
        (ConfigDir->Size < 0x48))
        return NULL;

    /* Now get the cookie */
    Cookie = (PVOID)ConfigDir->SecurityCookie;

    /* Check this cookie */
    if (Cookie == NULL ||
        (PCHAR)Cookie <= (PCHAR)BaseAddress ||
        (PCHAR)Cookie >= (PCHAR)BaseAddress + SizeOfImage)
    {
        Cookie = NULL;
    }

    /* Return validated security cookie */
    return Cookie;
}

PVOID
NTAPI
LdrpInitSecurityCookie(PLDR_DATA_TABLE_ENTRY LdrEntry)
{
    PVOID Cookie;

    /* Fetch address of the cookie */
    Cookie = LdrpFetchAddressOfSecurityCookie(LdrEntry->DllBase, LdrEntry->SizeOfImage);

    if (Cookie)
    {
        UNIMPLEMENTED;
        Cookie = NULL;
    }

    return Cookie;
}

NTSTATUS
NTAPI
LdrpRunInitializeRoutines(IN PCONTEXT Context OPTIONAL)
{
    PLDR_DATA_TABLE_ENTRY LocalArray[16];
    PLIST_ENTRY ListHead;
    PLIST_ENTRY NextEntry;
    PLDR_DATA_TABLE_ENTRY LdrEntry, *LdrRootEntry, OldInitializer;
    PVOID EntryPoint;
    ULONG Count, i;
    //ULONG BreakOnInit;
    NTSTATUS Status = STATUS_SUCCESS;
    PPEB Peb = NtCurrentPeb();
    RTL_CALLER_ALLOCATED_ACTIVATION_CONTEXT_STACK_FRAME_EXTENDED ActCtx;
    ULONG BreakOnDllLoad;
    PTEB OldTldTeb;
    BOOLEAN DllStatus;

    DPRINT("LdrpRunInitializeRoutines() called for %wZ\n", &LdrpImageEntry->BaseDllName);

    /* Check the Loader Lock */
    LdrpEnsureLoaderLockIsHeld();

     /* Get the number of entries to call */
    if ((Count = LdrpClearLoadInProgress()))
    {
        /* Check if we can use our local buffer */
        if (Count > 16)
        {
            /* Allocate space for all the entries */
            LdrRootEntry = RtlAllocateHeap(RtlGetProcessHeap(),
                                           0,
                                           Count * sizeof(LdrRootEntry));
            if (!LdrRootEntry) return STATUS_NO_MEMORY;
        }
        else
        {
            /* Use our local array */
            LdrRootEntry = LocalArray;
        }
    }
    else
    {
        /* Don't need one */
        LdrRootEntry = NULL;
    }

    /* Show debug message */
    if (ShowSnaps)
    {
        DPRINT1("[%x,%x] LDR: Real INIT LIST for Process %wZ\n",
                NtCurrentTeb()->RealClientId.UniqueThread,
                NtCurrentTeb()->RealClientId.UniqueProcess,
                &Peb->ProcessParameters->ImagePathName);
    }

    /* Loop in order */
    ListHead = &Peb->Ldr->InInitializationOrderModuleList;
    NextEntry = ListHead->Flink;
    i = 0;
    while (NextEntry != ListHead)
    {
        /* Get the Data Entry */
        LdrEntry = CONTAINING_RECORD(NextEntry, LDR_DATA_TABLE_ENTRY, InInitializationOrderModuleList);

        /* Check if we have a Root Entry */
        if (LdrRootEntry)
        {
            /* Check flags */
            if (!(LdrEntry->Flags & LDRP_ENTRY_PROCESSED))
            {
                /* Setup the Cookie for the DLL */
                LdrpInitSecurityCookie(LdrEntry);

                /* Check for valid entrypoint */
                if (LdrEntry->EntryPoint)
                {
                    /* Write in array */
                    LdrRootEntry[i] = LdrEntry;

                    /* Display debug message */
                    if (ShowSnaps)
                    {
                        DPRINT1("[%x,%x] LDR: %wZ init routine %p\n",
                                NtCurrentTeb()->RealClientId.UniqueThread,
                                NtCurrentTeb()->RealClientId.UniqueProcess,
                                &LdrEntry->FullDllName,
                                LdrEntry->EntryPoint);
                    }
                    i++;
                }
            }
        }

        /* Set the flag */
        LdrEntry->Flags |= LDRP_ENTRY_PROCESSED;
        NextEntry = NextEntry->Flink;
    }

    /* If we got a context, then we have to call Kernel32 for TS support */
    if (Context)
    {
        /* Check if we have one */
        //if (Kernel32ProcessInitPostImportfunction)
        //{
            /* Call it */
            //Kernel32ProcessInitPostImportfunction();
        //}

        /* Clear it */
        //Kernel32ProcessInitPostImportfunction = NULL;
        UNIMPLEMENTED;
    }

    /* No root entry? return */
    if (!LdrRootEntry) return STATUS_SUCCESS;

    /* Set the TLD TEB */
    OldTldTeb = LdrpTopLevelDllBeingLoadedTeb;
    LdrpTopLevelDllBeingLoadedTeb = NtCurrentTeb();

    /* Loop */
    i = 0;
    while (i < Count)
    {
        /* Get an entry */
        LdrEntry = LdrRootEntry[i];

        /* FIXME: Verify NX Compat */

        /* Move to next entry */
        i++;

        /* Get its entrypoint */
        EntryPoint = LdrEntry->EntryPoint;

        /* Are we being debugged? */
        BreakOnDllLoad = 0;
        if (Peb->BeingDebugged || Peb->ReadImageFileExecOptions)
        {
            /* Check if we should break on load */
            Status = LdrQueryImageFileExecutionOptions(&LdrEntry->BaseDllName,
                                                       L"BreakOnDllLoad",
                                                       REG_DWORD,
                                                       &BreakOnDllLoad,
                                                       sizeof(ULONG),
                                                       NULL);
            if (!NT_SUCCESS(Status)) BreakOnDllLoad = 0;
        }

        /* Break if aksed */
        if (BreakOnDllLoad)
        {
            /* Check if we should show a message */
            if (ShowSnaps)
            {
                DPRINT1("LDR: %wZ loaded.", &LdrEntry->BaseDllName);
                DPRINT1(" - About to call init routine at %p\n", EntryPoint);
            }

            /* Break in debugger */
            DbgBreakPoint();
        }

        /* Make sure we have an entrypoint */
        if (EntryPoint)
        {
            /* Save the old Dll Initializer and write the current one */
            OldInitializer = LdrpCurrentDllInitializer;
            LdrpCurrentDllInitializer = LdrEntry;

            /* Set up the Act Ctx */
            ActCtx.Size = sizeof(ActCtx);
            ActCtx.Frame.Flags = ACTCTX_FLAG_PROCESSOR_ARCHITECTURE_VALID;
            RtlZeroMemory(&ActCtx, sizeof(ActCtx));

            /* Activate the ActCtx */
            RtlActivateActivationContextUnsafeFast(&ActCtx,
                                                   LdrEntry->EntryPointActivationContext);

            /* Check if it has TLS */
            if (LdrEntry->TlsIndex && Context)
            {
                /* Call TLS */
                LdrpTlsCallback(LdrEntry->DllBase, DLL_PROCESS_ATTACH);
            }

            /* Call the Entrypoint */
            if (ShowSnaps)
            {
                DPRINT1("%wZ - Calling entry point at %p for DLL_PROCESS_ATTACH\n",
                        &LdrEntry->BaseDllName, EntryPoint);
            }
            DllStatus = LdrpCallDllEntry(EntryPoint,
                                         LdrEntry->DllBase,
                                         DLL_PROCESS_ATTACH,
                                         Context);

            /* Deactivate the ActCtx */
            RtlDeactivateActivationContextUnsafeFast(&ActCtx);

            /* Save the Current DLL Initializer */
            LdrpCurrentDllInitializer = OldInitializer;

            /* Mark the entry as processed */
            LdrEntry->Flags |= LDRP_PROCESS_ATTACH_CALLED;

            /* Fail if DLL init failed */
            if (!DllStatus)
            {
                DPRINT1("LDR: DLL_PROCESS_ATTACH for dll \"%wZ\" (InitRoutine: %p) failed\n",
                    &LdrEntry->BaseDllName, EntryPoint);

                Status = STATUS_DLL_INIT_FAILED;
                goto Quickie;
            }
        }
    }

    /* Loop in order */
    ListHead = &Peb->Ldr->InInitializationOrderModuleList;
    NextEntry = NextEntry->Flink;
    while (NextEntry != ListHead)
    {
        /* Get the Data Entrry */
        LdrEntry = CONTAINING_RECORD(NextEntry, LDR_DATA_TABLE_ENTRY, InInitializationOrderModuleList);

        /* FIXME: Verify NX Compat */
        // LdrpCheckNXCompatibility()

        /* Next entry */
        NextEntry = NextEntry->Flink;
    }

    /* Check for TLS */
    if (LdrpImageHasTls && Context)
    {
        /* Set up the Act Ctx */
        ActCtx.Size = sizeof(ActCtx);
        ActCtx.Frame.Flags = ACTCTX_FLAG_PROCESSOR_ARCHITECTURE_VALID;
        RtlZeroMemory(&ActCtx, sizeof(ActCtx));

        /* Activate the ActCtx */
        RtlActivateActivationContextUnsafeFast(&ActCtx,
                                               LdrpImageEntry->EntryPointActivationContext);

        /* Do TLS callbacks */
        LdrpTlsCallback(Peb->ImageBaseAddress, DLL_PROCESS_ATTACH);

        /* Deactivate the ActCtx */
        RtlDeactivateActivationContextUnsafeFast(&ActCtx);
    }

Quickie:
    /* Restore old TEB */
    LdrpTopLevelDllBeingLoadedTeb = OldTldTeb;

    /* Check if the array is in the heap */
    if (LdrRootEntry != LocalArray)
    {
        /* Free the array */
        RtlFreeHeap(RtlGetProcessHeap(), 0, LdrRootEntry);
    }

    /* Return to caller */
    DPRINT("LdrpRunInitializeRoutines() done\n");
    return Status;
}

NTSTATUS
NTAPI
LdrpInitializeTls(VOID)
{
    PLIST_ENTRY NextEntry, ListHead;
    PLDR_DATA_TABLE_ENTRY LdrEntry;
    PIMAGE_TLS_DIRECTORY TlsDirectory;
    PLDRP_TLS_DATA TlsData;
    ULONG Size;

    /* Initialize the TLS List */
    InitializeListHead(&LdrpTlsList);

    /* Loop all the modules */
    ListHead = &NtCurrentPeb()->Ldr->InLoadOrderModuleList;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        /* Get the entry */
        LdrEntry = CONTAINING_RECORD(NextEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        NextEntry = NextEntry->Flink;

        /* Get the TLS directory */
        TlsDirectory = RtlImageDirectoryEntryToData(LdrEntry->DllBase,
                                                    TRUE,
                                                    IMAGE_DIRECTORY_ENTRY_TLS,
                                                    &Size);

        /* Check if we have a directory */
        if (!TlsDirectory) continue;

        /* Check if the image has TLS */
        if (!LdrpImageHasTls) LdrpImageHasTls = TRUE;

        /* Show debug message */
        if (ShowSnaps)
        {
            DPRINT1("LDR: Tls Found in %wZ at %p\n",
                    &LdrEntry->BaseDllName,
                    TlsDirectory);
        }

        /* Allocate an entry */
        TlsData = RtlAllocateHeap(RtlGetProcessHeap(), 0, sizeof(LDRP_TLS_DATA));
        if (!TlsData) return STATUS_NO_MEMORY;

        /* Lock the DLL and mark it for TLS Usage */
        LdrEntry->LoadCount = -1;
        LdrEntry->TlsIndex = -1;

        /* Save the cached TLS data */
        TlsData->TlsDirectory = *TlsDirectory;
        InsertTailList(&LdrpTlsList, &TlsData->TlsLinks);

        /* Update the index */
        *(PLONG)TlsData->TlsDirectory.AddressOfIndex = LdrpNumberOfTlsEntries;
        TlsData->TlsDirectory.Characteristics = LdrpNumberOfTlsEntries++;
    }

    /* Done setting up TLS, allocate entries */
    return LdrpAllocateTls();
}

NTSTATUS
NTAPI
LdrpAllocateTls(VOID)
{
    PTEB Teb = NtCurrentTeb();
    PLIST_ENTRY NextEntry, ListHead;
    PLDRP_TLS_DATA TlsData;
    ULONG TlsDataSize;
    PVOID *TlsVector;

    /* Check if we have any entries */
    if (!LdrpNumberOfTlsEntries)
        return STATUS_SUCCESS;

    /* Allocate the vector array */
    TlsVector = RtlAllocateHeap(RtlGetProcessHeap(),
                                    0,
                                    LdrpNumberOfTlsEntries * sizeof(PVOID));
    if (!TlsVector) return STATUS_NO_MEMORY;
    Teb->ThreadLocalStoragePointer = TlsVector;

    /* Loop the TLS Array */
    ListHead = &LdrpTlsList;
    NextEntry = ListHead->Flink;
    while (NextEntry != ListHead)
    {
        /* Get the entry */
        TlsData = CONTAINING_RECORD(NextEntry, LDRP_TLS_DATA, TlsLinks);
        NextEntry = NextEntry->Flink;

        /* Allocate this vector */
        TlsDataSize = TlsData->TlsDirectory.EndAddressOfRawData - 
                      TlsData->TlsDirectory.StartAddressOfRawData;
        TlsVector[TlsData->TlsDirectory.Characteristics] = RtlAllocateHeap(RtlGetProcessHeap(),
                                                                           0,
                                                                           TlsDataSize);
        if (!TlsVector[TlsData->TlsDirectory.Characteristics])
        {
            /* Out of memory */
            return STATUS_NO_MEMORY;
        }

        /* Show debug message */
        if (ShowSnaps)
        {
            DPRINT1("LDR: TlsVector %x Index %d = %x copied from %x to %x\n",
                    TlsVector,
                    TlsData->TlsDirectory.Characteristics,
                    &TlsVector[TlsData->TlsDirectory.Characteristics],
                    TlsData->TlsDirectory.StartAddressOfRawData,
                    TlsVector[TlsData->TlsDirectory.Characteristics]);
        }

        /* Copy the data */
        RtlCopyMemory(TlsVector[TlsData->TlsDirectory.Characteristics],
                      (PVOID)TlsData->TlsDirectory.StartAddressOfRawData,
                      TlsDataSize);
    }

    /* Done */
    return STATUS_SUCCESS;
}

VOID
NTAPI
LdrpFreeTls(VOID)
{
    PLIST_ENTRY ListHead, NextEntry;
    PLDRP_TLS_DATA TlsData;
    PVOID *TlsVector;
    PTEB Teb = NtCurrentTeb();

    /* Get a pointer to the vector array */
    TlsVector = Teb->ThreadLocalStoragePointer;
    if (!TlsVector) return;

    /* Loop through it */
    ListHead = &LdrpTlsList;
    NextEntry = ListHead->Flink;
    while (NextEntry != ListHead)
    {
        TlsData = CONTAINING_RECORD(NextEntry, LDRP_TLS_DATA, TlsLinks);
        NextEntry = NextEntry->Flink;

        /* Free each entry */
        if (TlsVector[TlsData->TlsDirectory.Characteristics])
        {
            RtlFreeHeap(RtlGetProcessHeap(),
                        0,
                        TlsVector[TlsData->TlsDirectory.Characteristics]);
        }
    }

    /* Free the array itself */
    RtlFreeHeap(RtlGetProcessHeap(),
                0,
                TlsVector);
}

NTSTATUS
NTAPI
LdrpInitializeExecutionOptions(PUNICODE_STRING ImagePathName, PPEB Peb, PULONG Options)
{
    UNIMPLEMENTED;
    *Options = 0;
    return STATUS_SUCCESS;
}

VOID
NTAPI
LdrpValidateImageForMp(IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry)
{
    UNIMPLEMENTED;
}

NTSTATUS
NTAPI
LdrpInitializeProcess(IN PCONTEXT Context,
                      IN PVOID SystemArgument1)
{
    RTL_HEAP_PARAMETERS HeapParameters;
    ULONG ComSectionSize;
    //ANSI_STRING FunctionName = RTL_CONSTANT_STRING("BaseQueryModuleData");
    PVOID OldShimData;
    OBJECT_ATTRIBUTES ObjectAttributes;
    //UNICODE_STRING LocalFileName, FullImageName;
    HANDLE SymLinkHandle;
    //ULONG DebugHeapOnly;
    UNICODE_STRING CommandLine, NtSystemRoot, ImagePathName, FullPath, ImageFileName, KnownDllString;
    PPEB Peb = NtCurrentPeb();
    BOOLEAN IsDotNetImage = FALSE;
    BOOLEAN FreeCurDir = FALSE;
    //HKEY CompatKey;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    //LPWSTR ImagePathBuffer;
    ULONG ConfigSize;
    UNICODE_STRING CurrentDirectory;
    ULONG ExecuteOptions;
    ULONG HeapFlags;
    PIMAGE_NT_HEADERS NtHeader;
    LPWSTR NtDllName = NULL;
    NTSTATUS Status;
    NLSTABLEINFO NlsTable;
    PIMAGE_LOAD_CONFIG_DIRECTORY LoadConfig;
    PTEB Teb = NtCurrentTeb();
    PLIST_ENTRY ListHead;
    PLIST_ENTRY NextEntry;
    ULONG i;
    PWSTR ImagePath;
    ULONG DebugProcessHeapOnly = 0;
    WCHAR FullNtDllPath[MAX_PATH];
    PLDR_DATA_TABLE_ENTRY NtLdrEntry;
    PWCHAR Current;

    /* Set a NULL SEH Filter */
    RtlSetUnhandledExceptionFilter(NULL);

    /* Get the image path */
    ImagePath = Peb->ProcessParameters->ImagePathName.Buffer;

    /* Check if it's normalized */
    if (Peb->ProcessParameters->Flags & RTL_USER_PROCESS_PARAMETERS_NORMALIZED)
    {
        /* Normalize it*/
        ImagePath = (PWSTR)((ULONG_PTR)ImagePath + (ULONG_PTR)Peb->ProcessParameters);
    }

    /* Create a unicode string for the Image Path */
    ImagePathName.Length = Peb->ProcessParameters->ImagePathName.Length;
    ImagePathName.MaximumLength = ImagePathName.Length + sizeof(WCHAR);
    ImagePathName.Buffer = ImagePath;

    /* Get the NT Headers */
    NtHeader = RtlImageNtHeader(Peb->ImageBaseAddress);

    /* Get the execution options */
    Status = LdrpInitializeExecutionOptions(&ImagePathName, Peb, &ExecuteOptions);

    /* Check if this is a .NET executable */
    if (RtlImageDirectoryEntryToData(Peb->ImageBaseAddress,
                                     TRUE,
                                     IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR,
                                     &ComSectionSize))
    {
        /* Remeber this for later */
        IsDotNetImage = TRUE;
    }

    /* Save the NTDLL Base address */
    NtDllBase = SystemArgument1;

    /* If this is a Native Image */
    if (NtHeader->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_NATIVE)
    {
        /* Then do DLL Validation */
        LdrpDllValidation = TRUE;
    }

    /* Save the old Shim Data */
    OldShimData = Peb->pShimData;

    /* Clear it */
    Peb->pShimData = NULL;

    /* Save the number of processors and CS Timeout */
    LdrpNumberOfProcessors = Peb->NumberOfProcessors;
    RtlpTimeout = Peb->CriticalSectionTimeout;

    /* Normalize the parameters */
    ProcessParameters = RtlNormalizeProcessParams(Peb->ProcessParameters);
    ProcessParameters = Peb->ProcessParameters;
    if (ProcessParameters)
    {
        /* Save the Image and Command Line Names */
        ImageFileName = ProcessParameters->ImagePathName;
        CommandLine = ProcessParameters->CommandLine;
    }
    else
    {
        /* It failed, initialize empty strings */
        RtlInitUnicodeString(&ImageFileName, NULL);
        RtlInitUnicodeString(&CommandLine, NULL);
    }

    /* Initialize NLS data */
    RtlInitNlsTables(Peb->AnsiCodePageData,
                     Peb->OemCodePageData,
                     Peb->UnicodeCaseTableData,
                     &NlsTable);

    /* Reset NLS Translations */
    RtlResetRtlTranslations(&NlsTable);

    /* Get the Image Config Directory */
    LoadConfig = RtlImageDirectoryEntryToData(Peb->ImageBaseAddress,
                                              TRUE,
                                              IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG,
                                              &ConfigSize);

    /* Setup the Heap Parameters */
    RtlZeroMemory(&HeapParameters, sizeof(RTL_HEAP_PARAMETERS));
    HeapFlags = HEAP_GROWABLE;
    HeapParameters.Length = sizeof(RTL_HEAP_PARAMETERS);

    /* Check if we have Configuration Data */
    if ((LoadConfig) && (ConfigSize == sizeof(IMAGE_LOAD_CONFIG_DIRECTORY)))
    {
        /* FIXME: Custom heap settings and misc. */
        DPRINT1("We don't support LOAD_CONFIG data yet\n");
    }

    /* Check for custom affinity mask */
    if (Peb->ImageProcessAffinityMask)
    {
        /* Set it */
        Status = NtSetInformationProcess(NtCurrentProcess(),
                                         ProcessAffinityMask,
                                         &Peb->ImageProcessAffinityMask,
                                         sizeof(Peb->ImageProcessAffinityMask));
    }

    /* Check if verbose debugging (ShowSnaps) was requested */
    ShowSnaps = TRUE;//Peb->NtGlobalFlag & FLG_SHOW_LDR_SNAPS;

    /* Start verbose debugging messages right now if they were requested */
    if (ShowSnaps)
    {
        DPRINT1("LDR: PID: 0x%x started - '%wZ'\n",
                Teb->ClientId.UniqueProcess,
                &CommandLine);
    }

    /* If the timeout is too long */
    if (RtlpTimeout.QuadPart < Int32x32To64(3600, -10000000))
    {
        /* Then disable CS Timeout */
        RtlpTimeoutDisable = TRUE;
    }

    /* Initialize Critical Section Data */
    RtlpInitDeferedCriticalSection();

    /* Initialize VEH Call lists */
    RtlpInitializeVectoredExceptionHandling();

    /* Set TLS/FLS Bitmap data */
    Peb->FlsBitmap = &FlsBitMap;
    Peb->TlsBitmap = &TlsBitMap;
    Peb->TlsExpansionBitmap = &TlsExpansionBitMap;

    /* Initialize FLS Bitmap */
    RtlInitializeBitMap(&FlsBitMap,
                        Peb->FlsBitmapBits,
                        FLS_MAXIMUM_AVAILABLE);
    RtlSetBit(&FlsBitMap, 0);

    /* Initialize TLS Bitmap */
    RtlInitializeBitMap(&TlsBitMap,
                        Peb->TlsBitmapBits,
                        TLS_MINIMUM_AVAILABLE);
    RtlSetBit(&TlsBitMap, 0);
    RtlInitializeBitMap(&TlsExpansionBitMap,
                        Peb->TlsExpansionBitmapBits,
                        TLS_EXPANSION_SLOTS);
    RtlSetBit(&TlsExpansionBitMap, 0);

    /* Initialize the Hash Table */
    for (i = 0; i < LDR_HASH_TABLE_ENTRIES; i++)
    {
        InitializeListHead(&LdrpHashTable[i]);
    }

    /* Initialize the Loader Lock */
    //InsertTailList(&RtlCriticalSectionList, &LdrpLoaderLock.DebugInfo->ProcessLocksList);
    //LdrpLoaderLock.DebugInfo->CriticalSection = &LdrpLoaderLock;
    UNIMPLEMENTED;
    LdrpLoaderLockInit = TRUE;

    /* Check if User Stack Trace Database support was requested */
    if (Peb->NtGlobalFlag & FLG_USER_STACK_TRACE_DB)
    {
        DPRINT1("We don't support user stack trace databases yet\n");
    }

    /* Setup Fast PEB Lock */
    RtlInitializeCriticalSection(&FastPebLock);
    Peb->FastPebLock = &FastPebLock;
    //Peb->FastPebLockRoutine = (PPEBLOCKROUTINE)RtlEnterCriticalSection;
    //Peb->FastPebUnlockRoutine = (PPEBLOCKROUTINE)RtlLeaveCriticalSection;

    /* Setup Callout Lock and Notification list */
    //RtlInitializeCriticalSection(&RtlpCalloutEntryLock);
    InitializeListHead(&LdrpDllNotificationList);

    /* For old executables, use 16-byte aligned heap */
    if ((NtHeader->OptionalHeader.MajorSubsystemVersion <= 3) &&
        (NtHeader->OptionalHeader.MinorSubsystemVersion < 51))
    {
        HeapFlags |= HEAP_CREATE_ALIGN_16;
    }

    /* Setup the Heap */
    RtlInitializeHeapManager();
    Peb->ProcessHeap = RtlCreateHeap(HeapFlags,
                                     NULL,
                                     NtHeader->OptionalHeader.SizeOfHeapReserve,
                                     NtHeader->OptionalHeader.SizeOfHeapCommit,
                                     NULL,
                                     &HeapParameters);

    if (!Peb->ProcessHeap)
    {
        DPRINT1("Failed to create process heap\n");
        return STATUS_NO_MEMORY;
    }

    /* Allocate an Activation Context Stack */
    Status = RtlAllocateActivationContextStack((PVOID *)&Teb->ActivationContextStackPointer);
    if (!NT_SUCCESS(Status)) return Status;

    // FIXME: Loader private heap is missing
    DPRINT1("Loader private heap is missing\n");

    /* Check for Debug Heap */
    DPRINT1("Check for a debug heap is missing\n");
    if (FALSE)
    {
        /* Query the setting */
        Status = LdrQueryImageFileKeyOption(NULL,//hKey
                                            L"DebugProcessHeapOnly",
                                            REG_DWORD,
                                            &DebugProcessHeapOnly,
                                            sizeof(ULONG),
                                            NULL);

        if (NT_SUCCESS(Status))
        {
            /* Reset DPH if requested */
            if (RtlpPageHeapEnabled && DebugProcessHeapOnly)
            {
                RtlpDphGlobalFlags &= ~0x40;
                RtlpPageHeapEnabled = FALSE;
            }
        }
    }

    /* Build the NTDLL Path */
    FullPath.Buffer = FullNtDllPath;
    FullPath.Length = 0;
    FullPath.MaximumLength = sizeof(FullNtDllPath);
    RtlInitUnicodeString(&NtSystemRoot, SharedUserData->NtSystemRoot);
    RtlAppendUnicodeStringToString(&FullPath, &NtSystemRoot);
    RtlAppendUnicodeToString(&FullPath, L"\\System32\\");

    /* Open the Known DLLs directory */
    RtlInitUnicodeString(&KnownDllString, L"\\KnownDlls");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KnownDllString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = ZwOpenDirectoryObject(&LdrpKnownDllObjectDirectory,
                                   DIRECTORY_QUERY | DIRECTORY_TRAVERSE,
                                   &ObjectAttributes);

    /* Check if it exists */
    if (!NT_SUCCESS(Status))
    {
        /* It doesn't, so assume System32 */
        LdrpKnownDllObjectDirectory = NULL;
        RtlInitUnicodeString(&LdrpKnownDllPath, FullPath.Buffer);
        LdrpKnownDllPath.Length -= sizeof(WCHAR);
    }
    else
    {
        /* Open the Known DLLs Path */
        InitializeObjectAttributes(&ObjectAttributes,
                                   &KnownDllString,
                                   OBJ_CASE_INSENSITIVE,
                                   LdrpKnownDllObjectDirectory,
                                   NULL);
        Status = NtOpenSymbolicLinkObject(&SymLinkHandle,
                                          SYMBOLIC_LINK_QUERY,
                                          &ObjectAttributes);
        if (NT_SUCCESS(Status))
        {
            /* Query the path */
            LdrpKnownDllPath.Length = 0;
            LdrpKnownDllPath.MaximumLength = sizeof(LdrpKnownDllPathBuffer);
            LdrpKnownDllPath.Buffer = LdrpKnownDllPathBuffer;
            Status = ZwQuerySymbolicLinkObject(SymLinkHandle, &LdrpKnownDllPath, NULL);
            NtClose(SymLinkHandle);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("LDR: %s - failed call to ZwQuerySymbolicLinkObject with status %x\n", "", Status);
                return Status;
            }
        }
    }

    /* If we have process parameters, get the default path and current path */
    if (ProcessParameters)
    {
        /* Check if we have a Dll Path */
        if (ProcessParameters->DllPath.Length)
        {
            /* Get the path */
            LdrpDefaultPath = *(PUNICODE_STRING)&ProcessParameters->DllPath;
        }
        else
        {
            /* We need a valid path */
            LdrpInitFailure(STATUS_INVALID_PARAMETER);
        }

        /* Set the current directory */
        CurrentDirectory = ProcessParameters->CurrentDirectory.DosPath;

        /* Check if it's empty or invalid */
        if ((!CurrentDirectory.Buffer) ||
            (CurrentDirectory.Buffer[0] == UNICODE_NULL) ||
            (!CurrentDirectory.Length))
        {
            /* Allocate space for the buffer */
            CurrentDirectory.Buffer = RtlAllocateHeap(Peb->ProcessHeap,
                                                      0,
                                                      3 * sizeof(WCHAR) +
                                                      sizeof(UNICODE_NULL));

            /* Copy the drive of the system root */
            RtlMoveMemory(CurrentDirectory.Buffer,
                          SharedUserData->NtSystemRoot,
                          3 * sizeof(WCHAR));
            CurrentDirectory.Buffer[3] = UNICODE_NULL;
            CurrentDirectory.Length = 3 * sizeof(WCHAR);
            CurrentDirectory.MaximumLength = CurrentDirectory.Length + sizeof(WCHAR);

            FreeCurDir = TRUE;
        }
        else
        {
            /* Use the local buffer */
            CurrentDirectory.Length = NtSystemRoot.Length;
            CurrentDirectory.Buffer = NtSystemRoot.Buffer;
        }
    }

    /* Setup Loader Data */
    Peb->Ldr = &PebLdr;
    InitializeListHead(&PebLdr.InLoadOrderModuleList);
    InitializeListHead(&PebLdr.InMemoryOrderModuleList);
    InitializeListHead(&PebLdr.InInitializationOrderModuleList);
    PebLdr.Length = sizeof(PEB_LDR_DATA);
    PebLdr.Initialized = TRUE;

    /* Allocate a data entry for the Image */
    LdrpImageEntry = NtLdrEntry = LdrpAllocateDataTableEntry(Peb->ImageBaseAddress);

    /* Set it up */
    NtLdrEntry->EntryPoint = LdrpFetchAddressOfEntryPoint(NtLdrEntry->DllBase);
    NtLdrEntry->LoadCount = -1;
    NtLdrEntry->EntryPointActivationContext = 0;
    NtLdrEntry->FullDllName.Length = ImageFileName.Length;
    NtLdrEntry->FullDllName.Buffer = ImageFileName.Buffer;
    if (IsDotNetImage)
        NtLdrEntry->Flags = LDRP_COR_IMAGE;
    else
        NtLdrEntry->Flags = 0;

    /* Check if the name is empty */
    if (!ImageFileName.Buffer[0])
    {
        /* Use the same Base name */
        NtLdrEntry->BaseDllName = NtLdrEntry->BaseDllName;
    }
    else
    {
        /* Find the last slash */
        Current = ImageFileName.Buffer;
        while (*Current)
        {
            if (*Current++ == '\\')
            {
                /* Set this path */
                NtDllName = Current;
            }
        }

        /* Did we find anything? */
        if (!NtDllName)
        {
            /* Use the same Base name */
            NtLdrEntry->BaseDllName = NtLdrEntry->FullDllName;
        }
        else
        {
            /* Setup the name */
            NtLdrEntry->BaseDllName.Length = (USHORT)((ULONG_PTR)ImageFileName.Buffer + ImageFileName.Length - (ULONG_PTR)NtDllName);
            NtLdrEntry->BaseDllName.MaximumLength = NtLdrEntry->BaseDllName.Length + sizeof(WCHAR);
            NtLdrEntry->BaseDllName.Buffer = (PWSTR)((ULONG_PTR)ImageFileName.Buffer +
                                                     (ImageFileName.Length - NtLdrEntry->BaseDllName.Length));
        }
    }

    /* Processing done, insert it */
    LdrpInsertMemoryTableEntry(NtLdrEntry);
    NtLdrEntry->Flags |= LDRP_ENTRY_PROCESSED;

    /* Now add an entry for NTDLL */
    NtLdrEntry = LdrpAllocateDataTableEntry(SystemArgument1);
    NtLdrEntry->Flags = LDRP_IMAGE_DLL;
    NtLdrEntry->EntryPoint = LdrpFetchAddressOfEntryPoint(NtLdrEntry->DllBase);
    NtLdrEntry->LoadCount = -1;
    NtLdrEntry->EntryPointActivationContext = 0;
    //NtLdrEntry->BaseDllName.Length = NtSystemRoot.Length;
    //RtlAppendUnicodeStringToString(&NtSystemRoot, &NtDllString);
    NtLdrEntry->BaseDllName.Length = NtDllString.Length;
    NtLdrEntry->BaseDllName.MaximumLength = NtDllString.MaximumLength;
    NtLdrEntry->BaseDllName.Buffer = NtDllString.Buffer;

    // FIXME: Full DLL name?!

    /* Processing done, insert it */
    LdrpNtDllDataTableEntry = NtLdrEntry;
    LdrpInsertMemoryTableEntry(NtLdrEntry);

    /* Let the world know */
    if (ShowSnaps)
    {
        DPRINT1("LDR: NEW PROCESS\n");
        DPRINT1("     Image Path: %wZ (%wZ)\n", &LdrpImageEntry->FullDllName, &LdrpImageEntry->BaseDllName);
        DPRINT1("     Current Directory: %wZ\n", &CurrentDirectory);
        DPRINT1("     Search Path: %wZ\n", &LdrpDefaultPath);
    }

    /* Link the Init Order List */
    InsertHeadList(&Peb->Ldr->InInitializationOrderModuleList,
                   &LdrpNtDllDataTableEntry->InInitializationOrderModuleList);

    /* Set the current directory */
    Status = RtlSetCurrentDirectory_U(&CurrentDirectory);
    if (!NT_SUCCESS(Status))
    {
        /* We failed, check if we should free it */
        if (FreeCurDir) RtlFreeUnicodeString(&CurrentDirectory);

        /* Set it to the NT Root */
        CurrentDirectory = NtSystemRoot;
        RtlSetCurrentDirectory_U(&CurrentDirectory);
    }
    else
    {
        /* We're done with it, free it */
        if (FreeCurDir) RtlFreeUnicodeString(&CurrentDirectory);
    }

    /* Check if we should look for a .local file */
    if (ProcessParameters->Flags & RTL_USER_PROCESS_PARAMETERS_LOCAL_DLL_PATH)
    {
        /* FIXME */
        DPRINT1("We don't support .local overrides yet\n");
    }

    /* Check if the Application Verifier was enabled */
    if (Peb->NtGlobalFlag & FLG_POOL_ENABLE_TAIL_CHECK)
    {
        /* FIXME */
        DPRINT1("We don't support Application Verifier yet\n");
    }

    if (IsDotNetImage)
    {
        /* FIXME */
        DPRINT1("We don't support .NET applications yet\n");
    }

    /* FIXME: Load support for Terminal Services */
    if (NtHeader->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
    {
        /* Load kernel32 and call BasePostImportInit... */
        DPRINT1("Unimplemented codepath!\n");
    }

    /* Walk the IAT and load all the DLLs */
    LdrpWalkImportDescriptor(LdrpDefaultPath.Buffer, LdrpImageEntry);

    /* Check if relocation is needed */
    if (Peb->ImageBaseAddress != (PVOID)NtHeader->OptionalHeader.ImageBase)
    {
        DPRINT("LDR: Performing relocations\n");
        Status = LdrPerformRelocations(NtHeader, Peb->ImageBaseAddress);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("LdrPerformRelocations() failed\n");
            return STATUS_INVALID_IMAGE_FORMAT;
        }
    }

    /* Lock the DLLs */
    ListHead = &Peb->Ldr->InLoadOrderModuleList;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        NtLdrEntry = CONTAINING_RECORD(NextEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        NtLdrEntry->LoadCount = -1;
        NextEntry = NextEntry->Flink;
    }

    /* Phase 0 is done */
    LdrpLdrDatabaseIsSetup = TRUE;

    /* Initialize TLS */
    Status = LdrpInitializeTls();
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("LDR: LdrpProcessInitialization failed to initialize TLS slots; status %x\n",
                Status);
        return Status;
    }

    /* FIXME Mark the DLL Ranges for Stack Traces later */

    /* Notify the debugger now */
    if (Peb->BeingDebugged)
    {
        /* Break */
        DbgBreakPoint();

        /* Update show snaps again */
        ShowSnaps = Peb->NtGlobalFlag & FLG_SHOW_LDR_SNAPS;
    }

    /* Validate the Image for MP Usage */
    if (LdrpNumberOfProcessors > 1) LdrpValidateImageForMp(LdrpImageEntry);

    /* Check NX Options */
    if (SharedUserData->NXSupportPolicy == 1)
    {
        ExecuteOptions = 0xD;
    }
    else if (!SharedUserData->NXSupportPolicy)
    {
        ExecuteOptions = 0xA;
    }

    /* Let Mm know */
    ZwSetInformationProcess(NtCurrentProcess(),
                            ProcessExecuteFlags,
                            &ExecuteOptions,
                            sizeof(ULONG));

    /* Check if we had Shim Data */
    if (OldShimData)
    {
        /* Load the Shim Engine */
        Peb->AppCompatInfo = NULL;
        //LdrpLoadShimEngine(OldShimData, ImagePathName, OldShimData);
        DPRINT1("We do not support shims yet\n");
    }
    else
    {
        /* Check for Application Compatibility Goo */
        //LdrQueryApplicationCompatibilityGoo(hKey);
        DPRINT1("Querying app compat hacks is missing!\n");
    }

    /*
     * FIXME: Check for special images, SecuROM, SafeDisc and other NX-
     * incompatible images.
     */

    /* Now call the Init Routines */
    Status = LdrpRunInitializeRoutines(Context);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("LDR: LdrpProcessInitialization failed running initialization routines; status %x\n",
                Status);
        return Status;
    }

    /* FIXME: Unload the Shim Engine if it was loaded */

    /* Check if we have a user-defined Post Process Routine */
    if (NT_SUCCESS(Status) && Peb->PostProcessInitRoutine)
    {
        DPRINT1("CP\n");
        /* Call it */
        Peb->PostProcessInitRoutine();
    }

    ///* Close the key if we have one opened */
    //if (hKey) NtClose(hKey);
DbgBreakPoint();
    /* Return status */
    return Status;
}

VOID
NTAPI
LdrpInitFailure(NTSTATUS Status)
{
    ULONG Response;

    /* Print a debug message */
    DPRINT1("LDR: Process initialization failure; NTSTATUS = %08lx\n", Status);

    /* Raise a hard error */
    if (!LdrpFatalHardErrorCount)
    {
        ZwRaiseHardError(STATUS_APP_INIT_FAILURE, 1, 0, (PULONG_PTR)&Status, OptionOk, &Response);
    }
}

VOID
NTAPI
LdrpInit(PCONTEXT Context,
         PVOID SystemArgument1,
         PVOID SystemArgument2)
{
    LARGE_INTEGER Timeout;
    PTEB Teb = NtCurrentTeb();
    NTSTATUS Status, LoaderStatus = STATUS_SUCCESS;
    MEMORY_BASIC_INFORMATION MemoryBasicInfo;
    PPEB Peb = NtCurrentPeb();

    DPRINT("LdrpInit()\n");

    /* Check if we have a deallocation stack */
    if (!Teb->DeallocationStack)
    {
        /* We don't, set one */
        Status = NtQueryVirtualMemory(NtCurrentProcess(),
                                      Teb->NtTib.StackLimit,
                                      MemoryBasicInformation,
                                      &MemoryBasicInfo,
                                      sizeof(MEMORY_BASIC_INFORMATION),
                                      NULL);
        if (!NT_SUCCESS(Status))
        {
            /* Fail */
            LdrpInitFailure(Status);
            RtlRaiseStatus(Status);
            return;
        }

        /* Set the stack */
        Teb->DeallocationStack = MemoryBasicInfo.AllocationBase;
    }

    /* Now check if the process is already being initialized */
    while (_InterlockedCompareExchange(&LdrpProcessInitialized,
                                      1,
                                      0) == 1)
    {
        /* Set the timeout to 30 seconds */
        Timeout.QuadPart = Int32x32To64(30, -10000);

        /* Make sure the status hasn't changed */
        while (!LdrpProcessInitialized)
        {
            /* Do the wait */
            ZwDelayExecution(FALSE, &Timeout);
        }
    }

    /* Check if we have already setup LDR data */
    if (!Peb->Ldr)
    {
        /* Setup the Loader Lock */
        Peb->LoaderLock = &LdrpLoaderLock;

        /* Let other code know we're initializing */
        LdrpInLdrInit = TRUE;

        /* Protect with SEH */
        _SEH2_TRY
        {
            /* Initialize the Process */
            LoaderStatus = LdrpInitializeProcess_(Context,
                                                 SystemArgument1);

            /* Check for success and if MinimumStackCommit was requested */
            if (NT_SUCCESS(LoaderStatus) && Peb->MinimumStackCommit)
            {
                /* Enforce the limit */
                //LdrpTouchThreadStack(Peb->MinimumStackCommit);
                UNIMPLEMENTED;
            }
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            /* Fail with the SEH error */
            LoaderStatus = _SEH2_GetExceptionCode();
        }
        _SEH2_END;

        /* We're not initializing anymore */
        LdrpInLdrInit = FALSE;

        /* Check if init worked */
        if (NT_SUCCESS(LoaderStatus))
        {
            /* Set the process as Initialized */
            _InterlockedIncrement(&LdrpProcessInitialized);
        }
    }
    else
    {
        /* Loader data is there... is this a fork() ? */
        if(Peb->InheritedAddressSpace)
        {
            /* Handle the fork() */
            //LoaderStatus = LdrpForkProcess();
            LoaderStatus = STATUS_NOT_IMPLEMENTED;
            UNIMPLEMENTED;
        }
        else
        {
            /* This is a new thread initializing */
            LdrpInitializeThread(Context);
        }
    }

    /* All done, test alert the thread */
    NtTestAlert();

    /* Return */
    if (!NT_SUCCESS(LoaderStatus))
    {
        /* Fail */
        LdrpInitFailure(LoaderStatus);
        RtlRaiseStatus(LoaderStatus);
    }
}

/* EOF */
