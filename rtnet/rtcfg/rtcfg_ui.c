/***
 *
 *  rtcfg/rtcfg_ui.h
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtcfg/rtcfg_event.h>
#include <rtcfg/rtcfg_frame.h>
#include <rtcfg/rtcfg_ui.h>


static spinlock_t  pending_event_list_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t  processed_event_list_lock = SPIN_LOCK_UNLOCKED;
static SEM         event_sem;
static RT_TASK     event_task;
static int         rtcfg_srq;

LIST_HEAD(pending_event_list);
LIST_HEAD(processed_event_list);


#define __wait_event_interruptible_timeout(wq, condition, timeout, ret)     \
do {                                                                        \
    signed long __timeout;                                                  \
    wait_queue_t __wait;                                                    \
    init_waitqueue_entry(&__wait, current);                                 \
                                                                            \
    __timeout = timeout;                                                    \
    add_wait_queue(&wq, &__wait);                                           \
    for (;;) {                                                              \
        set_current_state(TASK_INTERRUPTIBLE);                              \
        if (condition)                                                      \
            break;                                                          \
        if (!signal_pending(current)) {                                     \
            if ((__timeout = schedule_timeout(__timeout)) == 0) {           \
                ret = -ETIME;                                               \
                break;                                                      \
            }                                                               \
            continue;                                                       \
        }                                                                   \
        ret = -ERESTARTSYS;                                                 \
        break;                                                              \
    }                                                                       \
    current->state = TASK_RUNNING;                                          \
    remove_wait_queue(&wq, &__wait);                                        \
} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)            \
({                                                                          \
    int __ret = 0;                                                          \
    if (!(condition))                                                       \
        __wait_event_interruptible_timeout(wq, condition, timeout, __ret);  \
    __ret;                                                                  \
})



static int rtcfg_queue_user_event(struct rtcfg_user_event *event)
{
    unsigned long flags;
    int           ret;


    event->processed = 0;
    event->result    = 0;
    atomic_set(&event->ref_count, 2); /* event creator + state machine */
    init_waitqueue_head(&event->event_wq);

    flags = rt_spin_lock_irqsave(&pending_event_list_lock);
    list_add_tail(&event->list_entry, &pending_event_list);
    rt_spin_unlock_irqrestore(flags, &pending_event_list_lock);

    rt_sem_signal(&event_sem);

    if (event->timeout > 0)
        ret = wait_event_interruptible_timeout(event->event_wq,
            event->processed, (event->timeout * HZ) / 1000);
    else
        ret = wait_event_interruptible(event->event_wq, event->processed);
    if (ret == 0)
        ret = event->result;

    if (atomic_dec_and_test(&event->ref_count)) {
        if (event->buffer != NULL)
            kfree(event->buffer);
        kfree(event);
    }

    return ret;
}



static inline struct rtcfg_user_event *rtcfg_dequeue_user_event(void)
{
    unsigned long flags;
    struct rtcfg_user_event *event;


    flags = rt_spin_lock_irqsave(&pending_event_list_lock);
    event = (struct rtcfg_user_event *)pending_event_list.next;
    list_del(&event->list_entry);
    rt_spin_unlock_irqrestore(flags, &pending_event_list_lock);

    return event;
}



static inline void rtcfg_queue_processed_event(struct rtcfg_user_event *event)
{
    unsigned long flags;


    flags = rt_spin_lock_irqsave(&processed_event_list_lock);
    list_add_tail(&event->list_entry, &processed_event_list);
    rt_spin_unlock_irqrestore(flags, &processed_event_list_lock);

    rt_pend_linux_srq(rtcfg_srq);
}



static inline struct rtcfg_user_event *rtcfg_dequeue_processed_event(void)
{
    unsigned long flags;
    struct rtcfg_user_event *event;


    flags = rt_spin_lock_irqsave(&processed_event_list_lock);
    if (!list_empty(&processed_event_list)) {
        event = (struct rtcfg_user_event *)processed_event_list.next;
        list_del(&event->list_entry);
    } else
        event = NULL;
    rt_spin_unlock_irqrestore(flags, &processed_event_list_lock);

    return event;
}



static void rtcfg_event_handler(int arg)
{
    struct rtcfg_user_event *event;
    int                     ret;


    while (1) {
        rt_sem_wait(&event_sem);

        event = rtcfg_dequeue_user_event();

        ret = rtcfg_do_main_event(event->ifindex, event->event_id, event);
        if (ret != -EVENT_PENDING)
            rtcfg_complete_event(event, ret);
    }
}



static void rtcfg_srq_handler(void)
{
    struct rtcfg_user_event *event;


    while ((event = rtcfg_dequeue_processed_event()) != NULL) {
        event->processed = 1;
        wake_up(&event->event_wq);

        if (atomic_dec_and_test(&event->ref_count)) {
            if (event->buffer != NULL)
                kfree(event->buffer);
            kfree(event);
        }
    }
}



int rtcfg_cmd_server(int ifindex)
{
    struct rtcfg_user_event *cmd_server;


    cmd_server = kmalloc(sizeof(struct rtcfg_user_event), GFP_KERNEL);
    if (cmd_server == NULL)
        return -ENOMEM;

    cmd_server->event_id = RTCFG_CMD_SERVER;
    cmd_server->timeout  = 0;
    cmd_server->ifindex  = ifindex;
    cmd_server->buffer   = NULL;

    cmd_server->args.server.period = 1000;

    return rtcfg_queue_user_event(cmd_server);
}



int rtcfg_cmd_add_ip(int ifindex, u32 ip_addr)
{
    struct rtcfg_user_event *cmd_add_ip;


    cmd_add_ip = kmalloc(sizeof(struct rtcfg_user_event), GFP_KERNEL);
    if (cmd_add_ip == NULL)
        return -ENOMEM;

    cmd_add_ip->event_id = RTCFG_CMD_ADD_IP;
    cmd_add_ip->timeout  = 0;
    cmd_add_ip->ifindex  = ifindex;

    cmd_add_ip->buffer = kmalloc(sizeof(struct rtcfg_connection), GFP_KERNEL);
    if (cmd_add_ip->buffer == NULL) {
        kfree(cmd_add_ip);
        return -ENOMEM;
    }

    cmd_add_ip->args.add_ip.ip_addr = ip_addr;

    return rtcfg_queue_user_event(cmd_add_ip);
}



int rtcfg_cmd_wait(int ifindex, unsigned int timeout)
{
    struct rtcfg_user_event *cmd_wait;


    cmd_wait = kmalloc(sizeof(struct rtcfg_user_event), GFP_KERNEL);
    if (cmd_wait == NULL)
        return -ENOMEM;

    cmd_wait->event_id = RTCFG_CMD_WAIT;
    cmd_wait->timeout  = timeout;
    cmd_wait->ifindex  = ifindex;
    cmd_wait->buffer   = NULL;

    return rtcfg_queue_user_event(cmd_wait);
}



int rtcfg_cmd_client(int ifindex, unsigned int timeout)
{
    struct rtcfg_user_event *cmd_client;


    cmd_client = kmalloc(sizeof(struct rtcfg_user_event), GFP_KERNEL);
    if (cmd_client == NULL)
        return -ENOMEM;

    cmd_client->event_id = RTCFG_CMD_CLIENT;
    cmd_client->timeout  = timeout;
    cmd_client->ifindex  = ifindex;

    cmd_client->buffer = kmalloc(RTCFG_MAX_ADDRSIZE * 32, GFP_KERNEL);
    if (cmd_client->buffer == NULL) {
        kfree(cmd_client);
        return -ENOMEM;
    }

    cmd_client->args.client.max_clients = 32; /* yet hard-coded */

    return rtcfg_queue_user_event(cmd_client);
}



int rtcfg_cmd_announce(int ifindex, unsigned int timeout)
{
    struct rtcfg_user_event *cmd_announce;


    cmd_announce = kmalloc(sizeof(struct rtcfg_user_event), GFP_KERNEL);
    if (cmd_announce == NULL)
        return -ENOMEM;

    cmd_announce->event_id = RTCFG_CMD_ANNOUNCE;
    cmd_announce->timeout  = timeout;
    cmd_announce->ifindex  = ifindex;
    cmd_announce->buffer   = NULL;

    return rtcfg_queue_user_event(cmd_announce);
}



void rtcfg_complete_event(struct rtcfg_user_event *event, int result)
{
    event->result = result;
    rtcfg_queue_processed_event(event);
}



void rtcfg_complete_event_nrt(struct rtcfg_user_event *event, int result)
{
    event->processed = 1;
    wake_up(&event->event_wq);

    if (atomic_dec_and_test(&event->ref_count)) {
        if (event->buffer != NULL)
            kfree(event->buffer);
        kfree(event);
    }
}



int __init rtcfg_init_ui(void)
{
    int ret;


    rtcfg_srq = rt_request_srq(0, rtcfg_srq_handler, 0);
    if (rtcfg_srq < 0)
        return rtcfg_srq;

    rt_typed_sem_init(&event_sem, 0, CNT_SEM);

    ret = rt_task_init(&event_task, rtcfg_event_handler, 0, 4096,
                       RT_LOWEST_PRIORITY, 0, NULL);
    if (ret != 0)
        rt_free_srq(rtcfg_srq);
    else
        rt_task_resume(&event_task);

    return ret;
}



void rtcfg_cleanup_ui(void)
{
    rt_task_delete(&event_task);
    rt_sem_delete(&event_sem);
    rt_free_srq(rtcfg_srq);
}
