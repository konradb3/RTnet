/* protocol.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C) 1999,2000 Zentropic Computing, LLC
 *               2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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
 */

#include <rtnet_socket.h>
#include <ipv4/protocol.h>


struct rtinet_protocol *rt_inet_protocols[MAX_RT_INET_PROTOCOLS];

/***
 * rt_inet_add_protocol
 */
void rt_inet_add_protocol(struct rtinet_protocol *prot)
{
    if ( prot!=NULL ) {
        unsigned char hash = rt_inet_hashkey(prot->protocol);
        if ( rt_inet_protocols[hash]==NULL )
            rt_inet_protocols[hash] = prot;
    }
}


/***
 * rt_inet_del_protocol
 */
void rt_inet_del_protocol(struct rtinet_protocol *prot)
{
    if ( prot!=NULL ) {
        unsigned char hash = rt_inet_hashkey(prot->protocol);
        if ( prot==rt_inet_protocols[hash] )
            rt_inet_protocols[hash] = NULL;
    }

}



/***
 * rt_inet_get_protocol - get protocol-structure
 * @protocol: protocol id (maybe IPPROTO_UDP)
 */
struct rtinet_protocol *rt_inet_get_protocol(int protocol)
{
    return  ( rt_inet_protocols[rt_inet_hashkey(protocol)] );
}



/***
 * rt_inet_socket - initialize an Internet socket
 * @sock: socket structure
 * @protocol: protocol id
 */
int rt_inet_socket(SOCKET *sock, int protocol)
{
    struct rtinet_protocol *prot;


    /* only datagram-sockets */
    if (sock->type != SOCK_DGRAM)
        return -EAFNOSUPPORT;

    /* default is UDP */
    if (protocol == 0)
        protocol = IPPROTO_UDP;

    prot = rt_inet_protocols[rt_inet_hashkey(protocol)];

    /* create the socket (call the socket creator) */
    if ((prot != NULL) && (prot->protocol == protocol)) {
        int ret = prot->init_socket(sock);

        RTNET_ASSERT(sock->ops             != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->bind       != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->connect    != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->listen     != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->accept     != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->recvmsg    != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->sendmsg    != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->close      != NULL, return -EPROTO;);
        RTNET_ASSERT(sock->ops->setsockopt != NULL, return -EPROTO;);

        return ret;
    } else {
        rt_printk("RTnet: protocol with id %d not found\n", protocol);

        return -ENOPROTOOPT;
    }
}



/***
 *
 */
unsigned long rt_inet_aton(const char *ip)
{
    int p, n, c;
    union { unsigned long l; char c[4]; } u;
    p = n = 0;
    while ((c = *ip++)) {
        if (c != '.') {
            n = n*10 + c-'0';
        } else {
            if (n > 0xFF) {
                return 0;
            }
            u.c[p++] = n;
            n = 0;
        }
    }
    u.c[3] = n;
    return u.l;
}
