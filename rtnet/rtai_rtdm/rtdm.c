/***************************************************************************
        rtdm.c  - core driver layer module (RTAI)

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

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <rtnet_config.h>
#include <rtai.h>
#ifdef CONFIG_RTAI_24
# define INTERFACE_TO_LINUX
#endif
#include <rtai_sched.h>
#include <rtai_proc_fs.h>
#include <rtai_lxrt.h>

#include <rtdm.h>
#include <rtdm_driver.h>


MODULE_LICENSE("GPL and additional rights");

#ifdef CONFIG_RTAI_24
# define RT_SCHED_LINUX_PRIORITY    RT_LINUX_PRIORITY
#else
# include <rtai_malloc.h>
#endif

#define RTDM_DEBUG
#ifdef RTDM_DEBUG
# define DEBUG_PK(txt, args...)  rt_printk(txt, ##args); /* general debug messages */
# define ERR_PK(txt, args...)    rt_printk(txt, ##args); /* error messages */
#else
# define DEBUG_PK(txt, args...)
# define ERR_PK(txt, args...)
#endif

#define SET_DEFAULT_OP(device, operation)                   \
    do {                                                    \
        (device).operation##_rt  = (void *)rtdm_nosys;      \
        (device).operation##_nrt = (void *)rtdm_nosys;      \
    } while (0)

#define SET_DEFAULT_OP_IF_NULL(device, operation)           \
    do {                                                    \
        if (!(device).operation##_rt)                       \
            (device).operation##_rt = (void *)rtdm_nosys;   \
        if (!(device).operation##_nrt)                      \
            (device).operation##_nrt = (void *)rtdm_nosys;  \
    } while (0)

#define NO_HANDLER(device, operation)                               \
    ((!(device).operation##_rt) && (!(device).operation##_nrt))

#define DEF_FILDES_COUNT        256 /* default number of file descriptors */
#define FILDES_INDEX_BITS       16  /* bits used to encode the table index */
#define MAX_FILDES              (1 << FILDES_INDEX_BITS)

#define DEF_NAME_HASH_TBL_SZ    256 /* default entries in name hash table */
#define DEF_PROTO_HASH_TBL_SZ   256 /* default entries in protocol hash tbl. */


struct rtdm_fildes {
    struct rtdm_fildes      *next;
    struct rtdm_dev_context *context;
    long                    instance_id;
    long                    __padding;
};

/* global variables */
static int fildes_count = DEF_FILDES_COUNT;
MODULE_PARM(fildes_count, "i");
MODULE_PARM_DESC(fildes_count, "Maximum number of file descriptors");

static int name_hash_table_size = DEF_NAME_HASH_TBL_SZ;
static int protocol_hash_table_size = DEF_PROTO_HASH_TBL_SZ;
MODULE_PARM(name_hash_table_size, "i");
MODULE_PARM(protocol_hash_table_size, "i");
MODULE_PARM_DESC(name_hash_table_size,
   "Size of hash table for named devices (must be power of 2)");
MODULE_PARM_DESC(protocol_hash_table_size,
   "Size of hash table for protocol devices (must be power of 2)");

static struct list_head     *rtdm_named_devices;    /* hash table */
static struct list_head     *rtdm_protocol_devices; /* hash table */
static spinlock_t           rt_dev_lock  = SPIN_LOCK_UNLOCKED;
static rwlock_t             nrt_dev_lock = RW_LOCK_UNLOCKED;

static struct rtdm_fildes   *fildes_table;  /* allocated on init */
static struct rtdm_fildes   *free_fildes;   /* chain of free descriptors */
static spinlock_t           rt_fildes_lock = SPIN_LOCK_UNLOCKED;
static int                  open_fildes;    /* used file descriptors */
static int                  name_hash_key_mask;
static int                  proto_hash_key_mask;

static RT_TASK              *rt_base_linux_task; /* for LXRT registering */

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *rtdm_proc_root; /* /proc/rtai/rtdm */
#endif



static inline int in_nrt_context(void)
{
    return (rt_whoami()->priority == RT_SCHED_LINUX_PRIORITY);
}



static int get_name_hash(const char* str)
{
    int key   = 0;
    int limit = MAX_DEV_NAME_LENGTH;

    while (*str != 0) {
        key += *str++;
        if (--limit == 0)
            break;
    }

    return key & name_hash_key_mask;
}



static int get_proto_hash(int protocol_family, int socket_type)
{
    return protocol_family & proto_hash_key_mask;
}



/***************************************************************************
    @brief Increments device reference counter.
 ***************************************************************************/
static inline void rtdm_reference_device(struct rtdm_device *device)
{
    atomic_inc(&device->reserved.refcount);
}



/***************************************************************************
    @brief Decrements device reference counter.
 ***************************************************************************/
static inline void rtdm_dereference_device(struct rtdm_device *device)
{
    atomic_dec(&device->reserved.refcount);
}



/***************************************************************************
    @brief Looks up a named devices.

    Lookup will be executed under real-time spin lock, thus with disabled
    interrupts. If a device is found, this function will increment the
    device's reference counter.

    @return pointer to device or NULL
 ***************************************************************************/
static struct rtdm_device *get_named_device(const char *name)
{
    struct list_head    *entry;
    struct rtdm_device  *device;
    int                 hash_key;
    unsigned long       flags;


    hash_key = get_name_hash(name);

    flags = rt_spin_lock_irqsave(&rt_dev_lock);

    list_for_each(entry, &rtdm_named_devices[hash_key]) {
        device = list_entry(entry, struct rtdm_device, reserved.entry);

        if (strcmp(device->device_name, device->device_name) == 0) {
            rtdm_reference_device(device);

            rt_spin_unlock_irqrestore(flags, &rt_dev_lock);

            return device;
        }
    }

    rt_spin_unlock_irqrestore(flags, &rt_dev_lock);

    return NULL;
}



/***************************************************************************
    @brief Looks up a protocol devices.

    Lookup will be executed under real-time spin lock, thus with disabled
    interrupts. If a device is found, this function will increment the
    device's reference counter.

    @return pointer to device or NULL
 ***************************************************************************/
static struct rtdm_device *get_protocol_device(int protocol_family,
                                               int socket_type)
{
    struct list_head    *entry;
    struct rtdm_device  *device;
    int                 hash_key;
    unsigned long       flags;


    hash_key = get_proto_hash(protocol_family, socket_type);

    flags = rt_spin_lock_irqsave(&rt_dev_lock);

    list_for_each(entry, &rtdm_protocol_devices[hash_key]) {
        device = list_entry(entry, struct rtdm_device, reserved.entry);

        if ((device->protocol_family == protocol_family) &&
            (device->socket_type == socket_type)) {
            rtdm_reference_device(device);

            rt_spin_unlock_irqrestore(flags, &rt_dev_lock);

            return device;
        }
    }

    rt_spin_unlock_irqrestore(flags, &rt_dev_lock);

    return NULL;
}



static inline int get_fd(struct rtdm_fildes *fildes)
{
    return (fildes - fildes_table) |
        (fildes->instance_id << FILDES_INDEX_BITS);
}



static struct rtdm_dev_context *get_context(int fd)
{
    struct rtdm_fildes      *fildes;
    struct rtdm_dev_context *context;
    int                     index;
    unsigned long           flags;


    index = fd & (MAX_FILDES - 1);
    if (index >= fildes_count)
        return NULL;

    fildes = &fildes_table[index];

    flags = rt_spin_lock_irqsave(&rt_fildes_lock);

    context = fildes->context;
    if (((fd >> FILDES_INDEX_BITS) != fildes->instance_id) || !context) {
        rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);
        return NULL;
    }

    atomic_inc(&context->close_lock_count);

    rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

    return context;
}



static inline void release_context(struct rtdm_dev_context *context)
{
    atomic_dec(&context->close_lock_count);
}



static int create_instance(struct rtdm_device *device,
                           struct rtdm_dev_context **context_ptr,
                           struct rtdm_fildes **fildes_ptr, int nrt_mem)
{
    struct rtdm_fildes      *fildes;
    struct rtdm_dev_context *context;
    unsigned long   flags;


    flags = rt_spin_lock_irqsave(&rt_fildes_lock);

    *fildes_ptr = fildes = free_fildes;
    if (!fildes) {
        rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

        *context_ptr = NULL;
        return -ENFILE;
    }
    free_fildes = fildes->next;
    open_fildes++;

    rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

    *context_ptr = context = device->reserved.exclusive_context;
    if (context) {
        flags = rt_spin_lock_irqsave(&rt_dev_lock);

        if (context->device) {
            rt_spin_unlock_irqrestore(flags, &rt_dev_lock);
            return -EBUSY;
        }
        context->device = device;

        rt_spin_unlock_irqrestore(flags, &rt_dev_lock);
    } else {
        if (nrt_mem)
            context = kmalloc(sizeof(struct rtdm_dev_context) +
                              device->context_size, GFP_KERNEL);
        else
            context = rt_malloc(sizeof(struct rtdm_dev_context) +
                                device->context_size);
        *context_ptr = context;
        if (!context)
            return -ENOMEM;

        context->device = device;
    }

    context->fd  = get_fd(fildes);
    context->ops = &device->ops;
    atomic_set(&context->close_lock_count, 0);

    return 0;
}



static void cleanup_instance(struct rtdm_device *device,
                             struct rtdm_dev_context *context,
                             struct rtdm_fildes *fildes, int nrt_mem)
{
    unsigned long   flags;


    if (fildes) {
        flags = rt_spin_lock_irqsave(&rt_fildes_lock);

        fildes->next = free_fildes;
        free_fildes = fildes;
        open_fildes--;

        fildes->context = NULL;

        /* assuming that sizeof(int) >= 32 bits */
        fildes->instance_id = (fildes->instance_id + 1) &
            ((1 << (31 - FILDES_INDEX_BITS)) - 1);

        rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);
    }

    if (context) {
        if (device->reserved.exclusive_context) {
            /* can be optimised on architectures which can atomically write
               pointers to memory */
            flags = rt_spin_lock_irqsave(&rt_dev_lock);
            context->device = NULL;
            rt_spin_unlock_irqrestore(flags, &rt_dev_lock);
        } else {
            if (nrt_mem)
                kfree(context);
            else
                rt_free(context);
        }
    }

    rtdm_dereference_device(device);
}



int rtdm_open(int call_flags, const char *path, int oflag)
{
    struct rtdm_device      *device;
    struct rtdm_fildes      *fildes;
    struct rtdm_dev_context *context;
    int                     ret;
    int                     nrt_mode = in_nrt_context();
    unsigned long           flags;


    device = get_named_device(path);
    if (!device)
        return -ENODEV;

    ret = create_instance(device, &context, &fildes, nrt_mode);
    if (ret < 0)
        goto err;

    if (nrt_mode) {
        context->context_flags = (1 << RTDM_CREATED_IN_NRT);
        ret = device->open_nrt(context, call_flags | RTDM_NRT_CALL, oflag);
    } else {
        context->context_flags = 0;
        ret = device->open_rt(context, call_flags, oflag);
    }

    if (ret < 0)
        goto err;

    /* can be optimised on architectures which can atomically write pointers
       to memory */
    flags = rt_spin_lock_irqsave(&rt_fildes_lock);
    fildes->context = context;
    rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

    return context->fd;

  err:
    cleanup_instance(device, context, fildes, nrt_mode);
    return ret;
}



int rtdm_socket(int call_flags, int protocol_family, int socket_type,
                int protocol)
{
    struct rtdm_device      *device;
    struct rtdm_fildes      *fildes;
    struct rtdm_dev_context *context;
    int                     ret;
    int                     nrt_mode = in_nrt_context();
    unsigned long           flags;


    device = get_protocol_device(protocol_family, socket_type);
    if (!device)
        return -EAFNOSUPPORT;

    ret = create_instance(device, &context, &fildes, nrt_mode);
    if (ret < 0)
        goto err;

    if (nrt_mode) {
        context->context_flags = (1 << RTDM_CREATED_IN_NRT);
        ret = device->socket_nrt(context, call_flags| RTDM_NRT_CALL,
                                 protocol);
    } else {
        context->context_flags = 0;
        ret = device->socket_rt(context, call_flags, protocol);
    }

    if (ret < 0)
        goto err;

    /* can be optimised on architectures which can atomically write pointers
       to memory */
    flags = rt_spin_lock_irqsave(&rt_fildes_lock);
    fildes->context = context;
    rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

    return context->fd;

  err:
    cleanup_instance(device, context, fildes, nrt_mode);
    return ret;
}



int rtdm_close(int call_flags, int fd)
{
    struct rtdm_fildes      *fildes;
    struct rtdm_dev_context *context;
    int                     index;
    unsigned long           flags;
    int                     ret;
    int                     nrt_mode = in_nrt_context();


    index = fd & (MAX_FILDES - 1);
    if (index >= fildes_count)
        return -EBADF;

    fildes = &fildes_table[index];

    flags = rt_spin_lock_irqsave(&rt_fildes_lock);

    context = fildes->context;
    if (((fd >> FILDES_INDEX_BITS) != fildes->instance_id) || !context) {
        rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);
        return -EBADF;
    }

    set_bit(RTDM_CLOSING, &context->context_flags);
    atomic_inc(&context->close_lock_count);

    rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

    if (nrt_mode)
        ret = context->ops->close_nrt(context, call_flags | RTDM_NRT_CALL);
    else {
        /* workaround until context switch support is available */
        if (test_bit(RTDM_CREATED_IN_NRT, &context->context_flags)) {
            ERR_PK("RTDM: calling rt_close in real-time mode while creation "
                   "ran in non-real-time - this is not yet supported!\n");
            return -ENOTSUPP;
        }

        ret = context->ops->close_rt(context, call_flags);
    }

    if (ret < 0)
        goto err;

    flags = rt_spin_lock_irqsave(&rt_fildes_lock);

    if (atomic_read(&context->close_lock_count) > 1) {
        rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);
        ret = -EAGAIN;
        goto err;
    }
    fildes->context = NULL;

    rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

    cleanup_instance(context->device, context, fildes,
                     test_bit(RTDM_CREATED_IN_NRT, &context->context_flags));

    return ret;

  err:
    release_context(context);
    return ret;
}



#define MAJOR_FUNCTION_WRAPPER_TOP(operation, args...)                      \
    struct rtdm_dev_context *context;                                       \
    struct rtdm_operations  *ops;                                           \
    int                     ret;                                            \
    int                     nrt_mode = in_nrt_context();                    \
                                                                            \
                                                                            \
    context = get_context(fd);                                              \
    if (!context)                                                           \
        return -EBADF;                                                      \
                                                                            \
    ops = context->ops;                                                     \
                                                                            \
    if (nrt_mode)                                                           \
        ret = ops->operation##_nrt(context, call_flags | RTDM_NRT_CALL,     \
                                   args);                                   \
    else                                                                    \
        ret = ops->operation##_rt(context, call_flags, args)



#define MAJOR_FUNCTION_WRAPPER_BOTTOM()                                     \
    release_context(context);                                               \
    return ret



int rtdm_ioctl(int call_flags, int fd, int request, void* arg)
{
    struct rtdm_getcontext_args *context_args = arg;


    MAJOR_FUNCTION_WRAPPER_TOP(ioctl, request, arg);

    if (ret < 0) {
        switch (request) {
            case RTIOC_GETCONTEXT:
                if (context_args->struct_version != RTDM_CONTEXT_STRUCT_VER)
                    return -EINVAL;
                context_args->context = context;
                return 0;

            case RTIOC_RELEASECONTEXT:
                release_context(context);
                ret = 0;
                break;
        }
    }

    MAJOR_FUNCTION_WRAPPER_BOTTOM();
}



int rtdm_read(int call_flags, int fd, void *buf, size_t nbyte)
{
    MAJOR_FUNCTION_WRAPPER_TOP(read, buf, nbyte);
    MAJOR_FUNCTION_WRAPPER_BOTTOM();
}



int rtdm_write(int call_flags, int fd, const void *buf, size_t nbyte)
{
    MAJOR_FUNCTION_WRAPPER_TOP(write, buf, nbyte);
    MAJOR_FUNCTION_WRAPPER_BOTTOM();
}



int rtdm_recvmsg(int call_flags, int fd, struct msghdr *msg, int flags)
{
    MAJOR_FUNCTION_WRAPPER_TOP(recvmsg, msg, flags);
    MAJOR_FUNCTION_WRAPPER_BOTTOM();
}



int rtdm_sendmsg(int call_flags, int fd, const struct msghdr *msg, int flags)
{
    MAJOR_FUNCTION_WRAPPER_TOP(sendmsg, msg, flags);
    MAJOR_FUNCTION_WRAPPER_BOTTOM();
}



static int rtdm_open_lxrt(const char *path, int oflag)
{
    char    krnl_path[MAX_DEV_NAME_LENGTH + 1];
    int     ret;


    ret = strncpy_from_user(krnl_path, path, MAX_DEV_NAME_LENGTH);
    if (ret >= 0)
        ret = rtdm_open(RTDM_USER_MODE_CALL, krnl_path, oflag);
    return ret;
}



static int rtdm_socket_lxrt(int protocol_family, int socket_type, int protocol)
{
    return rtdm_socket(RTDM_USER_MODE_CALL, protocol_family, socket_type,
                       protocol);
}



static int rtdm_close_lxrt(int fd)
{
    return rtdm_close(RTDM_USER_MODE_CALL, fd);
}



static int rtdm_ioctl_lxrt(int fd, int request, void* arg)
{
    return rtdm_ioctl(RTDM_USER_MODE_CALL, fd, request, arg);
}



static int rtdm_read_lxrt(int fd, void *buf, size_t nbyte)
{
    return rtdm_read(RTDM_USER_MODE_CALL, fd, buf, nbyte);
}



static int rtdm_write_lxrt(int fd, const void *buf, size_t nbyte)
{
    return rtdm_write(RTDM_USER_MODE_CALL, fd, buf, nbyte);
}



static int rtdm_recvmsg_lxrt(int fd, struct msghdr *msg, int flags)
{
    struct msghdr   krnl_msg;
    int             ret;


    ret = copy_from_user(&krnl_msg, msg, sizeof(struct msghdr));
    if (ret >= 0)
        ret = rtdm_recvmsg(RTDM_USER_MODE_CALL, fd, msg, flags);
    return ret;
}



static int rtdm_sendmsg_lxrt(int fd, const struct msghdr *msg, int flags)
{
    struct msghdr   krnl_msg;
    int             ret;


    ret = copy_from_user(&krnl_msg, msg, sizeof(struct msghdr));
    if (ret >= 0)
        ret = rtdm_sendmsg(RTDM_USER_MODE_CALL, fd, msg, flags);
    return ret;
}



/* used for optional LXRT registration of user API */
static struct rt_fun_entry lxrt_fun_entry[] = {
    [_RTDM_OPEN]    = {0, rtdm_open_lxrt},
    [_RTDM_SOCKET]  = {0, rtdm_socket_lxrt},
    [_RTDM_CLOSE]   = {0, rtdm_close_lxrt},
    [_RTDM_IOCTL]   = {0, rtdm_ioctl_lxrt},
    [_RTDM_READ]    = {0, rtdm_read_lxrt},
    [_RTDM_WRITE]   = {0, rtdm_write_lxrt},
    [_RTDM_RECVMSG] = {0, rtdm_recvmsg_lxrt},
    [_RTDM_SENDMSG] = {0, rtdm_sendmsg_lxrt}
};



/***************************************************************************
    @brief Default handler for unsupported driver functions.

    @return -ENOSYS
 ***************************************************************************/
static int rtdm_nosys(void)
{
    return -ENOSYS;
}



#ifdef CONFIG_PROC_FS
static int proc_read_named_devs(char* page, char** start, off_t off, int count,
                                int* eof, void* data)
{
    int                 i;
    struct list_head    *entry;
    struct rtdm_device  *device;
    PROC_PRINT_VARS;


    PROC_PRINT("Hash\tName\t\t\t\t/proc\n");

    write_lock_bh(&nrt_dev_lock);

    for (i = 0; i < name_hash_table_size; i++)
        list_for_each(entry, &rtdm_named_devices[i]) {
            device = list_entry(entry, struct rtdm_device, reserved.entry);

            PROC_PRINT("%02X\t%-31s\t%s\n", i, device->device_name,
                       device->proc_name);
        }

    write_unlock_bh(&nrt_dev_lock);

    PROC_PRINT_DONE;
}



static int proc_read_proto_devs(char* page, char** start, off_t off, int count,
                                int* eof, void* data)
{
    int                 i;
    struct list_head    *entry;
    struct rtdm_device  *device;
    char                buf[32];
    PROC_PRINT_VARS;


    PROC_PRINT("Hash\tProtocolFamily:SocketType\t/proc\n");

    write_lock_bh(&nrt_dev_lock);

    for (i = 0; i < protocol_hash_table_size; i++)
        list_for_each(entry, &rtdm_protocol_devices[i]) {
            device = list_entry(entry, struct rtdm_device, reserved.entry);

            snprintf(buf, sizeof(buf), "%u:%u", device->protocol_family,
                     device->socket_type);
            PROC_PRINT("%02X\t%-31s\t%s\n", i, buf, device->proc_name);
        }

    write_unlock_bh(&nrt_dev_lock);

    PROC_PRINT_DONE;
}



static int proc_read_open_fildes(char* page, char** start, off_t off,
                                 int count, int* eof, void* data)
{
    int                     i;
    struct rtdm_fildes      fildes;
    int                     close_lock_count;
    struct rtdm_device      *device;
    unsigned long           flags;
    PROC_PRINT_VARS;


    PROC_PRINT("Index\tInstance  fd\t\tLocked\tDevice\n");

    write_lock_bh(&nrt_dev_lock);

    for (i = 0; i < fildes_count; i++) {
        flags = rt_spin_lock_irqsave(&rt_fildes_lock);

        memcpy(&fildes, &fildes_table[i], sizeof(fildes));

        if (!fildes.context) {
            rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);
            continue;
        }

        close_lock_count = atomic_read(&fildes.context->close_lock_count);
        device           = fildes.context->device;

        rt_spin_unlock_irqrestore(flags, &rt_fildes_lock);

        PROC_PRINT("%d\t%-10ld%-14ld%d\t%s\n", i, fildes.instance_id,
                   i + (fildes.instance_id << FILDES_INDEX_BITS),
                   close_lock_count,
                   (device->device_flags & RTDM_NAMED_DEVICE) ?
                   device->device_name : device->proc_name);
    }

    write_unlock_bh(&nrt_dev_lock);

    PROC_PRINT_DONE;
}



static int proc_read_fildes(char* page, char** start, off_t off,
                            int count, int* eof, void* data)
{
    PROC_PRINT_VARS;


    write_lock_bh(&nrt_dev_lock);

    PROC_PRINT("total:\t%d\nopen:\t%d\nfree:\t%d\n", fildes_count, open_fildes,
               fildes_count - open_fildes);

    write_unlock_bh(&nrt_dev_lock);

    PROC_PRINT_DONE;
}



static int proc_read_dev_info(char* page, char** start, off_t off,
                              int count, int* eof, void* data)
{
    struct rtdm_device  *device = data;
    PROC_PRINT_VARS;


    write_lock_bh(&nrt_dev_lock);

    PROC_PRINT("driver:\t\t%s\nperipheral:\t%s\nprovider:\t%s\n",
               device->driver_name, device->peripheral_name,
               device->provider_name);
    PROC_PRINT("class:\t\t%d\nsub-class:\t%d\n",
               device->device_class, device->device_sub_class);
    PROC_PRINT("flags:\t\t%s%s%s\n",
               (device->device_flags & RTDM_EXCLUSIVE) ? "EXCLUSIVE  " : "",
               (device->device_flags & RTDM_NAMED_DEVICE) ?
               "NAMED_DEVICE  " : "",
               (device->device_flags & RTDM_PROTOCOL_DEVICE) ?
               "PROTOCOL_DEVICE  " : "");
    PROC_PRINT("lock count:\t%d\n", atomic_read(&device->reserved.refcount));

    write_unlock_bh(&nrt_dev_lock);

    PROC_PRINT_DONE;
}



static int rtdm_register_dev_proc(struct rtdm_device* device)
{
    struct proc_dir_entry   *dev_dir;
    struct proc_dir_entry   *proc_entry;


    dev_dir = create_proc_entry(device->proc_name, S_IFDIR, rtdm_proc_root);
    if (!dev_dir)
        return -EAGAIN;

    proc_entry = create_proc_entry("information", S_IFREG | S_IRUGO,
                                   dev_dir);
    if (!proc_entry) {
        remove_proc_entry(device->proc_name, rtdm_proc_root);
        return -EAGAIN;
    }
    proc_entry->data      = device;
    proc_entry->read_proc = proc_read_dev_info;

    device->proc_entry = dev_dir;

    return 0;
}
#endif /* CONFIG_PROC_FS */



/***************************************************************************
    Register a new driver.
    -> return = 0 | EINVAL | ENOMEM | EEXIST
 ***************************************************************************/
int rtdm_dev_register(struct rtdm_device* device)
{
    int                 hash_key;
    unsigned long       flags;
    struct list_head    *entry;
    struct rtdm_device  *existing_dev;
    int                 ret;


    /* Sanity check: structure version */
    if (device->struct_version != RTDM_DEVICE_STRUCT_VER) {
        ERR_PK("RTDM: unsupported device structure version\n");
        return -EINVAL;
    }

    switch (device->device_flags & RTDM_DEVICE_TYPE) {
        case RTDM_NAMED_DEVICE:
            /* Sanity check: any open handler? */
            if (NO_HANDLER(*device, open)) {
                ERR_PK("RTDM: no open handler for any context\n");
                return -EINVAL;
            }
            SET_DEFAULT_OP_IF_NULL(*device, open);
            SET_DEFAULT_OP(*device, socket);
            break;

        case RTDM_PROTOCOL_DEVICE:
            /* Sanity check: any socket handler? */
            if (NO_HANDLER(*device, socket)) {
                ERR_PK("RTDM: no socket handler for any context\n");
                return -EINVAL;
            }
            SET_DEFAULT_OP_IF_NULL(*device, socket);
            SET_DEFAULT_OP(*device, open);
            break;

        default:
            ERR_PK("RTDM: unknown device type\n");
            return -EINVAL;
    }

    /* Sanity check: any close handler? */
    if (NO_HANDLER(device->ops, close)) {
        ERR_PK("RTDM: no close handler for any context\n");
        return -EINVAL;
    }

    SET_DEFAULT_OP_IF_NULL(device->ops, ioctl);
    SET_DEFAULT_OP_IF_NULL(device->ops, close);
    SET_DEFAULT_OP_IF_NULL(device->ops, read);
    SET_DEFAULT_OP_IF_NULL(device->ops, write);
    SET_DEFAULT_OP_IF_NULL(device->ops, recvmsg);
    SET_DEFAULT_OP_IF_NULL(device->ops, sendmsg);

    atomic_set(&device->reserved.refcount, 0);
    device->reserved.exclusive_context = NULL;

    if (device->device_flags & RTDM_EXCLUSIVE) {
        device->reserved.exclusive_context =
            kmalloc(sizeof(struct rtdm_dev_context) + device->context_size,
                    GFP_KERNEL);
        if (!device->reserved.exclusive_context)
            return -ENOMEM;
        device->reserved.exclusive_context->device = NULL; /* mark as unused */
    }

    write_lock_bh(&nrt_dev_lock);

    if ((device->device_flags & RTDM_DEVICE_TYPE) == RTDM_NAMED_DEVICE) {
        hash_key = get_name_hash(device->device_name);

        list_for_each(entry, &rtdm_named_devices[hash_key]) {
            existing_dev =
                list_entry(entry, struct rtdm_device, reserved.entry);
            if (strcmp(device->device_name, existing_dev->device_name) == 0) {
                ret = -EEXIST;
                goto err;
            }
        }

#ifdef CONFIG_PROC_FS
        if ((ret = rtdm_register_dev_proc(device)) < 0)
            goto err;
#endif /* CONFIG_PROC_FS */

        flags = rt_spin_lock_irqsave(&rt_dev_lock);
        list_add_tail(&device->reserved.entry, &rtdm_named_devices[hash_key]);
        rt_spin_unlock_irqrestore(flags, &rt_dev_lock);

        write_unlock_bh(&nrt_dev_lock);

        DEBUG_PK("RTDM: registered named device %s\n", device->device_name);
    } else {
        hash_key = get_proto_hash(device->protocol_family,
                                  device->socket_type);

        list_for_each(entry, &rtdm_protocol_devices[hash_key]) {
            existing_dev =
                list_entry(entry, struct rtdm_device, reserved.entry);
            if ((device->protocol_family == existing_dev->protocol_family) &&
                (device->socket_type == existing_dev->socket_type)) {
                ret = -EEXIST;
                goto err;
            }
        }

#ifdef CONFIG_PROC_FS
        if ((ret = rtdm_register_dev_proc(device)) < 0)
            goto err;
#endif /* CONFIG_PROC_FS */

        flags = rt_spin_lock_irqsave(&rt_dev_lock);
        list_add_tail(&device->reserved.entry,
                      &rtdm_protocol_devices[hash_key]);
        rt_spin_unlock_irqrestore(flags, &rt_dev_lock);

        write_unlock_bh(&nrt_dev_lock);

        DEBUG_PK("RTDM: registered protocol device %d:%d\n",
                 device->protocol_family, device->socket_type);
    }
    return 0;

  err:
    write_unlock_bh(&nrt_dev_lock);
    if (device->reserved.exclusive_context)
        kfree(device->reserved.exclusive_context);
    return ret;
}



/***************************************************************************
    Unregister a driver.
    -> return = 0 | ENODEV | EOPENDEV
 ***************************************************************************/
int rtdm_dev_unregister(struct rtdm_device* device)
{
    unsigned long       flags;
    struct rtdm_device  *reg_dev;


    if ((device->device_flags & RTDM_DEVICE_TYPE) == RTDM_NAMED_DEVICE)
        reg_dev = get_named_device(device->device_name);
    else
        reg_dev = get_protocol_device(device->protocol_family,
                                      device->socket_type);
    if (!reg_dev)
        return -ENODEV;

    write_lock_bh(&nrt_dev_lock);

#ifdef CONFIG_PROC_FS
    remove_proc_entry("information", device->proc_entry);
    remove_proc_entry(device->proc_name, rtdm_proc_root);
#endif /* CONFIG_PROC_FS */

    flags = rt_spin_lock_irqsave(&rt_dev_lock);

    if (atomic_read(&reg_dev->reserved.refcount) > 1) {
        rt_spin_unlock_irqrestore(flags, &rt_dev_lock);
        write_unlock_bh(&nrt_dev_lock);

        rtdm_dereference_device(reg_dev);
        return -EAGAIN;
    }

    list_del(&reg_dev->reserved.entry);

    rt_spin_unlock_irqrestore(flags, &rt_dev_lock);
    write_unlock_bh(&nrt_dev_lock);

    return 0;
}



/***************************************************************************
    Module initialisation.
    -> return = 0 | ENOMEM | EPROC | ELXRT
 ***************************************************************************/
int init_module(void)
{
    int                     ret = 0;
    int                     i;
    RT_TASK                 *rt_linux_task[NR_RT_CPUS];
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry   *proc_entry;
#endif


    printk("RTDM Version 0.5.0\n");

    if (fildes_count > MAX_FILDES) {
        printk("RTDM: fildes_count exceeds %d\n", MAX_FILDES);
        return -EINVAL;
    }

    name_hash_key_mask  = name_hash_table_size - 1;
    proto_hash_key_mask = protocol_hash_table_size - 1;
    if (((name_hash_table_size & name_hash_key_mask) != 0) ||
        ((protocol_hash_table_size & proto_hash_key_mask) != 0))
        return -EINVAL;

    /* Initialise descriptor table */
    fildes_table = (struct rtdm_fildes *)
        kmalloc(fildes_count*sizeof(struct rtdm_fildes), GFP_KERNEL);
    if (!fildes_table)
        return -ENOMEM;
    memset(fildes_table, 0, fildes_count*sizeof(struct rtdm_fildes));
    for (i = 0; i < fildes_count-1; i++)
        fildes_table[i].next = &fildes_table[i+1];
    free_fildes = &fildes_table[0];

    /* Initialise hash tables */
    rtdm_named_devices = (struct list_head *)
        kmalloc(name_hash_table_size*sizeof(struct list_head), GFP_KERNEL);
    if (!rtdm_named_devices) {
        ret = -ENOMEM;
        goto rem_fildes;
    }
    for (i = 0; i < name_hash_table_size; i++)
        INIT_LIST_HEAD(&rtdm_named_devices[i]);

    rtdm_protocol_devices = (struct list_head *)
        kmalloc(protocol_hash_table_size*sizeof(struct list_head), GFP_KERNEL);
    if (!rtdm_protocol_devices) {
        ret = -ENOMEM;
        goto rem_named_devs;
    }
    for (i = 0; i < protocol_hash_table_size; i++)
        INIT_LIST_HEAD(&rtdm_protocol_devices[i]);

#ifdef CONFIG_PROC_FS
    /* Initialise /proc entries */
    rtdm_proc_root = create_proc_entry("rtdm", S_IFDIR, rtai_proc_root);
    if (!rtdm_proc_root)
        goto proc_err;

    proc_entry = create_proc_entry("named_devices", S_IFREG | S_IRUGO,
                                   rtdm_proc_root);
    if (!proc_entry)
        goto proc_err;
    proc_entry->read_proc = proc_read_named_devs;

    proc_entry = create_proc_entry("protocol_devices", S_IFREG | S_IRUGO,
                                   rtdm_proc_root);
    if (!proc_entry)
        goto proc_err;
    proc_entry->read_proc  = proc_read_proto_devs;

    proc_entry = create_proc_entry("open_fildes", S_IFREG | S_IRUGO , rtdm_proc_root);
    if (!proc_entry)
        goto proc_err;
    proc_entry->read_proc = proc_read_open_fildes;

    proc_entry = create_proc_entry("fildes", S_IFREG | S_IRUGO , rtdm_proc_root);
    if (!proc_entry)
        goto proc_err;
    proc_entry->read_proc = proc_read_fildes;
#endif /* CONFIG_PROC_FS */

    /* Initialise LXRT function table if LXRT is available */
    rt_base_linux_task = rt_get_base_linux_task(rt_linux_task);
    if (rt_base_linux_task->task_trap_handler[0]) {
        ret = ((int (*)(void *, int))rt_base_linux_task->task_trap_handler[0])(
            lxrt_fun_entry, RTDM_LXRT_IDX);
        if (ret) {
            ERR_PK("RTDM : error registering LXRT functions\n");
            goto rem_all;
        }
    }

    return 0;

#ifdef CONFIG_PROC_FS
  proc_err:
    ERR_PK("RTDM: error while creating proc entries\n");
    ret = -EAGAIN;
#endif /* CONFIG_PROC_FS */

  rem_all:
#ifdef CONFIG_PROC_FS
    remove_proc_entry("fildes", rtdm_proc_root);
    remove_proc_entry("open_fildes", rtdm_proc_root);
    remove_proc_entry("protocol_devices", rtdm_proc_root);
    remove_proc_entry("named_devices", rtdm_proc_root);
    remove_proc_entry("rtdm", rtdm_proc_root);
#endif /* CONFIG_PROC_FS */

    kfree(rtdm_protocol_devices);

  rem_named_devs:
    kfree(rtdm_named_devices);

  rem_fildes:
    kfree(fildes_table);

    return ret;
}



/***********************************************************************
    Module removal.
************************************************************************/
void cleanup_module(void)
{
#ifdef CONFIG_PROC_FS
    /* remove proc entries */
    remove_proc_entry("fildes", rtdm_proc_root);
    remove_proc_entry("open_fildes", rtdm_proc_root);
    remove_proc_entry("protocol_devices", rtdm_proc_root);
    remove_proc_entry("named_devices", rtdm_proc_root);
    remove_proc_entry("rtdm", rtdm_proc_root);
#endif /* CONFIG_PROC_FS */

    /* release file descriptor table */
    if (open_fildes > 0)
        printk("RTDM: WARNING - %d unreleased file descriptors!\n",
               open_fildes);
    kfree(fildes_table);

    kfree(rtdm_named_devices);
    kfree(rtdm_protocol_devices);

    /* unregister LXRT functions */
    if (rt_base_linux_task->task_trap_handler[1])
        ((int (*)(void *, int))rt_base_linux_task->task_trap_handler[1])(
            lxrt_fun_entry, RTDM_LXRT_IDX);

    printk("RTDM: unloaded\n");
    return;
}


EXPORT_SYMBOL(rtdm_dev_register);
EXPORT_SYMBOL(rtdm_dev_unregister);

EXPORT_SYMBOL(rtdm_open);
EXPORT_SYMBOL(rtdm_socket);
EXPORT_SYMBOL(rtdm_close);
EXPORT_SYMBOL(rtdm_ioctl);
EXPORT_SYMBOL(rtdm_read);
EXPORT_SYMBOL(rtdm_write);
EXPORT_SYMBOL(rtdm_recvmsg);
EXPORT_SYMBOL(rtdm_sendmsg);
