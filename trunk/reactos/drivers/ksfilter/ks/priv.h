#ifndef PRIV_H__
#define PRIV_H__

#include <ntifs.h>
#include <ntddk.h>
#define YDEBUG
#include <debug.h>
#include <portcls.h>
#include <ks.h>
#include <kcom.h>

#include "ksfunc.h"
#include "kstypes.h"
#include "ksiface.h"


#define TAG_DEVICE_HEADER TAG('H','D','S','K')

#define IOCTL_KS_OBJECT_CLASS CTL_CODE(FILE_DEVICE_KS, 0x7, METHOD_NEITHER, FILE_ANY_ACCESS)

#endif
