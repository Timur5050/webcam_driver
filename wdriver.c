#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>


#define VENDOR_ID 0x1b3f
#define DEVICE_ID 0x2247
#define DRV_NAME "webcam_driver"


static struct usb_device_id webcam_logger_table[] = {
    { USB_DEVICE_AND_INTERFACE_INFO(VENDOR_ID, DEVICE_ID, USB_CLASS_VIDEO, 1, 0) },
    {}
};
MODULE_DEVICE_TABLE(usb, webcam_logger_table);

struct webcam_logger {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct usb_endpoint_descriptor *ep_desc;
    unsigned char *data;
    struct urb *urb;
};

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

    for(i = 0; i < dev->interace->num_altsetting; i++)
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
