#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>


#define VENDOR_ID 0x1b3f
#define DEVICE_ID 0x2247
#define DRV_NAME "webcam_driver"

// urb settings
#define NUM_URBS                8
#define NUM_PACKETS_PER_URB     32


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
};

static void urb_complete_handler(struct urb *urb)
{
    struct webcam_logger *dev = urb->context;

    for(int i = 0; i < urb->number_of_packets;; i++)
    {
        struct usb_iso_packet_descriptor *desc = &urb->iso_frame_desc[i];

        if(desc->status != 0)
        {
            pr_warn(DRV_NAME ": ISO packet %d error: %d\n", i, desc->status);
            continue;
        }

        if(desc->actual_length > 0)
        {
            u8 *data = urb->transfer_buffer + desc->offset;
            pr_info(DRV_NAME " got packet, num: [%d], len: %d, data: %*ph\n",
                i, desc->actual_length, min(16, desc->actual_length), data);
        }
    }

    int ret = usb_submit_urb(urb, GFP_ATOMIC);
    if(ret)
    {
        pr_err(DRV_NAME, " : failed to resubmet: %d\n", ret);
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
            kree(buf);
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
        ret = urb_submit_urb(urb, GFP_KERNEL);
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
    int i;

    pr_info(DRV_NAME " probe called for device : %s:%s\n",
            udev->manufacturer ? udev->manufacturer : "Unknown",
            udev->product ? udev->product : "Unknown");
    
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if(!dev)
    {
        return -ENOMEM;
    }

    dev->udev = usb_get_dev(udev);
    dev->interface = interface;

    for(i = 0; i < dev->interface->num_altsetting; i++)
    {
        pr_info(DRV_NAME " : altset num : %d\n", i);
        struct usb_host_interface *temp = &dev->interface->altsetting[i];
        for(int j = 0; j < temp->desc.bNumEndpoints; j++)
        {
            struct usb_endpoint_descriptor *temp_desc = &temp->endpoint[j].desc;
            pr_info(DRV_NAME " : j = %d, type: 0x%x, address: 0x%02x, maxsize: %d\n",
                    j,
                    temp_desc->bmAttributes,
                    temp_desc->bEndpointAddress,
                    le16_to_cpu(temp_desc->wMaxPacketSize));
        }
    }
    
    struct usb_host_interface *cur_alt = interface->cur_altsetting;
    int alt_settings = 3;

    retval = usb_set_interface(udev, cur_alt->desc.bInterfaceNumber, alt_settings);
    if (retval) {
        pr_info(DRV_NAME ": not allowed to switch altsetting: %d\n", retval);
    }
    
    pr_info(DRV_NAME ": curr setting review and choosing endpoint:\n");
    cur_alt = interface->cur_altsetting;

    for(int i = 0; i < cur_alt->desc.bInterfaceNumber; i++)
    {
        struct usb_endpoint_descriptor *temp_desc = &cur_alt->endpoint[i].desc;
        pr_info(DRV_NAME " : i = %d, type: 0x%x, address: 0x%02x, maxsize: %d\n",
            i,
            temp_desc->bmAttributes,
            temp_desc->bEndpointAddress,
            le16_to_cpu(temp_desc->wMaxPacketSize));
        if(usb_endpoint_xfer_isoc(temp_desc))
        {
            dev->ep = &cur_alt->endpoint[i];
            pr_info(DRV_NAME " : found isoc ep, type: 0x%x, address: 0x%02x, maxsize: %d\n",
                dev->ep->desc.bmAttributes,
                dev->ep->desc.bEndpointAddress,
                le16_to_cpu(dev->ep->desc.wMaxPacketSize));
            if (usb_endpoint_dir_in(&dev->ep->desc)) {
                pr_info(DRV_NAME ": that endpoint is IN type\n");
            } else if (usb_endpoint_dir_out(&dev->ep->desc)) {
                pr_info(DRV_NAME ": that endpoint is OUT type\n");
            } else {
                pr_info(DRV_NAME ": error checking in/out type\n");
            }
        }
    }

    if(!dev->ep)
    {
        pr_err(DRV_NAME " could not find interrupt IN endpoint\n");
        retval = -ENODEV;
    }

    dev->data = kmalloc(usb_endpoint_maxp(dev->ep->desc));
    if(!dev->data)
    {
        retval = -ENOMEM;
    }

    dev->urb = usb_alloc_urb(0, GFP_KERNEL);
    if(!dev->urb)
    {
        retval = -ENOMEM;
    }
    

    return retval;
}

static void webcam_logger_disconnect(struct usb_interface *interface)
{
    pr_info(DRV_NAME " device successfullt disconnected\n");
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