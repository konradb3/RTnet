/***
 * rtnet/socket.c - sockets implementation for rtnet
 *
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
 *               2003      Jan Kiszka <jan.kiszka@web.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include <rtnet.h>
#include <rtnet_internal.h>
#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <ipv4/protocol.h>


static unsigned int socket_rtskbs = DEFAULT_SOCKET_RTSKBS;
MODULE_PARM(socket_rtskbs, "i");
MODULE_PARM_DESC(socket_rtskbs, "Default number of realtime socket buffers in socket pools");

static SOCKET       rt_sockets[RT_SOCKETS];
static SOCKET       *free_rtsockets;
static spinlock_t   socket_base_lock;

static unsigned int rtsocket_fd = 1;
#define new_rtsocket_fd() (rtsocket_fd++)


/************************************************************************
 *  internal socket functions                                           *
 ************************************************************************/

/***
 *  rt_socket_alloc
 */
SOCKET *rt_socket_alloc(void)
{
    unsigned long   flags;
    SOCKET          *sock;

    flags = rt_spin_lock_irqsave(&socket_base_lock);

    sock = free_rtsockets;
    if (!sock) {
        rt_spin_unlock_irqrestore(flags, &socket_base_lock);
        return NULL;
    }
    free_rtsockets = free_rtsockets->next;

    rt_spin_unlock_irqrestore(flags, &socket_base_lock);

    sock->state=TCP_CLOSE;
    sock->next=NULL;

    rt_sem_init(&sock->wakeup_sem, 0);

    if (rtskb_pool_init(&sock->skb_pool, socket_rtskbs) < socket_rtskbs) {
        rt_socket_release(sock);
        return NULL;
    }

    return sock;
}



/***
 *  rt_socket_release
 */
void rt_socket_release(SOCKET *sock)
{
    unsigned long flags;

    rtskb_pool_release(&sock->skb_pool);
    rt_sem_delete(&sock->wakeup_sem);

    memset(sock, 0, sizeof(SOCKET));

    flags = rt_spin_lock_irqsave(&socket_base_lock);

    sock->next = free_rtsockets;
    free_rtsockets = sock;

    rt_spin_unlock_irqrestore(flags, &socket_base_lock);
}



/***
 *  rt_scoket_lookup
 *  @fd - file descriptor
 */
SOCKET *rt_socket_lookup (int fd)
{
    int i;
    for (i=0; i<RT_SOCKETS; i++) {
        if (rt_sockets[i].fd==fd)
            return &rt_sockets[i];
    }
    return NULL;
}



/************************************************************************
 *  file discribtor socket interface                                    *
 ************************************************************************/

/***
 *  rt_socket
 *  Create a new socket of type TYPE in domain DOMAIN (family),
 *  using protocol PROTOCOL.  If PROTOCOL is zero, one is chosen
 *  automatically. Returns a file descriptor for the new socket,
 *  or errors < 0.
 */
int rt_socket(int family, int type, int protocol)
{
    SOCKET *sock = NULL;
    unsigned char hash;

    /* protol family (PF_INET) and adress family (AF_INET) only */
    if ( (family!=AF_INET) )
        return -EAFNOSUPPORT;

    /* only datagram-sockets */
    if ( type!=SOCK_DGRAM )
        return -EAFNOSUPPORT;

    /* allocate a new socket */
    if ( !(sock=rt_socket_alloc()) ) {
        rt_printk("RTnet: no more rt-sockets\n");
        return -ENOMEM;
    }

    /* create new file descriptor */
    sock->fd=new_rtsocket_fd();

    /* default priority */
    sock->priority = SOCK_DEF_PRIO;

    /* default UDP-Protocol */
    if (!protocol)
        hash = rt_inet_hashkey(IPPROTO_UDP);
    else
        hash = rt_inet_hashkey(protocol);

    /* create the socket (call the socket creator) */
    if  (rt_inet_protocols[hash]) {
        int s;
        s = rt_inet_protocols[hash]->socket(sock);

        /* This is the right place to check if sock->ops is not NULL. */
        if (NULL == sock->ops) {
            rt_printk("%s:%s: sock-ops is NULL!\n", __FUNCTION__, __LINE__);
            /* Do something reasonable...
             * (insert code here)
             */
        }
        /* This is the right place to check if sock->ops->... are not NULL. */
        return s;
    } else {
        rt_printk("RTnet: protocol with id %d not found\n", protocol);
        rt_socket_release(sock);
        return -ENOPROTOOPT;
    }
}



/***
 *  rt_sock_bind
 *  Bind a socket to a sockaddr
 */
int rt_socket_bind(int s, struct sockaddr *my_addr, socklen_t addrlen)
{
    SOCKET *sock = rt_socket_lookup(s);
    if ((NULL == sock) ||
        (NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->bind)) /* There should be no socket without ops! */
        return -ENOTSOCK;
    return ( sock->ops->bind(sock, my_addr, addrlen) );
}



/***
 *  rt_socket_listen
 */
int rt_socket_listen(int s, int backlog)
{
    SOCKET *sock = rt_socket_lookup(s);
    if ((NULL == sock) ||
        (NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->listen)) /* There should be no socket without ops! */
        return -ENOTSOCK;
    return ( sock->ops->listen(sock, backlog) );
}



/***
 *  rt_socket_connect
 */
int rt_socket_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
    SOCKET *sock = rt_socket_lookup(s);
    if ((NULL == sock) ||
        (NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->connect)) /* There should be no socket without ops! */
        return -ENOTSOCK;
    return ( sock->ops->connect(sock, serv_addr, addrlen) );
}



/***
 *  rt_socket_accept
 */
int rt_socket_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    SOCKET *sock = rt_socket_lookup(s);
    if ((NULL == sock) ||
        (NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->accept)) /* There should be no socket without ops! */
        return -ENOTSOCK;
    return ( sock->ops->accept(sock, addr, addrlen) );
}



/***
 *  rt_socket_close
 */
int rt_socket_close(int s)
{
    SOCKET *sock = rt_socket_lookup(s);
    if ((NULL == sock) ||
        (NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->close)) /* There should be no socket without ops! */
        return -ENOTSOCK;

    sock->ops->close(sock,0);
    rt_socket_release(sock);
    return 0;
}



/***
 *  rt_socket_send
 */
int rt_socket_send(int s, const void *msg, size_t len, int flags)
{
    return rt_socket_sendto(s, msg, len, flags, NULL, 0);
}



/***
 *  rt_socket_recv
 */
int rt_socket_recv(int s, void *buf, size_t len, int flags)
{
    int fromlen=0; /* fix for null pointer dereference-NZG */
    return rt_socket_recvfrom(s, buf, len, flags, NULL, &fromlen);
}



/***
 *  rt_socket_sendto
 */
int rt_socket_sendto(int s, const void *msg, size_t len, int flags,
                     const struct sockaddr *to, socklen_t tolen)
{
    SOCKET *sock=rt_socket_lookup(s);
    struct msghdr msg_hdr;
    struct iovec iov;

    if ( !sock ) {
        rt_printk("RTnet: socket %d not found\n", s);
        return -ENOTSOCK;
    }

    if ((NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->sendmsg)) /* There should be no socket without ops! */
        return -ENOTSOCK;

    iov.iov_base=(void *)msg;
    iov.iov_len=len;

    msg_hdr.msg_name=(void*)to;
    msg_hdr.msg_namelen=tolen;
    msg_hdr.msg_iov=&iov;
    msg_hdr.msg_iovlen=1;
/*    msg_hdr.msg_control=NULL;     not used
    msg_hdr.msg_controllen=0; */

    return sock->ops->sendmsg(sock, &msg_hdr, len, flags);
}



/***
 *  rt_recvfrom
 *  @s          socket descriptor
 *  @buf        buffer
 *  @len        length of buffer
 *  @flags      usermode -> kern
 *                  MSG_DONTROUTE: target is in lan
 *                  MSG_DONTWAIT : if there no data to recieve, get out
 *                  MSG_ERRQUEUE :
 *              kern->usermode
 *                  MSG_TRUNC    :
 */
int rt_socket_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    SOCKET *sock=rt_socket_lookup(s);
    struct msghdr msg_hdr;
    struct iovec iov;
    int error=0;

    if ((NULL == sock) ||
        (NULL == sock->ops) || /* This check shall be obsolete in the future! */
        (NULL == sock->ops->recvmsg)) /* There should be no socket without ops! */
        return -ENOTSOCK;

    iov.iov_base=buf;
    iov.iov_len=len;
    msg_hdr.msg_name=from;
    msg_hdr.msg_namelen=*fromlen;
    msg_hdr.msg_iov=&iov;
    msg_hdr.msg_iovlen=1;
/*    msg_hdr.msg_control=NULL;     not used
    msg_hdr.msg_controllen=0; */

    error = sock->ops->recvmsg(sock, &msg_hdr, len, flags);

    if ( (error>=0) && (*fromlen!=0) )
        *fromlen=msg_hdr.msg_namelen;

    return error;
}



/***
 *  rt_socket_sendmsg
 */
int rt_socket_sendmsg(int s, const struct msghdr *msg, int flags)
{
    SOCKET *sock=rt_socket_lookup(s);
    int len;

    if (sock == NULL)
        return -ENOTSOCK;

    ASSERT((sock->ops != NULL) && (sock->ops->sendmsg), return -ENOTSOCK;);

    len=rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    return sock->ops->sendmsg(sock, msg, len, flags);
}



/***
 *  rt_socket_recvmsg
 */
int rt_socket_recvmsg(int s, struct msghdr *msg, int flags)
{
    SOCKET *sock=rt_socket_lookup(s);
    int total_len;

    if (sock == NULL)
        return -ENOTSOCK;

    ASSERT((sock->ops != NULL) && (sock->ops->recvmsg), return -ENOTSOCK;);

    total_len=rt_iovec_len(msg->msg_iov,msg->msg_iovlen);
    return sock->ops->recvmsg(sock, msg, total_len, flags);
}



/***
 *  rt_sock_getsockname
 */
int rt_socket_getsockname(int s, struct sockaddr *addr, socklen_t addrlen)
{
    SOCKET *sock=rt_socket_lookup(s);
    struct sockaddr_in *usin = (struct sockaddr_in *)addr;

    if (sock == NULL)
        return -ENOTSOCK;

    usin->sin_family=sock->family;
    usin->sin_addr.s_addr=sock->saddr;
    usin->sin_port=sock->sport;

    return sizeof(struct sockaddr_in);
}



/***
 *  rt_socket_callback
 */
int rt_socket_callback(int s, int (*func)(int,void *), void *arg)
{
    SOCKET *sock=rt_socket_lookup(s);

    if (sock == NULL)
        return -ENOTSOCK;

    sock->private=arg;
    sock->wakeup=func;

    return 0;
}



/***
 *  rt_socket_setsockopt
 */
int rt_socket_setsockopt(int s, int level, int optname, const void *optval,
                         socklen_t optlen)
{
    SOCKET *sock=rt_socket_lookup(s);
    int ret = 0;

    if (sock == NULL)
        return -ENOTSOCK;

    if (level == SOL_SOCKET) {
        if (optlen < sizeof(unsigned int))
            return -EINVAL;

        switch (optname) {
            case RT_SO_EXTPOOL:
                ret = rtskb_pool_extend(&sock->skb_pool,
                                        *(unsigned int *)optval);
                break;

            case RT_SO_SHRPOOL:
                ret = rtskb_pool_shrink(&sock->skb_pool,
                                        *(unsigned int *)optval);
                break;

            case RT_SO_PRIORITY:
                sock->priority = *(unsigned int *)optval;
                break;

            case RT_SO_NONBLOCK:
                if (*(unsigned int *)optval != 0)
                    sock->flags |= RT_SOCK_NONBLOCK;
                else
                    sock->flags &= ~RT_SOCK_NONBLOCK;
                break;

            case RT_SO_TIMEOUT:
                if (optlen < sizeof(RTIME))
                    ret = -EINVAL;
                else
                    sock->timeout = *(RTIME *)optval;
                break;

            default:
                ret = -ENOPROTOOPT;
                break;
        }
    } else
        ret = sock->ops->setsockopt(sock, level, optname, optval, optlen);

    return ret;
}



/************************************************************************
 *  static socket interface                                             *
 ************************************************************************/

/***
 *  rt_ssocket
 */
int rt_ssocket(SOCKET* socket, int family, int type, int protocol)
{
    if (socket == NULL)
        return -ENOTSOCK;
    else {
        unsigned char hash;

        /* protol family (PF_INET) and adress family (AF_INET) only */
        if (family!=AF_INET)
            return -EAFNOSUPPORT;

        /* only datagram-sockets */
        if (type!=SOCK_DGRAM)
            return -EAFNOSUPPORT;

        /* create new file descriptor */
        socket->fd=new_rtsocket_fd();

        /* default priority */
        socket->priority = SOCK_DEF_PRIO;

        /* default UDP-Protocol */
        if (!protocol)
            hash = rt_inet_hashkey(IPPROTO_UDP);
        else
            hash = rt_inet_hashkey(protocol);

        /* create the socket (call the socket creator) */
        if  ((rt_inet_protocols[hash]) && (rt_inet_protocols[hash]->socket(socket) > 0))
            return 0;
        else {
            rt_printk("RTnet: protocol with id %d not found\n", protocol);
            rt_socket_release(socket);
            return -ENOPROTOOPT;
        }
    }
}



/***
 *  rt_ssocket_bind
 */
int rt_ssocket_bind(SOCKET *socket, struct sockaddr *addr, socklen_t addrlen)
{
    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->bind), return -ENOTSOCK;);

    return socket->ops->bind(socket, addr, addrlen);
}



/***
 *  rt_ssocket_listen
 */
int rt_ssocket_listen(SOCKET *socket, int backlog)
{
    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->listen), return -ENOTSOCK;);

    return socket->ops->listen(socket, backlog);
}



/***
 *  rt_ssocket_connect
 */
int rt_ssocket_connect(SOCKET *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->connect), return -ENOTSOCK;);

    return socket->ops->connect(socket, addr, addrlen);
}



/***
 *  rt_ssocket_accept
 */
int rt_ssocket_accept(SOCKET *socket, struct sockaddr *addr, socklen_t *addrlen)
{
    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->accept), return -ENOTSOCK;);

    return socket->ops->accept(socket, addr, addrlen);
}



/***
 *  rt_ssocket_close
 */
int rt_ssocket_close(SOCKET *socket)
{
    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->close), return -ENOTSOCK;);

    socket->ops->close(socket, 0);
    return 0;
}



/***
 *  rt_ssocket_writev
 */
int rt_ssocket_writev(SOCKET *socket, const struct iovec *iov, int count)
{
    struct msghdr msg_hdr;
    size_t total_len;

    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->sendmsg), return -ENOTSOCK;);

    msg_hdr.msg_name=NULL;
    msg_hdr.msg_namelen=0;
    msg_hdr.msg_iov=(struct iovec*)iov;
    msg_hdr.msg_iovlen=count;
/*    msg_hdr.msg_control=NULL;     not used
    msg_hdr.msg_controllen=0; */

    total_len = rt_iovec_len(iov, count);
    return socket->ops->sendmsg(socket, &msg_hdr, total_len, 0);
}



/***
 *  rt_ssocket_send
 */
int rt_ssocket_send(SOCKET *socket, const void *msg, size_t len, int flags)
{
    return rt_ssocket_sendto(socket, msg, len, flags, NULL, 0);
}



/***
 *  rt_ssocket_sendto
 */
int rt_ssocket_sendto(SOCKET *socket, const void *msg, size_t len, int flags,
                      const struct sockaddr *to, socklen_t tolen)
{
    struct msghdr msg_hdr;
    struct iovec iov;

    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->sendmsg), return -ENOTSOCK;);

    iov.iov_base=(void *)msg;
    iov.iov_len=len;

    msg_hdr.msg_name=(struct sockaddr*)to;
    msg_hdr.msg_namelen=tolen;
    msg_hdr.msg_iov=(void*)&iov;
    msg_hdr.msg_iovlen=1;
/*    msg_hdr.msg_control=NULL;     not used
    msg_hdr.msg_controllen=0; */

    return socket->ops->sendmsg(socket, &msg_hdr, len, flags);
}



/***
 *  rt_socket_sendmsg
 */
int rt_ssocket_sendmsg(SOCKET *socket, const struct msghdr *msg, int flags)
{
    int total_len;

    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->sendmsg), return -ENOTSOCK;);

    total_len=rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    return socket->ops->sendmsg(socket, msg, total_len, flags);
}



/***
 *  rt_ssocket_readv
 */
int rt_ssocket_readv(SOCKET *socket, const struct iovec *iov, int count)
{
    struct msghdr msg_hdr;
    int total_len;

    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->recvmsg), return -ENOTSOCK;);

    msg_hdr.msg_name=NULL;
    msg_hdr.msg_namelen=0;
    msg_hdr.msg_iov=(struct iovec*)iov;
    msg_hdr.msg_iovlen=count;
/*    msg_hdr.msg_control=NULL;     not used
    msg_hdr.msg_controllen=0; */

    total_len = rt_iovec_len(iov, count);
    return socket->ops->recvmsg(socket, &msg_hdr, total_len, 0);
}



/***
 *  rt_ssocket_recv
 */
int rt_ssocket_recv(SOCKET *socket, void *buf, size_t len, int flags)
{
    return rt_ssocket_recvfrom(socket, buf, len, flags, NULL, 0);
}



/***
 *  rt_ssocket_recfrom
 */
int rt_ssocket_recvfrom(SOCKET *socket, void *buf, size_t len, int flags,
                        struct sockaddr *from, socklen_t fromlen)
{
    struct msghdr msg_hdr;
    struct iovec iov;
    int error=0;

    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->recvmsg), return -ENOTSOCK;);

    iov.iov_base=buf;
    iov.iov_len=len;
    msg_hdr.msg_name=from;
    msg_hdr.msg_namelen=fromlen; //sizeof(struct sockaddr);
    msg_hdr.msg_iov=&iov;
    msg_hdr.msg_iovlen=1;
/*    msg_hdr.msg_control=NULL;     not used
    msg_hdr.msg_controllen=0; */

    error = socket->ops->recvmsg(socket, &msg_hdr, len, flags);
    if ( (error>=0) && (fromlen!=0) )
        fromlen=msg_hdr.msg_namelen;

    return error;
}



/***
 *  rt_ssocket_recvmsg
 */
int rt_ssocket_recvmsg(SOCKET *socket, struct msghdr *msg, int flags)
{
    size_t total_len;

    if (socket == NULL)
        return -ENOTSOCK;

    ASSERT((socket->ops != NULL) && (socket->ops->recvmsg), return -ENOTSOCK;);

    total_len = rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    return socket->ops->recvmsg(socket, msg, total_len, flags);
}



/***
 *  rt_ssocket_getsocketname
 */
int rt_ssocket_getsockname(SOCKET *socket, struct sockaddr *addr, socklen_t addrlen)
{
    if (socket == NULL)
        return -ENOTSOCK;
    // Längen-Check!!!
    else {
        struct sockaddr_in *usin = (struct sockaddr_in *)addr;

        usin->sin_family=socket->family;
        usin->sin_addr.s_addr=socket->saddr;
        usin->sin_port=socket->sport;

        return sizeof(struct sockaddr_in);
    }
}



/***
 *  rt_ssocket_callback
 */
int rt_ssocket_callback(SOCKET *socket, int (*func)(int,void *), void *arg)
{
    if (socket == NULL)
        return -ENOTSOCK;
    else {
        socket->private=arg;
        socket->wakeup=func;

        return 0;
    }
}



/************************************************************************
 *  initialisation of rt-socket interface                               *
 ************************************************************************/

/***
 *  rtsocket_init
 */
void rtsockets_init(void)
{
    int i;

    spin_lock_init(&socket_base_lock);

    /* initialise the first socket */
    rt_sockets[0].prev=NULL;
    rt_sockets[0].next=&rt_sockets[1];
    rt_sockets[0].state=TCP_CLOSE;
    rtskb_queue_head_init(&rt_sockets[0].incoming);

    /* initialise the last socket */
    rt_sockets[RT_SOCKETS-1].prev=&rt_sockets[RT_SOCKETS-2];
    rt_sockets[RT_SOCKETS-1].next=NULL;
    rt_sockets[RT_SOCKETS-1].state=TCP_CLOSE;
    rtskb_queue_head_init(&rt_sockets[RT_SOCKETS-1].incoming);

    for (i=1; i<RT_SOCKETS-1; i++) {
        rt_sockets[i].next=&rt_sockets[i+1];
        rt_sockets[i].prev=&rt_sockets[i-1];
        rt_sockets[i].state=TCP_CLOSE;
        rtskb_queue_head_init(&rt_sockets[i].incoming);
    }
    free_rtsockets=&rt_sockets[0];
}



/***
 *  rtsocket_release
 */
void rtsockets_release(void)
{
    free_rtsockets=NULL;
}
