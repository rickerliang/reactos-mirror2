/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS TCP/IP protocol driver
 * FILE:        include/pool.h
 * PURPOSE:     Prototypes for memory pooling
 */
#ifndef __POOL_H
#define __POOL_H


PVOID PoolAllocateBuffer(
    ULONG Size);

VOID PoolFreeBuffer(
    PVOID Buffer);

NDIS_STATUS PrependPacket( PNDIS_PACKET Packet, PCHAR Data, UINT Len,
			   BOOLEAN Copy );

#endif /* __POOL_H */

/* EOF */
