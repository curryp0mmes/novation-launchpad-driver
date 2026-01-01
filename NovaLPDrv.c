/*
 * Modernized Driver for Novation Launchpad (NVLPD01)
 * Based on work by Vincent Deca (2012-2018)
 * Updated for Kernel 6.x+ 
 * Licensed under GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include "protocol.h"

#define DRIVER_NAME     "novalpdrv"
#define VENDOR_ID       0x1235
#define PRODUCT_ID      0x000e
#define DATA_BUF_SIZE   256
#define WRITE_BUF_SIZE  8

struct novation_lp {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct usb_endpoint_descriptor *in_endpoint;
    struct usb_endpoint_descriptor *out_endpoint;
    struct urb *urb_in;
    unsigned char *in_buffer;
    unsigned char *data_buffer;
    size_t data_pos;
    struct mutex io_mutex;
    wait_queue_head_t wait_q;
    bool disconnected;
    bool is_open;
    unsigned char last_stat;
};

static struct usb_device_id lp_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, lp_table);

/* Forward declaration for the driver struct */
static struct usb_driver lp_driver;

static void lp_read_callback(struct urb *urb) {
    struct novation_lp *dev = urb->context;
    int status = urb->status;
    int len = urb->actual_length;
    unsigned char *data = urb->transfer_buffer;

    if (status) {
        if (!(status == -ENOENT || status == -ECONNRESET || status == -ESHUTDOWN))
            dev_err(&dev->interface->dev, "Read URB error: %d\n", status);
        return;
    }

    if (len > 0) {
        mutex_lock(&dev->io_mutex);
        if (dev->data_pos + len < DATA_BUF_SIZE) {
            memcpy(&dev->data_buffer[dev->data_pos], data, len);
            dev->data_pos += len;
        }
        for (int i = 0; i < len; i++) {
            if (data[i] == LP_MENU || data[i] == LP_GRID)
                dev->last_stat = data[i];
        }
        mutex_unlock(&dev->io_mutex);
        wake_up_interruptible(&dev->wait_q);
    }

    if (!dev->disconnected) {
        if (usb_submit_urb(dev->urb_in, GFP_ATOMIC))
            dev_err(&dev->interface->dev, "Resubmit URB failed\n");
    }
}

static void lp_write_callback(struct urb *urb) {
    struct novation_lp *dev = urb->context;
    if (urb->status && !(urb->status == -ENOENT || urb->status == -ECONNRESET || urb->status == -ESHUTDOWN))
        dev_err(&dev->interface->dev, "Write URB error: %d\n", urb->status);
    usb_free_coherent(urb->dev, urb->transfer_buffer_length, urb->transfer_buffer, urb->transfer_dma);
}

static int lp_open(struct inode *inode, struct file *file) {
    struct novation_lp *dev;
    struct usb_interface *interface;
    int subminor = iminor(inode);

    /* FIX: Simplified interface lookup */
    interface = usb_find_interface(&lp_driver, subminor);
    if (!interface) return -ENODEV;

    dev = usb_get_intfdata(interface);
    if (!dev) return -ENODEV;

    mutex_lock(&dev->io_mutex);
    if (dev->is_open) {
        mutex_unlock(&dev->io_mutex);
        return -EBUSY;
    }

    dev->is_open = true;
    dev->data_pos = 0;
    file->private_data = dev;
    mutex_unlock(&dev->io_mutex);

    return 0;
}

static int lp_release(struct inode *inode, struct file *file) {
    struct novation_lp *dev = file->private_data;
    if (dev) {
        mutex_lock(&dev->io_mutex);
        dev->is_open = false;
        mutex_unlock(&dev->io_mutex);
    }
    return 0;
}

static ssize_t lp_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos) {
    struct novation_lp *dev = file->private_data;
    size_t avail;
    int retval = 0;

    mutex_lock(&dev->io_mutex);
    if (dev->disconnected) {
        mutex_unlock(&dev->io_mutex);
        return -ENODEV;
    }

    if (dev->data_pos == 0) {
        mutex_unlock(&dev->io_mutex);
        if (file->f_flags & O_NONBLOCK) return -EAGAIN;
        if (wait_event_interruptible(dev->wait_q, dev->data_pos > 0 || dev->disconnected))
            return -ERESTARTSYS;
        mutex_lock(&dev->io_mutex);
    }

    avail = min(count, dev->data_pos);
    if (copy_to_user(buffer, dev->data_buffer, avail)) {
        retval = -EFAULT;
    } else {
        if (avail < dev->data_pos)
            memmove(dev->data_buffer, &dev->data_buffer[avail], dev->data_pos - avail);
        dev->data_pos -= avail;
        retval = avail;
    }

    mutex_unlock(&dev->io_mutex);
    return retval;
}

static ssize_t lp_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos) {
    struct novation_lp *dev = file->private_data;
    struct urb *urb;
    unsigned char *buf;
    int retval;

    if (count < 1 || count > WRITE_BUF_SIZE) return -EINVAL;

    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb) return -ENOMEM;

    buf = usb_alloc_coherent(dev->udev, count, GFP_KERNEL, &urb->transfer_dma);
    if (!buf) {
        usb_free_urb(urb);
        return -ENOMEM;
    }

    if (copy_from_user(buf, user_buf, count)) {
        retval = -EFAULT;
        goto error;
    }

    mutex_lock(&dev->io_mutex);
    if (dev->disconnected) {
        mutex_unlock(&dev->io_mutex);
        retval = -ENODEV;
        goto error;
    }

    usb_fill_int_urb(urb, dev->udev, 
                     usb_sndintpipe(dev->udev, dev->out_endpoint->bEndpointAddress),
                     buf, count, lp_write_callback, dev, dev->out_endpoint->bInterval);
    
    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    retval = usb_submit_urb(urb, GFP_KERNEL);
    mutex_unlock(&dev->io_mutex);

    if (retval) goto error;

    usb_free_urb(urb);
    return count;

error:
    usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
    usb_free_urb(urb);
    return retval;
}

static __poll_t lp_poll(struct file *file, poll_table *wait) {
    struct novation_lp *dev = file->private_data;
    __poll_t mask = 0;
    poll_wait(file, &dev->wait_q, wait);
    mutex_lock(&dev->io_mutex);
    if (dev->data_pos > 0) mask |= POLLIN | POLLRDNORM;
    if (dev->disconnected) mask |= POLLHUP | POLLERR;
    mutex_unlock(&dev->io_mutex);
    return mask;
}

static const struct file_operations lp_fops = {
    .owner = THIS_MODULE,
    .open = lp_open,
    .release = lp_release,
    .read = lp_read,
    .write = lp_write,
    .poll = lp_poll,
};

static struct usb_class_driver lp_class = {
    .name = "nlp%d",
    .fops = &lp_fops,
    .minor_base = 0,
};

static int lp_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(interface);
    struct novation_lp *dev;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int retval;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;

    mutex_init(&dev->io_mutex);
    /* FIX: Corrected function name */
    init_waitqueue_head(&dev->wait_q);

    dev->udev = udev;
    dev->interface = interface;
    dev->data_buffer = kzalloc(DATA_BUF_SIZE, GFP_KERNEL);
    
    iface_desc = interface->cur_altsetting;
    for (int i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;
        if (usb_endpoint_is_int_in(endpoint)) dev->in_endpoint = endpoint;
        if (usb_endpoint_is_int_out(endpoint)) dev->out_endpoint = endpoint;
    }

    if (!dev->in_endpoint || !dev->out_endpoint) {
        retval = -ENODEV;
        goto error;
    }

    dev->in_buffer = kmalloc(le16_to_cpu(dev->in_endpoint->wMaxPacketSize), GFP_KERNEL);
    dev->urb_in = usb_alloc_urb(0, GFP_KERNEL);

    usb_fill_int_urb(dev->urb_in, udev,
                     usb_rcvintpipe(udev, dev->in_endpoint->bEndpointAddress),
                     dev->in_buffer, le16_to_cpu(dev->in_endpoint->wMaxPacketSize),
                     lp_read_callback, dev, dev->in_endpoint->bInterval);

    usb_set_intfdata(interface, dev);
    retval = usb_register_dev(interface, &lp_class);
    if (retval) goto error;

    retval = usb_submit_urb(dev->urb_in, GFP_KERNEL);
    if (retval) goto error;

    return 0;

error:
    if (dev->urb_in) usb_free_urb(dev->urb_in);
    kfree(dev->in_buffer);
    kfree(dev->data_buffer);
    kfree(dev);
    return retval;
}

static void lp_disconnect(struct usb_interface *interface) {
    struct novation_lp *dev = usb_get_intfdata(interface);
    if (dev) {
        mutex_lock(&dev->io_mutex);
        dev->disconnected = true;
        mutex_unlock(&dev->io_mutex);
        usb_deregister_dev(interface, &lp_class);
        if (dev->urb_in) usb_kill_urb(dev->urb_in);
        usb_set_intfdata(interface, NULL);
        kfree(dev->in_buffer);
        kfree(dev->data_buffer);
        usb_free_urb(dev->urb_in);
        kfree(dev);
    }
}

static struct usb_driver lp_driver = {
    .name = DRIVER_NAME,
    .probe = lp_probe,
    .disconnect = lp_disconnect,
    .id_table = lp_table,
};

module_usb_driver(lp_driver);
MODULE_LICENSE("GPL");
