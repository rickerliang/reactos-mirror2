/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS TCP/IP protocol driver
 * FILE:        transport/tcp/tcp.c
 * PURPOSE:     Transmission Control Protocol Listen/Accept code
 * PROGRAMMERS: Art Yerkes (arty@users.sf.net)
 * REVISIONS:
 *   arty 12/21/2004 Created
 */

#include "precomp.h"

NTSTATUS TCPServiceListeningSocket( PCONNECTION_ENDPOINT Listener,
				    PCONNECTION_ENDPOINT Connection,
				    PTDI_REQUEST_KERNEL Request ) {
    NTSTATUS Status;
    SOCKADDR_IN OutAddr;
    OSK_UINT OutAddrLen;
    PTA_ADDRESS RequestAddressReturn;
    SOCKADDR_IN AddressConnecting;
    PTDI_CONNECTION_INFORMATION WhoIsConnecting;

    /* Unpack TDI info -- We need the return connection information
     * struct to return the address so it can be filtered if needed
     * by WSAAccept -- The returned address will be passed on to
     * userland after we complete this irp */
    WhoIsConnecting = (PTDI_CONNECTION_INFORMATION)
	Request->ReturnConnectionInformation;
    
    Status = TCPTranslateError
	( OskitTCPAccept( Listener->SocketContext, 
			  &Connection->SocketContext,
			  &OutAddr,
			  sizeof(OutAddr),
			  &OutAddrLen,
			  Request->RequestFlags & TDI_QUERY_ACCEPT ? 0 : 1 ) );
    
    TI_DbgPrint(DEBUG_TCP,("Status %x\n", Status));

    if( NT_SUCCESS(Status) && Status != STATUS_PENDING ) {
	TI_DbgPrint(DEBUG_TCP,("Copying address to %x (Who %x)\n", 
			       RequestAddressReturn, WhoIsConnecting));
	
	RequestAddressReturn = (PTA_ADDRESS)WhoIsConnecting->RemoteAddress;
	RequestAddressReturn->AddressLength = OutAddrLen;
	RequestAddressReturn->AddressType = 
	    AddressConnecting.sin_family;
	TI_DbgPrint(DEBUG_TCP,("Copying address proper: from %x to %x,%x\n",
			       ((PCHAR)&AddressConnecting) + 
			       sizeof(AddressConnecting.sin_family),
			       RequestAddressReturn->Address, 
			       RequestAddressReturn->AddressLength));
	RtlCopyMemory( RequestAddressReturn->Address,
		       ((PCHAR)&AddressConnecting) + 
		       sizeof(AddressConnecting.sin_family),
		       RequestAddressReturn->AddressLength );
	TI_DbgPrint(DEBUG_TCP,("Done copying\n"));
    }
    
    TI_DbgPrint(DEBUG_TCP,("Status %x\n", Status));

    return Status;
}

/* This listen is on a socket we keep as internal.  That socket has the same
 * lifetime as the address file */
NTSTATUS TCPListen( PCONNECTION_ENDPOINT Connection, UINT Backlog ) {
    NTSTATUS Status = STATUS_SUCCESS;
    SOCKADDR_IN AddressToBind;

    TI_DbgPrint(DEBUG_TCP,("TCPListen started\n"));

    TI_DbgPrint(DEBUG_TCP,("Connection->SocketContext %x\n",
	Connection->SocketContext));

    ASSERT(Connection);
    ASSERT_KM_POINTER(Connection->SocketContext);
    ASSERT_KM_POINTER(Connection->AddressFile);

    TcpipRecursiveMutexEnter( &TCPLock, TRUE );
   
    AddressToBind.sin_family = AF_INET;
    memcpy( &AddressToBind.sin_addr, 
	    &Connection->AddressFile->Address.Address.IPv4Address,
	    sizeof(AddressToBind.sin_addr) );
    AddressToBind.sin_port = Connection->AddressFile->Port;
    
    TI_DbgPrint(DEBUG_TCP,("AddressToBind - %x:%x\n", AddressToBind.sin_addr, AddressToBind.sin_port));
    
    OskitTCPBind( Connection->SocketContext,
		  Connection,
		  &AddressToBind,
		  sizeof(AddressToBind) );
 
    Status = TCPTranslateError( OskitTCPListen( Connection->SocketContext,
						Backlog ) );
    
    TcpipRecursiveMutexLeave( &TCPLock );

    TI_DbgPrint(DEBUG_TCP,("TCPListen finished %x\n", Status));
   
    return Status;
}

VOID TCPAbortListenForSocket( PCONNECTION_ENDPOINT Listener,
			      PCONNECTION_ENDPOINT Connection ) {
    PLIST_ENTRY ListEntry;
    PTDI_BUCKET Bucket;
    
    TcpipRecursiveMutexEnter( &TCPLock, TRUE );
    
    for( ListEntry = Listener->ListenRequest.Flink;
	 ListEntry != &Listener->ListenRequest;
	 ListEntry = ListEntry->Flink ) {
	Bucket = CONTAINING_RECORD(ListEntry, TDI_BUCKET, Entry);

	if( Bucket->Request.Handle.ConnectionContext == Connection ) {
#ifdef MEMTRACK
	    UntrackFL( __FILE__, __LINE__, Bucket->Request.RequestContext );
#endif
	    RemoveEntryList( ListEntry );
	    ExFreePool( Bucket );
	}
    }
	
   TcpipRecursiveMutexLeave( &TCPLock );
}

NTSTATUS TCPAccept
( PTDI_REQUEST Request,
  PCONNECTION_ENDPOINT Listener,
  PCONNECTION_ENDPOINT Connection,
  PTCP_COMPLETION_ROUTINE Complete,
  PVOID Context ) {
   NTSTATUS Status;
   PTDI_BUCKET Bucket;

   TI_DbgPrint(DEBUG_TCP,("TCPAccept started\n"));

   TcpipRecursiveMutexEnter( &TCPLock, TRUE );

   Status = TCPServiceListeningSocket( Listener, Connection, 
				       (PTDI_REQUEST_KERNEL)Request );

   if( Status == STATUS_PENDING ) {
       Bucket = ExAllocatePool( NonPagedPool, sizeof(*Bucket) );
       Bucket->AssociatedEndpoint = Connection;
       Bucket->Request.RequestNotifyObject = Complete;
       Bucket->Request.RequestContext = Context;
       InsertHeadList( &Listener->ListenRequest, &Bucket->Entry );
   }

   TcpipRecursiveMutexLeave( &TCPLock );
					       
   TI_DbgPrint(DEBUG_TCP,("TCPAccept finished %x\n", Status));
   return Status;
}
