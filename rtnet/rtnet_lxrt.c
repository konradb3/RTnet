/* lxrtnet.c
 *
 * lxrtnet - real-time networking in usermode
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>

#include <asm/io.h>
#include <asm/rtai.h>

#include <rtai_sched.h>
#include <rt_mem_mgr.h>
#include <rtai_lxrt.h>

#include <rtnet.h>
#include <rtnet_lxrt.h>

MODULE_LICENSE("GPL");

static struct rt_fun_entry rt_lxrtnet_fun[] = {
    [RT_SOCKET]             = {0,                   rt_socket            },
    [RT_SOCKET_CLOSE]       = {0,                   rt_socket_close      },
    [RT_SOCKET_BIND]        = {UR1(2,3),            rt_socket_bind       },
    [RT_SOCKET_CONNECT]     = {UR1(2,3),            rt_socket_connect    },
    [RT_SOCKET_ACCEPT]      = {UR1(2,3),            rt_socket_accept     },
    [RT_SOCKET_LISTEN]      = {0,                   rt_socket_listen     },
    [RT_SOCKET_SEND]        = {UR1(2,3),            rt_socket_send       },
    [RT_SOCKET_RECV]        = {UW1(2,3),            rt_socket_recv       },
    [RT_SOCKET_SENDTO]      = {UR1(2,3) | UR2(5,6), rt_socket_sendto     },
    [RT_SOCKET_RECVFROM]    = {UW1(2,3) | UW2(5,6), rt_socket_recvfrom   },
    [RT_SOCKET_SENDMSG]     = {UW1(2,3),            rt_socket_sendmsg    },
    [RT_SOCKET_RECVMSG]     = {UR1(2,3),            rt_socket_recvmsg    },
    [RT_SOCKET_WRITE]       = {UW1(2,3),            rt_socket_send       },
    [RT_SOCKET_READ]        = {UR1(2,3),            rt_socket_recv       },
    [RT_SOCKET_WRITEV]      = {UW1(2,3),            rt_socket_sendmsg    },
    [RT_SOCKET_READV]       = {UR1(2,3),            rt_socket_recvmsg    },
    [RT_SOCKET_GETSOCKNAME] = {UW1(2,3),            rt_socket_getsockname},
    [RT_SOCKET_SETSOCKOPT]  = {UR1(4,5),            rt_socket_setsockopt }
};


/***
 *  lxrtnet_init
 */
static int __init lxrtnet_init(void)
{
    if (set_rt_fun_ext_index(rt_lxrtnet_fun, LxRTNET_IDX)) {
        printk("Recompile your module with a different index\n");
        return -EACCES;
    }

    return(0);
}

/***
 *  lxrtnet_cleanup
 */
static void __exit lxrtnet_cleanup(void)
{
    reset_rt_fun_ext_index(rt_lxrtnet_fun, LxRTNET_IDX);
}

module_init(lxrtnet_init);
module_exit(lxrtnet_cleanup);
