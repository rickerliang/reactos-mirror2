/*
 * PROJECT:         ReactOS Kernel
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            ntoskrnl/config/i386/cmhardwr.c
 * PURPOSE:         Configuration Manager - Hardware-Specific Code
 * PROGRAMMERS:     Alex Ionescu (alex.ionescu@reactos.org)
 */

/* INCLUDES ******************************************************************/

#include "ntoskrnl.h"
#include "../cm.h"
#define NDEBUG
#include "debug.h"

/* GLOBALS *******************************************************************/

PCHAR CmpID1 = "PowerPC %u";
PCHAR CmpID2 = "No Data";
PCHAR CmpBiosStrings[] =
{
    "Ver",
    "Rev",
    "Rel",
    "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9",
    "v 0", "v 1", "v 2", "v 3", "v 4", "v 5", "v 6", "v 7", "v 8", "v 9",
    NULL
};

PCHAR CmpBiosBegin, CmpBiosSearchStart, CmpBiosSearchEnd;

/* FUNCTIONS *****************************************************************/

BOOLEAN
NTAPI
CmpGetBiosDate(IN PCHAR BiosStart,
               IN ULONG BiosLength,
               IN PCHAR BiosDate,
               IN BOOLEAN FromBios)
{
    CHAR LastDate[11] = {0}, CurrentDate[11];
    PCHAR p, pp;

    /* Skip the signature and the magic, and loop the BIOS ROM */
    p = BiosStart + 2;
    pp = BiosStart + BiosLength - 5;
    while (p < pp)
    {
        /* Check for xx/yy/zz which we assume to be a date */
        if ((p[0] == '/') &&
            (p[3] == '/') &&
            (isdigit(p[-1])) &&
            (isdigit(p[1])) &&
            (isdigit(p[2])) &&
            (isdigit(p[4])) &&
            (isdigit(p[5])))
        {
            /* Copy the string proper */
            RtlMoveMemory(&CurrentDate[5], p - 2, 5);

            /* Add a 0 if the month only has one digit */
            if (!isdigit(CurrentDate[5])) CurrentDate[5] = '0';

            /* Now copy the year */
            CurrentDate[2] = p[4];
            CurrentDate[3] = p[5];
            CurrentDate[4] = CurrentDate[7] = CurrentDate[10] = ANSI_NULL;

            /* If the date comes from the BIOS, check if it's a 4-digit year */
            if ((FromBios) &&
                (isdigit(p[6])) &&
                (isdigit(p[7])) &&
                ((RtlEqualMemory(&p[4], "19", 2)) ||
                 (RtlEqualMemory(&p[4], "20", 2))))
            {
                /* Copy the year proper */
                CurrentDate[0] = p[4];
                CurrentDate[1] = p[5];
                CurrentDate[2] = p[6];
                CurrentDate[3] = p[7];
            }
            else
            {
                /* Otherwise, we'll just assume anything under 80 is 2000 */
                if (strtoul(&CurrentDate[2], NULL, 10) < 80)
                {
                    /* Hopefully your BIOS wasn't made in 1979 */
                    CurrentDate[0] = '2';
                    CurrentDate[1] = '0';
                }
                else
                {
                    /* Anything over 80, was probably made in the 1900s... */
                    CurrentDate[0] = '1';
                    CurrentDate[1] = '9';
                }
            }

            /* Add slashes were we previously had NULLs */
            CurrentDate[4] = CurrentDate[7] = '/';

            /* Check which date is newer */
            if (memcmp(LastDate, CurrentDate, 10) < 0)
            {
                /* Found a newer date, select it */
                RtlMoveMemory(LastDate, CurrentDate, 10);
            }

            p += 2;
        }
        p++;
    }

    /* Make sure we found a date */
    if (LastDate[0])
    {
        /* Copy the year at the pp, and keep only the last two digits */
        RtlMoveMemory(BiosDate, &LastDate[5], 5);
        BiosDate[5] = '/';
        BiosDate[6] = LastDate[2];
        BiosDate[7] = LastDate[3];
        BiosDate[8] = ANSI_NULL;
        return TRUE;
    }

    /* No date found, return empty string */
    BiosDate[0] = ANSI_NULL;
    return FALSE;
}

BOOLEAN
NTAPI
CmpGetBiosVersion(IN PCHAR BiosStart,
                  IN ULONG BiosLength,
                  IN PCHAR BiosVersion)
{
    CHAR Buffer[128];
    PCHAR p, pp;
    USHORT i;

    /* Check if we were given intitial data for the search */
    if (BiosStart)
    {
        /* Save it for later use */
        CmpBiosBegin = BiosStart;
        CmpBiosSearchStart = BiosStart + 1;
        CmpBiosSearchEnd = BiosStart + BiosLength - 2;
    }

    /* Now loop the BIOS area */
    for (;;)
    {
        /* Start an initial search looking for numbers and periods */
        pp = NULL;
        while (CmpBiosSearchStart <= CmpBiosSearchEnd)
        {
            /* Check if we have an "x.y" version string */
            if ((*CmpBiosSearchStart == '.') &&
                (*(CmpBiosSearchStart + 1) >= '0') &&
                (*(CmpBiosSearchStart + 1) <= '9') &&
                (*(CmpBiosSearchStart - 1) >= '0') &&
                (*(CmpBiosSearchStart - 1) <= '9'))
            {
                /* Start looking in this area for the actual BIOS Version */
                pp = CmpBiosSearchStart;
                break;
            }
            else
            {
                /* Keep searching */
                CmpBiosSearchStart++;
            }
        }

        /* Break out if we're went past the BIOS area */
        if (CmpBiosSearchStart > CmpBiosSearchEnd) return FALSE;

        /* Move to the next 2 bytes */
        CmpBiosSearchStart += 2;

        /* Null-terminate our scratch buffer and start the string here */
        Buffer[127] = ANSI_NULL;
        p = &Buffer[127];

        /* Go back one character since we're doing this backwards */
        pp--;

        /* Loop the identifier we found as long as it's valid */
        i = 0;
        while ((i++ < 127) &&
               (pp >= CmpBiosBegin) &&
               (*pp >= ' ') &&
               (*pp != '$'))
        {
            /* Copy the character */
            *--p = *pp--;
        }

        /* Go past the last character since we went backwards */
        pp++;

        /* Loop the strings we recognize */
        for (i = 0; CmpBiosStrings[i]; i++)
        {
            /* Check if a match was found */
            if (strstr(p, CmpBiosStrings[i])) goto Match;
        }
    }

Match:
    /* Skip until we find a space */
    for (; *pp == ' '; pp++);

    /* Loop the final string */
    i = 0;
    do
    {
        /* Copy the character into the final string */
        BiosVersion[i] = *pp++;
    } while ((++i < 127) &&
             (pp <= (CmpBiosSearchEnd + 1)) &&
             (*pp >= ' ') &&
             (*pp != '$'));

    /* Null-terminate the version string */
    BiosVersion[i] = ANSI_NULL;
    return TRUE;
}

NTSTATUS
NTAPI
CmpInitializeMachineDependentConfiguration(IN PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    UNICODE_STRING KeyName, ValueName, Data, SectionName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    ULONG HavePae, CacheSize, ViewSize, Length, TotalLength = 0, i, Disposition;
    NTSTATUS Status;
    HANDLE KeyHandle, BiosHandle, SystemHandle, FpuHandle, SectionHandle;
    CONFIGURATION_COMPONENT_DATA ConfigData;
    CHAR Buffer[128];
    ULONG ExtendedId = 0; //, Dummy;
    PKPRCB Prcb;
    USHORT IndexTable[MaximumType + 1] = {0};
    ANSI_STRING TempString;
    PCHAR PartialString = NULL, BiosVersion;
    CHAR CpuString[48];
    PVOID BaseAddress = NULL;
    LARGE_INTEGER ViewBase = {{0}};
    ULONG_PTR VideoRomBase;
    PCHAR CurrentVersion;
    extern UNICODE_STRING KeRosProcessorName, KeRosBiosDate, KeRosBiosVersion;
    extern UNICODE_STRING KeRosVideoBiosDate, KeRosVideoBiosVersion;
    
    /* Open the SMSS Memory Management key */
    RtlInitUnicodeString(&KeyName,
                         L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\"
                         L"Control\\Session Manager\\Memory Management");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtOpenKey(&KeyHandle, KEY_READ | KEY_WRITE, &ObjectAttributes);
    if (NT_SUCCESS(Status))
    {
        /* Detect if PAE is enabled */
        HavePae = SharedUserData->ProcessorFeatures[PF_PAE_ENABLED];

        /* Set the value */
        RtlInitUnicodeString(&ValueName, L"PhysicalAddressExtension");
        NtSetValueKey(KeyHandle,
                      &ValueName,
                      0,
                      REG_DWORD,
                      &HavePae,
                      sizeof(HavePae));

        /* Close the key */
        NtClose(KeyHandle);
    }

    /* Open the hardware description key */
    RtlInitUnicodeString(&KeyName,
                         L"\\Registry\\Machine\\Hardware\\Description\\System");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtOpenKey(&SystemHandle, KEY_READ | KEY_WRITE, &ObjectAttributes);
    if (!NT_SUCCESS(Status)) return Status;

    /* Create the BIOS Information key */
    RtlInitUnicodeString(&KeyName,
                         L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\"
                         L"Control\\BIOSINFO");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = NtCreateKey(&BiosHandle,
                         KEY_ALL_ACCESS,
                         &ObjectAttributes,
                         0,
                         NULL,
                         REG_OPTION_NON_VOLATILE,
                         &Disposition);
    if (ExpInTextModeSetup)
    {
        if (!NT_SUCCESS(Status))
            BiosHandle = NULL;
    }
    else if (!NT_SUCCESS(Status))
        return Status;

    /* Create the CPU Key, and check if it already existed */
    RtlInitUnicodeString(&KeyName, L"CentralProcessor");
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               SystemHandle,
                               NULL);
    Status = NtCreateKey(&KeyHandle,
                         KEY_READ | KEY_WRITE,
                         &ObjectAttributes,
                         0,
                         NULL,
                         0,
                         &Disposition);
    NtClose(KeyHandle);

    /* The key shouldn't already exist */
    if (Disposition == REG_CREATED_NEW_KEY)
    {
        /* Allocate the configuration data for cmconfig.c */
        CmpConfigurationData = ExAllocatePoolWithTag(PagedPool,
                                                     CmpConfigurationAreaSize,
                                                     TAG_CM);
        if (!CmpConfigurationData) return STATUS_INSUFFICIENT_RESOURCES;

        /* Loop all CPUs */
        for (i = 0; i < KeNumberProcessors; i++)
        {
            /* Get the PRCB */
            Prcb = KiProcessorBlock[i];

            /* Setup the Configuration Entry for the Processor */
            RtlZeroMemory(&ConfigData, sizeof (ConfigData));
            ConfigData.ComponentEntry.Class = ProcessorClass;
            ConfigData.ComponentEntry.Type = CentralProcessor;
            ConfigData.ComponentEntry.Key = i;
            ConfigData.ComponentEntry.AffinityMask = AFFINITY_MASK(i);
            ConfigData.ComponentEntry.Identifier = Buffer;

            /* Check if the CPU doesn't support CPUID */
            if (!Prcb->CpuID)
            {
                /* Build ID1-style string for older CPUs */
                sprintf(Buffer,
                        CmpID1,
                        Prcb->CpuType,
                        (Prcb->CpuStep >> 8) + 'A',
                        Prcb->CpuStep & 0xff);
            }
            else
            {
                /* Build ID2-style string for newer CPUs */
                sprintf(Buffer,
                        CmpID2,
                        Prcb->CpuType,
                        (Prcb->CpuStep >> 8),
                        Prcb->CpuStep & 0xff);
            }

            /* Save the ID string length now that we've created it */
            ConfigData.ComponentEntry.IdentifierLength = strlen(Buffer) + 1;

            /* Initialize the registry configuration node for it */
            Status = CmpInitializeRegistryNode(&ConfigData,
                                               SystemHandle,
                                               &KeyHandle,
                                               InterfaceTypeUndefined,
                                               0xFFFFFFFF,
                                               IndexTable);
            if (!NT_SUCCESS(Status)) return(Status);

            {
                /* Setup the Configuration Entry for the FPU */
                RtlZeroMemory(&ConfigData, sizeof(ConfigData));
                ConfigData.ComponentEntry.Class = ProcessorClass;
                ConfigData.ComponentEntry.Type = FloatingPointProcessor;
                ConfigData.ComponentEntry.Key = i;
                ConfigData.ComponentEntry.AffinityMask = AFFINITY_MASK(i);
                ConfigData.ComponentEntry.Identifier = Buffer;

                /* For 386 cpus, the CPU pp is the identifier */
                if (Prcb->CpuType == 3) strcpy(Buffer, "80387");

                /* Save the ID string length now that we've created it */
                ConfigData.ComponentEntry.IdentifierLength = strlen(Buffer) + 1;

                /* Initialize the registry configuration node for it */
                Status = CmpInitializeRegistryNode(&ConfigData,
                                                   SystemHandle,
                                                   &FpuHandle,
                                                   InterfaceTypeUndefined,
                                                   0xFFFFFFFF,
                                                   IndexTable);
                if (!NT_SUCCESS(Status))
                {
                    /* Failed, close the CPU handle and return */
                    NtClose(KeyHandle);
                    return Status;
                }

                /* Close this new handle */
                NtClose(FpuHandle);

                /* Stay on this CPU only */
                KeSetSystemAffinityThread(Prcb->SetMember);
                if (!Prcb->CpuID)
                {
                    /* Uh oh, no CPUID! */
                }
                else
                {
                    /* Check if we have extended CPUID that supports name ID */
                    //Ki386Cpuid(0x80000000, &ExtendedId, &Dummy, &Dummy, &Dummy);
                    if (ExtendedId >= 0x80000004)
                    {
                        /* Do all the CPUIDs requred to get the full name */
                        PartialString = CpuString;
                        for (ExtendedId = 2; ExtendedId <= 4; ExtendedId++)
                        {
#if 0
                            /* Do the CPUID and save the name string */
                            Ki386Cpuid(0x80000000 | ExtendedId,
                                       (PULONG)PartialString,
                                       (PULONG)PartialString + 1,
                                       (PULONG)PartialString + 2,
                                       (PULONG)PartialString + 3);
#endif

                            /* Go to the next name string */
                            PartialString += 16;
                        }

                        /* Null-terminate it */
                        CpuString[48] = ANSI_NULL;
                    }
                }

                /* Get the cache size while we're still localized */
                CacheSize = 0; //((PKIPCR)KeGetPcr())->SecondLevelCacheSize;

                /* Go back to user affinity */
                KeRevertToUserAffinityThread();

                /* Check if we have a CPU Name */
                if (PartialString)
                {
                    /* Convert it to Unicode */
                    RtlInitAnsiString(&TempString, CpuString);
                    RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);

                    /* Add it to the registry */
                    RtlInitUnicodeString(&ValueName, L"ProcessorNameString");
                    Status = NtSetValueKey(KeyHandle,
                                           &ValueName,
                                           0,
                                           REG_SZ,
                                           Data.Buffer,
                                           Data.Length + sizeof(UNICODE_NULL));

                    /* ROS: Save a copy for bugzilla reporting */
                    RtlCreateUnicodeString(&KeRosProcessorName, Data.Buffer);
                    
                    /* Free the temporary buffer */
                    RtlFreeUnicodeString(&Data);
                }

                /* Check if we had a Vendor ID */
                if (Prcb->VendorString)
                {
                    /* Convert it to Unicode */
                    RtlInitAnsiString(&TempString, Prcb->VendorString);
                    RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);

                    /* Add it to the registry */
                    RtlInitUnicodeString(&ValueName, L"VendorIdentifier");
                    Status = NtSetValueKey(KeyHandle,
                                           &ValueName,
                                           0,
                                           REG_SZ,
                                           Data.Buffer,
                                           Data.Length + sizeof(UNICODE_NULL));
                    
                    /* Free the temporary buffer */
                    RtlFreeUnicodeString(&Data);
                }

                /* Check if we have features bits */
                if (Prcb->FeatureBits)
                {
                    /* Add them to the registry */
                    RtlInitUnicodeString(&ValueName, L"FeatureSet");
                    Status = NtSetValueKey(KeyHandle,
                                           &ValueName,
                                           0,
                                           REG_DWORD,
                                           &Prcb->FeatureBits,
                                           sizeof(Prcb->FeatureBits));
                }

                /* Check if we detected the CPU Speed */
                if (Prcb->MHz)
                {
                    /* Add it to the registry */
                    RtlInitUnicodeString(&ValueName, L"~MHz");
                    Status = NtSetValueKey(KeyHandle,
                                           &ValueName,
                                           0,
                                           REG_DWORD,
                                           &Prcb->MHz,
                                           sizeof(Prcb->MHz));
                }

                /* Check if we have an update signature */
                if (Prcb->UpdateSignature.QuadPart)
                {
                    /* Add it to the registry */
                    RtlInitUnicodeString(&ValueName, L"Update Signature");
                    Status = NtSetValueKey(KeyHandle,
                                           &ValueName,
                                           0,
                                           REG_BINARY,
                                           &Prcb->UpdateSignature,
                                           sizeof(Prcb->UpdateSignature));
                }

                /* Close the processor handle */
                NtClose(KeyHandle);

                /* FIXME: Detect CPU mismatches */
            }
        }

        /* Free the configuration data */
        ExFreePool(CmpConfigurationData);
    }

    /* Open physical memory */
    RtlInitUnicodeString(&SectionName, L"\\Device\\PhysicalMemory");
    InitializeObjectAttributes(&ObjectAttributes,
                               &SectionName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    Status = ZwOpenSection(&SectionHandle,
                           SECTION_ALL_ACCESS,
                           &ObjectAttributes);
    if (!NT_SUCCESS(Status)) goto Quickie;

    /* Map the first 1KB of memory to get the IVT */
    ViewSize = PAGE_SIZE;
    Status = ZwMapViewOfSection(SectionHandle,
                                NtCurrentProcess(),
                                &BaseAddress,
                                0,
                                ViewSize,
                                &ViewBase,
                                &ViewSize,
                                ViewUnmap,
                                MEM_DOS_LIM,
                                PAGE_READWRITE);
    if (!NT_SUCCESS(Status))
    {
        /* Assume default */
        VideoRomBase = 0xC0000;
    }
    else
    {
        /* Calculate the base address from the vector */
        VideoRomBase = (*((PULONG)BaseAddress + 0x10) >> 12) & 0xFFFF0;
        VideoRomBase += *((PULONG)BaseAddress + 0x10) & 0xFFF0;

        /* Now get to the actual ROM Start and make sure it's not invalid*/
        VideoRomBase &= 0xFFFF8000;
        if (VideoRomBase < 0xC0000) VideoRomBase = 0xC0000;

        /* And unmap the section */
        ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
    }

    /* Allocate BIOS Version pp Buffer */
    BiosVersion = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, TAG_CM);

    /* Setup settings to map the 64K BIOS ROM */
    BaseAddress = 0;
    ViewSize = 16 * PAGE_SIZE;
    ViewBase.LowPart = 0xF0000;
    ViewBase.HighPart = 0;

    /* Map it */
    Status = ZwMapViewOfSection(SectionHandle,
                                NtCurrentProcess(),
                                &BaseAddress,
                                0,
                                ViewSize,
                                &ViewBase,
                                &ViewSize,
                                ViewUnmap,
                                MEM_DOS_LIM,
                                PAGE_READWRITE);
    if (NT_SUCCESS(Status))
    {
        /* Scan the ROM to get the BIOS Date */
        if (CmpGetBiosDate(BaseAddress, 16 * PAGE_SIZE, Buffer, TRUE))
        {
            /* Convert it to Unicode */
            RtlInitAnsiString(&TempString, Buffer);
            RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);

            /* Write the date into the registry */
            RtlInitUnicodeString(&ValueName, L"SystemBiosDate");
            Status = NtSetValueKey(SystemHandle,
                                   &ValueName,
                                   0,
                                   REG_SZ,
                                   Data.Buffer,
                                   Data.Length + sizeof(UNICODE_NULL));

            /* Free the string */
            RtlFreeUnicodeString(&Data);

            if (BiosHandle)
            {
                /* Get the BIOS Date Identifier */
                RtlCopyMemory(Buffer, (PCHAR)BaseAddress + (16*PAGE_SIZE - 11), 8);
                Buffer[8] = ANSI_NULL;

                /* Convert it to unicode */
                RtlInitAnsiString(&TempString, Buffer);
                Status = RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);
                if (NT_SUCCESS(Status))
                {
                    /* Save it to the registry */
                    Status = NtSetValueKey(BiosHandle,
                                           &ValueName,
                                           0,
                                           REG_SZ,
                                           Data.Buffer,
                                           Data.Length + sizeof(UNICODE_NULL));
                    
                    /* ROS: Save a copy for bugzilla reporting */
                    RtlCreateUnicodeString(&KeRosBiosDate, Data.Buffer);

                    /* Free the string */
                    RtlFreeUnicodeString(&Data);
                }

                /* Close the bios information handle */
                NtClose(BiosHandle);
            }
        }

        /* Get the BIOS Version */
        if (CmpGetBiosVersion(BaseAddress, 16* PAGE_SIZE, Buffer))
        {
            /* Start at the beginning of our buffer */
            CurrentVersion = BiosVersion;
            do
            {
                /* Convert to Unicode */
                RtlInitAnsiString(&TempString, Buffer);
                RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);

                /* Calculate the length of this string and copy it in */
                Length = Data.Length + sizeof(UNICODE_NULL);
                RtlMoveMemory(CurrentVersion, Data.Buffer, Length);

                /* Free the unicode string */
                RtlFreeUnicodeString(&Data);

                /* Update the total length and see if we're out of space */
                TotalLength += Length;
                if (TotalLength + 256 + sizeof(UNICODE_NULL) > PAGE_SIZE)
                {
                    /* One more string would push us out, so stop here */
                    break;
                }

                /* Go to the next string inside the multi-string buffer */
                CurrentVersion += Length;

                /* Query the next BIOS Version */
            } while (CmpGetBiosVersion(NULL, 0, Buffer));

            /* Check if we found any strings at all */
            if (TotalLength)
            {
                /* Add the final null-terminator */
                *(PWSTR)CurrentVersion = UNICODE_NULL;
                TotalLength += sizeof(UNICODE_NULL);

                /* Write the BIOS Version to the registry */
                RtlInitUnicodeString(&ValueName, L"SystemBiosVersion");
                Status = NtSetValueKey(SystemHandle,
                                       &ValueName,
                                       0,
                                       REG_MULTI_SZ,
                                       BiosVersion,
                                       TotalLength);
                
                /* ROS: Save a copy for bugzilla reporting */
                RtlCreateUnicodeString(&KeRosBiosVersion, (PWCH)BiosVersion);
            }
        }

        /* Unmap the section */
        ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
    }

    /* Now prepare for Video BIOS Mapping of 32KB */
    BaseAddress = 0;
    ViewSize = 8 * PAGE_SIZE;
    ViewBase.LowPart = VideoRomBase;
    ViewBase.HighPart = 0;

    /* Map it */
    Status = ZwMapViewOfSection(SectionHandle,
                                NtCurrentProcess(),
                                &BaseAddress,
                                0,
                                ViewSize,
                                &ViewBase,
                                &ViewSize,
                                ViewUnmap,
                                MEM_DOS_LIM,
                                PAGE_READWRITE);
    if (NT_SUCCESS(Status))
    {
        /* Scan the ROM to get the BIOS Date */
        if (CmpGetBiosDate(BaseAddress, 8 * PAGE_SIZE, Buffer, FALSE))
        {
            /* Convert it to Unicode */
            RtlInitAnsiString(&TempString, Buffer);
            RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);

            /* Write the date into the registry */
            RtlInitUnicodeString(&ValueName, L"VideoBiosDate");
            Status = NtSetValueKey(SystemHandle,
                                   &ValueName,
                                   0,
                                   REG_SZ,
                                   Data.Buffer,
                                   Data.Length + sizeof(UNICODE_NULL));
            
            /* ROS: Save a copy for bugzilla reporting */
            RtlCreateUnicodeString(&KeRosVideoBiosDate, Data.Buffer);

            /* Free the string */
            RtlFreeUnicodeString(&Data);
        }

        /* Get the Video BIOS Version */
        if (CmpGetBiosVersion(BaseAddress, 8* PAGE_SIZE, Buffer))
        {
            /* Start at the beginning of our buffer */
            CurrentVersion = BiosVersion;
            do
            {
                /* Convert to Unicode */
                RtlInitAnsiString(&TempString, Buffer);
                RtlAnsiStringToUnicodeString(&Data, &TempString, TRUE);

                /* Calculate the length of this string and copy it in */
                Length = Data.Length + sizeof(UNICODE_NULL);
                RtlMoveMemory(CurrentVersion, Data.Buffer, Length);

                /* Free the unicode string */
                RtlFreeUnicodeString(&Data);

                /* Update the total length and see if we're out of space */
                TotalLength += Length;
                if (TotalLength + 256 + sizeof(UNICODE_NULL) > PAGE_SIZE)
                {
                    /* One more string would push us out, so stop here */
                    break;
                }

                /* Go to the next string inside the multi-string buffer */
                CurrentVersion += Length;

                /* Query the next BIOS Version */
            } while (CmpGetBiosVersion(NULL, 0, Buffer));

            /* Check if we found any strings at all */
            if (TotalLength)
            {
                /* Add the final null-terminator */
                *(PWSTR)CurrentVersion = UNICODE_NULL;
                TotalLength += sizeof(UNICODE_NULL);

                /* Write the BIOS Version to the registry */
                RtlInitUnicodeString(&ValueName, L"VideoBiosVersion");
                Status = NtSetValueKey(SystemHandle,
                                       &ValueName,
                                       0,
                                       REG_MULTI_SZ,
                                       BiosVersion,
                                       TotalLength);
                
                /* ROS: Save a copy for bugzilla reporting */
                RtlCreateUnicodeString(&KeRosVideoBiosVersion, (PWCH)BiosVersion);
            }
        }

        /* Unmap the section */
        ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
    }

    /* Close the section */
    ZwClose(SectionHandle);

    /* Free the BIOS version string buffer */
    if (BiosVersion) ExFreePool(BiosVersion);

Quickie:
    /* Close the procesor handle */
    NtClose(KeyHandle);
    return STATUS_SUCCESS;
}
