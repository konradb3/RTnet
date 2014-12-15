/***
 *
 *  rtmac_proc.c
 *
 *  rtmac - real-time networking medium access control subsystem
 *  Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
 *                2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/module.h>

#include <rtnet_internal.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/rtmac_vnic.h>
#include <rtmac/rtmac_proc.h>


#ifdef CONFIG_PROC_FS
struct proc_dir_entry *rtmac_proc_root;


static int  rtnet_rtmac_disc_show(struct seq_file *p, void *data)
{
  int (*handler)(struct seq_file *p, void *data) = data;

  return handler(p, NULL);
}

static int rtnet_rtmac_disc_open(struct inode *inode, struct file *file)
{
  return single_open(file, rtnet_rtmac_disc_show, NULL);
}

static const struct file_operations rtnet_rtmac_disc_fops = {
  .open = rtnet_rtmac_disc_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};



int rtmac_disc_proc_register(struct rtmac_disc *disc)
{
    int                     i;
    struct proc_dir_entry   *proc_entry;


    i = 0;
    while (disc->proc_entries[i].name != NULL) {
        proc_entry = proc_create_data(disc->proc_entries[i].name,
				      S_IFREG | S_IRUGO | S_IWUSR,
				      rtmac_proc_root,
				      &rtnet_rtmac_disc_fops,
				      disc->proc_entries[i].handler);
        if (!proc_entry) {
            while (--i > 0) {
                remove_proc_entry(disc->proc_entries[i].name, rtmac_proc_root);
                i--;
            }
            return -1;
        }
        i++;
    }

    return 0;
}



void rtmac_disc_proc_unregister(struct rtmac_disc *disc)
{
    int i;


    i = 0;
    while (disc->proc_entries[i].name != NULL) {
        remove_proc_entry(disc->proc_entries[i].name, rtmac_proc_root);
        i++;
    }
}

static int rtnet_rtmac_disciplines_open(struct inode *inode, struct file *file)
{
  return single_open(file, rtnet_rtmac_disciplines_show, NULL);
}

static const struct file_operations rtnet_rtmac_disciplines_fops = {
  .open = rtnet_rtmac_disciplines_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};



static int rtnet_rtmac_vnics_open(struct inode *inode, struct file *file)
{
  return single_open(file, rtnet_rtmac_vnics_show, NULL);
}

static const struct file_operations rtnet_rtmac_vnics_fops = {
  .open = rtnet_rtmac_vnics_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};



int rtmac_proc_register(void)
{
    struct proc_dir_entry *proc_entry;


    rtmac_proc_root = proc_mkdir("rtmac", rtnet_proc_root);
    if (!rtmac_proc_root)
        goto err1;

    proc_entry = proc_create("disciplines", S_IFREG | S_IRUGO | S_IWUSR,
			     rtmac_proc_root, &rtnet_rtmac_disciplines_fops);
    if (!proc_entry)
        goto err2;

    proc_entry = proc_create("vnics", S_IFREG | S_IRUGO | S_IWUSR,
			     rtmac_proc_root, &rtnet_rtmac_vnics_fops);
    if (!proc_entry)
        goto err3;

    return 0;

  err3:
    remove_proc_entry("disciplines", rtmac_proc_root);

  err2:
    remove_proc_entry("rtmac", rtnet_proc_root);

  err1:
    /*ERRMSG*/printk("RTmac: unable to initialize /proc entries\n");
    return -1;
}



void rtmac_proc_release(void)
{
    remove_proc_entry("vnics", rtmac_proc_root);
    remove_proc_entry("disciplines", rtmac_proc_root);
    remove_proc_entry("rtmac", rtnet_proc_root);
}

#endif /* CONFIG_PROC_FS */
