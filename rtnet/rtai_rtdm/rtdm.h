/***************************************************************************
        rtdm.h - user API header (RTAI)

        Real Time Driver Model
        Version:    0.5.3
        Copyright:  2003 Joerg Langenberg <joergel-at-gmx.de>
                    2004 Jan Kiszka <jan.kiszka-at-web.de>

 ***************************************************************************/

/***************************************************************************
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2 of the
 *   License, or (at your option) any later version.
 *
 ***************************************************************************/

#ifndef __RTDM_H
#define __RTDM_H


#ifdef __KERNEL__

#include <linux/ioctl.h>
#include <linux/socket.h>
#include <linux/fcntl.h>

typedef size_t          socklen_t;

#else  /* !__KERNEL__ */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#endif /* __KERNEL__ */

#include <rtnet_config.h>


#define MAX_DEV_NAME_LENGTH     31


#define RTDM_CLASS_PARPORT      1
#define RTDM_CLASS_SERIAL       2
#define RTDM_CLASS_CAN          3
#define RTDM_CLASS_NETWORK      4
#define RTDM_CLASS_RTMAC        5
/*
#define RTDM_CLASS_USB          ?
#define RTDM_CLASS_FIREWIRE     ?
#define RTDM_CLASS_INTERBUS     ?
#define RTDM_CLASS_PROFIBUS     ?
#define ...
*/
#define RTDM_CLASS_EXPERIMENTAL 224 /* up to 255 */


/* Sub-classes: RTDM_CLASS_NETWORK */
#define RTDM_SUBCLASS_RTNET     0


/* Sub-classes: RTDM_CLASS_RTMAC */
#define RTDM_SUBCLASS_TDMA      0
#define RTDM_SUBCLASS_UNMANAGED 1


/* LXRT function IDs */
#define RTDM_LXRT_IDX           6
#define _RTDM_OPEN              0
#define _RTDM_SOCKET            1
#define _RTDM_CLOSE             2
#define _RTDM_IOCTL             3
#define _RTDM_READ              4
#define _RTDM_WRITE             5
#define _RTDM_RECVMSG           6
#define _RTDM_SENDMSG           7
#ifdef CONFIG_RTNET_RTDM_SELECT
#define _RTDM_POLL              8
#define _RTDM_SELECT            9

#include <linux/poll.h>

#endif /* CONFIG_RTNET_RTDM_SELECT */

struct rtdm_dev_context;

struct rtdm_getcontext_args {
    int                     struct_version;
    struct rtdm_dev_context *context;
};

struct rtdm_getsockopt_args {
    int             level;
    int             optname;
    void            *optval;
    socklen_t       *optlen;
};

struct rtdm_setsockopt_args {
    int             level;
    int             optname;
    const void      *optval;
    socklen_t       optlen;
};

struct rtdm_getsockaddr_args {
    struct sockaddr *addr;
    socklen_t       *addrlen;
};

struct rtdm_setsockaddr_args {
    const struct sockaddr   *addr;
    socklen_t               addrlen;
};


#define RTIOC_TYPE_COMMON           0

#define RTIOC_GETCONTEXT            _IOWR(RTIOC_TYPE_COMMON, 0x00, \
                                          struct rtdm_getcontext_args)

#define RTIOC_PURGE                 _IOW(RTIOC_TYPE_COMMON, 0x10, int)

#define RTIOC_GETSOCKOPT            _IOW(RTIOC_TYPE_COMMON, 0x20, \
                                         struct rtdm_getsockopt_args)
#define RTIOC_SETSOCKOPT            _IOW(RTIOC_TYPE_COMMON, 0x21, \
                                         struct rtdm_setsockopt_args)
#define RTIOC_BIND                  _IOW(RTIOC_TYPE_COMMON, 0x22, \
                                         struct rtdm_setsockaddr_args)
#define RTIOC_CONNECT               _IOW(RTIOC_TYPE_COMMON, 0x23, \
                                         struct rtdm_setsockaddr_args)
#define RTIOC_LISTEN                _IOW(RTIOC_TYPE_COMMON, 0x24, int)
#define RTIOC_ACCEPT                _IOW(RTIOC_TYPE_COMMON, 0x25, \
                                         struct rtdm_getsockaddr_args)
#define RTIOC_GETSOCKNAME           _IOW(RTIOC_TYPE_COMMON, 0x26, \
                                         struct rtdm_getsockaddr_args)
#define RTIOC_GETPEERNAME           _IOW(RTIOC_TYPE_COMMON, 0x27, \
                                         struct rtdm_getsockaddr_args)
#define RTIOC_SHUTDOWN              _IOW(RTIOC_TYPE_COMMON, 0x28, int)


#define PURGE_RX_BUFFER             0x0001
#define PURGE_TX_BUFFER             0x0002


#ifdef __KERNEL__

extern int rtdm_open  (int call_flags, const char *path, int oflag);
extern int rtdm_socket(int call_flags, int protocol_family, int socket_type,
                       int protocol);

extern int rtdm_close(int call_flags, int fd);
extern int rtdm_ioctl(int call_flags, int fd, int request, void* arg);

extern ssize_t rtdm_read (int call_flags, int fd, void *buf, size_t nbyte);
extern ssize_t rtdm_write(int call_flags, int fd, const void *buf,
                          size_t nbyte);

extern ssize_t rtdm_recvmsg(int call_flags, int fd, struct msghdr *msg,
                            int flags);
extern ssize_t rtdm_sendmsg(int call_flags, int fd,
                            const struct msghdr *msg, int flags);

#ifdef CONFIG_RTNET_RTDM_SELECT
extern int rtdm_select(int call_flags, int n,
		       fd_set *readfds,
		       fd_set *writefds,
		       fd_set *exceptfds); /* struct timeval timeout */

#endif /* CONFIG_RTNET_RTDM_SELECT */


static inline int open_rt(const char *path, int oflag, ...)
{
    return rtdm_open(0, path, oflag);
}

static inline int socket_rt(int protocol_family, int socket_type,
                            int protocol)
{
    return rtdm_socket(0, protocol_family, socket_type, protocol);
}



static inline int close_rt(int fd)
{
    return rtdm_close(0, fd);
}

static inline int ioctl_rt(int fd, int request, void* arg)
{
    return rtdm_ioctl(0, fd, request, arg);
}



static inline ssize_t read_rt(int fd, void *buf, size_t nbyte)
{
    return rtdm_read(0, fd, buf, nbyte);
}

static inline ssize_t write_rt(int fd, const void *buf, size_t nbyte)
{
    return rtdm_write(0, fd, buf, nbyte);
}



static inline ssize_t recvmsg_rt(int fd, struct msghdr *msg, int flags)
{
    return rtdm_recvmsg(0, fd, msg, flags);
}

static inline ssize_t sendmsg_rt(int fd, const struct msghdr *msg, int flags)
{
    return rtdm_sendmsg(0, fd, msg, flags);
}

#ifdef CONFIG_RTNET_RTDM_SELECT
static inline int select_rt(int call_flags, int n,
			    fd_set *readfds,
			    fd_set *writefds,
			    fd_set *exceptfds)
{
    return rtdm_select(call_flags, n, readfds, writefds, exceptfds); /* timeval is missing here */
}
#endif /* CONFIG_RTNET_RTDM_SELECT */

#else /* !__KERNEL__ */


#ifdef CONFIG_NEWLXRT

#ifdef CONFIG_RTAI_24
# include <rtai_lxrt_user.h>
#endif

#include <rtai_lxrt.h>


#ifdef CONFIG_RTAI_24
# define RTAI_PROTO(type, name, arglist)     DECLARE type name arglist
#endif


RTAI_PROTO(int, open_rt, (const char *path, int oflag, ...))
{
    struct {const char *path; int oflag;} _arg = {path, oflag};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_OPEN, &_arg).i[LOW];
}

RTAI_PROTO(int, socket_rt, (int protocol_family, int socket_type,
                            int protocol))
{
    struct {int protocol_family; int socket_type; int protocol;} _arg =
        {protocol_family, socket_type, protocol};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_SOCKET, &_arg).i[LOW];
}



RTAI_PROTO(int, close_rt, (int fd))
{
    struct {int fd;} _arg = {fd};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_CLOSE, &_arg).i[LOW];
}

RTAI_PROTO(int, ioctl_rt, (int fd, int request, void *arg))
{
    struct {int fd; int request; void *arg;} _arg = {fd, request, arg};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_IOCTL, &_arg).i[LOW];
}



RTAI_PROTO(ssize_t, read_rt, (int fd, void *buf, size_t nbyte))
{
    struct {int fd; void *buf; size_t nbyte;} _arg = {fd, buf, nbyte};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_READ, &_arg).i[LOW];
}

RTAI_PROTO(ssize_t, write_rt, (int fd, const void *buf, size_t nbyte))
{
    struct {int fd; const void *buf; size_t nbyte;} _arg = {fd, buf, nbyte};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_WRITE, &_arg).i[LOW];
}



RTAI_PROTO(ssize_t, recvmsg_rt, (int fd, struct msghdr *msg, int flags))
{
    struct {int fd; struct msghdr *msg; int flags;} _arg = {fd, msg, flags};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_RECVMSG, &_arg).i[LOW];
}

RTAI_PROTO(ssize_t, sendmsg_rt, (int fd, const struct msghdr *msg, int flags))
{
    struct {int fd; const struct msghdr *msg; int flags;} _arg =
        {fd, msg, flags};
    return rtai_lxrt(RTDM_LXRT_IDX, sizeof(_arg), _RTDM_SENDMSG, &_arg).i[LOW];
}

#endif /* CONFIG_NEWLXRT */

#endif /* __KERNEL__ */


static inline ssize_t recvfrom_rt(int fd, void *buf, size_t len, int flags,
                                  struct sockaddr *from, socklen_t *fromlen)
{
    struct iovec    iov = {buf, len};
    struct msghdr   msg =
        {from, (from != NULL) ? *fromlen : 0, &iov, 1, NULL, 0};
    int             ret;

    ret = recvmsg_rt(fd, &msg, flags);
    if ((ret >= 0) && (from != NULL))
        *fromlen = msg.msg_namelen;
    return ret;
}

static inline ssize_t recv_rt(int fd, void *buf, size_t len, int flags)
{
    return recvfrom_rt(fd, buf, len, flags, NULL, NULL);
}



static inline ssize_t sendto_rt(int fd, const void *buf, size_t len, int flags,
                                const struct sockaddr *to, socklen_t tolen)
{
    struct iovec    iov = {(void *)buf, len};
    struct msghdr   msg =
        {(struct sockaddr *)to, tolen, &iov, 1, NULL, 0};

    return sendmsg_rt(fd, &msg, flags);
}

static inline ssize_t send_rt(int fd, const void *buf, size_t len, int flags)
{
    return sendto_rt(fd, buf, len, flags, NULL, 0);
}



static inline int getsockopt_rt(int fd, int level, int optname,
                                void *optval, socklen_t *optlen)
{
    struct rtdm_getsockopt_args args = {level, optname, optval, optlen};

    return ioctl_rt(fd, RTIOC_GETSOCKOPT, &args);
}

static inline int setsockopt_rt(int fd, int level, int optname,
                                const void *optval, socklen_t optlen)
{
    struct rtdm_setsockopt_args args =
        {level, optname, (void *)optval, optlen};

    return ioctl_rt(fd, RTIOC_SETSOCKOPT, &args);
}



static inline int bind_rt(int fd, const struct sockaddr *my_addr, socklen_t addrlen)
{
    struct rtdm_setsockaddr_args args = {my_addr, addrlen};

    return ioctl_rt(fd, RTIOC_BIND, &args);
}

static inline int connect_rt(int fd, const struct sockaddr *serv_addr,
                             socklen_t addrlen)
{
    struct rtdm_setsockaddr_args args = {serv_addr, addrlen};

    return ioctl_rt(fd, RTIOC_CONNECT, &args);
}



static inline int getsockname_rt(int fd, struct sockaddr *name,
                                 socklen_t *namelen)
{
    struct rtdm_getsockaddr_args args = {name, namelen};

    return ioctl_rt(fd, RTIOC_GETSOCKNAME, &args);
}

static inline int rt_socket_getpeername(int fd, struct sockaddr *name,
                                        socklen_t *namelen)
{
    struct rtdm_getsockaddr_args args = {name, namelen};

    return ioctl_rt(fd, RTIOC_GETPEERNAME, &args);
}

#endif /* __RTDM_H */
