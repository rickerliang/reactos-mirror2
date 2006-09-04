/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         File Management IFS Utility functions
 * FILE:            reactos/dll/win32/fmifs/format.c
 * PURPOSE:         Volume format
 *
 * PROGRAMMERS:     Emanuele Aliberti
 *                  Herv� Poussineau (hpoussin@reactos.org)
 */

#include "precomp.h"

#define NDEBUG
#include <debug.h>

/* FMIFS.6 */
VOID NTAPI
Format(VOID)
{
}

/* FMIFS.7 */
VOID NTAPI
FormatEx(
	IN PWCHAR DriveRoot,
	IN FMIFS_MEDIA_FLAG MediaFlag,
	IN PWCHAR Format,
	IN PWCHAR Label,
	IN BOOLEAN QuickFormat,
	IN ULONG ClusterSize,
	IN PFMIFSCALLBACK Callback)
{
	PIFS_PROVIDER Provider;
	UNICODE_STRING usDriveRoot;
	UNICODE_STRING usLabel;
	BOOLEAN Argument = FALSE;
	WCHAR VolumeName[MAX_PATH];
	CURDIR CurDir;

	Provider = GetProvider(Format);
	if (!Provider)
	{
		/* Unknown file system */
		Callback(
			DONE, /* Command */
			0, /* DWORD Modifier */
			&Argument); /* Argument */
		return;
	}

	if (!GetVolumeNameForVolumeMountPointW(DriveRoot, VolumeName, MAX_PATH)
	 || !RtlDosPathNameToNtPathName_U(VolumeName, &usDriveRoot, NULL, &CurDir))
	{
		/* Report an error. */
		Callback(
			DONE, /* Command */
			0, /* DWORD Modifier */
			&Argument); /* Argument */
		return;
	}

	RtlInitUnicodeString(&usLabel, Label);

	DPRINT1("FormatEx - %S\n", Format);
	Provider->FormatEx(
		&usDriveRoot,
		MediaFlag,
		&usLabel,
		QuickFormat,
		ClusterSize,
		Callback);
	RtlFreeUnicodeString(&usDriveRoot);
}

/* EOF */
