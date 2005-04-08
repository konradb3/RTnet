/***************************************************************************
        rtdm_driver.h - driver API header (RTAI)

        Real Time Driver Model
        Version:    0.6.0
        Copyright:  2003 Joerg Langenberg <joergel-at-gmx.de>
                    2004, 2005 Jan Kiszka <jan.kiszka-at-web.de>
                    2005 Hans-Peter Bock <rtnet@avaapgh.de>

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

#ifdef CONFIG_RTNET_RTDM_SELECT
#include <rtai_sem.h>

#if 1 /* change this to zero, if you are living on the bleeding edge and want to examine waitqueues */
typedef SEM             wait_queue_primitive_t;

static inline int wq_element_init(wait_queue_primitive_t *wqe)
{
    rt_typed_sem_init(wqe, 0, CNT_SEM);
    return 0;
}

static inline void wq_element_delete(wait_queue_primitive_t *wqe)
{
    rt_sem_delete(wqe);
}

static inline void wq_wakeup(wait_queue_primitive_t *wqe)
{
    rt_sem_signal(wqe);
}

static inline int wq_wait(wait_queue_primitive_t *wqe)
{
    return rt_sem_wait(wqe);
    /* return rt_sem_wait_timed(event, *timeout); */
}

#else /* 1 */

#include <rtnet_sys_rtai.h> /* needed for rtos_spinlock_t () */
typedef SEM wait_queue_primitive_t;

struct wait_queue_element;

struct wait_queue_head {
    struct wait_queue_element   *next;
    struct wait_queue_element   *last;
    rtos_spinlock_t             lock;
};

struct wait_queue_element {
    struct wait_queue_element   *next, *prev;
    struct wait_queue_head      *head;
    wait_queue_primitive_t      *wqp;
};

static inline void wq_init(struct wait_queue_head *wqh)
{
    wqh->next = NULL;
}

static inline int wq_element_init(struct wait_queue_element *wqe)
{
    wqe = rt_malloc(sizeof(struct wait_queue_element));
    if (wqe) {
	wqe->next = NULL;
	wqe->prev = NULL;
	wqe->head = NULL;
	wqe->wqp = NULL;
    }
    return ((int) wqe); /* this cast is evil */
}

static inline void wq_add(struct wait_queue_element *wqe,
			 struct wait_queue_head *wqh,
			 wait_queue_primitive_t *wqp)
{
    wqe->next = NULL;
    wqe->head = wqh;
    wqe->wqp = wqp;
    
    /* lock wait_queue_list */
    if (NULL == wqh->next) { /* => list is empty */
	wqe->prev = NULL;
	wqh->next = wqe;
    } else {
	wqe->prev = wqh->last;
	wqe->prev->next = wqe;
    }
    wqh->last = wqe;
    /* unlock wait_queue_list */
}

static inline void wq_remove(struct wait_queue_element *wqe)
{
    /* lock wait_queue_list */
    wqe->prev->next = wqe->next;
    wqe->next->prev = wqe->prev;
    /* unlock wait_queue_list */
}

static inline void wq_element_delete(struct wait_queue_element *wqe)
{
    rt_free(wqe);
    wqe = NULL;
}

static inline void wq_signal(struct wait_queue_head *wqh)
{
    struct wait_queue_element *wqe;
    /* lock wait_queue_list */
    wqe = wqh->next;
    while (NULL != wqe) {
	rt_sem_signal(wqe->wqp);
	wqe = wqe->next;
    }
    /* unlock wait_queue_list */
}

#endif /* 1 */
#endif /* CONFIG_RTNET_RTDM_SELECT */

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


/* ----------- Version Flags --------------------------------------------- */
#define RTDM_SECURE_DEVICE      0x80000000  /* not supported here */

/* ----------- Structure Versions ---------------------------------------- */
#define RTDM_DEVICE_STRUCT_VER  2
#define RTDM_CONTEXT_STRUCT_VER 2


struct rtdm_device;
struct rtdm_dev_context;


typedef int     (*open_handler)     (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     int                        oflag);

typedef int     (*socket_handler)   (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     int                        protocol);

typedef int     (*close_handler)    (struct rtdm_dev_context    *context,
                                     int                        call_flags);

typedef int     (*ioctl_handler)    (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     int                        request,
                                     void                       *arg);

typedef ssize_t (*read_handler)     (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     void                       *buf,
                                     size_t                     nbyte);

typedef ssize_t (*write_handler)    (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     const void                 *buf,
                                     size_t                     nbyte);

typedef ssize_t (*recvmsg_handler)  (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     struct msghdr              *msg,
                                     int                        flags);

typedef ssize_t (*sendmsg_handler)  (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     const struct msghdr        *msg,
                                     int                        flags);


typedef int     (*rt_handler)       (struct rtdm_dev_context    *context,
                                     int                        call_flags,
                                     void                       *arg);


struct rtdm_operations {
    /* common operations */
    close_handler               close_rt;
    close_handler               close_nrt;
    ioctl_handler               ioctl_rt;
    ioctl_handler               ioctl_nrt;

    /* stream-oriented device operations */
    read_handler                read_rt;
    read_handler                read_nrt;
    write_handler               write_rt;
    write_handler               write_nrt;

    /* message-oriented device operations */
    recvmsg_handler             recvmsg_rt;
    recvmsg_handler             recvmsg_nrt;
    sendmsg_handler             sendmsg_rt;
    sendmsg_handler             sendmsg_nrt;

#ifdef CONFIG_RTNET_RTDM_SELECT
    /* event-oriented device operations */
    unsigned int (*poll_rt)   (struct rtdm_dev_context *context); /* , poll_table *wait) */
    ssize_t     (*pollwait_rt)(struct rtdm_dev_context *context,
                               wait_queue_primitive_t *sem);
    ssize_t     (*pollfree_rt)(struct rtdm_dev_context *context);
#endif /* CONFIG_RTNET_RTDM_SELECT */
};

struct rtdm_dev_context {
    unsigned long               context_flags;
    int                         fd;
    atomic_t                    close_lock_count;
    struct rtdm_operations      *ops;
    volatile struct rtdm_device *device;
    char                        dev_private[0];
};

struct rtdm_dev_reserved {
    struct list_head            entry;
    atomic_t                    refcount;
    struct rtdm_dev_context     *exclusive_context;
};

struct rtdm_device {
    int                         struct_version;

    int                         device_flags;
    size_t                      context_size;

    /* named device identification */
    char                        device_name[RTDM_MAX_DEVNAME_LEN+1];

    /* protocol device identification */
    int                         protocol_family;
    int                         socket_type;

    /* device instance creation */
    open_handler                open_rt;
    open_handler                open_nrt;

    socket_handler              socket_rt;
    socket_handler              socket_nrt;

    struct rtdm_operations      ops;

    int                         device_class;
    int                         device_sub_class;
    const char                  *driver_name;
    const char                  *peripheral_name;
    const char                  *provider_name;

    /* /proc entry */
    const char                  *proc_name;
    struct proc_dir_entry       *proc_entry;

    /* driver-definable id */
    int                         device_id;

    struct rtdm_dev_reserved    reserved;
};


#define RTDM_LOCK_CONTEXT(context)      atomic_inc(&(context)->close_lock_count)
#define RTDM_UNLOCK_CONTEXT(context)    atomic_dec(&(context)->close_lock_count)


extern int rtdm_dev_register(struct rtdm_device* device);
extern int rtdm_dev_unregister(struct rtdm_device* device);


#endif /* __RTDM_DRIVER_H */

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
