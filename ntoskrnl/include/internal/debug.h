/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS kernel
 * FILE:            include/internal/debug.h
 * PURPOSE:         Useful debugging macros
 * PROGRAMMER:      David Welch (welch@mcmail.com)
 * UPDATE HISTORY: 
 *                28/05/98: Created
 */

/*
 * NOTES: Define DBG in configuration file for "checked" version
 *        Define NDEBUG before including this header to disable debugging
 *        macros
 *        Define NASSERT before including this header to disable assertions
 */

#ifndef __INTERNAL_DEBUG
#define __INTERNAL_DEBUG

#include <internal/ntoskrnl.h>
#include <internal/config.h>

#define UNIMPLEMENTED do {DbgPrint("%s at %s:%d is unimplemented, have a nice day\n",__FUNCTION__,__FILE__,__LINE__); for(;;);  } while(0);

/*  FIXME: should probably remove this later  */
#if !defined(CHECKED) && !defined(NDEBUG)
#define CHECKED
#endif

#ifdef DBG

/* Assert only on "checked" version */
#ifndef NASSERT
#define assert(x) if (!(x)) {DbgPrint("Assertion "#x" failed at %s:%d\n", __FILE__,__LINE__); KeBugCheck(0); }
#else
#define assert(x)
#endif

/* Print if using a "checked" version */
#define CPRINT(args...) do { DbgPrint("(%s:%d) ",__FILE__,__LINE__); DbgPrint(args); } while(0);

#else /* DBG */

#define CPRINT(args...)
#define assert(x)

#endif /* DBG */

#define DPRINT1(args...) do { DbgPrint("(%s:%d) ",__FILE__,__LINE__); DbgPrint(args); } while(0);
#define CHECKPOINT1 do { DbgPrint("%s:%d\n",__FILE__,__LINE__); } while(0);

extern unsigned int old_idt[256][2];
//extern unsigned int idt;
extern unsigned int old_idt_valid;

#ifdef __NTOSKRNL__
//#define DPRINT_CHECKS ExAllocatePool(NonPagedPool,0); assert(old_idt_valid || (!memcmp(old_idt,KiIdt,256*2)));
//#define DPRINT_CHECKS ExAllocatePool(NonPagedPool,0);
#define DPRINT_CHECKS
#else
#define DPRINT_CHECKS
#endif

#ifndef NDEBUG
#define OLD_DPRINT(fmt,args...) do { DbgPrint("(%s:%d) ",__FILE__,__LINE__); DbgPrint(fmt,args); } while(0);
#define DPRINT(args...) do { DbgPrint("(%s:%d) ",__FILE__,__LINE__); DbgPrint(args); DPRINT_CHECKS  } while(0);
#define CHECKPOINT do { DbgPrint("%s:%d\n",__FILE__,__LINE__); ExAllocatePool(NonPagedPool,0); } while(0);
#else
//#define DPRINT(args...) do { DPRINT_CHECKS } while (0);
#define DPRINT(args...)
#define OLD_DPRINT(args...)
#define CHECKPOINT
#endif /* NDEBUG */

/*
 * FUNCTION: Assert a maximum value for the current irql
 * ARGUMENTS:
 *        x = Maximum irql
 */
#define ASSERT_IRQL(x) assert(KeGetCurrentIrql()<=(x))
#define assert_irql(x) assert(KeGetCurrentIrql()<=(x))

#endif /* __INTERNAL_DEBUG */
