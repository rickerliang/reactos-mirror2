#include <oskittcp.h>
#include <oskitdebug.h>
#include <ntddk.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>

struct linker_set domain_set;

OSKITTCP_EVENT_HANDLERS OtcpEvent = { 0 };

OSK_UINT OskitDebugTraceLevel = OSK_DEBUG_ULTRA;

/* SPL */
unsigned cpl;
unsigned net_imask;
unsigned volatile ipending;
struct timeval boottime;

void InitOskitTCP() {
    OS_DbgPrint(OSK_MID_TRACE,("Init Called\n"));
    OS_DbgPrint(OSK_MID_TRACE,("Init fake freebsd scheduling\n"));
    init_freebsd_sched();
    OS_DbgPrint(OSK_MID_TRACE,("Init clock\n"));
    clock_init();
    OS_DbgPrint(OSK_MID_TRACE,("Init TCP\n"));
    tcp_init();
    OS_DbgPrint(OSK_MID_TRACE,("Init routing\n"));
    domaininit();
    memset( &OtcpEvent, 0, sizeof( OtcpEvent ) );
    OS_DbgPrint(OSK_MID_TRACE,("Init Finished\n"));
    tcp_iss = 1024;
}

void DeinitOskitTCP() {
}

void TimerOskitTCP() {
    tcp_slowtimo();
}

void RegisterOskitTCPEventHandlers( POSKITTCP_EVENT_HANDLERS EventHandlers ) {
    memcpy( &OtcpEvent, EventHandlers, sizeof(OtcpEvent) );
    if( OtcpEvent.PacketSend ) 
	OS_DbgPrint(OSK_MID_TRACE,("SendPacket handler registered: %x\n",
				   OtcpEvent.PacketSend));
    if( OtcpEvent.Bind )
	OS_DbgPrint(OSK_MID_TRACE,("Bind handler registered: %x\n",
				   OtcpEvent.Bind));
}

void OskitDumpBuffer( OSK_PCHAR Data, OSK_UINT Len ) {
    unsigned int i;
    
    for( i = 0; i < Len; i++ ) {
	if( i && !(i & 0xf) ) DbgPrint( "\n" );
	if( !(i & 0xf) ) DbgPrint( "%08x: ", (UINT)(Data + i) );
	DbgPrint( " %02x", Data[i] );
    }
    DbgPrint("\n");
}

/* From uipc_syscalls.c */

int OskitTCPSocket( void *context,
		    void **aso, 
		    int domain, 
		    int type, 
		    int proto ) 
{
    struct socket *so;
    int error = socreate(domain, &so, type, proto);
    if( !error ) {
	so->so_connection = context;
	*aso = so;
    }
    return error;
#if 0
    register struct protosw *prp;
    register struct socket *so;
    register int error;

    if (proto) {
	prp = pffindproto(domain, proto, type);
    } else {
	prp = pffindtype(domain, type);
    }
    if (prp == 0 || prp->pr_usrreq == 0) {
	return (EPROTONOSUPPORT);
    }
    if (prp->pr_type != type) {
	return (EPROTOTYPE);
    }
    MALLOC(so, struct socket *, sizeof(*so), M_SOCKET, M_WAIT);
    bzero((caddr_t)so, sizeof(*so));
    so->so_type = type;
    so->so_proto = prp;
    error =
	(*prp->pr_usrreq)(so, PRU_ATTACH,
			  (struct mbuf *)0, 
			  (struct mbuf *)proto, 
			  (struct mbuf *)0);
    if (error) {
	so->so_state |= SS_NOFDREF;
	sofree(so);
	return (error);
    }
    *aso = so;
    OS_DbgPrint(OSK_MAX_TRACE,("Returning Socket %x\n", so));
    return STATUS_SUCCESS;
#endif
}

static int
getsockaddr(namp, uaddr, len)
/* [<][>][^][v][top][bottom][index][help] */
    struct sockaddr **namp;
caddr_t uaddr;
size_t len;
{
    struct sockaddr *sa;
    int error;

    if (len > SOCK_MAXADDRLEN)
	return ENAMETOOLONG;
    MALLOC(sa, struct sockaddr *, len, M_SONAME, M_WAITOK);
    error = copyin(uaddr, sa, len);
    if (error) {
	FREE(sa, M_SONAME);
    } else {
	*namp = sa;
    }
    return error;
}

NTSTATUS OskitTCPConnect( PVOID socket, PVOID connection, 
			  PVOID nam, OSK_UINT namelen ) {
    struct socket *so = socket;
    struct connect_args _uap = {
	0, nam, namelen
    }, *uap = &_uap;
    int error = EFAULT, s;
    struct mbuf sabuf = { 0 };
    struct sockaddr addr;

    OS_DbgPrint(OSK_MID_TRACE,("Called, socket = %08x\n", socket));

    so->so_connection = connection;

    if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
	error = EALREADY;
	goto done;
    }

    OS_DbgPrint(OSK_MIN_TRACE,("Nam: %x\n", nam));
    if( nam )
	addr = *((struct sockaddr *)nam);

    sabuf.m_data = (void *)&addr;
    sabuf.m_len = sizeof(addr);
    
    addr.sa_family = addr.sa_len;
    addr.sa_len = sizeof(struct sockaddr);

    error = soconnect(so, &sabuf);

    if (error)
	goto bad;

    if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
	error = EINPROGRESS;
	goto done;
    }

    s = splnet();

    while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
	error = tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH,
		       "connect", 0);
	if (error)
	    break;
    }

    if (error == 0) {
	error = so->so_error;
	so->so_error = 0;
    }

    splx(s);

bad:
    so->so_state &= ~SS_ISCONNECTING;

    if (error == ERESTART)
	error = EINTR;

done:
    OS_DbgPrint(OSK_MID_TRACE,("Ending: %08x\n", error));
    return (error);    
}

DWORD OskitTCPClose( VOID *socket ) {
}

DWORD OskitTCPSend( VOID *socket, OSK_PCHAR Data, OSK_UINT Len, int flags ) {
    OskitDumpBuffer( Data, Len );
    struct mbuf mb;
    mb.m_data = Data;
    mb.m_len  = Len;
    return sosend( socket, NULL, NULL, (struct mbuf *)&mb, NULL, 0 );
}

void OskitTCPReceive( VOID *socket, PVOID AddrOut, 
		      OSK_PCHAR Data, OSK_UINT Len, OSK_UINT *OutLen ) {
}

VOID *OskitTCPAccept( VOID *socket, PVOID AddrOut ) {
}

void OskitTCPReceiveDatagram( OSK_PCHAR Data, OSK_UINT Len, 
			      OSK_UINT IpHeaderLen ) {
    struct mbuf *Ip = m_get(M_DONTWAIT, MT_DATA);
    char *NewData = malloc( Len );

    OS_DbgPrint(OSK_MAX_TRACE, ("OskitTCPReceiveDatagram: %d Bytes\n", Len));

    OskitDumpBuffer( Data, Len );

    memset( Ip, 0, sizeof( *Ip ) );
    Ip->m_len = Len;
    Ip->m_data = NewData;
    memcpy( Ip->m_data, Data, Len );

    tcp_input(Ip, IpHeaderLen);

    /* The buffer Ip is freed by tcp_input */
}

void OskitTCPBind( VOID *socket, PVOID name ) {
}

void OskitTCPListen( VOID *socket, int backlog ) {
}

void OskitTCPSetAddress( VOID *socket, 
			 ULONG LocalAddress,
			 USHORT LocalPort,
			 ULONG RemoteAddress,
			 USHORT RemotePort ) {
    struct socket *so = socket;
    struct inpcb *inp = so->so_pcb;
    inp->inp_laddr.s_addr = LocalAddress;
    inp->inp_lport = LocalPort;
    inp->inp_faddr.s_addr = RemoteAddress;
    inp->inp_fport = RemotePort;
    DbgPrint("OSKIT: SET ADDR %x:%x -> %x:%x\n", 
	     LocalAddress, LocalPort,
	     RemoteAddress, RemotePort);
}

void OskitTCPGetAddress( VOID *socket, 
			 PULONG LocalAddress,
			 PUSHORT LocalPort,
			 PULONG RemoteAddress,
			 PUSHORT RemotePort ) {
    struct socket *so = socket;
    struct inpcb *inp = so ? so->so_pcb : 0;
    if( inp ) {
	*LocalAddress = inp->inp_laddr.s_addr;
	*LocalPort = inp->inp_lport;
	*RemoteAddress = inp->inp_faddr.s_addr;
	*RemotePort = inp->inp_fport;
	DbgPrint("OSKIT: GET ADDR %x:%x -> %x:%x\n", 
		 *LocalAddress, *LocalPort,
		 *RemoteAddress, *RemotePort);
    }
}

void oskittcp_die( const char *file, int line ) {
    DbgPrint("\n\n*** OSKITTCP: Panic Called at %s:%d ***\n", file, line);
    KeBugCheck(0);
}
