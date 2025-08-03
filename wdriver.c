#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>


#define VENDOR_ID 0x1b3f
#define DEVICE_ID 0x2247
#define DRV_NAME "webcam_driver"

// urb settings
#define NUM_URBS                8
#define NUM_PACKETS_PER_URB     32

// frame setting
#define MAX_FRAME_SIZE (1024 * 1024) // 1 MB
static int frame_counter = 0;


static struct usb_device_id webcam_logger_table[] = {
    { USB_DEVICE_AND_INTERFACE_INFO(VENDOR_ID, DEVICE_ID, USB_CLASS_VIDEO, 2, 0) },
    {}
};
MODULE_DEVICE_TABLE(usb, webcam_logger_table);


struct webcam_urb {
    struct urb *urb;
    u8 *buffer;
};

struct webcam_logger {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct usb_host_endpoint *ep;
    unsigned char *data;
    struct webcam_urb urbs[NUM_URBS];

    u8 *frame_buf;
    int frame_len;
    bool capturing;
    u8 prev_byte;
};

static void save_to_file(const char *path, const u8 *data, size_t len)
{
    struct file *filp;
    loff_t pos = 0;
    ssize_t ret;

    filp = filp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(filp)) {
        pr_err("webcam_driver: Cannot open file: %s, err = %ld\n", path, PTR_ERR(filp));
        return;
    }

    // write directly from kernel buffer
    ret = kernel_write(filp, data, len, &pos);
    if (ret < 0)
        pr_err("webcam_driver: Failed to write file %s, err = %zd\n", path, ret);

    filp_close(filp, NULL);
}

static void urb_complete_handler(struct urb *urb)
{
    struct webcam_logger *dev = urb->context;
    
    for(int i = 0; i < urb->number_of_packets; i++)
    {
        struct usb_iso_packet_descriptor *desc = &urb->iso_frame_desc[i];
        
        if(desc->status != 0)
        {
            pr_warn(DRV_NAME ": ISO packet %d error: %d\n", i, desc->status);
            continue;
        }
        
        if(desc->actual_length > 12) 
        {
            u8 *data = urb->transfer_buffer + desc->offset;
            
            int header_len = 12;
            u8 *payload = data + header_len;
            int payload_len = desc->actual_length - header_len;
            
            pr_info(DRV_NAME " got payload, len: %d (total: %d)\n", payload_len, desc->actual_length);
            
            pr_info(DRV_NAME " UVC header: %02x %02x %02x %02x\n", 
                    data[0], data[1], data[2], data[3]);
            
            if(payload_len > 0)
            {
                for(int j = 0; j < payload_len; j++)
                {
                    u8 byte = payload[j];
                    
                    // JPEG (FF D8)
                    if(!dev->capturing && dev->prev_byte == 0xFF && byte == 0xD8)
                    {
                        dev->capturing = true;
                        dev->frame_len = 0;
                        dev->frame_buf[dev->frame_len++] = 0xFF;
                        dev->frame_buf[dev->frame_len++] = 0xD8;
                        pr_info(DRV_NAME ": JPEG start detected\n");
                    }
                    else if (dev->capturing)
                    {
                        if(dev->frame_len < MAX_FRAME_SIZE)
                        {
                            dev->frame_buf[dev->frame_len++] = byte;
                        }
                        
                        // JPEG (FF D9)
                        if(dev->prev_byte == 0xFF && byte == 0xD9)
                        {
                            dev->capturing = false;
                            char filename[64];
                            snprintf(filename, sizeof(filename), "/home/timur/test/frame_%03d.jpg", frame_counter++);
                            save_to_file(filename, dev->frame_buf, dev->frame_len);
                            pr_info(DRV_NAME ": JPEG end detected, saved %d bytes\n", dev->frame_len);
                        }
                    }
                    dev->prev_byte = byte;
                }
            }
        }
        else if(desc->actual_length == 12)
        {
            // просто UVC заголовок без даних - ignore
            u8 *header = urb->transfer_buffer + desc->offset;
            // pr_info(DRV_NAME " UVC header only: %02x %02x\n", header[0], header[1]);
        }
    }
    
    int ret = usb_submit_urb(urb, GFP_ATOMIC);
    if(ret)
    {
        pr_err(DRV_NAME ": failed to resubmit URB: %d\n", ret);
    }
}


int setup_iso_urbs(struct webcam_logger *dev)
{
    int i, j, ret;
    int packet_size, buffer_size;
    struct usb_device *udev = dev->udev;
    struct usb_host_endpoint *ep = dev->ep;
    int ep_addr = ep->desc.bEndpointAddress;

    // 1. size of iso packet
    packet_size = le16_to_cpu(ep->desc.wMaxPacketSize);
    buffer_size = packet_size * NUM_PACKETS_PER_URB;

    for(int i = 0; i < NUM_URBS; i++)
    {
        struct urb *urb;
        unsigned char *buf;

        // 2. alloc buffer for all packets in URB
        buf = kmalloc(buffer_size, GFP_KERNEL);
        if(!buf)
        {
            goto error;
        }

        // 3. usb creating
        urb = usb_alloc_urb(NUM_PACKETS_PER_URB, GFP_KERNEL);
        if(!urb)
        {
            kfree(buf);
            goto error;
        }

        // 4. fill URBS
        urb->dev = udev;
        urb->pipe = usb_rcvisocpipe(udev, ep_addr);
        urb->interval = ep->desc.bInterval;
        urb->transfer_flags = 0; // no DMA for now
        urb->transfer_buffer = buf;
        urb->transfer_buffer_length = buffer_size;
        urb->complete = urb_complete_handler;
        urb->context = dev;
        urb->number_of_packets = NUM_PACKETS_PER_URB;

        for(j = 0; j < NUM_PACKETS_PER_URB; j++)
        {
            urb->iso_frame_desc[j].offset = j * packet_size; // offset in bytes (where to write that packet)
            urb->iso_frame_desc[j].length = packet_size;     // how many bytes is allowed to write
            // urb->iso_frame_desc[j].status                 // status after the end (success of getting or no)
            // urb->iso_frame_desc[j].actual_length          // how many bytes really we got
        }

        // 5. save struct
        dev->urbs[i].urb = urb;
        dev->urbs[i].buffer = buf;

        // 6. sent urb to host-contrller
        ret = usb_submit_urb(urb, GFP_KERNEL);
        if(ret)
        {
            pr_err(DRV_NAME, " : submit URB %d failed: %d\n", i, ret);
            goto error;
        }
    }

    return 0;

error:
    for(j = 0; j < NUM_URBS; j++)   
    {
        if(dev->urbs[j].urb)
        {
            usb_kill_urb(dev->urbs[j].urb);
            usb_free_urb(dev->urbs[j].urb);
        }
        kfree(dev->urbs[j].buffer);
    }
    return -ENOMEM;
}


static int webcam_logger_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct webcam_logger *dev;
    int retval = 0;

    pr_info(DRV_NAME " probe called for device : %s:%s\n",
            udev->manufacturer ? udev->manufacturer : "Unknown",
            udev->product ? udev->product : "Unknown");
    
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->udev = usb_get_dev(udev);
    dev->interface = interface;

    for (int i = 0; i < dev->interface->num_altsetting; i++) 
    {
        struct usb_host_interface *alt = &dev->interface->altsetting[i];
        pr_info(DRV_NAME ": altset num : %d\n", i);
        for (int j = 0; j < alt->desc.bNumEndpoints; j++)
        {
            struct usb_endpoint_descriptor *d = &alt->endpoint[j].desc;
            pr_info(DRV_NAME ": ep %d, type: 0x%x, addr: 0x%02x, maxsize: %d\n",
                    j, d->bmAttributes, d->bEndpointAddress, le16_to_cpu(d->wMaxPacketSize));
        }
    }

    int alt_settings = 3;
    retval = usb_set_interface(udev, interface->cur_altsetting->desc.bInterfaceNumber, alt_settings);
    if (retval) 
    {
        pr_info(DRV_NAME ": usb_set_interface failed: %d\n", retval);
        goto fail_free;
    }

    struct usb_host_interface *cur_alt = interface->cur_altsetting;
    for (int i = 0; i < cur_alt->desc.bNumEndpoints; i++)
    {
        struct usb_endpoint_descriptor *desc = &cur_alt->endpoint[i].desc;
        if (usb_endpoint_xfer_isoc(desc))
        {
            dev->ep = &cur_alt->endpoint[i];
            pr_info(DRV_NAME ": found isoc ep: addr=0x%02x size=%d\n",
                    desc->bEndpointAddress,
                    le16_to_cpu(desc->wMaxPacketSize));
            break;
        }
    }

    if (!dev->ep) {
        pr_err(DRV_NAME ": no isoc endpoint found\n");
        retval = -ENODEV;
        goto fail_free;
    }

    dev->data = kmalloc(usb_endpoint_maxp(&dev->ep->desc), GFP_KERNEL);
    if (!dev->data) {
        retval = -ENOMEM;
        goto fail_free;
    }

    dev->frame_buf = kmalloc(MAX_FRAME_SIZE, GFP_KERNEL);
    dev->frame_len = 0;
    dev->capturing = false;
    dev->prev_byte = 0;

    retval = setup_iso_urbs(dev);
    if (retval) {
        pr_err(DRV_NAME ": failed to setup urbs\n");
        goto fail_data;
    }

    usb_set_intfdata(interface, dev);
    return 0;

fail_data:
    kfree(dev->data);
fail_free:
    usb_put_dev(dev->udev);
    kfree(dev);
    return retval;
}


static void webcam_logger_disconnect(struct usb_interface *interface)
{
    struct webcam_logger *dev = usb_get_intfdata(interface);

    pr_info(DRV_NAME " device successfully disconnected\n");

    if (!dev)
        return;

    for (int i = 0; i < NUM_URBS; i++) {
        if (dev->urbs[i].urb) {
            usb_kill_urb(dev->urbs[i].urb);
            usb_free_urb(dev->urbs[i].urb);
            kfree(dev->urbs[i].buffer);
        }
    }

    kfree(dev->frame_buf);
    kfree(dev->data);

    usb_put_dev(dev->udev);
    kfree(dev);
}


static struct usb_driver webcam_logger_driver = {
    .name = DRV_NAME,
    .id_table = webcam_logger_table,
    .probe = webcam_logger_probe,
    .disconnect = webcam_logger_disconnect,
};

module_usb_driver(webcam_logger_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Timur5050");
MODULE_DESCRIPTION("Simple USB WEBCAM logger driver");