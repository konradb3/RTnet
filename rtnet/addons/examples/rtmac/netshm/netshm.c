/***
 *
 *  netshm.c
 *
 *  netshm - simple device providing a distributed pseudo shared memory
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/if_packet.h>
#include <asm/uaccess.h>

#include <rtdm_driver.h>
#include <rtnet_sys.h>
#include <rtnet.h>
#include <rtmac.h>

#include <netshm.h>


#define NETSHM_PROTOCOL         0x2004
#define DEFAULT_RECV_TASK_PRIO  10

#define MODE_DROPPING           0
#define MODE_DISABLED           1
#define MODE_ENABLED            2

#define GET_PRIV(context)       (struct netshm_priv *) \
    &(context)-> dev_private[16 - (sizeof(struct rtdm_dev_context) & 15)]

struct netshm_hdr {
    u16                     offset;
    u16                     length;
} __attribute__((packed));

struct netshm_priv {
    /* rtos_task_t must be aligned. We will move the private structure
       in rtdm_dev_context appropriately. */
    rtos_task_t             recv_task;
    rtos_event_sem_t        recv_sem;
    rtos_res_lock_t         mem_lock;
    volatile int            receiver_mode;
    int                     sock;
    struct rtdm_dev_context *sock_ctx;
    ssize_t                 (*sock_sendmsg)(struct rtdm_dev_context *context,
                                            int call_flags,
                                            const struct msghdr *msg,
                                            int flags);
    ssize_t                 (*sock_recvmsg)(struct rtdm_dev_context *context,
                                            int call_flags,
                                            struct msghdr *msg,
                                            int flags);
    int                     rtmac;
    struct rtdm_dev_context *rtmac_ctx;
    int                     (*rtmac_ioctl)(struct rtdm_dev_context *context,
                                           int call_flags, int request,
                                           void *arg);
    void*                   mem_start;
    size_t                  mem_size;
    size_t                  local_mem_offs;
    size_t                  local_mem_size;
    int                     call_flags;
    struct msghdr           msg_out;
    struct iovec            iov_out[2];
    struct netshm_hdr       hdr_out;
    char                    __align; /* create room for structure alignment */
};


static char *shm_name = "myNETSHM";
static int  shm_if    = 1;

MODULE_PARM(shm_name, "s");
MODULE_PARM(shm_if, "i");
MODULE_PARM_DESC(shm_name, "name of the shared memory");
MODULE_PARM_DESC(shm_if, "network interface to be used (1-n)");

MODULE_LICENSE("GPL");

static struct sockaddr_ll   broadcast_addr = {
    sll_family:     PF_PACKET,
    sll_protocol:   __constant_htons(NETSHM_PROTOCOL),
    sll_halen:      6,
    sll_addr:       { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};



void receive_callback(struct rtdm_dev_context *context, void *arg)
{
    struct netshm_priv  *priv = (struct netshm_priv *)arg;


    rtos_res_lock(&priv->mem_lock);

    if (priv->receiver_mode != MODE_DISABLED)
        rtos_event_sem_signal(&priv->recv_sem);

    rtos_res_unlock(&priv->mem_lock);
}



void receiver(int arg)
{
    struct netshm_priv  *priv = (struct netshm_priv *)arg;
    struct netshm_hdr   hdr;
    struct iovec        iov[2];
    struct msghdr       msg;
    int                 ret;


    msg.msg_name        = NULL;
    msg.msg_namelen     = 0;
    msg.msg_iov         = iov;
    msg.msg_control     = NULL;
    msg.msg_controllen  = 0;

    RTDM_LOCK_CONTEXT(priv->sock_ctx);
    while (1) {
        if (RTOS_EVENT_ERROR(rtos_event_sem_wait(&priv->recv_sem)))
            goto done; /* we are shutting down */

        rtos_res_lock(&priv->mem_lock);

        /* double-check receiver_mode to avoid races */
        if (priv->receiver_mode != MODE_DISABLED)
            while (1) {
                msg.msg_iovlen  = 1;

                iov[0].iov_base = &hdr;
                iov[0].iov_len  = sizeof(struct netshm_hdr);

                ret = priv->sock_recvmsg(priv->sock_ctx, 0, &msg, MSG_PEEK);
                if (ret < 0) {
                    if (ret != -EAGAIN)
                        goto done; /* error */
                    break;  /* no more messages - leave inner loop */
                }

                hdr.offset = ntohs(hdr.offset);
                hdr.length = ntohs(hdr.length);

                if ((hdr.offset + hdr.length > priv->mem_size) ||
                    (priv->receiver_mode == MODE_DROPPING)) {
                    iov[0].iov_len = 0;
                    if (priv->sock_recvmsg(priv->sock_ctx, priv->call_flags,
                                        &msg, 0) < 0)
                        goto done; /* error */
                } else {
                    msg.msg_iovlen  = 2;

                    /* write the header to the user buffer as well, will be
                    * overwritten with data afterwards. */
                    iov[0].iov_base = priv->mem_start + hdr.offset;
                    iov[0].iov_len  = sizeof(struct netshm_hdr);
                    iov[1].iov_base = iov[0].iov_base;
                    iov[1].iov_len  = hdr.length;

                    ret = priv->sock_recvmsg(priv->sock_ctx, priv->call_flags,
                                            &msg, 0);
                    if (ret < 0)
                        goto done; /* error */
                }
            }

        rtos_res_unlock(&priv->mem_lock);
    }

  done:
    rtos_res_unlock(&priv->mem_lock);
    RTDM_UNLOCK_CONTEXT(priv->sock_ctx);
}



int netshm_open(struct rtdm_dev_context *context, int call_flags, int oflags)
{
    struct netshm_priv          *priv = GET_PRIV(context);
    struct sockaddr_ll          local_addr;
    struct rtdm_getcontext_args getcontext;
    struct rtnet_callback       callback = {receive_callback, priv};
    int                         sock;
    int                         rtmac;
    char                        rtmac_name[] = "TDMA0";
    int                         ret;
    int                         nonblock = 1;


    sock = socket_rt(AF_PACKET, SOCK_DGRAM, htons(NETSHM_PROTOCOL));
    if (sock < 0)
        return sock;
    priv->sock = sock;

    priv->receiver_mode = MODE_DROPPING;

    local_addr.sll_family   = PF_PACKET;
    local_addr.sll_protocol = htons(NETSHM_PROTOCOL);
    local_addr.sll_ifindex  = shm_if;
    ret = bind_rt(sock, (struct sockaddr *)&local_addr,
                  sizeof(struct sockaddr_ll));
    if (ret < 0)
        goto err_sock;

    getcontext.struct_version = RTDM_CONTEXT_STRUCT_VER;
    ret = ioctl_rt(sock, RTIOC_GETCONTEXT, &getcontext);
    if (ret < 0)
        goto err_sock;
    priv->sock_ctx     = getcontext.context;
    priv->sock_sendmsg = getcontext.context->ops->sendmsg_rt;
    priv->sock_recvmsg = getcontext.context->ops->recvmsg_rt;

    ret = ioctl_rt(sock, RTNET_RTIOC_NONBLOCK, &nonblock);
    if (ret < 0)
        goto err_sock;

    ret = ioctl_rt(sock, RTNET_RTIOC_CALLBACK, &callback);
    if (ret < 0)
        goto err_sock;

    rtmac_name[4] = shm_if-1 + '0';
    rtmac = open_rt(rtmac_name, O_RDONLY);
    if (rtmac < 0) {
        ret = rtmac;
        goto err_rtmac1;
    }
    priv->rtmac = rtmac;

    getcontext.struct_version = RTDM_CONTEXT_STRUCT_VER;
    ret = ioctl_rt(rtmac, RTIOC_GETCONTEXT, &getcontext);
    if (ret < 0)
        goto err_rtmac2;
    priv->rtmac_ctx   = getcontext.context;
    priv->rtmac_ioctl = getcontext.context->ops->ioctl_rt;

    ret = rtos_task_init_suspended(&priv->recv_task, receiver, (int)priv,
                                   DEFAULT_RECV_TASK_PRIO);
    if (ret < 0)
        goto err_task;

    rtos_event_sem_init(&priv->recv_sem);
    rtos_res_lock_init(&priv->mem_lock);

    priv->msg_out.msg_name        = &broadcast_addr;
    priv->msg_out.msg_namelen     = sizeof(broadcast_addr);
    priv->msg_out.msg_iov         = NULL;
    priv->msg_out.msg_iovlen      = 2;
    priv->msg_out.msg_control     = NULL;
    priv->msg_out.msg_controllen  = 0;

    return 0;

  err_task:

  err_rtmac2:
    close_rt(rtmac);

  err_rtmac1:

  err_sock:
    close_rt(sock);
    return ret;
}



int netshm_close(struct rtdm_dev_context *context, int call_flags)
{
    struct netshm_priv  *priv = GET_PRIV(context);
    int                 ret;


    rtos_event_sem_delete(&priv->recv_sem);
    rtos_res_lock_delete(&priv->mem_lock);

    if (atomic_read(&context->close_lock_count) > 1)
        return -EAGAIN;

    if (priv->sock >= 0) {
        ret = close_rt(priv->sock);
        if (ret < 0)
            return ret;
        priv->sock = -1;
    }

    if (priv->rtmac >= 0) {
        ret = close_rt(priv->rtmac);
        if (ret < 0)
            return ret;
        priv->rtmac = -1;
    }

    rtos_task_delete(&priv->recv_task);
    return 0;
}



int netshm_ioctl_nrt(struct rtdm_dev_context *context, int call_flags,
                     int request, void *arg)
{
    struct netshm_priv          *priv = GET_PRIV(context);
    struct netshm_attach_args   args;
    struct netshm_attach_args   *args_ptr;
    int                         ret;


    switch (request) {
        case NETSHM_RTIOC_ATTACH:
            if (priv->msg_out.msg_iov) {
                ret = -EBUSY;
                break;
            }

            args_ptr = arg;

            if (call_flags & RTDM_USER_MODE_CALL) {
                ret = copy_from_user(&args, arg,
                                     sizeof(struct netshm_attach_args));
                if (ret < 0)
                    break;
                args_ptr = &args;
            }

            if (args_ptr->mem_size < args_ptr->local_mem_offs +
                                     args_ptr->local_mem_size) {
                ret = -EINVAL;
                break;
            }

            priv->mem_start      = args_ptr->mem_start;
            priv->mem_size       = args_ptr->mem_size;
            priv->local_mem_offs = args_ptr->local_mem_offs;
            priv->local_mem_size = args_ptr->local_mem_size;

            priv->hdr_out.offset = htons(priv->local_mem_offs);
            priv->hdr_out.length = htons(priv->local_mem_size);

            priv->call_flags = call_flags & RTDM_USER_MODE_CALL;

            priv->msg_out.msg_iov = priv->iov_out;

            if (args_ptr->recv_task_prio >= 0)
                rtos_task_set_priority(&priv->recv_task,
                                       args_ptr->recv_task_prio);
            rtos_task_resume(&priv->recv_task);

            if (args_ptr->xmit_prio >= 0)
                ioctl_rt(priv->sock, RTNET_RTIOC_PRIORITY,
                         &args_ptr->xmit_prio);

            ret = 0;
            break;

        default:
            ret = -ENOTTY;
            break;
    }

    return ret;
}



int netshm_ioctl_rt(struct rtdm_dev_context *context, int call_flags,
                    int request, void *arg)
{
    struct netshm_priv  *priv  = GET_PRIV(context);
    int                 waiton = RTMAC_WAIT_ON_DEFAULT;
    int                 ret;


    switch (request) {
        case NETSHM_RTIOC_CYCLE:
            if (!priv->msg_out.msg_iov)
                return -EACCES;

            if (priv->local_mem_size > 0) {
                priv->iov_out[0].iov_base = &priv->hdr_out;
                priv->iov_out[0].iov_len  = sizeof(struct netshm_hdr);
                priv->iov_out[1].iov_base = priv->mem_start +
                                            priv->local_mem_offs;
                priv->iov_out[1].iov_len  = priv->local_mem_size;

                RTDM_LOCK_CONTEXT(priv->sock_ctx);
                ret = priv->sock_sendmsg(priv->sock_ctx, call_flags,
                                         &priv->msg_out, 0);
                RTDM_UNLOCK_CONTEXT(priv->sock_ctx);
                if (ret < 0)
                    return ret;
            }

            /* wait on completion of processing cycle (first cycle) */
            RTDM_LOCK_CONTEXT(priv->rtmac_ctx);
            ret = priv->rtmac_ioctl(priv->rtmac_ctx, call_flags,
                                    RTMAC_RTIOC_WAITONCYCLE, &waiton);
            if (ret < 0) {
                RTDM_UNLOCK_CONTEXT(priv->rtmac_ctx);
                return ret;
            }

            /* now accept update packets */
            priv->receiver_mode = MODE_ENABLED;

            /* explicitely wake up receiver to process pending messages */
            rtos_event_sem_signal(&priv->recv_sem);

            /* wait on completion of communication cycle (second cycle) */
            ret = priv->rtmac_ioctl(priv->rtmac_ctx, call_flags,
                                    RTMAC_RTIOC_WAITONCYCLE, &waiton);
            RTDM_UNLOCK_CONTEXT(priv->rtmac_ctx);

            rtos_res_lock(&priv->mem_lock);
            priv->receiver_mode = MODE_DISABLED;
            rtos_res_unlock(&priv->mem_lock);

            break;

        default:
            ret = netshm_ioctl_nrt(context, call_flags, request, arg);
            break;
    }

    return ret;
}



static struct rtdm_device netshm_dev = {
    struct_version:     RTDM_DEVICE_STRUCT_VER,

    /* RTDM_EXCLUSIVE: only one user at the same time */
    device_flags:       RTDM_NAMED_DEVICE | RTDM_EXCLUSIVE,
    context_size:       sizeof(struct netshm_priv),

    /* Note: open_rt() and close_rt() of this device can run in any context */
    open_rt:            netshm_open,
    open_nrt:           netshm_open,

    ops: {
        close_rt:       netshm_close,
        close_nrt:      netshm_close,

        /* Note: Instead of using two different entry functions for ioctl,
           we could also check the caller's context (=> context_flags). But
           this is a demo... */
        ioctl_rt:       netshm_ioctl_rt,
        ioctl_nrt:      netshm_ioctl_nrt,
    },

    device_class:       RTDM_CLASS_EXPERIMENTAL,
    device_sub_class:   100,
    driver_name:        "netshm",
    peripheral_name:    "Simple shared memory over RTnet",
    provider_name:      "(C) 2004 RTnet Development Team, "
                        "http://rtnet.sf.net",

    proc_name:          netshm_dev.device_name
};



int __init init_module(void)
{
    printk("netshm: loading\n");
    broadcast_addr.sll_ifindex = shm_if;

    strncpy(netshm_dev.device_name, shm_name, MAX_DEV_NAME_LENGTH);
    netshm_dev.device_name[MAX_DEV_NAME_LENGTH] = 0;

    return rtdm_dev_register(&netshm_dev);
}



void cleanup_module(void)
{
    printk("netshm: unloading\n");
    while (rtdm_dev_unregister(&netshm_dev) == -EAGAIN) {
        printk("netshm: waiting for remaining open devices\n");
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* sleep 1 second */
    }
}
