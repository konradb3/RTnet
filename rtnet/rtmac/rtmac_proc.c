/* rtmac_proc.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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
#include <linux/module.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <rtnet_sys.h>
#include <rtmac/rtmac_proc.h>


#ifdef CONFIG_PROC_FS
int rtmac_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    RTNET_PROC_PRINT_VARS;
    RTNET_PROC_PRINT("\nRTmac\n\n");
    RTNET_PROC_PRINT_DONE;
}

int rtmac_proc_register(void)
{
    static struct proc_dir_entry *rtmac_proc_entry;

    rtmac_proc_entry = create_proc_entry(RTMAC_PROC_NAME, S_IFREG | S_IRUGO | S_IWUSR, rtai_proc_root);

    if (!rtmac_proc_entry) {
        printk("RTmac: Unable to initialize: /proc/rtai/rtmac\n"); // FIXME: remove static path
        return -1;
    }

    rtmac_proc_entry->read_proc = rtmac_proc_read;

    return 0;
}

void rtmac_proc_release(void)
{
    remove_proc_entry(RTMAC_PROC_NAME, rtai_proc_root);
}



#endif // CONFIG_PROC_FS
