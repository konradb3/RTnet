/* rtnet_lxrt.c
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
#include <rtai_lxrt.h>

#include <rtnet.h>
#include <rtnet_lxrt.h>

MODULE_LICENSE("GPL");

/* Note that we don't use LXRT's copy to/from user mechanism as it will not
 * work with the socket API (and it is slow!). Instead we demand NewLXRT which
 * takes care that the real-time kernel task can always access the user mode
 * buffer passed to it.
 * TODO: Write our own interface (probably RTDM-based) which will also have to
 * deal with invalid pointers passed by the user.
 */
static struct rt_fun_entry rt_lxrtnet_fun[] = {
    [RT_SOCKET]             = {0, rt_socket            },
    [RT_SOCKET_CLOSE]       = {0, rt_socket_close      },
    [RT_SOCKET_BIND]        = {0, rt_socket_bind       },
    [RT_SOCKET_CONNECT]     = {1, rt_socket_connect    },
    [RT_SOCKET_ACCEPT]      = {1, rt_socket_accept     },
    [RT_SOCKET_LISTEN]      = {1, rt_socket_listen     },
    [RT_SOCKET_SEND]        = {1, rt_socket_send       },
    [RT_SOCKET_RECV]        = {1, rt_socket_recv       },
    [RT_SOCKET_SENDTO]      = {1, rt_socket_sendto     },
    [RT_SOCKET_RECVFROM]    = {1, rt_socket_recvfrom   },
    [RT_SOCKET_SENDMSG]     = {1, rt_socket_sendmsg    },
    [RT_SOCKET_RECVMSG]     = {1, rt_socket_recvmsg    },
/*    [RT_SOCKET_WRITE]       = {1, rt_socket_send       },
    [RT_SOCKET_READ]        = {1, rt_socket_recv       },
    [RT_SOCKET_WRITEV]      = {1, rt_socket_sendmsg    },
    [RT_SOCKET_READV]       = {1, rt_socket_recvmsg    },
    [RT_SOCKET_GETSOCKNAME] = {0, rt_socket_getsockname},*/
    [RT_SOCKET_SETSOCKOPT]  = {0, rt_socket_setsockopt },
    [RT_SOCKET_IOCTL]       = {0, rt_socket_ioctl      }
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
