/***************************************************************************
        rtdm_driver.h - driver API header (RTAI)

        Real Time Driver Model
        Version:    0.5.0
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

#ifndef __RTDM_DRIVER_H
#define __RTDM_DRIVER_H

#include <asm/atomic.h>
#include <linux/list.h>

#include <rtdm.h>


/* ----------- Device Flags ---------------------------------------------- */
#define RTDM_EXCLUSIVE          0x0001
#define RTDM_NAMED_DEVICE       0x0010
#define RTDM_PROTOCOL_DEVICE    0x0020
#define RTDM_DEVICE_TYPE        0x00F0

/* ----------- Context Flags (bit numbers) ------------------------------- */
#define RTDM_CREATED_IN_NRT     0
#define RTDM_CLOSING            1
#define RTDM_USER_CONTEXT_FLAG  8   /* first user-definable flag */

/* ----------- Call Flags ------------------------------------------------ */
#define RTDM_USER_MODE_CALL     0x0001
#define RTDM_NRT_CALL           0x0002


#define RTDM_DEVICE_STRUCT_VER  1
#define RTDM_CONTEXT_STRUCT_VER 1


struct rtdm_device;
struct rtdm_dev_context;

struct rtdm_operations {
    /* common operations */
    int         (*close_rt)   (struct rtdm_dev_context *context,
                               int call_flags);
    int         (*close_nrt)  (struct rtdm_dev_context *context,
                               int call_flags);
    int         (*ioctl_rt)   (struct rtdm_dev_context *context,
                               int call_flags, int request, void* arg);
    int         (*ioctl_nrt)  (struct rtdm_dev_context *context,
                               int call_flags, int request, void* arg);

    /* stream-oriented device operations */
    ssize_t     (*read_rt)    (struct rtdm_dev_context *context,
                               int call_flags, void *buf, size_t nbyte);
    ssize_t     (*read_nrt)   (struct rtdm_dev_context *context,
                               int call_flags, void *buf, size_t nbyte);
    ssize_t     (*write_rt)   (struct rtdm_dev_context *context,
                               int call_flags, const void *buf, size_t nbyte);
    ssize_t     (*write_nrt)  (struct rtdm_dev_context *context,
                               int call_flags, const void *buf, size_t nbyte);

    /* message-oriented device operations */
    ssize_t     (*recvmsg_rt) (struct rtdm_dev_context *context,
                               int call_flags, struct msghdr *msg, int flags);
    ssize_t     (*recvmsg_nrt)(struct rtdm_dev_context *context,
                               int call_flags, struct msghdr *msg, int flags);
    ssize_t     (*sendmsg_rt) (struct rtdm_dev_context *context,
                               int call_flags, const struct msghdr *msg,
                               int flags);
    ssize_t     (*sendmsg_nrt)(struct rtdm_dev_context *context,
                               int call_flags, const struct msghdr *msg,
                               int flags);
};

struct rtdm_dev_context {
    int         context_flags;
    int         fd;
    atomic_t    close_lock_count;
    struct rtdm_operations *ops;
    struct rtdm_device *device;
    char        dev_private[0];
};

struct rtdm_dev_reserved {
    struct list_head entry;
    atomic_t    refcount;
    struct rtdm_dev_context *exclusive_context;
};

struct rtdm_device {
    int         struct_version;

    int         device_flags;
    size_t      context_size;

    /* named device identification */
    char        device_name[MAX_DEV_NAME_LENGTH+1];

    /* protocol device identification */
    int         protocol_family;
    int         socket_type;

    /* device instance creation */
    int         (*open_rt)   (struct rtdm_dev_context *context,
                              int call_flags, int oflag);
    int         (*open_nrt)  (struct rtdm_dev_context *context,
                              int call_flags, int oflag);
    int         (*socket_rt) (struct rtdm_dev_context *context,
                              int call_flags, int protocol);
    int         (*socket_nrt)(struct rtdm_dev_context *context,
                              int call_flags, int protocol);

    struct rtdm_operations ops;

    int         device_class;
    int         device_sub_class;
    const char  *driver_name;
    const char  *peripheral_name;
    const char  *provider_name;

    /* /proc entry */
    const char  *proc_name;
    struct proc_dir_entry *proc_entry;

    /* driver-definable value */
    void        *driver_arg;

    struct rtdm_dev_reserved reserved;
};


extern int rtdm_dev_register(struct rtdm_device* device);
extern int rtdm_dev_unregister(struct rtdm_device* device);


#endif /* __RTDM_DRIVER_H */
